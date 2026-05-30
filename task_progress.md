# 打包 WinLine 为绿色版可执行环境

## 任务清单
- [x] 分析项目结构和依赖
- [x] 重新配置 CMake（Qt 6.7.2 MSVC2019_64）
- [x] 编译 Release 版本
- [x] 创建打包目录并复制 exe
- [x] 运行 windeployqt 部署 Qt 依赖
- [x] 复制 NetCore/Mt4Core DLL 依赖
- [x] 复制 OpenSSL DLL 依赖
- [x] 复制 VC++ 运行库依赖
- [x] 复制配置文件和数据文件
- [x] 清理不必要的翻译文件
- [x] 验证程序可正常启动
