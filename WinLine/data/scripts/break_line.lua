-- break_line.lua - 收线突破/跌破水平线策略
--
-- 用法：
--   1. 在图表上画一条水平线（HLine）
--   2. 在水平线的属性中设置 "关联脚本" 为 "break_line"
--   3. 当 K 线收盘价上穿该线时触发买入标记和提醒
--   4. 当 K 线收盘价下穿该线时触发卖出标记和提醒
--
-- on_bar_new(candle, scriptName) 回调：
--   candle  : K 线数据 table
--   scriptName : 当前脚本名称（传入 core.get_shape_price 使用）

function on_init(params)
    core.log("break_line 策略初始化完成, 参数: " .. params)
    return true
end

function on_bar_new(candle, scriptName)
    -- 获取关联的图形的价格（水平线的 price1）
    local linePrice = core.get_shape_price(scriptName)
    if linePrice == nil then
        core.log("未找到关联的图形，请在图形属性中设置脚本名称: " .. scriptName)
        return
    end

    -- 获取当前 K 线收盘价和上一根收盘价
    local close = candle.close
    local prevBar = core.bar(1)
    if prevBar == nil then
        return -- 至少需要 2 根 K 线才能判断穿越
    end
    local prevClose = prevBar.close

    -- 判断穿越方向
    if close > linePrice and prevClose <= linePrice then
        -- 收盘价上穿水平线 → 突破
        core.alert("收线突破! " .. candle.symbol .. " @" .. string.format("%.2f", close) ..
                   " 突破 " .. string.format("%.2f", linePrice))
        core.shape_add("TradeBuy", candle.index, candle.low, candle.index, candle.high,
                       "突破 " .. string.format("%.2f", linePrice))
        core.log("突破信号: " .. candle.symbol .. " 收盘=" .. string.format("%.2f", close) ..
                 " 线=" .. string.format("%.2f", linePrice))

    elseif close < linePrice and prevClose >= linePrice then
        -- 收盘价下穿水平线 → 跌破
        core.alert("收线跌破! " .. candle.symbol .. " @" .. string.format("%.2f", close) ..
                   " 跌破 " .. string.format("%.2f", linePrice))
        core.shape_add("TradeSell", candle.index, candle.high, candle.index, candle.low,
                       "跌破 " .. string.format("%.2f", linePrice))
        core.log("跌破信号: " .. candle.symbol .. " 收盘=" .. string.format("%.2f", close) ..
                 " 线=" .. string.format("%.2f", linePrice))
    end
end
