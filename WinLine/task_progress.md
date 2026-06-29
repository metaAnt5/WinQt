# 实时数据更新接口改进任务

- [x] 分析现有代码架构
- [ ] 增强 KLineWidget 实时数据成员（m_lastHigh, m_lastLow, m_lastVolume, m_prevClose）
- [ ] 添加 RealtimeInfo 数据表结构和绘制方法
- [ ] 添加十字悬浮信息框（crosshair info box）
- [ ] 修改 paintEvent：始终绘制实时价格水平线，添加信息面板
- [ ] 修改 updateRealtimeCandle：记录更多实时数据、更新 IndicatorCalculator
- [ ] 修改 SimWindow：连接 IndicatorCalculator 更新
