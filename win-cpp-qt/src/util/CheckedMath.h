#pragma once

#include <QtGlobal>
#include <limits>
#include <stdexcept>

namespace wtw::util {

inline bool tryAdd(quint64 a, quint64 b, quint64* out) {
    if (a > std::numeric_limits<quint64>::max() - b) {
        return false;
    }
    *out = a + b;
    return true;
}

inline quint64 checkedAdd(quint64 a, quint64 b) {
    quint64 out = 0;
    if (!tryAdd(a, b, &out)) {
        throw std::overflow_error("quint64 addition overflow");
    }
    return out;
}

}  // namespace wtw::util
