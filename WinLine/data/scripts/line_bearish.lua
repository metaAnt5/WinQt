-- line_bearish.lua
-- 收线在趋势线/水平线下方 → 飞书提醒（看跌）

function on_init(params)
    core.log("line_bearish initialized (收线看跌跌破提醒)")
    return true
end

-- 当前交易对
local symbol = ""
local tf = 0

-- 是否已经加载过参数（避免重复打印日志）
local params_loaded = false

function on_bar_new(candle, script_name)
    -- 只在初始化时更新 symbol/tf，避免每次都重新取
    if not params_loaded then
        symbol = candle.symbol
        tf = candle.timeframe
        params_loaded = true
    end

    -- 计算上一根完成的 K 线的绝对索引
    -- bar(1) 对应 data[data.size() - 2]，所以绝对索引 = bars_count() - 2
    local total_bars = core.bars_count()
    local prev_idx = total_bars - 2
    if prev_idx < 0 then
        return
    end

    -- 获取线价格（传入K线索引，支持趋势线插值计算）
    local price = core.get_shape_price(script_name, prev_idx)
    if not price then
        return
    end

    -- 使用上一根完成的 K 线（index=1）
    local prev = core.bar(1)
    if not prev then
        return
    end

        local close_price = prev.close
        if close_price < price then
            local msg = string.format(
                "【收线看跌提醒】%s %dmin\n" ..
                "线价格: %.2f\n" ..
                "收盘价: %.2f\n" ..
                "时间: %s",
                symbol, tf, price, close_price, candle.time
            )
            core.send_feishu(msg)
            core.log("line_bearish feishu sent: " .. msg)
        end
end
