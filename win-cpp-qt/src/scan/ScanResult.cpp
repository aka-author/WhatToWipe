#include "scan/ScanResult.h"

namespace wtw::scan {

ScanResult::ScanResult(ScanOutcome outcome, ScanIdentity identity, std::optional<model::FolderDescriptor> tree,
                       ScanDiagnostics diagnostics)
    : outcome_(outcome), identity_(identity), tree_(std::move(tree)), diagnostics_(diagnostics) {}

ScanResult ScanResult::success(ScanIdentity identity, model::FolderDescriptor tree, ScanDiagnostics diagnostics) {
    return ScanResult(ScanOutcome::Success, identity, std::move(tree), diagnostics);
}

ScanResult ScanResult::cancelled(ScanIdentity identity, ScanDiagnostics diagnostics) {
    return ScanResult(ScanOutcome::Cancelled, identity, std::nullopt, diagnostics);
}

ScanResult ScanResult::rootUnavailable(ScanIdentity identity, ScanDiagnostics diagnostics) {
    return ScanResult(ScanOutcome::RootUnavailable, identity, std::nullopt, diagnostics);
}

ScanResult ScanResult::technicalFailure(ScanIdentity identity, ScanDiagnostics diagnostics) {
    return ScanResult(ScanOutcome::TechnicalFailure, identity, std::nullopt, diagnostics);
}

}  // namespace wtw::scan
