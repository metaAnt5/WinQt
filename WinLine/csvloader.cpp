#include "csvloader.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QDate>
#include <QTime>

static QString stripQuotes(const QString &s) {
    QString t = s.trimmed();
    if (t.startsWith('"') && t.endsWith('"') && t.size() >= 2) t = t.mid(1, t.size()-2);
    return t.trimmed();
}

static QDateTime parseDateTime(const QString &datePart, const QString &timePart) {
    QString ds = stripQuotes(datePart);
    QString ts = stripQuotes(timePart);
    QDateTime dt;
    // try common combined formats using 24-hour H/HH
    QStringList combinedFormats = {
        "yyyy.MM.dd H:mm",
        "yyyy.MM.dd HH:mm",
        "yyyy.MM.dd H:mm:ss",
        "yyyy-MM-dd H:mm",
        "yyyy/MM/dd H:mm",
        "yyyy.MM.dd HH:mm:ss",
        "yyyy-MM-dd HH:mm",
        "yyyy/MM/dd HH:mm",
        "yyyy.MM.dd" // allow date only as fallback
    };
    QString combined = ds + (ts.isEmpty() ? QString() : (" " + ts));
    for (const QString &f : combinedFormats) {
        dt = QDateTime::fromString(combined, f);
        if (dt.isValid()) return dt;
    }

    // If not combined, try parsing date-only (then apply timePart if present)
    QStringList dateOnly = {"yyyy.MM.dd", "yyyy-MM-dd", "yyyy/MM/dd"};
    QStringList timeOnly = {"H:mm", "HH:mm", "H:mm:ss"};
    for (const QString &df : dateOnly) {
        QDate d = QDate::fromString(ds, df);
        if (d.isValid()) {
            if (ts.isEmpty()) return QDateTime(d, QTime(0,0));
            // try combine with time formats
            for (const QString &tf : timeOnly) {
                dt = QDateTime::fromString(ds + " " + ts, df + " " + tf);
                if (dt.isValid()) return dt;
            }
        }
    }

    // try ISO fallback
    dt = QDateTime::fromString(combined.trimmed(), Qt::ISODate);
    if (dt.isValid()) return dt;

    // last resort: try parse ds alone as date
    dt = QDateTime::fromString(ds, "yyyy.MM.dd");
    return dt;
}

bool loadCsvFile(const QString &path, QVector<Candle> &outData, QString &symbol, int &baseMinutes)
{
    outData.clear(); symbol.clear(); baseMinutes = 1;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;
        // try to split by comma or tab
        QStringList parts = line.split(QRegularExpression("[,\t]+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        // strip whitespace/quotes
        for (int i = 0; i < parts.size(); ++i) parts[i] = stripQuotes(parts[i]);
        QDateTime dt;
        double open=0, high=0, low=0, close=0, vol=0;
        bool ok=false;

        bool parsed = false;
        if (parts.size() >= 7) {
            // usual case: first two columns are date and time
            QRegularExpression dateRe("^\\d{4}[.\\-/]\\d{1,2}[.\\-/]\\d{1,2}$");
            QRegularExpression timeRe("^\\d{1,2}:\\d{2}(:\\d{2})?$");
            if (dateRe.match(parts[0]).hasMatch() && timeRe.match(parts[1]).hasMatch()) {
                QDate d = QDate::fromString(parts[0], "yyyy.MM.dd");
                if (!d.isValid()) d = QDate::fromString(parts[0], "yyyy-MM-dd");
                if (!d.isValid()) d = QDate::fromString(parts[0], "yyyy/MM/dd");
                QTime t = QTime::fromString(parts[1], "HH:mm");
                if (!t.isValid()) t = QTime::fromString(parts[1], "H:mm");
                if (d.isValid() && t.isValid()) dt = QDateTime(d, t);
                else dt = parseDateTime(parts[0], parts[1]);
            } else {
                dt = parseDateTime(parts[0], parts[1]);
            }
            open = parts[2].toDouble(&ok); if (!ok) parsed = false; else {
                high = parts[3].toDouble(&ok); if (!ok) parsed = false; else {
                    low = parts[4].toDouble(&ok); if (!ok) parsed = false; else {
                        close = parts[5].toDouble(&ok); if (!ok) parsed = false; else {
                            vol = parts[6].toDouble(&ok); if (!ok) vol = 0;
                            parsed = true;
                        }
                    }
                }
            }
        }

        // fallback: detect first numeric column index (open)
        if (!parsed) {
            int n = parts.size();
            QVector<bool> isNum(n);
            for (int i = 0; i < n; ++i) {
                bool ok2; parts[i].toDouble(&ok2); isNum[i] = ok2;
            }
            int firstNum = -1;
            for (int i = 0; i < n; ++i) {
                if (isNum[i]) { firstNum = i; break; }
            }
            if (firstNum >= 0 && firstNum + 3 < n) {
                // assume [firstNum]..[firstNum+3] are open,high,low,close
                open = parts[firstNum].toDouble(&ok); if (!ok) { parsed = false; }
                else { high = parts[firstNum+1].toDouble(&ok); if (!ok) parsed = false; else { low = parts[firstNum+2].toDouble(&ok); if (!ok) parsed = false; else { close = parts[firstNum+3].toDouble(&ok); if (!ok) parsed = false; else { vol = (firstNum+4 < n) ? parts[firstNum+4].toDouble(&ok) : 0; if (!ok) vol = 0; parsed = true; } } } }
                // date/time are the tokens before firstNum; try to map
                if (parsed) {
                    if (firstNum >= 2) {
                        dt = parseDateTime(parts[0], parts[1]);
                    } else if (firstNum == 1) {
                        dt = parseDateTime(parts[0], QString());
                    } else {
                        dt = QDateTime();
                    }
                }
            }
        }

        if (!parsed) continue;
        if (!dt.isValid()) {
            // final fallback: try parse combined first token
            dt = parseDateTime(parts[0], (parts.size() > 1)? parts[1] : QString());
            if (!dt.isValid()) continue;
        }

        Candle c{ dt, open, high, low, close, vol };
        outData.append(c);
    }
    f.close();
    QFileInfo fi(path);
    QString name = fi.completeBaseName(); // e.g. USOil1440
    QRegularExpression re("([A-Za-z0-9_]+)(\\d+)");
    QRegularExpressionMatch m = re.match(name);
    if (m.hasMatch()) {
        symbol = m.captured(1);
        baseMinutes = m.captured(2).toInt();
    } else {
        symbol = name;
        baseMinutes = 1;
    }
    return !outData.isEmpty();
}

// ============================================================
// 追加 K 线数据到 CSV 文件
// 按时间排序，跳过重复（按时间戳去重）
// ============================================================
bool appendCsvFile(const QString &path, const QVector<Candle> &candles)
{
    if (candles.isEmpty()) return true;

    // 1. 读取现有数据
    QVector<Candle> existing;
    QString dummySym;
    int dummyTf = 1;
    loadCsvFile(path, existing, dummySym, dummyTf);

    // 2. 合并：将现有时间戳放入 set
    QSet<qint64> existingTimes;
    for (const auto &c : existing) {
        existingTimes.insert(c.date.toSecsSinceEpoch());
    }

    // 3. 只添加不存在的、且时间 >= 现有最后时间的数据
    qint64 lastTime = 0;
    if (!existing.isEmpty()) {
        lastTime = existing.last().date.toSecsSinceEpoch();
    }

    QVector<Candle> toAppend;
    toAppend.reserve(candles.size());
    for (const auto &c : candles) {
        qint64 t = c.date.toSecsSinceEpoch();
        if (t >= lastTime && !existingTimes.contains(t)) {
            toAppend.append(c);
        }
    }

    if (toAppend.isEmpty()) return true;

    // 4. 按时间排序
    std::sort(toAppend.begin(), toAppend.end(),
        [](const Candle &a, const Candle &b) {
            return a.date.toSecsSinceEpoch() < b.date.toSecsSinceEpoch();
        });

    // 5. 追加写入
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return false;
    QTextStream out(&f);
    for (const auto &c : toAppend) {
        out << c.date.toString("yyyy.MM.dd") << ","
            << c.date.toString("HH:mm") << ","
            << QString::number(c.open, 'f', 5) << ","
            << QString::number(c.high, 'f', 5) << ","
            << QString::number(c.low, 'f', 5) << ","
            << QString::number(c.close, 'f', 5) << ","
            << QString::number(c.volume, 'f', 0) << "\n";
    }
    f.close();
    return true;
}
