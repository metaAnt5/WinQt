#pragma once

#include "NetCore/ByteBuffer.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

using namespace NetCore;

// ============================================================
// GetKbarsRequest - GET_KBARS (3000) 请求协议封装
//
// 请求 payload 格式: [symbol_len(u32)][symbol_data(char[])][timeFrame(i32)][count(u16)]
//   - count=0 表示获取全部 K 线
// ============================================================
struct GetKbarsRequest {
    std::string symbol;
    int timeFrame = 1;
    uint16_t count = 0;  // 0 = 获取全部

    void serialize(ByteBuffer& buf) const {
        uint32_t sym_len = static_cast<uint32_t>(symbol.size());
        buf.write_value(sym_len);
        if (sym_len > 0) {
            buf.write(symbol.data(), sym_len);
        }
        buf.write_value(timeFrame);
        buf.write_value(count);
    }

    static GetKbarsRequest deserialize(ByteBuffer& buf) {
        GetKbarsRequest req;
        uint32_t sym_len = buf.read_value<uint32_t>();
        if (sym_len > 0) {
            const char* sym_data = buf.read(sym_len);
            req.symbol.assign(sym_data, sym_len);
        }
        req.timeFrame = buf.read_value<int32_t>();
        req.count = buf.read_value<uint16_t>();
        return req;
    }

    std::vector<char> to_payload() const {
        ByteBuffer buf;
        serialize(buf);
        return std::vector<char>(buf.data(), buf.data() + buf.size());
    }

    static GetKbarsRequest from_payload(const std::vector<char>& payload) {
        ByteBuffer buf(payload.data(), payload.size());
        return deserialize(buf);
    }
};

// ============================================================
// GetKbarsResponse - GET_KBARS (3000) 响应协议封装
//
// 响应 payload 格式: [count(u16)][KBar_1]...[KBar_N]
//   每个 KBar 使用 KBarT::serialize(buf) / KBarT::deserialize(buf) 序列化
// ============================================================
template<typename KBarT>
struct GetKbarsResponse {
    std::vector<KBarT> bars;

    void serialize(ByteBuffer& buf) const {
        buf.write_value(static_cast<uint16_t>(bars.size()));
        for (const auto& bar : bars) {
            bar.serialize(buf);
        }
    }

    std::vector<char> to_payload() const {
        ByteBuffer buf;
        serialize(buf);
        return std::vector<char>(buf.data(), buf.data() + buf.size());
    }

    static GetKbarsResponse deserialize(ByteBuffer& buf) {
        GetKbarsResponse resp;
        uint16_t count = buf.read_value<uint16_t>();
        resp.bars.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            resp.bars.push_back(KBarT::deserialize(buf));
        }
        return resp;
    }

    static GetKbarsResponse from_payload(const std::vector<char>& payload) {
        ByteBuffer buf(payload.data(), payload.size());
        return deserialize(buf);
    }
};
