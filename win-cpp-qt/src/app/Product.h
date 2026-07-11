#pragma once

#include <QString>

namespace wtw::app {

inline QString productDisplayName() { return QStringLiteral("Erase & Rewrite"); }

// Use only in QAction/QMenu labels where a single & marks a mnemonic.
inline QString productDisplayNameMenuEscaped() { return QStringLiteral("Erase && Rewrite"); }

}  // namespace wtw::app
