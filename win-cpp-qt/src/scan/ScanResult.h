#pragma once

#include "model/FolderDescriptor.h"
#include "scan/ScanTypes.h"

#include <optional>

namespace wtw::scan {

struct ScanIdentity {
    quint64 scanId = 0;
    quint64 targetSessionId = 0;
    quint64 baseDescriptorVersion = 0;
};

class ScanResult {
public:
    static ScanResult success(ScanIdentity identity, model::FolderDescriptor tree, ScanDiagnostics diagnostics = {});
    static ScanResult cancelled(ScanIdentity identity, ScanDiagnostics diagnostics = {});
    static ScanResult rootUnavailable(ScanIdentity identity, ScanDiagnostics diagnostics = {});
    static ScanResult technicalFailure(ScanIdentity identity, ScanDiagnostics diagnostics = {});

    ScanOutcome outcome() const { return outcome_; }
    const ScanIdentity& identity() const { return identity_; }
    const ScanDiagnostics& diagnostics() const { return diagnostics_; }
    const std::optional<model::FolderDescriptor>& tree() const { return tree_; }

private:
    ScanResult(ScanOutcome outcome, ScanIdentity identity, std::optional<model::FolderDescriptor> tree,
               ScanDiagnostics diagnostics);

    ScanOutcome outcome_ = ScanOutcome::Invalid;
    ScanIdentity identity_;
    std::optional<model::FolderDescriptor> tree_;
    ScanDiagnostics diagnostics_;
};

}  // namespace wtw::scan

Q_DECLARE_METATYPE(wtw::scan::ScanIdentity)
Q_DECLARE_METATYPE(wtw::scan::ScanResult)
