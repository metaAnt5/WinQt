-- kdj_exhaustion.lua
-- KDJ 动能衰竭检测
-- 判定：KDJ方向反转 + K棒力度减弱 + 突破失败
-- 用于识别当前趋势末端，预判反转

function on_init(params)
    return true
end

-- 配置参数（可在图形属性中覆盖）
local params_loaded = false

function on_bar_new(candle, script_name)
    -- 需要至少 3 根 K 线数据
    if core.bars_count() < 3 then
        return
    end

    -- 取上一根（index=1）和上上根（index=2）的数据
    local b1 = core.bar(1)
    local b2 = core.bar(2)
    if not b1 or not b2 then
        return
    end

    -- KDJ 值
    local k1, d1, j1 = core.kdj(1)
    local k2, d2, j2 = core.kdj(2)
    if not k1 or not k2 then
        return
    end

    -- ① KDJ 方向反转
    local k_dir_before = (k2 - (core.kdj(3)))  -- 前一根的方向
    local k_dir_now = k1 - k2                     -- 当前方向
    local reversed = (k_dir_before > 0 and k_dir_now < 0) or (k_dir_before < 0 and k_dir_now > 0)

    if not reversed then
        return
    end

    -- ② K 棒力度减弱（实体更小）
    local body1 = math.abs(b1.close - b1.open)
    local body2 = math.abs(b2.close - b2.open)
    local weakening = body1 < body2 * 0.7  -- 当前实体 < 前一根实体的 70%

    -- ③ 价格没突破（没创前高/前低）
    local no_breakthrough = (b1.high <= b2.high) and (b1.low >= b2.low)

    -- ④ 影线过长（上影线 > 实体，看跌方向；下影线 > 实体，看涨方向）
    local upper_shadow = b1.high - math.max(b1.open, b1.close)
    local lower_shadow = math.min(b1.open, b1.close) - b1.low
    local long_upper = k_dir_now < 0 and upper_shadow > body1  -- 看跌衰竭有长上影
    local long_lower = k_dir_now > 0 and lower_shadow > body1  -- 看涨衰竭有长下影
    local long_shadow = long_upper or long_lower

    -- 综合判定：命中至少 3 条
    local score = 0
    if weakening then score = score + 1 end
    if no_breakthrough then score = score + 1 end
    if long_shadow then score = score + 1 end
    if reversed then score = score + 1 end

    if score < 3 then
        return
    end

    -- 判定通过：飞书提醒
    local dir_text = "看跌（多头衰竭）"
    if k_dir_now > 0 then
        dir_text = "看涨（空头衰竭）"
    end

    local msg = string.format(
        "【KDJ动能衰竭】%s %s\n" ..
        "时间: %s\n" ..
        "K值: %.2f → %.2f\n" ..
        "实体: %.4f → %.4f\n" ..
        "影线/实体: 上=%.4f 下=%.4f",
        candle.symbol, dir_text,
        b1.time,
        k2, k1,
        body2, body1,
        upper_shadow, lower_shadow
    )

    core.send_feishu(msg)
end
