#pragma once

#include "app/Session.h"
#include "scan/ScanResult.h"

namespace wtw::app {

inline scan::ScanIdentity liveScanIdentity(const Session& session) {
    return {session.scanId, session.sessionId, session.descriptorVersion};
}

inline bool acceptsScanDelivery(const scan::ScanIdentity& delivered, const Session& session) {
    const scan::ScanIdentity live = liveScanIdentity(session);
    return delivered.scanId == live.scanId && delivered.targetSessionId == live.targetSessionId &&
           delivered.baseDescriptorVersion == live.baseDescriptorVersion;
}

}  // namespace wtw::app
