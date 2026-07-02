#pragma once

#include <QObject>
#include <QString>
#include <QPointF>
#include <QHash>
#include <QPair>
#include <QVector>
#include <memory>
#include <QMutex>
#include "klinewidget.h"

struct KBar;
class KLineWidget;
class FeishuSender;

namespace NetCore {
class IoContextManager;
}

// ============================================================
// ScriptBinding - 记录每个脚本绑定的品种/周期信息
// ============================================================
struct ScriptBinding {
    QString symbol;    // 关联的品种，空字符串表示所有品种
    int timeframe = 0; // 关联的周期（分钟），0 表示所有周期
};

// ============================================================
// LuaScriptEngine - Lua 5.5 脚本引擎
//
// 设计原则：脚本加载和执行独立于 UI
//   - 启动时自动扫描 data/shapes/*.json 加载关联的脚本
//   - core.bar() 等 API 从内部缓存或 KLineWidget 读取数据
//   - core.get_line_price() 从内部缓存的 shapes 读取
//     不依赖 KLineWidget::shapes()
// ============================================================
class LuaScriptEngine : public QObject {
    Q_OBJECT
public:
    explicit LuaScriptEngine(QObject *parent = nullptr);
    ~LuaScriptEngine();

    // ── 初始化 ──
    bool initialize();

    // 加载/卸载脚本
    bool loadScript(const QString &scriptName, const QString &params = QString(),
                    const ScriptBinding &binding = ScriptBinding());
    void unloadScript(const QString &scriptName);
    void unloadByBinding(const QString &symbol, int timeframe);

    // ── 从 data/shapes/ 目录自动扫描加载脚本（不依赖 UI） ──
    void loadShapesFromDisk();

    // ── K 线事件触发 ──
    void onBarEvent(const QString &symbol, int timeframe,
                    const Candle &candle, bool isNewBar);

    QString lastError() const { return m_lastError; }

    // ── KLineWidget 注册表：按 (symbol, tf) 管理 ──
    void registerKLineWidget(KLineWidget *kw);
    void unregisterKLineWidget(KLineWidget *kw);
    KLineWidget *klineWidgetFor(const QString &symbol, int timeframe) const;

    // 设置/获取当前活动的 KLineWidget
    // klineWidget() 在 onBarEvent 期间自动按 (currentSymbol, currentTimeframe) 从注册表解析
    void setKLineWidget(KLineWidget *kw) { m_klineWidget = kw; }
    KLineWidget *klineWidget() const;

    // ── 跨线程安全调度 ──
    void requestBarEvent(const QString &symbol, int timeframe,
                         const Candle &candle, bool isNewBar);

    // ── 从 KBarManager 获取缓存的 K 线数据（独立于 UI） ──
    // 返回指定 (symbol, tf) 的全部 K 线（按时间升序排列）
    QVector<Candle> fetchCandlesFromManager(const QString &symbol, int timeframe) const;

    // ── shapes 缓存查询 ──
    // 获取指定 (symbol, tf) 的缓存的 shapes
    QVector<QSharedPointer<KLineWidget::Shape>> shapesForBinding(const QString &symbol, int timeframe) const;

    // 手动刷新某 (symbol,tf) 的 shapes 缓存（由 UI 保存 shapes 后调用）
    void reloadShapesForSymbol(const QString &symbol, int timeframe);

    // 获取所有已从磁盘加载的 (symbol,tf) 组合
    QList<QPair<QString,int>> allLoadedShapeSymbols() const;

    // ---- Fixed shape child management ----
    // 为指定父 shape 创建子 shape（Fixed 类型）
    int addChildShape(int parentShapeId, const QString &type, double normX, double normY, const QString &text);
    // 删除子 shape
    bool removeChildShape(int childShapeId);
    // 获取某父 shape 的所有子 shape id 列表
    QVector<int> childShapeIds(int parentShapeId) const;
    // 获取当前脚本上下文中的父 shape id
    int currentScriptParentShapeId() const { return m_currentParentShapeId; }

    // ── 回放模式（历史数据加载/模拟回放时启用，抑制 alert/send_feishu） ──
    void setReplayMode(bool replay) {
        m_isReplay = replay;
        if (replay) m_replayCount = 0; // 开始回放时重置计数器
    }
    bool isReplayMode() const { return m_isReplay; }

    // 回放期间累计的 K 线数量（仅供 main.cpp 回放结束后打印汇总）
    int replayCount() const { return m_replayCount; }

    // ── 脚本说明（从文件头部 -- 注释解析） ──
    // 获取单个脚本的说明文字（按行提取，每行一个 -- 注释行）
    QString getScriptDescription(const QString &scriptName) const;
    // 获取所有脚本的 {文件名(不带.lua): 说明} 映射
    QHash<QString, QString> getAllScriptDescriptions() const;

    // 获取已加载脚本名称列表（用于日志和调试）
    QStringList loadedScriptNames() const {
        return QStringList(m_loadedScripts.keys());
    }

    // ── 以下访问器供 C 回调函数使用 ──
    // 当前脚本上下文（由 onBarEvent 设置）
    QString currentSymbol() const { return m_currentSymbol; }
    int currentTimeframe() const { return m_currentTimeframe; }

    // 脚本名→shapes 的快速索引（供 findShapesForScript 使用）
    const QHash<QString, QVector<QSharedPointer<KLineWidget::Shape>>>& scriptShapesIndex() const { return m_scriptShapesIndex; }

    // 获取当前正在执行的脚本名（供 C 回调在无显式参数时确定上下文）
    QString currentScriptName() const { return m_currentScriptName; }

    // 飞书消息发送器（供 Lua C API 回调使用）
    std::shared_ptr<FeishuSender> feishuSender() const { return m_feishuSender; }

    // IoContextManager 访问器（供 Lua C API 创建临时 FeishuSender 使用）
    std::shared_ptr<NetCore::IoContextManager> ioContextManager() const { return m_ioCtxMgr; }

    // 获取某 (symbol,tf) 的磁盘 shapes 缓存（供 main.cpp 自动加载时使用）
    const QVector<QSharedPointer<KLineWidget::Shape>> &shapesDiskCache(const QString &key) const {
        static QVector<QSharedPointer<KLineWidget::Shape>> empty;
        auto it = m_shapesDiskCache.find(key);
        return it != m_shapesDiskCache.end() ? it.value() : empty;
    }

Q_SIGNALS:
    void scriptLog(const QString &msg);
    void scriptError(const QString &scriptName, const QString &error);

    // 跨线程调度信号
    void barEventRequested(const QString &symbol, int timeframe,
                           const Candle &candle, bool isNewBar);



    // 脚本初始化完成，携带所有需要预加载的 (symbol, tf) 列表
    // 由 loadShapesFromDisk() 完成后发射
    void scriptsInitialized(const QList<QPair<QString,int>> &symbols);

private:
    mutable QMutex m_mutex;

    void *m_state = nullptr; // lua_State*

    // 已加载的脚本 + 绑定信息
    QHash<QString, ScriptBinding> m_loadedScripts;

    // 从 data/shapes/*.json 解析缓存的 shapes（独立于 KLineWidget）
    // key = "symbol|timeframe"
    QHash<QString, QVector<QSharedPointer<KLineWidget::Shape>>> m_shapesDiskCache;

    // ── 脚本名 → shapes 的快速索引（避免每次 O(N) 遍历） ──
    // 在 loadShapesFromDisk / reloadShapesForSymbol 时重建
    // key = scriptName (不带 .lua 后缀)
    // 注意：一个脚本可能关联多个符号/周期的多条线，所以用 QVector
    QHash<QString, QVector<QSharedPointer<KLineWidget::Shape>>> m_scriptShapesIndex;

    // 当前脚本正在处理的 (symbol,tf)，用于 core.* API 在无 UI 时获取数据
    // 由 onBarEvent 调用前设置
    QString m_currentSymbol;
    int m_currentTimeframe = 0;

    // 当前正在执行的脚本名（由 onBarEvent / loadScript 设置）
    QString m_currentScriptName;

    // ── (symbol|tf) → 脚本名列表，快速索引脚本 ──
    // 在 loadShapesFromDisk / reloadShapesForSymbol 时重建
    QHash<QString, QStringList> m_symbolScriptMap;

    // ── (symbol|tf) → true（回放模式）/ false（实时模式） ──
    // 每个 (symbol,tf) 独立控制，加载历史数据时设为 true
    // 收到第一条实时推送后自动变为 false，飞书才可发送
    QHash<QString, bool> m_replayMap;

    // 回放模式标志：默认 true（回放模式），收到第一条实时数据后自动变为 false
    // 历史加载/模拟回放时抑制 alert/send_feishu
    bool m_isReplay = true;
    // 回放期间 K 线事件累计计数（用于 main.cpp 回放结束后打印汇总）
    int m_replayCount = 0;

    // 飞书发送器（复用，通过 FeishuSender::SendMarkdown 异步发送）
    std::shared_ptr<FeishuSender> m_feishuSender;

    // IoContextManager：驱动 FeishuSender 的异步 io_context（共享给临时 FeishuSender）
    std::shared_ptr<NetCore::IoContextManager> m_ioCtxMgr;

    int m_currentParentShapeId = 0; // 当前脚本执行上下文中的父 shape id

    QString m_lastError;

    // ── KLineWidget 注册表 ──
    // key = "symbol|timeframe" → KLineWidget*
    QHash<QString, KLineWidget*> m_klineWidgetRegistry;
    KLineWidget *m_klineWidget = nullptr;

    void registerCoreAPI();
    void initFeishuSender();
    struct lua_State *L() const;

    // 辅助: 解析 shapes JSON 文件中的 candleIdx（用时间戳重新定位索引）
    static int resolveCandleIndex(const QVector<Candle> &data,
                                   int oldIdx, const QDateTime &dt);
    // 获取当前 (symbol,tf) 对应的正确 KLineWidget
    KLineWidget *resolveKLineWidget() const;
};

// ============================================================
// 便捷函数: 检查 Candle 是否为新 K 线
// ============================================================
inline bool isNewBar(const Candle &newCandle, const QVector<Candle> &allData)
{
    if (allData.isEmpty()) return true;
    return newCandle.date != allData.last().date;
}
