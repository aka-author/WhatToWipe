#pragma once

#include <QString>

namespace wtw::ui {

inline QString menuEllipsis() { return QStringLiteral("\u2026"); }

inline QString openFolderMenuLabel() { return QStringLiteral("&Open a Folder") + menuEllipsis(); }
inline QString exploreMenuLabel() { return QStringLiteral("&Explore") + menuEllipsis(); }
inline QString settingsMenuLabel() { return QStringLiteral("&Settings") + menuEllipsis(); }
inline QString aboutMenuLabel() { return QStringLiteral("&About") + menuEllipsis(); }

inline QString exploreContextLabel() { return QStringLiteral("Explore") + menuEllipsis(); }
inline QString openContextLabel() { return QStringLiteral("Open") + menuEllipsis(); }

}  // namespace wtw::ui
