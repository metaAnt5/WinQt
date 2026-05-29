#include "KBarRpcService.h"

#include <asio/io_context.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/detached.hpp>
#include <asio/redirect_error.hpp>
#include <future>
#include <thread>
#include <chrono>

// ============================================================
// 引入 NetCore RPC 组件
// ============================================================
#include "NetCore/IoContextManager.h"
#include "NetCore/Client.h"
#include "NetCore/Connection.h"
#include "NetCore/RpcMessage.h"
#include "NetCore/RpcTypes.h"
#include "NetCore/ProcessCfgMessage.h"
#include "NetCore/BinaryProtocol.h"
#include "NetCore/Logger.h"

using namespace NetCore;

// ============================================================
// KBarRpcService::Impl - 内部实现（隐藏 NetCore 依赖）
// ============================================================
class KBarRpcService::Impl {
public:
    Impl(const Config& config)
        : config_(config)
        , io_mgr_(std::make_shared<IoContextManager>()) {
    }

    ~Impl() {
        stop();
    }

    bool start() {
        if (running_) return true;

        if (!io_mgr_->start()) {
            if (on_error_) on_error_("Failed to start IoContextManager");
            return false;
        }

        running_ = true;

        // 在 io_context 上发起连接和读循环
        asio::co_spawn(io_mgr_->get_io_context(),
            [this]() -> asio::awaitable<void> {
                co_await connect_and_read_loop();
            },
            asio::detached);

        return true;
    }

    void stop() {
        running_ = false;
        if (client_) {
            client_->disconnect();
            client_.reset();
        }
        if (io_mgr_) {
            io_mgr_->stop();
        }
    }

    // ---- 数据接口（同步方式，内部用 async + promise）----
    bool fetch_kbars(const std::string& symbol, int timeFrame, std::vector<KBar>& out) {
        return do_fetch(symbol, timeFrame, 0, out);
    }

    bool fetch_latest_kbars(const std::string& symbol, int timeFrame, size_t count, std::vector<KBar>& out) {
        return do_fetch(symbol, timeFrame, static_cast<uint16_t>(count), out);
    }

    KBar fetch_latest_kbar(const std::string& symbol, int timeFrame) {
        std::vector<KBar> bars;
        if (do_fetch(symbol, timeFrame, 1, bars) && !bars.empty()) {
            return bars.back();
        }
        return KBar{};
    }

    // ---- 回调设置 ----
    void set_on_kbar_pushed(std::function<void(const KBar&)> cb) { on_kbar_pushed_ = std::move(cb); }
    void set_on_connection_changed(std::function<void(bool)> cb) { on_connection_changed_ = std::move(cb); }
    void set_on_error(std::function<void(const std::string&)> cb) { on_error_ = std::move(cb); }

private:
    Config config_;
    std::shared_ptr<IoContextManager> io_mgr_;
    std::shared_ptr<Client> client_;
    std::atomic<bool> running_{false};

    std::function<void(const KBar&)> on_kbar_pushed_;
    std::function<void(bool)> on_connection_changed_;
    std::function<void(const std::string&)> on_error_;

    // 同步等待响应
    bool do_fetch(const std::string& symbol, int timeFrame, uint16_t count, std::vector<KBar>& out) {
        if (!running_) return false;

        std::promise<bool> promise;
        auto future = promise.get_future();

        asio::co_spawn(io_mgr_->get_io_context(),
            [this, symbol, timeFrame, count, &out, promise = std::move(promise)]() mutable
            -> asio::awaitable<void> {
                try {
                    IResponse resp = co_await async_fetch(symbol, timeFrame, count);
                    if (resp.error_code() == 0) {
                        out = GetKbarsResponse<KBar>::from_payload(resp.payload()).bars;
                        promise.set_value(true);
                    } else {
                        promise.set_value(false);
                    }
                } catch (...) {
                    promise.set_value(false);
                }
            },
            asio::detached);

        return future.get();
    }

    // 异步执行 RPC 请求
    asio::awaitable<IResponse> async_fetch(const std::string& symbol, int timeFrame, uint16_t count) {
        // 确保已连接
        if (!ensure_connected()) {
            co_return IResponse(0, 0, METHOD_GET_KBARS, {}, RpcError::NOT_CONNECTED);
        }

        auto conn = client_->GetConnection();
        if (!conn || conn->state() != ConnectionState::CONNECTED) {
            co_return IResponse(0, 0, METHOD_GET_KBARS, {}, RpcError::NOT_CONNECTED);
        }

        // 构建请求 payload（使用协议封装类）
        auto payload = GetKbarsRequest{symbol, timeFrame, count}.to_payload();

        // 组装 RPC 请求
        static std::atomic<uint32_t> s_req_id{1};
        uint32_t req_id = s_req_id.fetch_add(1, std::memory_order_relaxed);

        auto req_msg = std::make_shared<IRequest>(0, METHOD_GET_KBARS, payload);
        req_msg->set_flags(static_cast<uint8_t>(RpcFlag::REQUEST));
        req_msg->set_req_id(req_id);

        // 使用 BinaryPacker 发送
        BinaryPacker packer;
        IMessage::Ptr imsg = std::static_pointer_cast<IMessage>(req_msg);
        if (!packer.pack(conn, imsg)) {
            co_return IResponse(req_id, 0, METHOD_GET_KBARS, {}, RpcError::SEND_FAILED);
        }

        // 等待响应（在 read_loop 中匹配 req_id）
        auto response = co_await wait_for_response(req_id, config_.request_timeout_ms);

        co_return response;
    }

    asio::awaitable<IResponse> wait_for_response(uint32_t req_id, int timeout_ms) {
        // 使用 promise/future 模式在 read_loop 中等待匹配的响应
        auto promise = std::make_shared<std::promise<IResponse>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_responses_[req_id] = [promise](IResponse resp) {
                promise->set_value(std::move(resp));
            };
        }

        // 设置超时
        auto timer = std::make_shared<asio::steady_timer>(io_mgr_->get_io_context());
        timer->expires_after(std::chrono::milliseconds(timeout_ms));
        timer->async_wait([this, req_id, timer](const asio::error_code& ec) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                auto it = pending_responses_.find(req_id);
                if (it != pending_responses_.end()) {
                    auto cb = std::move(it->second);
                    pending_responses_.erase(it);
                    cb(IResponse(req_id, 0, METHOD_GET_KBARS, {}, RpcError::TIMEOUT));
                }
            }
        });

        // 阻塞等待
        IResponse resp = future.get();
        co_return resp;
    }

    bool ensure_connected() {
        if (client_ && client_->state() == ConnectionState::CONNECTED) {
            return true;
        }

        client_ = std::make_shared<Client>(io_mgr_->get_io_context());
        auto conn = client_->GetConnection();

        // 同步连接（阻塞）
        std::promise<bool> conn_promise;
        auto conn_future = conn_promise.get_future();

        asio::co_spawn(io_mgr_->get_io_context(),
            [this, promise = std::move(conn_promise)]() mutable
            -> asio::awaitable<void> {
                bool ok = co_await client_->async_connect(config_.host, config_.port);
                if (ok) {
                    // 启动读循环
                    asio::co_spawn(io_mgr_->get_io_context(),
                        [this]() -> asio::awaitable<void> {
                            co_await read_loop();
                        },
                        asio::detached);
                }
                promise.set_value(ok);
            },
            asio::detached);

        bool ok = conn_future.get();
        if (ok && on_connection_changed_) {
            on_connection_changed_(true);
        }
        return ok;
    }

    asio::awaitable<void> read_loop() {
        auto conn = client_->GetConnection();
        if (!conn) co_return;

        auto factory = std::make_shared<RpcFactory>();
        BinaryUnpacker unpacker(factory);
        IMessage::Ptr msg;

        while (running_ && conn->state() == ConnectionState::CONNECTED) {
            try {
                // 等待至少头部大小（6 字节），与服务器协议一致
                // HEADER_SIZE = PktLen(4) + Type(2) = 6
                size_t avail = co_await conn->async_wait_available(BinaryUnpacker::HEADER_SIZE);
                if (avail == 0) {
                    break;
                }

                // 先 peek 头部获取完整包长度，等数据到齐后再拆包（与服务器逻辑一致）
                uint32_t pack_size = 0;
                uint16_t type = 0;
                while (unpacker.try_read_header(conn, pack_size, type)) {
                    co_await conn->async_wait_available(BinaryUnpacker::HEADER_SIZE + pack_size);
                    unpacker.unpack(conn, msg);
                    if (!msg) continue;

                    // 检查是否是 IRequest (RPC 消息)
                    auto rpc_req = std::dynamic_pointer_cast<IRequest>(msg);
                    if (rpc_req) {
                        if (rpc_req->is_response()) {
                            // ---- 响应：按 req_id 分发给等待者 ----
                            uint32_t resp_req_id = rpc_req->req_id();
                            IResponse response(
                                resp_req_id,
                                rpc_req->process_id(),
                                rpc_req->method_id(),
                                rpc_req->payload(),
                                static_cast<RpcError>(rpc_req->error_code()));

                            std::lock_guard<std::mutex> lock(pending_mutex_);
                            auto it = pending_responses_.find(resp_req_id);
                            if (it != pending_responses_.end()) {
                                auto cb = std::move(it->second);
                                pending_responses_.erase(it);
                                cb(std::move(response));
                            }
                        }
                        else if (rpc_req->is_message()) {
                            // ---- 推送消息：根据 method_id 解析 payload ----
                            uint16_t method_id = rpc_req->method_id();
                            if (method_id == METHOD_PUSH_KBAR && on_kbar_pushed_) {
                                // PUSH_KBAR payload 是 KBar 的二进制序列化
                                const auto& payload = rpc_req->payload();
                                if (!payload.empty()) {
                                    size_t offset = 0;
                                    KBar kbar = KBar::deserialize(payload, offset);
                                    on_kbar_pushed_(kbar);
                                }
                            }
                        }
                    }

                    msg.reset();
                }
            }
            catch (const std::exception& e) {
                if (on_error_) {
                    on_error_(std::string("Read error: ") + e.what());
                }
                break;
            }
        }

        // 连接断开
        if (on_connection_changed_) {
            on_connection_changed_(false);
        }

        // 清理所有等待的请求
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [id, cb] : pending_responses_) {
            cb(IResponse(id, 0, 0, {}, RpcError::CONNECTION_LOST));
        }
        pending_responses_.clear();

        // 如果是自动重连
        if (running_ && config_.auto_reconnect) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
            if (running_) {
                asio::co_spawn(io_mgr_->get_io_context(),
                    [this]() -> asio::awaitable<void> {
                        co_await connect_and_read_loop();
                    },
                    asio::detached);
            }
        }

        co_return;
    }

    asio::awaitable<void> connect_and_read_loop() {
        while (running_) {
            if (ensure_connected()) {
                co_await read_loop();
            }
            if (running_ && config_.auto_reconnect) {
                co_await async_sleep(config_.reconnect_interval_ms);
            } else {
                break;
            }
        }
    }

    asio::awaitable<void> async_sleep(int ms) {
        asio::steady_timer timer(io_mgr_->get_io_context());
        timer.expires_after(std::chrono::milliseconds(ms));
        asio::error_code ec;
        co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
    }

    std::mutex pending_mutex_;
    std::unordered_map<uint32_t, std::function<void(IResponse)>> pending_responses_;
};

// ============================================================
// KBarRpcService 公开接口实现
// ============================================================

KBarRpcService::KBarRpcService(const Config& config)
    : config_(config)
    , impl_(std::make_unique<Impl>(config)) {
}

KBarRpcService::~KBarRpcService() = default;

bool KBarRpcService::start() {
    if (running_) return true;

    // 设置回调
    impl_->set_on_kbar_pushed(on_kbar_pushed);
    impl_->set_on_connection_changed(on_connection_changed);
    impl_->set_on_error(on_error);

    if (impl_->start()) {
        running_ = true;
        return true;
    }
    return false;
}

void KBarRpcService::stop() {
    running_ = false;
    impl_->stop();
}

bool KBarRpcService::fetch_kbars(const std::string& symbol, int timeFrame,
                                   std::vector<KBar>& out) {
    return impl_->fetch_kbars(symbol, timeFrame, out);
}

bool KBarRpcService::fetch_latest_kbars(const std::string& symbol, int timeFrame,
                                         size_t count, std::vector<KBar>& out) {
    return impl_->fetch_latest_kbars(symbol, timeFrame, count, out);
}

KBar KBarRpcService::fetch_latest_kbar(const std::string& symbol, int timeFrame) {
    return impl_->fetch_latest_kbar(symbol, timeFrame);
}
