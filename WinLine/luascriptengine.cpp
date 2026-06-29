#include "luascriptengine.h"
#include "klinewidget.h"
#include "indicatorcalc.h"
#include "apppaths.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QJsonArray>
#include <QMutexLocker>
#include <QThread>
#include <QTextStream>

// Lua 5.5 headers
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ============================================================
// Lua C API 函数
// ============================================================

// forward declarations
static KLineWidget* getKLineWidget(lua_State *L);

// core.log(msg)
static int lua_core_log(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    qDebug() << "[Lua]" << msg;
    // emit signal if we can find the engine
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    if (engine) {
        emit engine->scriptLog(QString::fromUtf8(msg));
    }
    return 0;
}

// core.bar(index) -> table {time, open, high, low, close, volume}
// index: 0=当前最新, 1=上一根, 2=上两根...
static int lua_core_bar(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushnil(L);
        return 1;
    }

    int index = (int)luaL_checkinteger(L, 1);
    const auto &data = kw->allData();
    int idx = data.size() - 1 - index; // index: 0 = latest, 1 = previous
    if (idx < 0 || idx >= data.size()) {
        lua_pushnil(L);
        return 1;
    }

    const Candle &c = data[idx];
    lua_createtable(L, 0, 6);
    lua_pushstring(L, "time");    lua_pushstring(L, c.date.toString(Qt::ISODate).toUtf8().constData()); lua_settable(L, -3);
    lua_pushstring(L, "open");    lua_pushnumber(L, c.open);   lua_settable(L, -3);
    lua_pushstring(L, "high");    lua_pushnumber(L, c.high);   lua_settable(L, -3);
    lua_pushstring(L, "low");     lua_pushnumber(L, c.low);    lua_settable(L, -3);
    lua_pushstring(L, "close");   lua_pushnumber(L, c.close);  lua_settable(L, -3);
    lua_pushstring(L, "volume");  lua_pushnumber(L, c.volume); lua_settable(L, -3);
    return 1;
}

// core.bars_count() -> int
static int lua_core_bars_count(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, kw->allData().size());
    return 1;
}

// core.current_symbol() -> string
static int lua_core_current_symbol(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, kw->symbol().toUtf8().constData());
    return 1;
}

// core.current_tf() -> int (minutes)
static int lua_core_current_tf(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, kw->baseMinutes());
    return 1;
}

// 辅助: 将 Lua index（0=最新, 1=上一根...）转为数组下标
static int luaIndexToArrayIdx(const QVector<Candle> &data, int index)
{
    return data.size() - 1 - index;
}

// core.ma(period, index) -> double
// index: 0=最新, 1=上一根...
static int lua_core_ma(lua_State *L)
{
    int period = (int)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2);

    // get symbol and tf from engine's upvalue
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    if (!engine || !engine->klineWidget()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    KLineWidget *kw = engine->klineWidget();
    const auto &data = kw->allData();
    int dataIdx = luaIndexToArrayIdx(data, index);

    // read from IndicatorCalculator cache
    auto &calc = IndicatorCalculator::instance();
    double val = calc.getMA(kw->symbol(), kw->baseMinutes(), period, dataIdx);
    lua_pushnumber(L, val);
    return 1;
}

// core.kdj(index) -> k, d, j
// index: 0=最新, 1=上一根...
// 从 IndicatorCalculator 缓存中读取 KDJ 值
static int lua_core_kdj(lua_State *L)
{
    int index = (int)luaL_checkinteger(L, 1);

    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    if (!engine || !engine->klineWidget()) {
        lua_pushnumber(L, 50.0);
        lua_pushnumber(L, 50.0);
        lua_pushnumber(L, 50.0);
        return 3;
    }
    KLineWidget *kw = engine->klineWidget();
    const auto &data = kw->allData();
    int dataIdx = luaIndexToArrayIdx(data, index);

    auto &calc = IndicatorCalculator::instance();
    QVector<double> k, d, j;
    calc.getKDJ(kw->symbol(), kw->baseMinutes(), k, d, j);

    double vk = (dataIdx >= 0 && dataIdx < k.size()) ? k[dataIdx] : 50.0;
    double vd = (dataIdx >= 0 && dataIdx < d.size()) ? d[dataIdx] : 50.0;
    double vj = (dataIdx >= 0 && dataIdx < j.size()) ? j[dataIdx] : 50.0;

    lua_pushnumber(L, vk);
    lua_pushnumber(L, vd);
    lua_pushnumber(L, vj);
    return 3;
}

// core.highest(period, index) -> double
// index: 0=最新, 1=上一根...
static int lua_core_highest(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    int period = (int)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    const auto &data = kw->allData();
    int dataIdx = luaIndexToArrayIdx(data, index);
    if (data.isEmpty() || dataIdx < 0 || dataIdx >= data.size()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    double highest = -1e9;
    int start = qMax(0, dataIdx - period + 1);
    for (int i = start; i <= dataIdx; ++i) {
        if (data[i].high > highest) highest = data[i].high;
    }
    lua_pushnumber(L, highest);
    return 1;
}

// core.lowest(period, index) -> double
// index: 0=最新, 1=上一根...
static int lua_core_lowest(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    int period = (int)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    const auto &data = kw->allData();
    int dataIdx = luaIndexToArrayIdx(data, index);
    if (data.isEmpty() || dataIdx < 0 || dataIdx >= data.size()) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    double lowest = 1e9;
    int start = qMax(0, dataIdx - period + 1);
    for (int i = start; i <= dataIdx; ++i) {
        if (data[i].low < lowest) lowest = data[i].low;
    }
    lua_pushnumber(L, lowest);
    return 1;
}

// core.alert(msg)
static int lua_core_alert(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    qDebug() << "[Lua Alert]" << msg;
    // TODO: trigger UI alert
    return 0;
}

// core.shape_add(type, candleIdx1, price1, candleIdx2, price2, name) -> int (shape id)
static int lua_core_shape_add(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushinteger(L, -1);
        return 1;
    }

    const char *typeStr = luaL_checkstring(L, 1); // "Line", "Trend", "GestureUp", "Text", etc.
    int candleIdx1 = (int)luaL_checkinteger(L, 2);
    double price1 = luaL_checknumber(L, 3);
    int candleIdx2 = (int)luaL_checkinteger(L, 4);
    double price2 = luaL_checknumber(L, 5);
    const char *name = luaL_optstring(L, 6, "");

    // Map type string to ShapeType
    using ShapeType = KLineWidget::ShapeType;
    ShapeType st = ShapeType::Shape_Line; // default
    if (strcmp(typeStr, "Line") == 0 || strcmp(typeStr, "line") == 0) st = ShapeType::Shape_Line;
    else if (strcmp(typeStr, "Trend") == 0 || strcmp(typeStr, "trend") == 0) st = ShapeType::Shape_Trend;
    else if (strcmp(typeStr, "UpTriangle") == 0) st = ShapeType::Shape_UpTriangle;
    else if (strcmp(typeStr, "DownTriangle") == 0) st = ShapeType::Shape_DownTriangle;
    else if (strcmp(typeStr, "Note") == 0) st = ShapeType::Shape_UpTriangle;

    KLineWidget::Shape s;
    s.type = st;
    s.candleIdx1 = candleIdx1;
    s.price1 = price1;
    s.candleIdx2 = candleIdx2;
    s.price2 = price2;
    s.name = QString::fromUtf8(name);
    s.color = QColor(Qt::white);
    // Set scriptName from current script context
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    if (engine) {
        s.scriptName = engine->currentScriptName();
    }
    int newId = kw->addShape(s);

    lua_pushinteger(L, newId);
    return 1;
}

// core.get_shape_price(scriptName) -> number (price1 of the shape) or nil
// 根据脚本名称查找关联的图形，返回其 price1
static int lua_core_get_shape_price(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) {
        lua_pushnil(L);
        return 1;
    }

    const char *scriptName = luaL_checkstring(L, 1);
    if (!scriptName) {
        lua_pushnil(L);
        return 1;
    }

    QString sn = QString::fromUtf8(scriptName);
    const auto shapes = kw->shapes();
    for (const auto &s : shapes) {
        if (s.scriptName == sn || s.scriptName == sn + ".lua") {
            // 返回 price1（水平线就是线的价格）
            lua_pushnumber(L, s.price1);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

// core.shape_remove(id)
static int lua_core_shape_remove(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) return 0;

    int id = (int)luaL_checkinteger(L, 1);
    QVector<KLineWidget::Shape> shapes = kw->shapes();
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
                                [id](const KLineWidget::Shape &s) { return s.id == id; }),
                 shapes.end());
    kw->setShapes(shapes);
    kw->saveShapes();
    return 0;
}

// core.shape_add_fixed(type, normX, normY, text, name) -> int (shape id)
static int lua_core_shape_add_fixed(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) { lua_pushinteger(L, -1); return 1; }

    const char *typeStr = luaL_checkstring(L, 1);
    double normX = luaL_checknumber(L, 2);
    double normY = luaL_checknumber(L, 3);
    const char *text = luaL_optstring(L, 4, "");
    const char *name = luaL_optstring(L, 5, "");

    using ShapeType = KLineWidget::ShapeType;
    ShapeType st = ShapeType::Shape_FixedDot;
    if (strcmp(typeStr, "Circle") == 0 || strcmp(typeStr, "circle") == 0) st = ShapeType::Shape_FixedDot;
    else if (strcmp(typeStr, "Triangle") == 0 || strcmp(typeStr, "triangle") == 0) st = ShapeType::Shape_FixedTriangle;
    else if (strcmp(typeStr, "Dot") == 0 || strcmp(typeStr, "dot") == 0) st = ShapeType::Shape_FixedDot;
    else if (strcmp(typeStr, "Note") == 0 || strcmp(typeStr, "note") == 0) st = ShapeType::Shape_FixedDot;
    else if (strcmp(typeStr, "Label") == 0 || strcmp(typeStr, "label") == 0) st = ShapeType::Shape_FixedDot;

    KLineWidget::Shape s;
    s.type = st;
    s.attachment = KLineWidget::Attach_Fixed;
    s.normX = qBound(0.0, normX, 1.0);
    s.normY = qBound(0.0, normY, 1.0);
    s.text = QString::fromUtf8(text);
    s.name = QString::fromUtf8(name);
    s.color = QColor(255, 200, 100);
    // Set scriptName from current script context
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    if (engine) {
        s.scriptName = engine->currentScriptName();
    }
    int newId = kw->addShape(s);
    lua_pushinteger(L, newId);
    return 1;
}

// core.child_add(type, normX, normY, text) -> int (child shape id)
// Creates a child Fixed shape owned by the currently executing script/bar event
static int lua_core_child_add(lua_State *L)
{
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    KLineWidget *kw = engine ? engine->klineWidget() : nullptr;
    if (!kw) { lua_pushinteger(L, -1); return 1; }

    const char *typeStr = luaL_checkstring(L, 1);
    double normX = luaL_checknumber(L, 2);
    double normY = luaL_checknumber(L, 3);
    const char *text = luaL_optstring(L, 4, "");

    int parentId = engine->currentScriptParentShapeId();
    if (parentId <= 0) { lua_pushinteger(L, -1); return 1; }

    int childId = engine->addChildShape(parentId, QString::fromUtf8(typeStr), normX, normY, QString::fromUtf8(text));
    lua_pushinteger(L, childId);
    if (kw) kw->saveShapes();
    return 1;
}

// core.child_remove(id)
static int lua_core_child_remove(lua_State *L)
{
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    KLineWidget *kw = engine ? engine->klineWidget() : nullptr;
    if (!kw) return 0;

    int childId = (int)luaL_checkinteger(L, 1);
    engine->removeChildShape(childId);
    if (kw) kw->saveShapes();
    return 0;
}

// core.child_select(id)
// core.child_clear()
static int lua_core_child_select(lua_State *L)
{
    KLineWidget *kw = getKLineWidget(L);
    if (!kw) return 0;

    QVector<KLineWidget::Shape> shapes = kw->shapes();

    if (lua_gettop(L) == 0) {
        // child_clear: deselect all
        for (auto &s : shapes) s.selected = false;
        kw->setShapes(shapes);
        return 0;
    }
    int id = (int)luaL_checkinteger(L, 1);
    for (int i = 0; i < shapes.size(); ++i) {
        shapes[i].selected = (shapes[i].id == id);
    }
    kw->setShapes(shapes);
    return 0;
}

// Helper: get KLineWidget from upvalue
static KLineWidget* getKLineWidget(lua_State *L)
{
    LuaScriptEngine *engine = (LuaScriptEngine*)lua_touserdata(L, lua_upvalueindex(1));
    if (!engine) return nullptr;
    return engine->klineWidget();
}


// ============================================================
// LuaScriptEngine 实现
// ============================================================

LuaScriptEngine::LuaScriptEngine(QObject *parent)
    : QObject(parent), m_state(nullptr)
{
    // 连接跨线程信号：barEventRequested 自动在主线程调用 onBarEvent
    QObject::connect(this, &LuaScriptEngine::barEventRequested,
                     this, &LuaScriptEngine::onBarEvent,
                     Qt::QueuedConnection);
}

LuaScriptEngine::~LuaScriptEngine()
{
    if (m_state) {
        lua_close((lua_State*)m_state);
        m_state = nullptr;
    }
}

struct lua_State *LuaScriptEngine::L() const
{
    return (lua_State*)m_state;
}

bool LuaScriptEngine::initialize()
{
    // 创建 Lua 状态
    m_state = luaL_newstate();
    if (!m_state) {
        m_lastError = QStringLiteral("Failed to create Lua state");
        return false;
    }

    // 打开标准库
    luaL_openlibs((lua_State*)m_state);

    // 注册 core API
    registerCoreAPI();

    return true;
}

void LuaScriptEngine::registerCoreAPI()
{
    lua_State *L = (lua_State*)m_state;
    if (!L) return;

    static const luaL_Reg core_funcs[] = {
        {"log",         lua_core_log},
        {"bar",         lua_core_bar},
        {"bars_count",  lua_core_bars_count},
        {"current_symbol", lua_core_current_symbol},
        {"current_tf",  lua_core_current_tf},
        {"ma",          lua_core_ma},
        {"kdj",         lua_core_kdj},
        {"highest",     lua_core_highest},
        {"lowest",      lua_core_lowest},
        {"alert",       lua_core_alert},
        {"shape_add",   lua_core_shape_add},
        {"shape_remove", lua_core_shape_remove},
        {"get_shape_price", lua_core_get_shape_price},
        {"shape_add_fixed", lua_core_shape_add_fixed},
        {"child_add", lua_core_child_add},
        {"child_remove", lua_core_child_remove},
        {"child_select", lua_core_child_select},
        {nullptr, nullptr}
    };

    // Create 'core' table with upvalue pointing to this engine
    lua_createtable(L, 0, 12); // core table
    for (const luaL_Reg *f = core_funcs; f->name; ++f) {
        lua_pushlightuserdata(L, this);         // upvalue 1: engine pointer
        lua_pushcclosure(L, f->func, 1);        // closure with 1 upvalue
        lua_setfield(L, -2, f->name);           // core[name] = func
    }
    lua_setglobal(L, "core");                   // _G.core = table
}

bool LuaScriptEngine::loadScript(const QString &scriptName, const QString &params,
                                  const ScriptBinding &binding)
{
    lua_State *L = (lua_State*)m_state;
    if (!L) return false;

    // 脚本路径: data/scripts/{scriptName}.lua（使用统一的路径解析）
    QString scriptDir = AppPaths::resolveDataDir("data/scripts");
    QString scriptPath = scriptDir + "/" + scriptName + ".lua";

    if (!QFile::exists(scriptPath)) {
        m_lastError = QString("Script not found: %1").arg(scriptPath);
        emit scriptError(scriptName, m_lastError);
        return false;
    }

    // 如果脚本已加载且绑定信息相同，直接返回成功
    if (m_loadedScripts.contains(scriptName) && m_loadedScripts[scriptName].symbol == binding.symbol
        && m_loadedScripts[scriptName].timeframe == binding.timeframe) {
        return true;
    }

    // 加载 Lua 文件
    int ret = luaL_loadfile(L, scriptPath.toUtf8().constData());
    if (ret != LUA_OK) {
        m_lastError = QString::fromUtf8(lua_tolstring(L, -1, nullptr));
        lua_pop(L, 1);
        emit scriptError(scriptName, m_lastError);
        return false;
    }

    // 执行 Lua 文件（定义函数）
    ret = lua_pcall(L, 0, 0, 0);
    if (ret != LUA_OK) {
        m_lastError = QString::fromUtf8(lua_tolstring(L, -1, nullptr));
        lua_pop(L, 1);
        emit scriptError(scriptName, m_lastError);
        return false;
    }

    // 调用 on_init(params) 如果存在
    lua_getglobal(L, "on_init");
    if (lua_type(L, -1) == LUA_TFUNCTION) {
        lua_pushstring(L, params.toUtf8().constData());
        ret = lua_pcall(L, 1, 1, 0);
        if (ret != LUA_OK) {
            m_lastError = QString("on_init: %1").arg(QString::fromUtf8(lua_tolstring(L, -1, nullptr)));
            lua_pop(L, 1);
            emit scriptError(scriptName, m_lastError);
            return false;
        }
        bool ok = lua_toboolean(L, -1);
        lua_pop(L, 1);
        if (!ok) {
            m_lastError = QString("Script %1 on_init returned false").arg(scriptName);
            emit scriptError(scriptName, m_lastError);
            return false;
        }
    } else {
        lua_pop(L, 1); // pop nil/non-function
    }

    m_loadedScripts[scriptName] = binding;
    qDebug() << "[Lua] Loaded script:" << scriptName
             << "symbol:" << binding.symbol << "tf:" << binding.timeframe;
    return true;
}

void LuaScriptEngine::unloadScript(const QString &scriptName)
{
    m_loadedScripts.remove(scriptName);
    // Lua 中没法真正卸载已加载的函数，只能移除全局函数
    lua_State *L = (lua_State*)m_state;
    if (L) {
        lua_pushnil(L);
        lua_setglobal(L, scriptName.toUtf8().constData());
    }
}

void LuaScriptEngine::unloadByBinding(const QString &symbol, int timeframe)
{
    QMutexLocker lock(&m_mutex);
    QList<QString> toRemove;
    for (auto it = m_loadedScripts.begin(); it != m_loadedScripts.end(); ++it) {
        const ScriptBinding &b = it.value();
        if ((b.symbol.isEmpty() || b.symbol == symbol) && (b.timeframe == 0 || b.timeframe == timeframe)) {
            toRemove.append(it.key());
        }
    }
    for (const auto &name : toRemove) {
        m_loadedScripts.remove(name);
        lua_State *L = (lua_State*)m_state;
        if (L) {
            lua_pushnil(L);
            lua_setglobal(L, name.toUtf8().constData());
        }
    }
}


// ================================================================
// 跨线程安全调度：如果从非 GUI 线程调用，通过信号投递到主线程
// ================================================================
void LuaScriptEngine::requestBarEvent(const QString &symbol, int timeframe,
                                       const Candle &candle, bool isNewBar)
{
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        // 已经在主线程，直接调用
        onBarEvent(symbol, timeframe, candle, isNewBar);
    } else {
        // 非主线程，通过信号投递到主线程
        emit barEventRequested(symbol, timeframe, candle, isNewBar);
    }
}

void LuaScriptEngine::onBarEvent(const QString &symbol, int timeframe,
                                  const Candle &candle, bool isNewBar)
{
    QMutexLocker lock(&m_mutex);

    lua_State *L = (lua_State*)m_state;
    if (!L) return;

    // 遍历所有已加载的脚本
    for (auto it = m_loadedScripts.begin(); it != m_loadedScripts.end(); ++it) {
        const QString &scriptName = it.key();
        const ScriptBinding &binding = it.value();

        // 根据绑定信息过滤：如果脚本绑定了特定品种/周期，只有匹配时才执行
        if (!binding.symbol.isEmpty() && binding.symbol != symbol) {
            continue; // 品种不匹配，跳过
        }
        // 基础周期（5m）的推送会影响所有高周期，因此只要 binding 的周期 >= 基础周期就触发
        // 如果绑定周期为 0（无特定周期），也触发
        if (binding.timeframe > 0 && timeframe > 0 && binding.timeframe < timeframe) {
            continue; // 绑定周期比基础周期还小，不可能发生，跳过
        }

        // 检查对应的回调函数
        const char *funcName = isNewBar ? "on_bar_new" : "on_bar_update";
        lua_getglobal(L, funcName);
        if (lua_type(L, -1) != LUA_TFUNCTION) {
            lua_pop(L, 1);
            continue;
        }

        // 构建 candle table
        lua_createtable(L, 0, 6);
        lua_pushstring(L, "symbol"); lua_pushstring(L, symbol.toUtf8().constData()); lua_settable(L, -3);
        lua_pushstring(L, "timeframe"); lua_pushinteger(L, timeframe); lua_settable(L, -3);
        lua_pushstring(L, "time"); lua_pushstring(L, candle.date.toString(Qt::ISODate).toUtf8().constData()); lua_settable(L, -3);
        lua_pushstring(L, "open"); lua_pushnumber(L, candle.open); lua_settable(L, -3);
        lua_pushstring(L, "high"); lua_pushnumber(L, candle.high); lua_settable(L, -3);
        lua_pushstring(L, "low"); lua_pushnumber(L, candle.low); lua_settable(L, -3);
        lua_pushstring(L, "close"); lua_pushnumber(L, candle.close); lua_settable(L, -3);
        lua_pushstring(L, "volume"); lua_pushnumber(L, candle.volume); lua_settable(L, -3);
        // index: 0=最新（新 K 线就是最新一根）
        lua_pushstring(L, "index"); lua_pushinteger(L, 0); lua_settable(L, -3);

        // 第二个参数：脚本名称（用于 core.get_shape_price）
        lua_pushstring(L, scriptName.toUtf8().constData());

        // 调用（2个参数：candle table, scriptName string）
        int ret = lua_pcall(L, 2, 0, 0);
        if (ret != LUA_OK) {
            QString err = QString::fromUtf8(lua_tolstring(L, -1, nullptr));
            lua_pop(L, 1);
            emit scriptError(scriptName, err);
        }
    }
}

KLineWidget *LuaScriptEngine::klineWidget() const
{
    return m_klineWidget;
}

void LuaScriptEngine::reloadShapesForSymbol(const QString &symbol, int timeframe)
{
    QMutexLocker lock(&m_mutex);
    QString key = symbol + "|" + QString::number(timeframe);
    m_shapesDiskCache.remove(key);
    // Clear script index and rebuild below
    m_scriptShapesIndex.clear();
    QString shapesDir = AppPaths::resolveDataDir("data/shapes");
    QString fname = symbol + "_" + QString::number(timeframe) + ".json";
    QFile f(QDir(shapesDir).filePath(fname));
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;
    QJsonArray arr = doc.object()["shapes"].toArray();
    QVector<QSharedPointer<KLineWidget::Shape>> shapes;
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        auto sp = QSharedPointer<KLineWidget::Shape>::create();
        sp->id = obj["id"].toInt();
        sp->type = static_cast<KLineWidget::ShapeType>(obj["type"].toInt());
        sp->attachment = static_cast<KLineWidget::ShapeAttachment>(obj["attachment"].toInt(0));
        sp->name = obj["name"].toString();
        sp->text = obj["text"].toString();
        sp->color = QColor(obj["color"].toString("#FFFFFF"));
        sp->candleIdx1 = obj["candleIdx1"].toInt();
        sp->price1 = obj["price1"].toDouble();
        sp->candleIdx2 = obj["candleIdx2"].toInt();
        sp->price2 = obj["price2"].toDouble();
        sp->normX = obj["normX"].toDouble(0.5);
        sp->normY = obj["normY"].toDouble(0.5);
        sp->ownerShapeId = obj["ownerShapeId"].toInt(0);
        sp->scriptName = obj["scriptName"].toString();
        sp->scriptParams = obj["scriptParams"].toString();
        shapes.append(sp);
        QString sn = sp->scriptName;
        if (sn.endsWith(".lua", Qt::CaseInsensitive)) sn = sn.left(sn.length() - 4);
        if (!sn.isEmpty()) m_scriptShapesIndex[sn].append(sp);
    }
    m_shapesDiskCache[key] = shapes;
}

QList<QPair<QString,int>> LuaScriptEngine::allLoadedShapeSymbols() const
{
    QMutexLocker lock(&m_mutex);
    QList<QPair<QString,int>> result;
    for (auto it = m_shapesDiskCache.begin(); it != m_shapesDiskCache.end(); ++it) {
        QStringList parts = it.key().split('|');
        if (parts.size() == 2) {
            bool ok = false;
            int tf = parts[1].toInt(&ok);
            if (ok) result.append(qMakePair(parts[0], tf));
        }
    }
    return result;
}

int LuaScriptEngine::addChildShape(int parentShapeId, const QString &type,
                                     double normX, double normY, const QString &text)
{
    KLineWidget *kw = klineWidget();
    if (!kw) return -1;
    KLineWidget::Shape s;
    s.attachment = KLineWidget::Attach_Fixed;
    s.ownerShapeId = parentShapeId;
    s.normX = normX;
    s.normY = normY;
    s.text = text;
    s.color = QColor(255, 200, 100);
    if (type.compare("Circle", Qt::CaseInsensitive) == 0) s.type = KLineWidget::Shape_FixedDot;
    else if (type.compare("Triangle", Qt::CaseInsensitive) == 0) s.type = KLineWidget::Shape_FixedTriangle;
    else if (type.compare("Dot", Qt::CaseInsensitive) == 0) s.type = KLineWidget::Shape_FixedDot;
    else if (type.compare("Note", Qt::CaseInsensitive) == 0) s.type = KLineWidget::Shape_FixedDot;
    else if (type.compare("Label", Qt::CaseInsensitive) == 0) s.type = KLineWidget::Shape_FixedDot;
    else s.type = KLineWidget::Shape_FixedDot;
    return kw->addShape(s);
}

bool LuaScriptEngine::removeChildShape(int childShapeId)
{
    KLineWidget *kw = klineWidget();
    if (!kw) return false;
    QVector<KLineWidget::Shape> shapes = kw->shapes();
    int before = shapes.size();
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
        [childShapeId](const KLineWidget::Shape &s) { return s.id == childShapeId; }),
        shapes.end());
    if (shapes.size() == before) return false;
    kw->setShapes(shapes);
    return true;
}

QVector<int> LuaScriptEngine::childShapeIds(int parentShapeId) const
{
    KLineWidget *kw = klineWidget();
    if (!kw) return {};
    QVector<int> ids;
    for (const auto &s : kw->shapes()) {
        if (s.ownerShapeId == parentShapeId)
            ids.append(s.id);
    }
    return ids;
}

// ── 脚本说明（从文件头部 -- 注释解析） ──
QString LuaScriptEngine::getScriptDescription(const QString &scriptName) const
{
    // 脚本路径: data/scripts/{scriptName}.lua
    QString scriptDir = AppPaths::resolveDataDir("data/scripts");
    QString path = scriptDir + "/" + scriptName + ".lua";
    if (!scriptName.endsWith(".lua", Qt::CaseInsensitive))
        path = scriptDir + "/" + scriptName + ".lua";

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QStringList descLines;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        // 只解析开头的 -- 注释（前面的行）
        if (line.trimmed().startsWith("--")) {
            // 去掉 "-- " 或 "--" 前缀
            QString text = line.trimmed();
            if (text.startsWith("-- "))
                text = text.mid(3);
            else if (text.startsWith("--"))
                text = text.mid(2);
            descLines.append(text);
        } else if (!line.trimmed().isEmpty()) {
            // 遇到非空非注释行就停止（函数定义等）
            break;
        }
    }
    f.close();
    return descLines.join("\n");
}

QHash<QString, QString> LuaScriptEngine::getAllScriptDescriptions() const
{
    QHash<QString, QString> result;
    QString scriptDir = AppPaths::resolveDataDir("data/scripts");
    QDir dir(scriptDir);
    auto files = dir.entryList({"*.lua"}, QDir::Files);
    for (const auto &f : files) {
        QString name = f;
        if (name.endsWith(".lua", Qt::CaseInsensitive))
            name = name.left(name.length() - 4);
        result[name] = getScriptDescription(name);
    }
    return result;
}


void LuaScriptEngine::loadShapesFromDisk()
{
    QMutexLocker lock(&m_mutex);
    m_shapesDiskCache.clear();
    m_scriptShapesIndex.clear();

    QString shapesDir = AppPaths::resolveDataDir("data/shapes");
    QDir dir(shapesDir);
    if (!dir.exists()) return;

    QStringList jsonFiles = dir.entryList(QStringList() << "*.json", QDir::Files);
    QList<QPair<QString,int>> loadedSymbols;

    for (const QString &fname : jsonFiles) {
        QString base = fname;
        base.chop(5);
        int underscore = base.lastIndexOf('_');
        if (underscore < 0) continue;
        QString symbol = base.left(underscore);
        bool ok = false;
        int tf = base.mid(underscore + 1).toInt(&ok);
        if (!ok) continue;

        QFile f(dir.filePath(fname));
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) continue;

        QJsonObject root = doc.object();
        QJsonArray arr = root["shapes"].toArray();
        QString key = symbol + "|" + QString::number(tf);
        QVector<QSharedPointer<KLineWidget::Shape>> shapes;

        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            auto sp = QSharedPointer<KLineWidget::Shape>::create();
            sp->id = obj["id"].toInt();
            sp->type = static_cast<KLineWidget::ShapeType>(obj["type"].toInt());
            sp->attachment = static_cast<KLineWidget::ShapeAttachment>(obj["attachment"].toInt(0));
            sp->name = obj["name"].toString();
            sp->text = obj["text"].toString();
            sp->color = QColor(obj["color"].toString("#FFFFFF"));
            sp->candleIdx1 = obj["candleIdx1"].toInt();
            sp->price1 = obj["price1"].toDouble();
            sp->candleIdx2 = obj["candleIdx2"].toInt();
            sp->price2 = obj["price2"].toDouble();
            sp->normX = obj["normX"].toDouble(0.5);
            sp->normY = obj["normY"].toDouble(0.5);
            sp->ownerShapeId = obj["ownerShapeId"].toInt(0);
            sp->scriptName = obj["scriptName"].toString();
            sp->scriptParams = obj["scriptParams"].toString();
            shapes.append(sp);

            QString sn = sp->scriptName;
            if (sn.endsWith(".lua", Qt::CaseInsensitive))
                sn = sn.left(sn.length() - 4);
            if (!sn.isEmpty())
                m_scriptShapesIndex[sn].append(sp);
        }

        m_shapesDiskCache[key] = shapes;
        loadedSymbols.append(qMakePair(symbol, tf));
    }

    if (!loadedSymbols.isEmpty())
        emit scriptsInitialized(loadedSymbols);
}
