-- kdj_divergence.lua
-- KDJ 与价格背离检测
-- 顶背离（K值涨但价格跌）→ 看跌
-- 底背离（K值跌但价格涨）→ 看涨

function on_init(params)
    core.log("kdj_divergence initialized (顶底背离检测)")
    return true
end

-- 从参数读取配置
local lookback = 5      -- 对比区间长度
local threshold = 3.0   -- 最小背离幅度(%)

-- 自动从 params JSON 解析参数
function parse_params(pjson)
    if not pjson or pjson == "{}" then return end
    local ok, tbl = pcall(core.json_decode, pjson)
    if ok and type(tbl) == "table" then
        if tbl.lookback then lookback = tbl.lookback end
        if tbl.threshold then threshold = tbl.threshold end
    end
end

function on_bar_update(candle, script_name)
    parse_params(candle.params or script_name)
end

function on_bar_new(candle, script_name)
    parse_params(candle.params or script_name)
end

-- 取收盘价（不用 core.bar 因为 idx 可能超出范围，用 core.bar 更安全）
local function get_close(idx)
    local b = core.bar(idx)
    if b then return b.close end
    return nil
end

-- 取 KDJ-K 值
local function get_k(idx)
    local k, d, j = core.kdj(idx)
    return k
end

-- 主逻辑：新 K 棒生成时判断背离
function on_bar_new_real(candle, script_name)
    -- 需要足够的数据
    if core.bars_count() < lookback + 3 then
        return
    end

    -- 取区间数据
    local start_idx = lookback - 1  -- lookback 根前的 index（0=最新）
    local end_idx = 0               -- 最新 K 线

    local start_close = get_close(start_idx)
    local end_close = get_close(end_idx)
    local start_k = get_k(start_idx)
    local end_k = get_k(end_idx)

    if not start_close or not end_close or not start_k or not end_k then
        return
    end

    local close_change = (end_close - start_close) / start_close * 100
    local k_change = end_k - start_k

    -- 顶背离：K 在涨（k_change > 0），价格在跌（close_change < -threshold）
    if k_change > 0 and close_change < -threshold then
        core.log(string.format(
            "顶背离: K %.2f→%.2f (+%.2f) 收盘 %.2f→%.2f (%.2f%%)",
            start_k, end_k, k_change, start_close, end_close, close_change
        ))
        return
    end

    -- 底背离：K 在跌（k_change < 0），价格在涨（close_change > threshold）
    if k_change < 0 and close_change > threshold then
        core.log(string.format(
            "底背离: K %.2f→%.2f (%.2f) 收盘 %.2f→%.2f (+%.2f%%)",
            start_k, end_k, k_change, start_close, end_close, close_change
        ))
        return
    end
end
