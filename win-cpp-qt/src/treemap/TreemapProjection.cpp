#include "treemap/TreemapProjection.h"

#include "scan/ScanTypes.h"
#include "util/CheckedMath.h"

#include <QFileInfo>
#include <algorithm>

namespace wtw::treemap {

namespace {

struct Candidate {
    QString name;
    QString path;
    quint64 size = 0;
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
    for (const auto& child : cur->children) {
        if (child.traversalState == scan::TraversalState::Unreadable) {
            continue;
        }
        if (child.measuredSize == 0) {
            continue;
        }
        cands.push_back(Candidate{
            child.name,
            child.fullPath,
            child.measuredSize,
            true,
            !child.children.empty(),
            child.children.empty() && child.files.empty(),
            false,
            child.volumeShare,
            false,
            model::PackingType::Native,
            false,
        });
    }
    for (const auto& file : cur->files) {
        if (file.size == 0) {
            continue;
        }
        double share = 0.0;
        if (driveTotal > 0) {
            share = static_cast<double>(file.size) / static_cast<double>(driveTotal);
        }
        cands.push_back(Candidate{
            file.name,
            file.fullPath,
            file.size,
            false,
            false,
            false,
            isExecutablePath(file.fullPath, cfg),
            share,
            false,
            file.packing,
            false,
        });
    }
    if (cands.empty()) {
        return {};
    }

    std::vector<Candidate> forcedClump;
    std::vector<Candidate> kept;
    kept.reserve(cands.size());
    if (cur->measuredSize > 0 && clumpThreshold > 0) {
        const double limit = static_cast<double>(cur->measuredSize) * clumpThreshold;
        for (const Candidate& candidate : cands) {
            if (static_cast<double>(candidate.size) < limit) {
                forcedClump.push_back(candidate);
            } else {
                kept.push_back(candidate);
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

        quint64 sum = 0;
        bool anyNonNative = false;
        std::vector<Candidate> clumpMembers = forcedClump;
        clumpMembers.insert(clumpMembers.end(), kept.begin() + head, kept.end());
        // Clump members are a subset of context children; sum cannot exceed measuredSize when aggregates are consistent.
        for (const Candidate& candidate : clumpMembers) {
            sum = util::checkedAdd(sum, candidate.size);
            if (!candidate.isFolder && candidate.packing != model::PackingType::Native) {
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
    for (const Candidate& candidate : picked) {
        model::TreemapItem item;
        item.name = candidate.name;
        item.path = candidate.path;
        item.size = candidate.size;
        item.driveShare = candidate.share;
        item.isNode = candidate.isNode;
        item.isEmpty = candidate.isEmpty;
        item.isExecFile = candidate.isExecFile;

        if (candidate.clump) {
            item.kind = model::TreemapItemKind::Clump;
            if (candidate.clumpNonNative) {
                item.bg = cfg.packedClumpBg;
                item.text = cfg.packedClumpText;
            } else {
                item.bg = cfg.nativeClumpBg;
                item.text = cfg.nativeClumpText;
            }
        } else if (candidate.isFolder) {
            item.kind = model::TreemapItemKind::Folder;
            item.bg = cfg.nativeFolderBg;
            item.text = cfg.nativeFolderText;
        } else {
            item.kind = model::TreemapItemKind::File;
            switch (candidate.packing) {
            case model::PackingType::PackedFile:
                item.bg = cfg.packedFileBg;
                item.text = cfg.packedFileText;
                break;
            case model::PackingType::PackedFolder:
                item.bg = cfg.packedFolderBg;
                item.text = cfg.packedFolderText;
                break;
            case model::PackingType::PackedClump:
                item.bg = cfg.packedClumpBg;
                item.text = cfg.packedClumpText;
                break;
            default:
                item.bg = cfg.nativeFileBg;
                item.text = cfg.nativeFileText;
                break;
            }
        }
        out.push_back(item);
    }
    return out;
}

}  // namespace wtw::treemap
