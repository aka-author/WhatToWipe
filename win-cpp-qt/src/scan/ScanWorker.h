#pragma once

#include "model/FolderDescriptor.h"
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <atomic>

namespace wtw::scan {

class ScanWorker : public QObject {
    Q_OBJECT

public:
    explicit ScanWorker(QString rootPath, QObject* parent = nullptr);

    void requestCancel();

public slots:
    void run();

signals:
    void progress(QString path);
    void finished(wtw::model::FolderDescriptor tree, int errorCount, bool cancelled, bool rootUnavailable,
                  bool technicalFailure);

private:
    model::FolderDescriptor scanDir(const QString& path, int* errorCount, bool* cancelled);
    QVector<QFileInfo> readDirBounded(const QString& path, bool* timedOut) const;
    bool isDirectoryReparsePoint(const QFileInfo& fi) const;
    void maybeEmitProgress(const QString& path);

    QString m_rootPath;
    std::atomic<bool> m_cancel{false};
    qint64 m_lastProgressMs = 0;
};

}  // namespace wtw::scan
