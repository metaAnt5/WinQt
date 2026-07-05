-- ma_cross_signal.lua
-- MA5 穿越信号：仅在收盘价穿越 MA5 时画三角形
--   上穿（金叉）：前一根收盘 < MA5，当前收盘 >= MA5 → 在K线下方画向上的三角形 ▲
--   下穿（死叉）：前一根收盘 > MA5，当前收盘 <= MA5 → 在K线上方画向下的三角形 ▼
-- 不每根K棒都画，只在发生穿越时才画

function on_init(params)
    return true
end

function on_bar_new(candle, script_name)
    -- 需要至少 7 根 K 线（前5根计算MA5，再加2根做比较）
    if core.bars_count() < 7 then
        return
    end

    -- 刚刚结束的那根K线 = core.bar(1)
    local cur_bar  = core.bar(1)
    if not cur_bar then
        return
    end
    local cur_close = cur_bar.close
    local cur_low   = cur_bar.low
    local cur_high  = cur_bar.high
    local cur_ma5   = core.ma(5, 1)

    -- 前一根K线 = core.bar(2)
    local prev_bar = core.bar(2)
    if not prev_bar then
        return
    end
    local prev_close = prev_bar.close
    local prev_ma5   = core.ma(5, 2)

    -- 刚刚结束的K线在数据数组中的下标（用于形状定位）
    local idx = core.bars_count() - 2

    -- 上穿（金叉）：前一根收盘 < MA5，当前收盘 >= MA5
    -- 三角形与K线的间距由 C++ 层 pixelOffsetY 控制，不受缩放影响
    if prev_close < prev_ma5 and cur_close >= cur_ma5 then
        core.shape_add_child("TriangleUp", idx, cur_low)
    end

    -- 下穿（死叉）：前一根收盘 > MA5，当前收盘 <= MA5
    -- 三角形与K线的间距由 C++ 层 pixelOffsetY 控制，不受缩放影响
    if prev_close > prev_ma5 and cur_close <= cur_ma5 then
        core.shape_add_child("TriangleDown", idx, cur_high)
    end
end
