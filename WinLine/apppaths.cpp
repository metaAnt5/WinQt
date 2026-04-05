#include "apppaths.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

static QString s_dataRoot;

void AppPaths::setDataRoot(const QString &root)
{
    s_dataRoot = root;
}

QString AppPaths::dataRoot()
{
    return s_dataRoot;
}

QString tryCandidate(const QString &base, const QString &dataDir)
{
    QString cand = QDir(base).filePath(dataDir);
    qDebug() << "Trying candidate" << cand;
    if (QDir(cand).exists()) return QDir(cand).absolutePath();
    return QString();
}

QString AppPaths::resolveDataDir(const QString &dataDir)
{
    if (dataDir.isEmpty()) return QDir(QCoreApplication::applicationDirPath()).filePath("data");
    if (QDir(dataDir).isAbsolute() && QDir(dataDir).exists()) return QDir(dataDir).absolutePath();
    // explicit override
    if (!s_dataRoot.isEmpty()) {
        QString cand = QDir(s_dataRoot).filePath(dataDir);
        qDebug() << "Trying override" << cand;
        if (QDir(cand).exists()) return QDir(cand).absolutePath();
    }
    QString appDir = QCoreApplication::applicationDirPath();
    QString cand;
    cand = tryCandidate(appDir, dataDir); if (!cand.isEmpty()) return cand;
    QString cwd = QDir::currentPath();
    cand = tryCandidate(cwd, dataDir); if (!cand.isEmpty()) return cand;
    // ascend from appDir
    QDir d(appDir);
    while (d.cdUp()) {
        cand = tryCandidate(d.absolutePath(), dataDir);
        if (!cand.isEmpty()) return cand;
        if (d.isRoot()) break;
    }
    // env
    QByteArray env = qgetenv("WINLINE_DATADIR");
    if (!env.isEmpty()) {
        cand = QDir(QString::fromLocal8Bit(env)).filePath(dataDir);
        qDebug() << "Trying env" << cand;
        if (QDir(cand).exists()) return QDir(cand).absolutePath();
    }
    // fallback
    cand = QDir(appDir).filePath(dataDir);
    qDebug() << "Fallback" << cand;
    return QDir(cand).absolutePath();
}