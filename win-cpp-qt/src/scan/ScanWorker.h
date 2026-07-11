#pragma once

#include "model/FolderDescriptor.h"
#include "platform/DirEntry.h"
#include "scan/ScanResult.h"
#include <QObject>
#include <QString>
#include <atomic>

namespace wtw::scan {

class ScanWorker : public QObject {
    Q_OBJECT

public:
    ScanWorker(QString rootPath, ScanIdentity identity, QObject* parent = nullptr);

    void requestCancel();

public slots:
    void run();

signals:
    void progress(scan::ScanIdentity identity, QString path);
    void finished(ScanResult result);

private:
    model::FolderDescriptor scanDir(const QString& path, ScanDiagnostics* diagnostics, bool* cancelled,
                                    bool isRoot, DirectoryReadStatus* rootReadStatus);
    static bool isDirectoryReparsePoint(const platform::DirEntry& entry);
    static QDateTime fileTimeFromEntry(const platform::DirEntry& entry, bool creation);
    void maybeEmitProgress(const QString& path);

    QString m_rootPath;
    ScanIdentity m_identity;
    std::atomic<bool> m_cancel{false};
    qint64 m_lastProgressMs = 0;
};

}  // namespace wtw::scan
