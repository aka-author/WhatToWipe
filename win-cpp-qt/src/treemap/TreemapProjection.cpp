#include "treemap/TreemapProjection.h"

#include <QFileInfo>
#include <algorithm>

namespace wtw::treemap {

namespace {

struct Candidate {
    QString name;
    QString path;
    qint64 size = 0;
    bool isFolder = false;
    bool isNode = false;
    bool isEmpty = false;
    bool isExecFile = false;
    double share = 0.0;
    bool clump = false;
    model::PackingType packing = model::PackingType::Native;
    bool clumpNonNative = false;
};

QStringList exeTypeList(const config::TreemapSettings& cfg) {
    QString raw = cfg.winExeFiles.trimmed();
    if (raw.isEmpty()) {
        return {QStringLiteral(".com"), QStringLiteral(".exe"), QStringLiteral(".dll"),
                QStringLiteral(".scr"), QStringLiteral(".msi")};
    }
    QStringList out;
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString tok : parts) {
        tok = tok.trimmed().toLower();
        if (tok.isEmpty()) {
            continue;
        }
        if (!tok.startsWith(QLatin1Char('.'))) {
            tok = QLatin1Char('.') + tok;
        }
        out.push_back(tok);
    }
    if (out.isEmpty()) {
        return {QStringLiteral(".com"), QStringLiteral(".exe"), QStringLiteral(".dll"),
                QStringLiteral(".scr"), QStringLiteral(".msi")};
    }
    return out;
}

bool isExecutablePath(const QString& path, const config::TreemapSettings& cfg) {
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty()) {
        return false;
    }
    ext = QLatin1Char('.') + ext;
    const QStringList exts = exeTypeList(cfg);
    for (const QString& e : exts) {
        if (ext == e) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<model::TreemapItem> buildTreemapItems(const model::FolderDescriptor* cur, quint64 driveTotal,
                                                  const config::TreemapSettings& cfg) {
    if (!cur) {
        return {};
    }

    int maxTiles = cfg.maxTiles;
    if (maxTiles < 1) {
        maxTiles = 25;
    }
    double clumpThreshold = cfg.clumpThreshold;
    if (clumpThreshold <= 0) {
        clumpThreshold = 0.01;
    }

    std::vector<Candidate> cands;
    for (const auto& k : cur->children) {
        cands.push_back(Candidate{
            k.name,
            k.fullPath,
            k.size,
            true,
            !k.children.empty(),
            k.children.empty() && k.files.empty(),
            false,
            k.volumeShare,
            false,
            model::PackingType::Native,
            false,
        });
    }
    for (const auto& f : cur->files) {
        double sh = 0.0;
        if (driveTotal > 0) {
            sh = static_cast<double>(f.size) / static_cast<double>(driveTotal);
        }
        cands.push_back(Candidate{
            f.name,
            f.fullPath,
            f.size,
            false,
            false,
            false,
            isExecutablePath(f.fullPath, cfg),
            sh,
            false,
            f.packing,
            false,
        });
    }
    if (cands.empty()) {
        return {};
    }

    std::vector<Candidate> forcedClump;
    std::vector<Candidate> kept;
    kept.reserve(cands.size());
    if (cur->size > 0 && clumpThreshold > 0) {
        const double limit = static_cast<double>(cur->size) * clumpThreshold;
        for (const Candidate& c : cands) {
            if (static_cast<double>(c.size) < limit) {
                forcedClump.push_back(c);
            } else {
                kept.push_back(c);
            }
        }
    } else {
        kept = std::move(cands);
    }

    std::stable_sort(kept.begin(), kept.end(), [](const Candidate& a, const Candidate& b) {
        if (a.size != b.size) {
            return a.size > b.size;
        }
        return a.path < b.path;
    });

    std::vector<Candidate> picked;
    const bool needClump = !forcedClump.empty() || static_cast<int>(kept.size()) > maxTiles;
    if (!needClump) {
        picked = std::move(kept);
    } else {
        int head = maxTiles - 1;
        if (head < 0) {
            head = 0;
        }
        if (head > static_cast<int>(kept.size())) {
            head = static_cast<int>(kept.size());
        }
        picked.insert(picked.end(), kept.begin(), kept.begin() + head);

        qint64 sum = 0;
        bool anyNonNative = false;
        std::vector<Candidate> clumpMembers = forcedClump;
        clumpMembers.insert(clumpMembers.end(), kept.begin() + head, kept.end());
        for (const Candidate& c : clumpMembers) {
            sum += c.size;
            if (!c.isFolder && c.packing != model::PackingType::Native) {
                anyNonNative = true;
            }
        }
        double clShare = 0.0;
        if (driveTotal > 0) {
            clShare = static_cast<double>(sum) / static_cast<double>(driveTotal);
        }
        picked.push_back(Candidate{
            QStringLiteral("Other"),
            QString(),
            sum,
            false,
            false,
            false,
            false,
            clShare,
            true,
            model::PackingType::Native,
            anyNonNative,
        });
    }

    std::vector<model::TreemapItem> out;
    out.reserve(picked.size());
    for (const Candidate& c : picked) {
        model::TreemapItem ti;
        ti.name = c.name;
        ti.path = c.path;
        ti.size = c.size;
        ti.driveShare = c.share;
        ti.isNode = c.isNode;
        ti.isEmpty = c.isEmpty;
        ti.isExecFile = c.isExecFile;

        if (c.clump) {
            ti.kind = model::TreemapItemKind::Clump;
            if (c.clumpNonNative) {
                ti.bg = cfg.packedClumpBg;
                ti.text = cfg.packedClumpText;
            } else {
                ti.bg = cfg.nativeClumpBg;
                ti.text = cfg.nativeClumpText;
            }
        } else if (c.isFolder) {
            ti.kind = model::TreemapItemKind::Folder;
            ti.bg = cfg.nativeFolderBg;
            ti.text = cfg.nativeFolderText;
        } else {
            ti.kind = model::TreemapItemKind::File;
            switch (c.packing) {
            case model::PackingType::PackedFile:
                ti.bg = cfg.packedFileBg;
                ti.text = cfg.packedFileText;
                break;
            case model::PackingType::PackedFolder:
                ti.bg = cfg.packedFolderBg;
                ti.text = cfg.packedFolderText;
                break;
            case model::PackingType::PackedClump:
                ti.bg = cfg.packedClumpBg;
                ti.text = cfg.packedClumpText;
                break;
            default:
                ti.bg = cfg.nativeFileBg;
                ti.text = cfg.nativeFileText;
                break;
            }
        }
        out.push_back(ti);
    }
    return out;
}

}  // namespace wtw::treemap
