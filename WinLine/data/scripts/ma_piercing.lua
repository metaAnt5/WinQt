-- ma_piercing.lua
-- 一阳穿三线 / 一阴破三线 → 趋势突破信号
-- 看涨：K棒开盘<三线，收盘>三线，且MA5>MA10>MA20
-- 看跌：K棒开盘>三线，收盘<三线，且MA5<MA10<MA20

function on_init(params)
    core.log("ma_piercing initialized (一阳穿三线/一阴破三线)")
    return true
end

-- 上次信号的 index，避免重复画
local last_signal_idx = -1

function on_bar_new(candle, script_name)
    -- 需要足够多的 K 线数据（至少 25 根）
    if core.bars_count() < 25 then
        return
    end

    -- 只在新 K 棒收线时判断，使用上一根完成的 K 棒（index=1）
    local b = core.bar(1)
    if not b then
        return
    end

    local o, c, h, l = b.open, b.close, b.high, b.low

    -- 获取 MA5, MA10, MA20
    local ma5 = core.ma(5, 1)
    local ma10 = core.ma(10, 1)
    local ma20 = core.ma(20, 1)
    if not ma5 or not ma10 or not ma20 then
        return
    end

    -- 均线排列
    local ma5_0 = core.ma(5, 0)  -- 当前 MA5
    local ma5_2 = core.ma(5, 2)  -- 前前一根 MA5（用于判断斜率方向）
    local ma10_0 = core.ma(10, 0)

    -- 看涨突破（一阳穿三线）：
    --   ① 开盘 < MA5, MA10, MA20
    --   ② 收盘 > MA5, MA10, MA20
    --   ③ 实体足够大
    --   ④ MA5 > MA10 > MA20 多头排列
    local body = c - o  -- 实体
    local bullish_piercing = (
        o < ma5 and o < ma10 and o < ma20 and
        c > ma5 and c > ma10 and c > ma20 and
        body > 0 and
        math.abs(body) > (h - l) * 0.5 and  -- 实体 > 影线
        ma5 > ma10 and ma10 > ma20
    )

    if bullish_piercing then
        if b.index ~= last_signal_idx then
            last_signal_idx = b.index
            core.log(string.format(
                "一阳穿三线: O=%.2f C=%.2f MA5=%.2f MA10=%.2f MA20=%.2f",
                o, c, ma5, ma10, ma20
            ))
            core.shape_add_child("TriangleUp", b.index, l - 0.3)
        end
        return
    end

    -- 看跌突破（一阴破三线）：
    --   ① 开盘 > MA5, MA10, MA20
    --   ② 收盘 < MA5, MA10, MA20
    --   ③ 实体足够大
    --   ④ MA5 < MA10 < MA20 空头排列
    body = o - c  -- 阴线实体
    local bearish_piercing = (
        o > ma5 and o > ma10 and o > ma20 and
        c < ma5 and c < ma10 and c < ma20 and
        body > 0 and
        math.abs(body) > (h - l) * 0.5 and
        ma5 < ma10 and ma10 < ma20
    )

    if bearish_piercing then
        if b.index ~= last_signal_idx then
            last_signal_idx = b.index
            core.log(string.format(
                "一阴破三线: O=%.2f C=%.2f MA5=%.2f MA10=%.2f MA20=%.2f",
                o, c, ma5, ma10, ma20
            ))
            core.shape_add_child("TriangleDown", b.index, h + 0.3)
        end
        return
    end
end
