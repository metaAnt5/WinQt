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
#include <QMutexLocker>
#include <QThread>

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
    else if (strcmp(typeStr, "GestureUp") == 0) st = ShapeType::Shape_GestureUp;
    else if (strcmp(typeStr, "GestureDown") == 0) st = ShapeType::Shape_GestureDown;
    else if (strcmp(typeStr, "Text") == 0) st = ShapeType::Shape_Text;
    else if (strcmp(typeStr, "HLine") == 0) st = ShapeType::Shape_HLine;
    else if (strcmp(typeStr, "VLine") == 0) st = ShapeType::Shape_VLine;
    else if (strcmp(typeStr, "TradeBuy") == 0) st = ShapeType::Shape_TradeBuy;
    else if (strcmp(typeStr, "TradeSell") == 0) st = ShapeType::Shape_TradeSell;
    else if (strcmp(typeStr, "TradeShort") == 0) st = ShapeType::Shape_TradeShort;
    else if (strcmp(typeStr, "TradeCover") == 0) st = ShapeType::Shape_TradeCover;

    KLineWidget::Shape s;
    s.type = st;
    s.candleIdx1 = candleIdx1;
    s.price1 = price1;
    s.candleIdx2 = candleIdx2;
    s.price2 = price2;
    s.name = QString::fromUtf8(name);
    s.color = QColor(Qt::white);
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
