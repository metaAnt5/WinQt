-- trend_line_alert.lua
-- 收线时判断K线与趋势线/水平线的关系，发送飞书提醒
-- 收线在线上方 → 看涨提醒；在线下方 → 看跌提醒

function on_init(params)
    return true
end

-- 当前交易对
local symbol = ""
local tf = 0
local params_loaded = false

-- 修整小数精度（避免浮点误差）
local function round2(val)
    return math.floor(val * 100 + 0.5) / 100
end

-- 获取趋势线在指定 K 线索引处的价格
local function get_line_price_at(script_name, idx)
    return core.get_line_price(script_name, idx)
end

-- 发送飞书消息
local function send_alert(msg)
    core.send_feishu(msg)
end

-- 主逻辑
function on_bar_new(candle, script_name)
    -- 初始化 symbol/tf
    if not params_loaded then
        symbol = candle.symbol
        tf = candle.timeframe
        params_loaded = true
    end

    -- 获取线价格
    local price = get_line_price_at(script_name, 0)
    if not price then
        return
    end

    -- 使用上一根完成的 K 线（index=1）
    local prev = core.bar(1)
    if not prev then
        return
    end

    local close_price = prev.close
    local dir_text = ""
    local emoji = ""

    if close_price > price then
        dir_text = "看涨"
        emoji = "🟢"
    elseif close_price < price then
        dir_text = "看跌"
        emoji = "🔴"
    else
        return  -- 价格持平，不触发
    end

    local msg = string.format(
        "%s【趋势线提醒】%s %dmin\n" ..
        "方向: %s\n" ..
        "线价格: %.2f\n" ..
        "收盘价: %.2f\n" ..
        "时间: %s",
        emoji, symbol, tf,
        dir_text,
        price, close_price,
        candle.time
    )
    send_alert(msg)
end
