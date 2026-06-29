-- ma_cross_signal.lua
-- MA5/MA10 金叉死叉信号
-- 金叉（MA5上穿MA10）→ TriangleUp；死叉（MA5下穿MA10）→ TriangleDown

function on_init(params)
    core.log("ma_cross_signal initialized (MA均线交叉信号)")
    return true
end

-- 上次信号状态
local last_signal = 0  -- 0:无, 1:金叉, 2:死叉

function on_bar_new(candle, script_name)
    -- 需要足够的 K 线数据
    if core.bars_count() < 15 then
        return
    end

    -- 获取 MA5 和 MA10
    local ma5_0 = core.ma(5, 0)
    local ma10_0 = core.ma(10, 0)
    local ma5_1 = core.ma(5, 1)
    local ma10_1 = core.ma(10, 1)

    if not ma5_0 or not ma10_0 or not ma5_1 or not ma10_1 then
        return
    end

    -- 金叉：前一根 MA5 <= MA10，当前 MA5 > MA10
    if ma5_1 <= ma10_1 and ma5_0 > ma10_0 then
        if last_signal ~= 1 then
            last_signal = 1
            core.log(string.format(
                "金叉: MA5=%.2f MA10=%.2f (index=0)",
                ma5_0, ma10_0
            ))
            core.shape_add_child("TriangleUp", 0, candle.close - 0.5)
        end
        return
    end

    -- 死叉：前一根 MA5 >= MA10，当前 MA5 < MA10
    if ma5_1 >= ma10_1 and ma5_0 < ma10_0 then
        if last_signal ~= 2 then
            last_signal = 2
            core.log(string.format(
                "死叉: MA5=%.2f MA10=%.2f (index=0)",
                ma5_0, ma10_0
            ))
            core.shape_add_child("TriangleDown", 0, candle.close + 0.5)
        end
        return
    end
end
