-- kdj_trend_start.lua
-- KDJ 三线排列 → 趋势开始信号
-- J>K>D 多头排列 → 上涨趋势开始
-- J<K<D 空头排列 → 下跌趋势开始

function on_init(params)
    return true
end

-- 上次信号时的 (方向, index) 避免重复
local last_signal = { dir = "", idx = -1 }

function on_bar_new(candle, script_name)
    -- 需要至少有 3 根 K 线数据
    if core.bars_count() < 3 then
        return
    end

    -- 取当前和上一根的 K, D, J
    local k0, d0, j0 = core.kdj(0)
    local k1, d1, j1 = core.kdj(1)

    -- 判断当前排列
    local bullish_now = (j0 > k0) and (k0 > d0)  -- 多头排列 J>K>D
    local bearish_now = (j0 < k0) and (k0 < d0)  -- 空头排列 J<K<D

    -- 判断前一根排列（确认刚形成，不是持续状态）
    local bullish_before = (j1 > k1) and (k1 > d1)
    local bearish_before = (j1 < k1) and (k1 < d1)

    -- 刚形成多头排列（前一根 NOT 多头，当前多头）
    if bullish_now and not bullish_before then
        if last_signal.dir ~= "bullish" or last_signal.idx ~= 0 then
            last_signal = { dir = "bullish", idx = 0 }
            core.shape_add_child("TriangleUp", 0, candle.close - 0.5)
        end
    end

    -- 刚形成空头排列（前一根 NOT 空头，当前空头）
    if bearish_now and not bearish_before then
        if last_signal.dir ~= "bearish" or last_signal.idx ~= 0 then
            last_signal = { dir = "bearish", idx = 0 }
            core.shape_add_child("TriangleDown", 0, candle.close + 0.5)
        end
    end
end
