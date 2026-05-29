#include "simreader.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>

// ============================================================
// 辅助函数
// ============================================================

static QString stripQuotes(const QString &s) {
    QString t = s.trimmed();
    if (t.startsWith('"') && t.endsWith('"') && t.size() >= 2) t = t.mid(1, t.size()-2);
    return t.trimmed();
}

static QDateTime parseDateTime(const QString &ds, const QString &ts) {
    QString combined = stripQuotes(ds);
    if (!ts.isEmpty()) combined += " " + stripQuotes(ts);

    QStringList formats = {
        "yyyy.MM.dd H:mm",
        "yyyy.MM.dd HH:mm",
        "yyyy.MM.dd H:mm:ss",
        "yyyy.MM.dd HH:mm:ss",
        "yyyy-MM-dd H:mm",
        "yyyy-MM-dd HH:mm",
        "yyyy-MM-dd H:mm:ss",
        "yyyy-MM-dd HH:mm:ss",
        "yyyy/MM/dd H:mm",
        "yyyy/MM/dd HH:mm",
        "yyyy/MM/dd H:mm:ss",
        "yyyy/MM/dd HH:mm:ss",
        "yyyy.MM.dd",          // date only
        "yyyy-MM-dd",
        "yyyy/MM/dd"
    };
    for (const QString &f : formats) {
        QDateTime dt = QDateTime::fromString(combined, f);
        if (dt.isValid()) return dt;
    }
    // try ISO
    return QDateTime::fromString(combined.trimmed(), Qt::ISODate);
}

// ============================================================
// GenericCsvReader
// 标准格式: time,open,high,low,close,volume
// time 支持时间戳(秒) 或 ISO 格式
// ============================================================
bool GenericCsvReader::readFile(const QString &path, QVector<Candle> &out)
{
    out.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);

    // detect header line
    bool isFirst = true;
    bool hasHeader = false;

    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(QRegularExpression("[,\t]+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;

        // check if first line is header (non-numeric first token)
        if (isFirst) {
            isFirst = false;
            bool isNum; parts[0].toDouble(&isNum);
            if (!isNum && parts[0].contains(QRegularExpression("[a-zA-Z/]"))) {
                // skip header line
                // try to detect column indices
                hasHeader = true;
                continue;
            }
        }

        for (int i = 0; i < parts.size(); ++i) parts[i] = stripQuotes(parts[i]);

        Candle c;
        bool ok;

        if (parts.size() >= 6) {
            // try: date,time,open,high,low,close,volume
            if (parts.size() >= 7) {
                c.date = parseDateTime(parts[0], parts[1]);
                c.open = parts[2].toDouble(&ok); if (!ok) continue;
                c.high = parts[3].toDouble(&ok); if (!ok) continue;
                c.low  = parts[4].toDouble(&ok); if (!ok) continue;
                c.close = parts[5].toDouble(&ok); if (!ok) continue;
                c.volume = parts[6].toDouble(&ok);
            } else {
                // 6 columns: time,open,high,low,close,volume
                c.date = parseDateTime(parts[0], QString());
                c.open = parts[1].toDouble(&ok); if (!ok) continue;
                c.high = parts[2].toDouble(&ok); if (!ok) continue;
                c.low  = parts[3].toDouble(&ok); if (!ok) continue;
                c.close = parts[4].toDouble(&ok); if (!ok) continue;
                c.volume = parts[5].toDouble(&ok);
            }
        } else if (parts.size() == 5) {
            // O,H,L,C,V (no timestamp) — assign sequential index as time
            double oh = parts[0].toDouble(&ok); if (!ok) continue;
            double hh = parts[1].toDouble(&ok); if (!ok) continue;
            double ll = parts[2].toDouble(&ok); if (!ok) continue;
            double cc = parts[3].toDouble(&ok); if (!ok) continue;
            double vv = parts[4].toDouble(&ok);
            // use current time + index as placeholder
            c.date = QDateTime::currentDateTime();
            c.open = oh; c.high = hh; c.low = ll; c.close = cc; c.volume = vv;
        } else {
            continue;
        }

        if (!c.date.isValid()) continue;
        out.append(c);
    }
    f.close();
    return !out.isEmpty();
}

// ============================================================
// FxcmCsvReader - 福汇 FXCM 格式
// 格式: Date/Time,Open,High,Low,Close,Volume
// 时间: "yyyy.MM.dd HH:mm:ss"
// ============================================================
bool FxcmCsvReader::readFile(const QString &path, QVector<Candle> &out)
{
    out.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);

    bool isFirst = true;
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(',');
        if (parts.size() < 6) continue;
        if (isFirst) {
            isFirst = false;
            // skip header if first column contains non-numeric header text
            bool isNum; parts[0].toDouble(&isNum);
            if (!isNum) continue;
        }
        for (int i = 0; i < parts.size(); ++i) parts[i] = parts[i].trimmed();

        Candle c;
        bool ok;
        c.date = parseDateTime(parts[0], QString());
        c.open  = parts[1].toDouble(&ok); if (!ok) continue;
        c.high  = parts[2].toDouble(&ok); if (!ok) continue;
        c.low   = parts[3].toDouble(&ok); if (!ok) continue;
        c.close = parts[4].toDouble(&ok); if (!ok) continue;
        c.volume = (parts.size() > 5) ? parts[5].toDouble(&ok) : 0;
        if (!c.date.isValid()) continue;
        out.append(c);
    }
    f.close();
    return !out.isEmpty();
}

// ============================================================
// Mt4CsvReader - MT4 CSV 格式
// 典型: <DATE>,<TIME>,<OPEN>,<HIGH>,<LOW>,<CLOSE>,<TICKVOL>
// 日期: "YYYY.MM.DD"  时间: "HH:MM"
// ============================================================
bool Mt4CsvReader::readFile(const QString &path, QVector<Candle> &out)
{
    out.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);

    bool isFirst = true;
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(',');
        if (parts.size() < 6) continue;
        if (isFirst) {
            isFirst = false;
            bool isNum; parts[0].toDouble(&isNum);
            if (!isNum) continue;
        }
        for (int i = 0; i < parts.size(); ++i) parts[i] = parts[i].trimmed();

        Candle c;
        bool ok;
        // MT4: column 0 = DATE, col 1 = TIME
        c.date = parseDateTime(parts[0], parts.size() > 1 ? parts[1] : QString());
        int idxOpen = 2;
        // if there are exactly 7 columns, standard MT4 format
        if (parts.size() >= 7) {
            c.open  = parts[2].toDouble(&ok); if (!ok) continue;
            c.high  = parts[3].toDouble(&ok); if (!ok) continue;
            c.low   = parts[4].toDouble(&ok); if (!ok) continue;
            c.close = parts[5].toDouble(&ok); if (!ok) continue;
            c.volume = parts[6].toDouble(&ok);
        } else if (parts.size() == 6) {
            c.open  = parts[2].toDouble(&ok); if (!ok) continue;
            c.high  = parts[3].toDouble(&ok); if (!ok) continue;
            c.low   = parts[4].toDouble(&ok); if (!ok) continue;
            c.close = parts[5].toDouble(&ok); if (!ok) continue;
            c.volume = 0;
        } else {
            continue;
        }
        if (!c.date.isValid()) continue;
        out.append(c);
    }
    f.close();
    return !out.isEmpty();
}

// ============================================================
// WhCsvReader - 文华财经 WH 格式
// 列: 日期,时间,开盘,最高,最低,收盘,成交量[,持仓量]
// 日期: "yyyy/MM/dd"
// ============================================================
bool WhCsvReader::readFile(const QString &path, QVector<Candle> &out)
{
    out.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);

    bool isFirst = true;
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(',');
        if (parts.size() < 6) continue;
        if (isFirst) {
            isFirst = false;
            bool isNum; parts[0].toDouble(&isNum);
            if (!isNum) continue;
        }
        for (int i = 0; i < parts.size(); ++i) parts[i] = parts[i].trimmed();

        Candle c;
        bool ok;
        // 文华: 日期,时间,开盘,最高,最低,收盘,成交量
        c.date = parseDateTime(parts[0], parts.size() > 1 ? parts[1] : QString());
        c.open  = parts[2].toDouble(&ok); if (!ok) continue;
        c.high  = parts[3].toDouble(&ok); if (!ok) continue;
        c.low   = parts[4].toDouble(&ok); if (!ok) continue;
        c.close = parts[5].toDouble(&ok); if (!ok) continue;
        c.volume = (parts.size() > 6) ? parts[6].toDouble(&ok) : 0;
        if (!c.date.isValid()) continue;
        out.append(c);
    }
    f.close();
    return !out.isEmpty();
}
