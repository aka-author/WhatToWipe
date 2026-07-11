#pragma once

#include "model/FolderDescriptor.h"
#include <QString>

namespace wtw::scan {

model::PackingType classifyArchiveFile(const QString& path);

}  // namespace wtw::scan
