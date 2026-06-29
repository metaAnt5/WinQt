-- ma_cross.lua
-- KDJ 金叉死叉信号
-- 金叉（K上穿D）→ TriangleUp；死叉（K下穿D）→ TriangleDown

function on_init(params)
    core.log("kdj_strategy initialized with params: " .. params)
    return true
end

function on_bar_new(candle)
    local k, d, j = core.kdj(0)
    local prev_k, prev_d, prev_j = core.kdj(1)

    -- K 上穿 D = 金叉
    if k > d and prev_k <= prev_d then
        core.alert("KDJ 金叉! " .. candle.symbol)
        -- TriangleUp 箭头向上，画在最低点下方（加偏移避免被 K 线挡住）
        core.shape_add_child("TriangleUp", candle.index, candle.low - 1.5)
    -- K 下穿 D = 死叉
    elseif k < d and prev_k >= prev_d then
        core.alert("KDJ 死叉! " .. candle.symbol)
        -- TriangleDown 箭头向下，画在最高点上方（加偏移避免被 K 线挡住）
        core.shape_add_child("TriangleDown", candle.index, candle.high + 1.5)
    end
end

function on_bar_update(candle)
    -- 当前 K 线更新时的逻辑
end
