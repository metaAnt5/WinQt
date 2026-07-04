-- kdj_turn_signal.lua
-- KDJ 强势/弱势区拐头信号
-- K>80 拐头向下 → 下跌预警；K<20 拐头向上 → 上涨预警

function on_init(params)
    return true
end

-- 上次标记的 index（避免重复画三角形）
local last_mark_idx = -1

function on_bar_new(candle, script_name)
    -- 需要至少 3 根 K 线
    if core.bars_count() < 3 then
        return
    end

    local k0, d0 = core.kdj(0)
    local k1, d1 = core.kdj(1)
    local k2, d2 = core.kdj(2)

    local b0 = core.bar(0)
    if not b0 then
        return
    end

    -- 拐头判定：需要至少 3 根数据来判断
    -- 上拐（弱势区拐头向上）：
    --   (k2 >= k1 < k0) → 前两根向下/走平，最新一根向上
    --   且 K0 < 20 或 D0 < 20
    local up_cond1 = (k2 >= k1 and k1 < k0)
    local up_cond2 = (k0 < 20 or d0 < 20)
    if up_cond1 and up_cond2 then
        if b0.index ~= last_mark_idx then
            last_mark_idx = b0.index
            local id = core.shape_add_child("TriangleUp", 0, candle.close - 0.5)
        end
        return
    end

    -- 下拐（强势区拐头向下）：
    --   (k2 <= k1 > k0) → 前两根向上/走平，最新一根向下
    --   且 K0 > 80 或 D0 > 80
    local down_cond1 = (k2 <= k1 and k1 > k0)
    local down_cond2 = (k0 > 80 or d0 > 80)
    if down_cond1 and down_cond2 then
        if b0.index ~= last_mark_idx then
            last_mark_idx = b0.index
            local id = core.shape_add_child("TriangleDown", 0, candle.close + 0.5)
        end
        return
    end
end
