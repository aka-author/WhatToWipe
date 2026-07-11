#pragma once

#include <QtGlobal>
#include <limits>
#include <stdexcept>

namespace wtw::util {

inline quint64 checkedAdd(quint64 a, quint64 b) {
    if (a > std::numeric_limits<quint64>::max() - b) {
        throw std::overflow_error("quint64 addition overflow");
    }
    return a + b;
}

}  // namespace wtw::util
