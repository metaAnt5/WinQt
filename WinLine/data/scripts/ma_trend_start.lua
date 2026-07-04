-- ma_trend_start.lua
-- 均线趋势启动信号（价格穿过MA5/MA10 + 同方向延续确认）
--
-- 多头触发条件：
--   ① 前一根K棒收盘未同时站上MA5和MA10
--   ② 当前K棒（刚收线）收盘同时站上MA5和MA10 → 价格穿过两条均线
--   ③ 当前收盘价 > MA5 → 同方向延续确认
--   动作: 画 TriangleUp + 飞书提醒
--
-- 空头触发条件：
--   ① 前一根K棒收盘未同时跌破MA5和MA10
--   ② 当前K棒（刚收线）收盘同时跌破MA5和MA10 → 价格跌破两条均线
--   ③ 当前收盘价 < MA5 → 同方向延续确认
--   动作: 画 TriangleDown + 飞书提醒

function on_init(params)
    return true
end

-- 上次触发信号的K线索引，避免重复触发
local last_signal_idx = -1
local symbol = ""
local tf = 0
local params_loaded = false

function on_bar_new(candle, script_name)
    -- 需要足够多的K线数据（至少12根供MA10计算）
    if core.bars_count() < 12 then
        return
    end

    -- 初始化品种和周期信息
    if not params_loaded then
        symbol = candle.symbol
        tf = candle.timeframe
        params_loaded = true
    end

    -- 当前K棒（刚收线，index=0）
    local close_0 = candle.close

    -- 前一根K棒（index=1）
    local prev = core.bar(1)
    if not prev then
        return
    end
    local close_1 = prev.close

    -- 获取MA5和MA10
    local ma5_0 = core.ma(5, 0)   -- 当前MA5
    local ma5_1 = core.ma(5, 1)   -- 前一根MA5
    local ma10_0 = core.ma(10, 0) -- 当前MA10
    local ma10_1 = core.ma(10, 1) -- 前一根MA10

    if not ma5_0 or not ma5_1 or not ma10_0 or not ma10_1 then
        return
    end

    -- ─────────────────────────────────────────────────
    -- 多头趋势启动：
    --   前一根K棒收盘未同时站上MA5和MA10（避免连续触发），
    --   当前K棒收盘同时站上MA5和MA10（价格穿过两根均线），
    --   且当前收盘价 > MA5（同方向延续确认）
    -- ─────────────────────────────────────────────────
    local not_bullish_before = close_1 <= ma5_1 or close_1 <= ma10_1
    local bullish_now = close_0 > ma5_0 and close_0 > ma10_0

    if not_bullish_before and bullish_now then
        if prev.index ~= last_signal_idx then
            last_signal_idx = prev.index

            -- 画三角形（向上，画在最低点下方）
            core.shape_add_child("TriangleUp", 0, candle.low - 0.5)

            -- 飞书提醒
            local msg = string.format(
                "【多头趋势启动】%s %dmin\n" ..
                "收盘价: %.2f\n" ..
                "MA5: %.2f\n" ..
                "MA10: %.2f\n" ..
                "时间: %s",
                symbol, tf, close_0, ma5_0, ma10_0, candle.time
            )
            core.send_feishu(msg)
        end
        return
    end

    -- ─────────────────────────────────────────────────
    -- 空头趋势启动：
    --   前一根K棒收盘未同时跌破MA5和MA10（避免连续触发），
    --   当前K棒收盘同时跌破MA5和MA10（价格跌破两根均线），
    --   且当前收盘价 < MA5（同方向延续确认）
    -- ─────────────────────────────────────────────────
    local not_bearish_before = close_1 >= ma5_1 or close_1 >= ma10_1
    local bearish_now = close_0 < ma5_0 and close_0 < ma10_0

    if not_bearish_before and bearish_now then
        if prev.index ~= last_signal_idx then
            last_signal_idx = prev.index

            -- 画三角形（向下，画在最高点上方）
            core.shape_add_child("TriangleDown", 0, candle.high + 0.5)

            -- 飞书提醒
            local msg = string.format(
                "【空头趋势启动】%s %dmin\n" ..
                "收盘价: %.2f\n" ..
                "MA5: %.2f\n" ..
                "MA10: %.2f\n" ..
                "时间: %s",
                symbol, tf, close_0, ma5_0, ma10_0, candle.time
            )
            core.send_feishu(msg)
        end
        return
    end
end
