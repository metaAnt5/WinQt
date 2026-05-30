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

        log_msg("[RPC] IoContextManager starting...");
        if (!io_mgr_->start()) {
            if (on_error_) on_error_("Failed to start IoContextManager");
            return false;
        }

        running_ = true;

        log_msg("[RPC] Spawning connect_and_read_loop...");

        // 在 io_context 上发起连接和读循环
        asio::co_spawn(io_mgr_->get_io_context(),
            [this]() -> asio::awaitable<void> {
                co_await connect_and_read_loop();
            },
            asio::detached);

        return true;
    }

    void stop() {
        log_msg("[RPC] Stopping RPC client...");
        running_ = false;
        if (client_) {
            client_->disconnect();
            client_.reset();
        }
        if (io_mgr_) {
            io_mgr_->stop();
        }
    }

    void set_on_log_message(std::function<void(const std::string&)> cb) {
        on_log_message_ = std::move(cb);
    }

    // ---- 数据接口（同步方式，内部用 async + promise）----
    bool fetch_kbars(const std::string& symbol, int timeFrame, std::vector<KBar>& out) {
        return do_fetch(symbol, timeFrame, 0, out);
    }

    bool fetch_kbars_since(const std::string& symbol, int timeFrame,
                           uint64_t startTime, std::vector<KBar>& out) {
        return do_fetch(symbol, timeFrame, startTime, out);
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
    std::function<void(const std::string&)> on_log_message_;

    void log_msg(const std::string& msg) {
        if (on_log_message_) on_log_message_(msg);
    }

    // 同步等待响应（从外部线程调用，非 bg 线程安全）
    bool do_fetch(const std::string& symbol, int timeFrame, uint64_t startTime, std::vector<KBar>& out) {
        if (!running_) return false;

        std::promise<bool> promise;
        auto future = promise.get_future();

        asio::co_spawn(io_mgr_->get_io_context(),
            [this, symbol, timeFrame, startTime, &out, promise = std::move(promise)]() mutable
            -> asio::awaitable<void> {
                try {
                    IResponse resp = co_await async_fetch(symbol, timeFrame, startTime);
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

    // 异步执行 RPC 请求（协程上下文，永不阻塞 bg 线程）
    asio::awaitable<IResponse> async_fetch(const std::string& symbol, int timeFrame, uint64_t startTime) {
        // 确保已连接（协程版，不会阻塞 bg 线程）
        if (!co_await async_ensure_connected()) {
            log_msg("[RPC] async_fetch: not connected");
            co_return IResponse(0, 0, METHOD_GET_KBARS, {}, RpcError::NOT_CONNECTED);
        }

        auto conn = client_->GetConnection();
        if (!conn || conn->state() != ConnectionState::CONNECTED) {
            log_msg("[RPC] async_fetch: connection state not CONNECTED");
            co_return IResponse(0, 0, METHOD_GET_KBARS, {}, RpcError::NOT_CONNECTED);
        }

        // 构建请求 payload（使用协议封装类）
        auto payload = GetKbarsRequest{symbol, timeFrame, startTime}.to_payload();

        // 组装 RPC 请求
        static std::atomic<uint32_t> s_req_id{1};
        uint32_t req_id = s_req_id.fetch_add(1, std::memory_order_relaxed);

        auto req_msg = std::make_shared<IRequest>(0, METHOD_GET_KBARS, payload);
        req_msg->set_flags(static_cast<uint8_t>(RpcFlag::REQUEST));
        req_msg->set_req_id(req_id);

        log_msg(std::string("[RPC] Sending GET_KBARS req_id=") + std::to_string(req_id)
                + " symbol=" + symbol + " tf=" + std::to_string(timeFrame)
                + " startTime=" + std::to_string(startTime));

        // 使用 BinaryPacker 发送
        BinaryPacker packer;
        IMessage::Ptr imsg = std::static_pointer_cast<IMessage>(req_msg);
        if (!packer.pack(conn, imsg)) {
            log_msg(std::string("[RPC] GET_KBARS req_id=") + std::to_string(req_id) + " SEND_FAILED");
            co_return IResponse(req_id, 0, METHOD_GET_KBARS, {}, RpcError::SEND_FAILED);
        }

        // 等待响应（在 read_loop 中匹配 req_id）
        auto response = co_await wait_for_response(req_id, config_.request_timeout_ms);

        if (response.error_code() == 0) {
            log_msg(std::string("[RPC] GET_KBARS req_id=") + std::to_string(req_id) + " OK");
        } else {
            log_msg(std::string("[RPC] GET_KBARS req_id=") + std::to_string(req_id)
                    + " error=" + std::to_string(response.error_code()));
        }

        co_return response;
    }

    asio::awaitable<IResponse> wait_for_response(uint32_t req_id, int timeout_ms) {
        // 不能直接用 {} 作为 vector 参数传给 make_shared，MSVC 会解析为 initializer_list
        auto result = std::make_shared<IResponse>(req_id, 0, METHOD_GET_KBARS,
                                                  std::vector<char>(), RpcError::TIMEOUT);
        auto timer = std::make_shared<asio::steady_timer>(io_mgr_->get_io_context());

        // 注册回调：响应到达时设置结果并取消定时器
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_responses_[req_id] = [result, timer](IResponse resp) {
                *result = std::move(resp);
                timer->cancel();
            };
        }

        // 启动超时定时器
        timer->expires_after(std::chrono::milliseconds(timeout_ms));
        asio::error_code ec;
        co_await timer->async_wait(asio::redirect_error(asio::use_awaitable, ec));

        // 清理回调（如果超时了还没被移除）
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_responses_.find(req_id);
            if (it != pending_responses_.end()) {
                pending_responses_.erase(it);
            }
        }

        co_return *result;
    }

    // 协程版连接（用于 bg 线程协程上下文，不死锁）
    asio::awaitable<bool> async_ensure_connected() {
        if (client_ && client_->state() == ConnectionState::CONNECTED) {
            co_return true;
        }

        log_msg(std::string("[RPC] Connecting to ") + config_.host + ":" + std::to_string(config_.port) + " ...");

        client_ = std::make_shared<Client>(io_mgr_->get_io_context());

        bool ok = co_await client_->async_connect(config_.host, config_.port);
        if (ok) {
            log_msg("[RPC] TCP connected successfully, starting read loop...");
            // 启动读循环（但在调用栈里 read_loop 由 connect_and_read_loop 调，不在此处启动）
        } else {
            log_msg("[RPC] TCP connection failed");
        }

        if (ok && on_connection_changed_) {
            log_msg("[RPC] Connection established");
            on_connection_changed_(true);
        } else {
            log_msg("[RPC] Connection NOT established");
        }
        co_return ok;
    }

    // 同步版连接（用于 do_fetch 从外部线程 co_spawn + wait）
    // 内部调用 async_ensure_connected 再用 promise/future 包装
    bool ensure_connected() {
        if (client_ && client_->state() == ConnectionState::CONNECTED) {
            return true;
        }

        std::promise<bool> promise;
        auto future = promise.get_future();

        asio::co_spawn(io_mgr_->get_io_context(),
            [this, promise = std::move(promise)]() mutable
            -> asio::awaitable<void> {
                bool ok = co_await async_ensure_connected();
                promise.set_value(ok);
            },
            asio::detached);

        return future.get();
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
        log_msg("[RPC] Connection lost / disconnected");
        if (on_connection_changed_) {
            on_connection_changed_(false);
        }

        // 清理所有等待的请求
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            size_t pending_count = pending_responses_.size();
            if (pending_count > 0) {
                log_msg(std::string("[RPC] Cancelling ") + std::to_string(pending_count) + " pending requests");
            }
            for (auto& [id, cb] : pending_responses_) {
                cb(IResponse(id, 0, 0, {}, RpcError::CONNECTION_LOST));
            }
            pending_responses_.clear();
        }

        // 如果是自动重连
        if (running_ && config_.auto_reconnect) {
            log_msg(std::string("[RPC] Auto-reconnecting in ") + std::to_string(config_.reconnect_interval_ms) + "ms...");
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
            if (running_) {
                log_msg("[RPC] Reconnecting...");
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
            if (co_await async_ensure_connected()) {
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
    impl_->set_on_log_message(on_log_message);

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

bool KBarRpcService::fetch_kbars_since(const std::string& symbol, int timeFrame,
                                        uint64_t startTime, std::vector<KBar>& out) {
    return impl_->fetch_kbars_since(symbol, timeFrame, startTime, out);
}
