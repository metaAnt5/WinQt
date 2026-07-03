-- ma_cross_signal.lua
-- 测试: 每个 K 棒画一个向上的三角形（测试 shape_add_child）
-- 数据量太少时不画
function on_init(params)
    core.log("ma_cross_signal initialized (DEBUG: 每K棒画三角)")
    return true
end

function on_bar_new(candle, script_name)
    -- 需要足够的 K 线数据
    if core.bars_count() < 3 then
        return
    end
    -- 每根 K 棒都在收盘价上方画一个向上的三角形
    local id = core.shape_add_child("TriangleUp", candle.index, candle.close + 1.5)
    core.log(string.format("[DEBUG] 创建子 shape id=%d  candle.index=%d price=%.2f",
        id, candle.index, candle.close + 1.5))
end
