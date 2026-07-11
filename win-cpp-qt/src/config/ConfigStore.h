#pragma once

#include "config/TreemapConfig.h"
#include <QString>

namespace wtw::config {

QString fsConfigPath();
QString legacyConfigPath();

bool loadOrInitTreemap(TreemapSettings& out);
bool saveTreemap(const TreemapSettings& settings);
bool importLegacyIntoFsFile();

void applyTreemapLines(TreemapSettings& settings, const QString& text);
QString serializeTreemap(const TreemapSettings& settings);

}  // namespace wtw::config
