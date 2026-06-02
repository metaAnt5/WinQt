-- break_line.lua - 收线突破/跌破水平线策略
--
-- 用法：
--   1. 在图表上画一条水平线（HLine）
--   2. 在水平线的属性中设置 "关联脚本" 为 "break_line"
--   3. 当 K 线最低价已越过线上方（整根线在线上方）时触发买入标记
--   4. 当 K 线最高价已越过线下方（整根线在线下方）时触发卖出标记
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

    -- 获取当前 K 线的最低价/最高价和上一根的最低价/最高价
    local curLow = candle.low
    local curHigh = candle.high
    local prevBar = core.bar(1)
    if prevBar == nil then
        return -- 至少需要 2 根 K 线才能判断穿越
    end
    local prevLow = prevBar.low
    local prevHigh = prevBar.high

    -- 上穿：当前 K 线最低价在线上方，且上一根 K 线最高价在线下方或线上
    --      → 整根 K 线从下方完全越到线上方
    if curLow > linePrice and prevHigh <= linePrice then
        core.alert("收线突破! " .. candle.symbol .. " 最低=" .. string.format("%.2f", curLow) ..
                   " 突破 " .. string.format("%.2f", linePrice))
        core.shape_add("TradeBuy", candle.index, candle.low, candle.index, candle.high,
                       "突破 " .. string.format("%.2f", linePrice))
        core.log("突破信号: " .. candle.symbol .. " 最低=" .. string.format("%.2f", curLow) ..
                 " 线=" .. string.format("%.2f", linePrice))

    -- 下穿：当前 K 线最高价在线下方，且上一根 K 线最低价在线下方或线上
    --      → 整根 K 线从上方完全越到线下方
    elseif curHigh < linePrice and prevLow >= linePrice then
        core.alert("收线跌破! " .. candle.symbol .. " 最高=" .. string.format("%.2f", curHigh) ..
                   " 跌破 " .. string.format("%.2f", linePrice))
        core.shape_add("TradeSell", candle.index, candle.high, candle.index, candle.low,
                       "跌破 " .. string.format("%.2f", linePrice))
        core.log("跌破信号: " .. candle.symbol .. " 最高=" .. string.format("%.2f", curHigh) ..
                 " 线=" .. string.format("%.2f", linePrice))
    end
end
