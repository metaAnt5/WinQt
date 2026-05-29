#pragma once
#include <QString>
#include <QVector>
#include "klinewidget.h"

// ============================================================
// SimReader - 模拟数据读取器接口
// 不同软件导出的 CSV 格式不同，通过子类实现各自的解析逻辑
// ============================================================
class SimReader {
public:
    virtual ~SimReader() = default;
    virtual QString name() const = 0;
    virtual bool readFile(const QString &path, QVector<Candle> &out) = 0;
};

// ------------------------------------------------------------
// 通用 CSV 读取器：标准格式  time,open,high,low,close,volume
// time 支持时间戳(秒) 或  ISO 格式 "yyyy-MM-dd HH:mm:ss"
// ------------------------------------------------------------
class GenericCsvReader : public SimReader {
public:
    QString name() const override { return QStringLiteral("通用 CSVM 格式"); }
    bool readFile(const QString &path, QVector<Candle> &out) override;
};

// ------------------------------------------------------------
// 福汇 (FXCM) CSV 格式读取器
// 典型列: Date/Time,Open,High,Low,Close,Volume
// 时间格式: "yyyy.MM.dd HH:mm:ss"
// ------------------------------------------------------------
class FxcmCsvReader : public SimReader {
public:
    QString name() const override { return QStringLiteral("福汇 FXCM 格式"); }
    bool readFile(const QString &path, QVector<Candle> &out) override;
};

// ------------------------------------------------------------
// MT4 CSV 导出格式读取器
// 典型列: <DATE>,<TIME>,<OPEN>,<HIGH>,<LOW>,<CLOSE>,<TICKVOL>
// 日期格式: "YYYY.MM.DD"  时间: "HH:MM"
// ------------------------------------------------------------
class Mt4CsvReader : public SimReader {
public:
    QString name() const override { return QStringLiteral("MT4 CSV 格式"); }
    bool readFile(const QString &path, QVector<Candle> &out) override;
};

// ------------------------------------------------------------
// 文华财经 (WH) CSV 格式读取器
// 列: 日期,时间,开盘,最高,最低,收盘,成交量,持仓量
// 日期格式: "yyyy/MM/dd"
// ------------------------------------------------------------
class WhCsvReader : public SimReader {
public:
    QString name() const override { return QStringLiteral("文华财经 WH 格式"); }
    bool readFile(const QString &path, QVector<Candle> &out) override;
};
