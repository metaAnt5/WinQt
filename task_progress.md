# 修改任务清单

## 🔴 高优先级

- [ ] 1. `klinewidget.h` - Shape 结构加 `bool fromScript = false`
- [ ] 2. `klinewidget.cpp` - `saveShapes()` 只保存 `fromScript==false` 的 shape
- [ ] 3. `klinewidget.cpp` - `loadShapes()` 加载时 `fromScript=false`
- [ ] 4. `luascriptengine.h` - 新增 `m_symbolScriptMap`，`m_replayMap`，移除 `m_currentCandleIndex`
- [ ] 5. `luascriptengine.cpp` - `loadScript()` 维护 `m_symbolScriptMap`
- [ ] 6. `luascriptengine.cpp` - `unloadScript()` 维护 `m_symbolScriptMap`
- [ ] 7. `luascriptengine.cpp` - `unloadByBinding()` 维护 `m_symbolScriptMap`
- [ ] 8. `luascriptengine.cpp` - `loadShapesFromDisk()` 重建 `m_symbolScriptMap`
- [ ] 9. `luascriptengine.cpp` - `reloadShapesForSymbol()` 重建 `m_symbolScriptMap`
- [ ] 10. `luascriptengine.cpp` - `onBarEvent()` 改为通过 `m_symbolScriptMap` 直接索引
- [ ] 11. `dataloader.cpp` - `requestLoad` 允许 `symItem=nullptr`
- [ ] 12. `main.cpp` - `scriptsInitialized` 回调自动加载

## 🟡 中优先级

- [ ] 13. `luascriptengine.cpp` - `shape_add()/shape_add_fixed()/child_add()` 设 `fromScript=true`
- [ ] 14. `luascriptengine.cpp` - `child_add()` 删除 `saveShapes()`
- [ ] 15. `luascriptengine.cpp` - `child_remove()` 删除 `saveShapes()`
- [ ] 16. `luascriptengine.cpp` - `reloadShapesForSymbol()` 中 `sp->fromScript = false`
- [ ] 17. `luascriptengine.cpp` - `onBarEvent` 中去掉 candle index 字段
- [ ] 18. `main.cpp` - 非当前周期推送也触发指标计算
- [ ] 19. `indicatorcalc.cpp` - `appendCandle()/updateLastCandle()` 真正计算

## 🟢 低优先级

- [ ] 20. `luascriptengine.h` - 移除 `m_currentCandleIndex` 成员
