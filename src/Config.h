#pragma once

#include <spdlog/common.h>

namespace Config
{
    void Load();
    spdlog::level::level_enum GetLogLevel();
    bool GetAllowCustomDestination();
    bool GetAllowDirectQuestMarkers();
    bool GetAllowDoorQuestMarkers();
    bool GetAllowUndiscoveredLocations();
}
