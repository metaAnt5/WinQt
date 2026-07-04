-- ma_cross_signal.lua
-- MA5 穿越信号：仅在收盘价穿越 MA5 时画三角形
--   上穿（金叉）：前一根收盘 < MA5，当前收盘 >= MA5 → 在K线下方画向上的三角形 ▲
--   下穿（死叉）：前一根收盘 > MA5，当前收盘 <= MA5 → 在K线上方画向下的三角形 ▼
-- 不每根K棒都画，只在发生穿越时才画

function on_init(params)
    core.log("ma_cross_signal initialized (MA5穿越信号)")
    return true
end

function on_bar_new(candle, script_name)
    -- 需要至少 7 根 K 线（前5根计算MA5，再加2根做比较）
    if core.bars_count() < 7 then
        core.log(string.format("MA5: bars_count=%d < 7, skip", core.bars_count()))
        return
    end

    -- 刚刚结束的那根K线 = core.bar(1)
    local cur_bar  = core.bar(1)
    if not cur_bar then
        core.log("MA5: cur_bar is nil, skip")
        return
    end
    local cur_close = cur_bar.close
    local cur_low   = cur_bar.low
    local cur_high  = cur_bar.high
    local cur_ma5   = core.ma(5, 1)

    -- 前一根K线 = core.bar(2)
    local prev_bar = core.bar(2)
    if not prev_bar then
        core.log("MA5: prev_bar is nil, skip")
        return
    end
    local prev_close = prev_bar.close
    local prev_ma5   = core.ma(5, 2)

    -- 刚刚结束的K线在数据数组中的下标（用于形状定位）
    local idx = core.bars_count() - 2

    -- 传入的 candle 参数中的 index（回放循环中的 i）
    local candle_index = candle.index or -1
    -- MA 计算引用的实际数据数组下标
    local bar1_idx = core.bars_count() - 2  -- bar(1) 在数组中的位置
    local bar2_idx = core.bars_count() - 3  -- bar(2) 在数组中的位置

    -- 价格偏移量
    local offset = 2.0

    -- 打印详细调试信息
    core.log(string.format(
        "MA5_CHECK: candleIdx=%d bar1_close=%.2f bar1_ma5=%.2f bar2_close=%.2f bar2_ma5=%.2f bar1_idx=%d bar2_idx=%d bars_count=%d",
        candle_index, cur_close, cur_ma5, prev_close, prev_ma5, bar1_idx, bar2_idx, core.bars_count()))

    -- 上穿（金叉）：前一根收盘 < MA5，当前收盘 >= MA5
    if prev_close < prev_ma5 and cur_close >= cur_ma5 then
        core.log(string.format("MA5_CROSS_UP: condition TRUE, calling shape_add_child(TriangleUp, idx=%d, price=%.2f)",
            idx, cur_low - offset))
        local id = core.shape_add_child("TriangleUp", idx, cur_low - offset)
        if id > 0 then
            core.log(string.format("上穿 ▲ idx=%d close=%.2f >= MA5=%.2f",
                idx, cur_close, cur_ma5))
        else
            core.log(string.format("MA5_CROSS_UP: shape_add_child FAILED id=%d", id))
        end
    else
        core.log(string.format("MA5_CROSS_UP: condition FALSE (prev_close=%.2f < prev_ma5=%.2f = %s, cur_close=%.2f >= cur_ma5=%.2f = %s)",
            prev_close, prev_ma5, tostring(prev_close < prev_ma5), cur_close, cur_ma5, tostring(cur_close >= cur_ma5)))
    end

    -- 下穿（死叉）：前一根收盘 > MA5，当前收盘 <= MA5
    if prev_close > prev_ma5 and cur_close <= cur_ma5 then
        core.log(string.format("MA5_CROSS_DOWN: condition TRUE, calling shape_add_child(TriangleDown, idx=%d, price=%.2f)",
            idx, cur_high + offset))
        local id = core.shape_add_child("TriangleDown", idx, cur_high + offset)
        if id > 0 then
            core.log(string.format("下穿 ▼ idx=%d close=%.2f < MA5=%.2f",
                idx, cur_close, cur_ma5))
        else
            core.log(string.format("MA5_CROSS_DOWN: shape_add_child FAILED id=%d", id))
        end
    else
        core.log(string.format("MA5_CROSS_DOWN: condition FALSE (prev_close=%.2f > prev_ma5=%.2f = %s, cur_close=%.2f <= cur_ma5=%.2f = %s)",
            prev_close, prev_ma5, tostring(prev_close > prev_ma5), cur_close, cur_ma5, tostring(cur_close <= cur_ma5)))
    end
end
