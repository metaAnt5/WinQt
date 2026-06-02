# WinLine Lua 脚本 API 文档

## 概述

Lua 脚本存放在 `data/scripts/` 目录下，扩展名为 `.lua`。
脚本通过 `core.*` API 访问 K 线数据、计算指标、操作图形等。

脚本加载时自动调用 `on_init(params)`，K 线更新时调用 `on_bar_new(candle)` 或 `on_bar_update(candle)`。

---

## 全局 API (`core.*`)

### `core.log(msg)`
输出日志到 Log 面板。
```lua
core.log("均线计算完成")
```

### `core.bar(index)`
获取 K 线数据。`index` 从 0 开始，0=最新一根，1=上一根，以此类推。
返回 `table` 或 `nil`（索引越界时）。
```lua
local bar = core.bar(0)
if bar then
    print(bar.time, bar.open, bar.high, bar.low, bar.close, bar.volume)
end
```
返回表字段：`time`, `open`, `high`, `low`, `close`, `volume`

### `core.bars_count()`
返回当前品种的总 K 线数量。
```lua
local total = core.bars_count()
core.log("总 K 线数: " .. total)
```

### `core.current_symbol()`
返回当前正在显示的品种名称（字符串）。
```lua
local sym = core.current_symbol()
```

### `core.current_tf()`
返回当前周期（分钟）。
```lua
local tf = core.current_tf()
-- 5, 15, 60, 240, 1440 等
```

### `core.ma(period, index)`
计算移动平均线。`period`=周期数，`index`=K 线位置(0=最新)。
```lua
local ma5 = core.ma(5, 0)   -- 最新 5 周期均线
local ma10 = core.ma(10, 1) -- 上一根 K 线的 10 周期均线
```

### `core.rsi(period, index)`
计算 RSI。`period`=周期数，`index`=K 线位置(0=最新)。
```lua
local rsi14 = core.rsi(14, 0)
```

### `core.highest(period, index)`
返回最近 `period` 根 K 线内的最高价。`index`=结束位置(0=最新)。
```lua
local hh = core.highest(20, 0)  -- 最近 20 根 K 线最高价
```

### `core.lowest(period, index)`
返回最近 `period` 根 K 线内的最低价。`index`=结束位置(0=最新)。
```lua
local ll = core.lowest(20, 0)   -- 最近 20 根 K 线最低价
```

### `core.alert(msg)`
触发提醒消息。
```lua
core.alert("价格突破! " .. core.current_symbol())
```

### `core.shape_add(type, candleIdx1, price1, candleIdx2, price2, name)`
在图表上添加一个图形。返回图形的 id（整数），可用于后续删除。
- `type`: 图形类型字符串（见下方列表）
- `candleIdx1/price1`: 起点（K 线索引, 价格）
- `candleIdx2/price2`: 终点（K 线索引, 价格）
- `name`: 图形名称（可选）

```lua
-- 在最新 K 线画一个买入标记
local id = core.shape_add("TradeBuy", 0, core.bar(0).low, 0, core.bar(0).high, "金叉买入")
```
**type 可选值:**
| 字符串 | 含义 |
|--------|------|
| `"Line"` | 线段 |
| `"Trend"` | 趋势线 |
| `"GestureUp"` | 上涨手势 |
| `"GestureDown"` | 下跌手势 |
| `"Text"` | 文字 |
| `"HLine"` | 水平线 |
| `"VLine"` | 垂直线 |
| `"TradeBuy"` | 买入标记 |
| `"TradeSell"` | 卖出标记 |
| `"TradeShort"` | 做空标记 |
| `"TradeCover"` | 平仓标记 |

### `core.get_shape_price(scriptName)`
获取关联图形的价格。`scriptName` 是脚本文件名（不带 `.lua` 或带都可以）。
返回 `number`（图形的 `price1`）或 `nil`（没找到关联图形）。
```lua
-- 获取当前脚本关联图形的价格
local price = core.get_shape_price("break_line")
if price then
    core.log("关联线价格: " .. price)
end
```

### `core.shape_remove(id)`
删除指定 id 的图形。
```lua
core.shape_remove(3)  -- 删除 id=3 的图形
```

---

## 回调函数

### `on_init(params)`
脚本被加载时调用。`params` 是传入的参数 JSON 字符串。
返回 `true` 表示初始化成功，返回 `false` 或其它值表示失败。
```lua
function on_init(params)
    core.log("脚本初始化, 参数: " .. params)
    return true
end
```

### `on_bar_new(candle, scriptName)`
当新 K 线生成时调用。`candle` 是包含以下字段的 table：
```lua
function on_bar_new(candle, scriptName)
    -- candle.symbol    : 品种名称 (string)
    -- candle.timeframe : 周期分钟 (int)
    -- candle.time      : ISO 时间 (string)
    -- candle.open      : 开盘价
    -- candle.high      : 最高价
    -- candle.low       : 最低价
    -- candle.close     : 收盘价
    -- candle.volume    : 成交量
    -- candle.index     : K 线索引 (0=最新)
    -- scriptName       : 脚本名称（可用于 core.get_shape_price）
    core.log("新 K 线: " .. candle.symbol .. " 收盘=" .. candle.close)
end
```

### `on_bar_update(candle)`
当当前 K 线数据更新（实时 tick 推送）时调用。`candle` 字段同 `on_bar_new`。
```lua
function on_bar_update(candle)
    -- 实时更新逻辑
end
```

---

## 完整示例

参见同目录下的 `ma_cross.lua`：

```lua
-- ma_cross.lua - 均线金叉死叉策略
function on_init(params)
    core.log("ma_cross initialized with params: " .. params)
    return true
end

function on_bar_new(candle)
    local fast = core.ma(5, 0)
    local slow = core.ma(20, 0)
    local prev_fast = core.ma(5, 1)
    local prev_slow = core.ma(20, 1)

    if fast > slow and prev_fast <= prev_slow then
        core.alert("金叉! " .. candle.symbol)
        core.shape_add("TradeBuy", candle.index, candle.low, candle.index, candle.high, "金叉")
    elseif fast < slow and prev_fast >= prev_slow then
        core.alert("死叉! " .. candle.symbol)
        core.shape_add("TradeSell", candle.index, candle.high, candle.index, candle.low, "死叉")
    end
end
```

---

## 注意事项

1. **K 线索引规则**: 所有 `index` 参数中，`0` 永远表示最新一根 K 线，`1` 表示上一根，依此类推。
2. **参数传递**: 脚本加载时通过 `on_init(params)` 传入 JSON 字符串参数，可在脚本中自行解析。
3. **性能**: `on_bar_update` 在实时推送时高频调用，避免在内部执行耗时操作。
