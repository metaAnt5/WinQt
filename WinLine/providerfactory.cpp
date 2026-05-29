#include "providerfactory.h"
#include "dataprovider.h"
#include "fhdataprovider.h"

DataProvider* ProviderFactory::createProvider(const QString &apiType, const QString &dataDir, const QString &filenamePattern, const QString &readerType, QObject *parent)
{
    // If a specific readerType is provided, prefer it for creating specialized providers
    if (!readerType.isEmpty()) {
        if (readerType.compare(QLatin1String("fh"), Qt::CaseInsensitive) == 0) {
            return new FhDataProvider(dataDir, parent);
        }
        // future readerType handlers can go here
    }

    // No explicit readerType -> use apiType rules
    if (apiType.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0) {
        // choose filename pattern: priority: filenamePattern arg, default
        QString pattern = filenamePattern;
        if (pattern.isEmpty()) {
            pattern = QStringLiteral("%{symbol}_%{tf}.csv");
        }
        return new FileDataProvider(dataDir, pattern, parent);
    }

    // default: file provider with pattern
    QString pattern = filenamePattern.isEmpty() ? QStringLiteral("%{symbol}_%{tf}.csv") : filenamePattern;
    return new FileDataProvider(dataDir, pattern, parent);
}
