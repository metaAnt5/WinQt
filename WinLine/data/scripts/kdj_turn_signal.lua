-- kdj_turn_signal.lua
-- KDJ 强势/弱势区拐头信号
-- K>80 拐头向下 → 下跌预警；K<20 拐头向上 → 上涨预警

function on_init(params)
    core.log("kdj_turn_signal initialized (KDJ拐头信号)")
    return true
end

-- 上次标记的 index（避免重复画三角形）
local last_mark_idx = -1

function on_bar_new(candle, script_name)
    -- 需要至少 3 根 K 线
    if core.bars_count() < 3 then
        core.log(string.format("KDJ: bars_count=%d < 3, skip", core.bars_count()))
        return
    end

    local k0, d0 = core.kdj(0)
    local k1, d1 = core.kdj(1)
    local k2, d2 = core.kdj(2)

    local b0 = core.bar(0)
    if not b0 then
        core.log("KDJ: bar(0) is nil, skip")
        return
    end

    -- 打印详细调试信息
    local candle_idx = candle.index or -1
    core.log(string.format(
        "KDJ_CHECK: candleIdx=%d bars_count=%d k0=%.2f d0=%.2f k1=%.2f d1=%.2f k2=%.2f d2=%.2f last_mark=%d",
        candle_idx, core.bars_count(), k0, d0, k1, d1, k2, d2, last_mark_idx))

    -- 拐头判定：需要至少 3 根数据来判断
    -- 上拐（弱势区拐头向上）：
    --   (k2 >= k1 < k0) → 前两根向下/走平，最新一根向上
    --   且 K0 < 20 或 D0 < 20
    local up_cond1 = (k2 >= k1 and k1 < k0)
    local up_cond2 = (k0 < 20 or d0 < 20)
    if up_cond1 and up_cond2 then
        if b0.index ~= last_mark_idx then
            last_mark_idx = b0.index
            core.log(string.format("KDJ_UP: condition TRUE, calling shape_add_child(TriangleUp, idx=0, price=%.2f)",
                candle.close - 0.5))
            local id = core.shape_add_child("TriangleUp", 0, candle.close - 0.5)
            if id > 0 then
                core.log(string.format("弱势区上拐: K=%.2f D=%.2f (index=%d) childId=%d",
                    k0, d0, b0.index, id))
            else
                core.log(string.format("KDJ_UP: shape_add_child FAILED id=%d", id))
            end
        else
            core.log(string.format("KDJ_UP: condition TRUE but already marked at idx=%d, skip", last_mark_idx))
        end
        return
    else
        core.log(string.format("KDJ_UP: condition FALSE (k2>=k1 && k1<k0 = %s, k0<20||d0<20 = %s)",
            tostring(up_cond1), tostring(up_cond2)))
    end

    -- 下拐（强势区拐头向下）：
    --   (k2 <= k1 > k0) → 前两根向上/走平，最新一根向下
    --   且 K0 > 80 或 D0 > 80
    local down_cond1 = (k2 <= k1 and k1 > k0)
    local down_cond2 = (k0 > 80 or d0 > 80)
    if down_cond1 and down_cond2 then
        if b0.index ~= last_mark_idx then
            last_mark_idx = b0.index
            core.log(string.format("KDJ_DOWN: condition TRUE, calling shape_add_child(TriangleDown, idx=0, price=%.2f)",
                candle.close + 0.5))
            local id = core.shape_add_child("TriangleDown", 0, candle.close + 0.5)
            if id > 0 then
                core.log(string.format("强势区下拐: K=%.2f D=%.2f (index=%d) childId=%d",
                    k0, d0, b0.index, id))
            else
                core.log(string.format("KDJ_DOWN: shape_add_child FAILED id=%d", id))
            end
        else
            core.log(string.format("KDJ_DOWN: condition TRUE but already marked at idx=%d, skip", last_mark_idx))
        end
        return
    else
        core.log(string.format("KDJ_DOWN: condition FALSE (k2<=k1 && k1>k0 = %s, k0>80||d0>80 = %s)",
            tostring(down_cond1), tostring(down_cond2)))
    end
end
