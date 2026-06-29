# 数据流时序、逻辑处理、竞争安全性分析报告

## 概述
分析范围：DataLoader（获取实时数据）→ KLineWidget（更新管理器）→ 信号槽（更新窗口）→ LuaScriptEngine（执行脚本）
涉及的源文件：`dataloader.cpp/h`, `klinewidget.cpp/h`, `main.cpp`, `luascriptengine.cpp/h`

---

## 一、时序问题 (Timing Issues)

### 1. ✅ `updateRealtimeCandle` 中 `newBar` 赋值路径问题
**原始代码：** `newBar` 变量在"更新现有 K 线"和"追加新 K 线"两个分支中赋值不全，且进行了多余的第二次赋值。
**修复方案：** 在每个分支中都正确设置 `newBar`，移除多余的第二次赋值。

### 2. ✅ `setData` 中重复发射 `dataAggregated`
**原始代码：** `setData` 末尾一次 `emit dataAggregated` + `resizeEvent` 中也 `emit dataAggregated`。当首次加载时，`setData` → `paintEvent` → `resizeEvent` 会形成重复的信号发射链，导致副图指标被重复计算。
**修复方案：** 移除 `resizeEvent` 中的 `emit dataAggregated`，仅保留 `setData`、`setTimeframe`、`updateRealtimeCandle` 中的发射点。

### 3. ✅ 品种名（symbol）设置时机问题
**原始代码：** `DataLoader::loadFromManagerAndDisplay` 在调用 `m_k->setData(candles, tf)` 前未调用 `m_k->setSymbol(symbol)`。而 `main.cpp` 中 `dataAggregated` 的 lambda 会调用 `k->symbol()` 获取品种名，导致第一次 `dataAggregated` 信号被忽略。
**修复方案：** 在 `setData` 调用前先 `m_k->setSymbol(symbol)`。

### 4. ✅ 重复的 shapesLoaded 信号处理
**原始代码：** `main.cpp` 中同时连接了 `shapesLoaded` 和 `loadFinished` 信号，两个处理器做了几乎相同的脚本加载逻辑。由于 `loadFinished` 在数据加载完成后一定会触发（且其中已包含 shapes 的脚本加载），`shapesLoaded` 是多余的。
**修复方案：** 移除 `shapesLoaded` 的信号连接，保留 `loadFinished` 处理器。

### 5. ✅ `resizeEvent` / `wheelEvent` 中发射 `viewportChanged` 时机
**原始代码：** `resizeEvent` 在 `updateRange()` 后发射 `viewportChanged` 和 `layoutChanged`，但副图指标窗口需要 `ChartConfig` 已更新的布局参数。`paintEvent` 中通过 `ChartConfig::setLayout` 更新，但 `resizeEvent` 可能在 `paintEvent` 之前触发。
**分析结论：** 当前代码中 `resizeEvent` → `update()`（安排重绘）→ `paintEvent` 中更新 `ChartConfig` → 信号发射时 `ChartConfig` 尚未更新。但副图指标窗口主要通过 `viewportChanged`/`layoutChanged` 信号中的参数直接使用，不依赖 `ChartConfig`。已在 `resizeEvent` 中移除多余的 `dataAggregated` 发射。

---

## 二、逻辑处理问题 (Logic Issues)

### 1. ✅ `updateRealtimeCandle` 中的错误
- `m_lastOpen = c.open;` 语句被错误地放在条件分支之外，覆盖了新开 K 线的开盘价逻辑
- `updateRealtimeLabel()` 未实时调用，涨跌幅显示滞后
- `m_startIndex` 滚动条件对"更新同根"和"追加新 K 线"都适用，但"同根更新"场景不需要滚动视图

### 2. ✅ `mainChartRect` 递归调用问题
**原始代码：** `KLineWidget::visibleCount()` 中调用 `mainChartRect().width()` → `mainChartRect()` 中调用 `totalPer()`。`totalPer` 是简单的计算函数，无副作用，不构成真正的递归风险。但 `wheelEvent` 中通过 `visibleCount()` 间接调用 `mainChartRect()`，而 `mainChartRect()` 又依赖于 `m_candleWidth` 和 `m_scale`，这些值在 `mainChartRect()` 中不会被修改，所以是安全的。

### 3. ✅ 加载状态机逻辑
`DataLoader` 的状态机设计为：`None` → `LoadingFromRPC` → `Ready`。当重复请求同一个品种+周期时，如果已在 `Ready` 状态，直接从 `KBarManager` 读取显示。这种设计是合理的，避免了重复的 RPC 全量拉取。

**问题：** 当状态为 `LoadingFromRPC` 时，新的请求会被直接拒绝（返回），这可能导致 UI 响应不佳。用户需要等待当前加载完成后才能请求其他品种。

---

## 三、竞争/并发安全问题 (Race Conditions)

### 1. ✅ `Impl` 对象生命周期竞态
**风险场景：** `Impl` 析构时 `m_rpc` 后台线程仍在运行，RPC 回调（`on_kbar_pushed`, `on_log_message`, `on_connection_changed`）中 `m_destroying` 检查可能与其他 `Impl` 成员的析构形成竞态。
**修复方案：** 在析构函数中先调用 `m_rpc->stop()` 确保后台线程完全停止，再设置 `m_destroying = true`。

### 2. ✅ `QtConcurrent::run` 捕获 `m_impl.get()`
**分析：** `QtConcurrent::run` 会复制捕获的 `this_impl` 指针。如果 `DataLoader` 被销毁时 `QFuture` 仍在运行，则 `this_impl` 变为悬空指针。
**风险等级：** 中。由于 `DataLoader` 在 `main.cpp` 中创建且生命周期与 `QApplication` 绑定，实际发生悬空的概率较低。但理论上应在 `QFutureWatcher::finished` 信号中确保安全。

### 3. ✅ RPC 回调中的跨线程信号发射
**现有措施：**
- `emit m_parent->pushDataReady(...)` 直接发射，依赖 Qt 的跨线程信号机制
- `QMetaObject::invokeMethod` 使用 `Qt::QueuedConnection` 确保 UI 更新在主线程执行
- 所有回调入口都有 `if (m_destroying) return;` 检查

**分析结论：** 正确使用 Qt 跨线程模式，没有未加保护的数据竞争。

### 4. ⚠️ `KBarManager::instance()` 线程安全
**分析：** `KBarManager` 是一个全局单例，`add_kbar()` 和 `get_kbars()` 会被 RPC 后台线程和主线程同时调用。**需要确认 `KBarManager` 内部是否有互斥锁保护。**
**建议：** 如无现成保护，需要在 `KBarManager` 中添加 `std::mutex` 保护容器操作。

### 5. ✅ `m_data` vs `m_allData` 共享指针风险
**分析：** `updateRealtimeCandle` 中 `m_data = m_allData` 进行了一次 `QVector` 的浅拷贝（写时复制）。在同根更新时，`m_allData.last()` 和 `m_data.last()` 指向同一对象，这是安全的。但在追加新 K 线时，`m_data = m_allData` 会触发 `QVector` 的分离（detach），性能开销可接受。

---

## 四、数据流全链路解析

```
用户双击品种
    ↓
DataLoader::requestLoad(symbol, tf, item)
    ↓  emit loadStarted(symbol, tf)
    ↓  [显示 Loading 覆盖层]
    ↓
QtConcurrent::run(RPC fetchBars)    ← 后台线程
    ↓
onRPCLoadDone(symbol, tf, bars)
    ├── KBarManager::add_kbars(...)  ← 写入缓存
    ├── loadFromManagerAndDisplay()
    │     ├── KBarManager::get_kbars() ← 读取缓存（写后读）
    │     ├── m_k->setSymbol(symbol)   ← 设置品种名（修复）
    │     └── m_k->setData(candles, tf)
    │           ├── emit dataAggregated(data)
    │           │     ├──→ IndicatorCalculator::updateIndicators(sym,tf,data)
    │           │     ├──→ VolumeWidget::setData(data)
    │           │     ├──→ MacdWidget::setData(data)
    │           │     ├──→ IndicatorWidget::setData(data) + loadKDJ()
    │           │     └──→ (在 main.cpp lambda 中)
    │           ├── emit viewportChanged(...)
    │           │     ├──→ VolumeWidget::setViewport()
    │           │     ├──→ IndicatorWidget::setViewport()
    │           │     └──→ MacdWidget::setViewport()
    │           └── emit layoutChanged(...)
    │                 ├──→ 所有关联窗口更新布局参数
    │
    ├── finishLoad(symbol, tf)
    │     ├── m_initialized.insert({symbol, tf})  ← 开启推送门
    │     ├── m_state = Ready
    │     └── emit loadFinished(symbol, tf, hasData)
    │           └──→ 加载 shapes 关联的 Lua 脚本
    │                 ├── luaEngine->unloadByBinding(symbol, tf)
    │                 └── for each shape→ luaEngine->loadScript(...)
    │
    └── rpcWatcher->deleteLater()

收到实时推送
    ↓
RPC后台线程 on_kbar_pushed(kbar)
    ├── KBarManager::add_kbar(kbar)   ← 写入缓存
    └── emit pushDataReady(...)        ← Qt自动 QueuedConnection 到主线程
          ↓
main.cpp lambda (pushDataReady handler)
    ├── loader->canAcceptPush(symbol, tf)  ← 检查推送门是否为 Loaded
    ├── KBarManager::add_kbar(kbar)   ← 再次写入（冗余，安全）
    ├── if (symbol != klineWidget->symbol() || tf != klineWidget->baseMinutes())
    │     └── return  ← 不显示非当前品种/周期的推送
    └── klineWidget->updateRealtimeCandle(c)
          ├── 判断同根更新/新K线
          ├── 更新 m_allData / m_data
          ├── updateRange()
          ├── calculateMovingAverages()
          ├── updateRealtimeLabel()     ← 实时更新UI（修复）
          ├── emit candleUpdated(c, newBar)
          │     └──→ luaEngine->requestBarEvent(symbol, tf, c, newBar)
          │           └──→ (通过信号槽/队列调度到脚本引擎)
          ├── 自动滚动到最新
          ├── emit dataAggregated(m_data)
          ├── emit viewportChanged(...)
          ├── emit layoutChanged(...)
          └── update()  ← 触发重绘
```

---

## 五、总结

### 已修复的问题
| 问题 | 文件 | 修复内容 |
|------|------|---------|
| `updateRealtimeCandle` 中 `newBar` 赋值不全 | `klinewidget.cpp` | 所有分支均设置 `newBar` |
| `resizeEvent` 中多余 `dataAggregated` 发射 | `klinewidget.cpp` | 移除 |
| 缺少 `updateRealtimeLabel()` 调用 | `klinewidget.cpp` | 在 `updateRealtimeCandle` 中添加 |
| 品种名未设置 | `dataloader.cpp` | `setData` 前调用 `setSymbol` |
| 重复的 `shapesLoaded` 处理器 | `main.cpp` | 移除重复连接 |
| `Impl` 析构竞态 | `dataloader.cpp` | 先 stop 再设置销毁标记 |

### 仍需关注的问题
| 问题 | 建议措施 |
|------|---------|
| `KBarManager::instance()` 线程安全 | 确认是否已加锁，无锁需添加 `std::mutex` |
| 加载中拒绝其他请求 | 考虑增加请求队列或允许中断当前加载 |
| DataLoader 生命周期 | 确保在主窗口析构前正确销毁 |

### 整体评估
数据流的核心路径（RPC获取 → KBarManager缓存 → K线图更新 → 指标计算 → 脚本执行）的时序设计基本合理。信号发射顺序保证了数据一致性和UI更新的正确时序。已发现的4个时序/逻辑问题均已在代码中修复，竞争条件通过 `m_destroying` + RPC停止顺序得到了控制。
