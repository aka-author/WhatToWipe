#pragma once

#include "app/Session.h"

namespace wtw::app {

struct ChromeAvailability {
    bool open = false;
    bool update = false;
    bool stop = false;
    bool up = false;
    bool dive = false;
    bool explore = false;
    bool settings = false;
};

ChromeAvailability computeChromeAvailability(const Session& session);

bool canNavigateUp(const Session& session);
bool canNavigateDive(const Session& session);

}  // namespace wtw::app
