#pragma once

#include <QString>

class AppPaths {
public:
    // Set an explicit root directory for data (optional)
    static void setDataRoot(const QString &root);
    // Resolve a configured dataDir (may be relative) to an absolute existing path or best-effort path
    static QString resolveDataDir(const QString &dataDir);
    // Get current data root (may be empty)
    static QString dataRoot();
};