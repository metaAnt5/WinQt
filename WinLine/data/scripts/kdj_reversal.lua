-- kdj_reversal.lua
-- KDJ 极值区 + K 棒影线反转判定
-- 超买区(K>80)上影线>实体 → 看跌反转
-- 超卖区(K<20)下影线>实体 → 看涨反转

function on_init(params)
    return true
end

-- 上次画三角形时的 (index, type) 避免重复
local last_bearish_idx = -1
local last_bullish_idx = -1

function on_bar_new(candle, script_name)
    if core.bars_count() < 3 then
        return
    end

    -- 取上一根完成的 K 棒（index=1）
    local b1 = core.bar(1)
    if not b1 then
        return
    end

    -- 获取 KDJ-K 值
    local k1, d1 = core.kdj(1)
    if not k1 then
        return
    end

    -- 计算实体和影线
    local body = math.abs(b1.close - b1.open)
    if body == 0 then
        return
    end

    local upper_shadow = b1.high - math.max(b1.close, b1.open)
    local lower_shadow = math.min(b1.close, b1.open) - b1.low

    -- 看跌反转：超买区 + 上影线 > 实体
    if (k1 > 80 or d1 > 80) and upper_shadow > body then
        if b1.index ~= last_bearish_idx then
            last_bearish_idx = b1.index
        end
        return
    end

    -- 看涨反转：超卖区 + 下影线 > 实体
    if (k1 < 20 or d1 < 20) and lower_shadow > body then
        if b1.index ~= last_bullish_idx then
            last_bullish_idx = b1.index
        end
        return
    end
end
