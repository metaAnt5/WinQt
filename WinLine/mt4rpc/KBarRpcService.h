#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include <algorithm>
#include <cstring>

#include "KBarRpcProtocol.h"

// ============================================================
// KBar 数据结构（协议定义：二进制序列化格式）
// 
// 序列化格式 (pos-based):
//   [symbol_len(u32)] [symbol_data(char[])] [timeFrame(i32)]
//   [time(u64)] [open(f64)] [high(f64)] [low(f64)] [close(f64)] [volume(u64)]
// ============================================================
struct KBar {
    std::string symbol;
    int timeFrame = 1;
    uint64_t time = 0;           // epoch seconds
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    uint64_t volume = 0;

    bool isValid() const { return time > 0; }

    // ---------------------------------------------------------------
    // ByteBuffer 序列化接口（供 GetKbarsResponse 模板使用）
    // ---------------------------------------------------------------
    void serialize(ByteBuffer& buf) const {
        buf.write_value(static_cast<uint32_t>(symbol.size()));
        if (!symbol.empty()) {
            buf.write(symbol.data(), symbol.size());
        }
        buf.write_value(timeFrame);
        buf.write_value(time);
        buf.write_value(open);
        buf.write_value(high);
        buf.write_value(low);
        buf.write_value(close);
        buf.write_value(volume);
    }

    static KBar deserialize(ByteBuffer& buf) {
        KBar k;
        uint32_t sym_len = buf.read_value<uint32_t>();
        if (sym_len > 0) {
            const char* sym_data = buf.read(sym_len);
            k.symbol.assign(sym_data, sym_len);
        }
        k.timeFrame = buf.read_value<int32_t>();
        k.time = buf.read_value<uint64_t>();
        k.open = buf.read_value<double>();
        k.high = buf.read_value<double>();
        k.low = buf.read_value<double>();
        k.close = buf.read_value<double>();
        k.volume = buf.read_value<uint64_t>();
        return k;
    }

    // --- 遗留的 vector<char> 序列化接口（兼容旧代码）---
    std::vector<char> serialize() const {
        ByteBuffer buf;
        serialize(buf);
        return std::vector<char>(buf.data(), buf.data() + buf.size());
    }

    static KBar deserialize(const char* data, size_t& offset) {
        KBar k;
        uint32_t sym_len = 0;
        std::memcpy(&sym_len, data + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        if (sym_len > 0) {
            k.symbol.assign(data + offset, sym_len);
            offset += sym_len;
        }
        std::memcpy(&k.timeFrame, data + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        std::memcpy(&k.time, data + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        std::memcpy(&k.open, data + offset, sizeof(double));
        offset += sizeof(double);
        std::memcpy(&k.high, data + offset, sizeof(double));
        offset += sizeof(double);
        std::memcpy(&k.low, data + offset, sizeof(double));
        offset += sizeof(double);
        std::memcpy(&k.close, data + offset, sizeof(double));
        offset += sizeof(double);
        std::memcpy(&k.volume, data + offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        return k;
    }

    static KBar deserialize(const std::vector<char>& payload, size_t& offset) {
        return deserialize(payload.data(), offset);
    }
};

// ============================================================
// KBarRpcService - K 线数据 RPC 客户端
// 作为 RPC 客户端连接 Mt4Server，请求 K 线数据和接收实时推送
//
// 协议定义：
//   Method GET_KBARS (3000): Request/Response
//     Request payload:  [symbol_len(u32)][symbol_data][timeFrame(i32)][count(u16)]
//       - count=0 表示获取全部
//     Response payload: [count(u16)][KBar_1]...[KBar_N]
//       - 每个 KBar 按 KBar::serialize 格式
//
//   Method PUSH_KBAR (3001): Server Push (one-way message)
//     Payload: [KBar] 按 KBar::serialize 格式
// ============================================================
class KBarRpcService {
public:
    // RPC 方法 ID
    static constexpr uint16_t METHOD_GET_KBARS = 3000;
    static constexpr uint16_t METHOD_PUSH_KBAR  = 3001;

    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 9200;
        int reconnect_interval_ms = 3000;
        int request_timeout_ms = 10000;
        bool auto_reconnect = true;
    };

    explicit KBarRpcService(const Config& config = Config());
    virtual ~KBarRpcService();

    // 启动/停止
    bool start();
    void stop();
    bool is_running() const { return running_; }

    // ---- 数据接口 ----
    // 获取指定品种/周期的全部 K 线
    bool fetch_kbars(const std::string& symbol, int timeFrame,
                     std::vector<KBar>& out);

    // 获取指定品种/周期的最新 N 根 K 线
    bool fetch_latest_kbars(const std::string& symbol, int timeFrame,
                            size_t count, std::vector<KBar>& out);

    // 获取指定品种/周期的最新一根 K 线
    KBar fetch_latest_kbar(const std::string& symbol, int timeFrame);

    // ---- 回调 ----
    // 当收到服务器推送的 K 线更新时回调
    std::function<void(const KBar&)> on_kbar_pushed;

    // 连接状态回调
    std::function<void(bool connected)> on_connection_changed;

    // 错误回调
    std::function<void(const std::string& error)> on_error;

    // 获取配置
    const Config& config() const { return config_; }

private:
    // 内部实现类（PIMPL 模式，隐藏 NetCore 依赖）
    class Impl;
    std::unique_ptr<Impl> impl_;

    Config config_;
    std::atomic<bool> running_{false};
};
