#pragma once

#include "../NetCore/ByteBuffer.h"
#include "../NetCore/IPacket.h"
#include <string>
#include <cstdint>

using namespace NetCore;

struct KBar {
    std::string symbol;
    int timeFrame = 1;
    uint64_t time = 0; // epoch seconds
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    uint64_t volume = 0;

    // Serialize to buffer using default position APIs
    void serialize(ByteBuffer& buf) const {
        size_t offset = buf.pos();
        offset = buf.write_value(offset, static_cast<uint32_t>(symbol.size()));
        offset = buf.write(offset, symbol.c_str(), symbol.size());
        offset = buf.write_value(offset, timeFrame);
        offset = buf.write_value(offset, time);
        offset = buf.write_value(offset, open);
        offset = buf.write_value(offset, high);
        offset = buf.write_value(offset, low);
        offset = buf.write_value(offset, close);
        offset = buf.write_value(offset, volume);
        buf.set_pos(offset);
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
};