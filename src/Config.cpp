#include "PCH.h"
#include "Config.h"

#include <SimpleIni.h>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string_view>

namespace Config
{
    namespace
    {
        // Read once at startup, following WMS's SimpleIni configuration pattern.
        auto logLevel = spdlog::level::info;
        bool allowCustomDestination = true;
        bool allowQuestMarkers = false;
        bool allowUndiscoveredLocations = false;

        bool EqualsIgnoreCase(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size()) { return false; }
            for (std::size_t index = 0; index < left.size(); ++index) {
                if (std::tolower(static_cast<unsigned char>(left[index])) !=
                    std::tolower(static_cast<unsigned char>(right[index]))) { return false; }
            }
            return true;
        }

        std::optional<spdlog::level::level_enum> ParseLogLevel(std::string_view text)
        {
            if (EqualsIgnoreCase(text, "trace"))    return spdlog::level::trace;
            if (EqualsIgnoreCase(text, "debug"))    return spdlog::level::debug;
            if (EqualsIgnoreCase(text, "info"))     return spdlog::level::info;
            if (EqualsIgnoreCase(text, "warn"))     return spdlog::level::warn;
            if (EqualsIgnoreCase(text, "err"))      return spdlog::level::err;
            if (EqualsIgnoreCase(text, "critical")) return spdlog::level::critical;
            if (EqualsIgnoreCase(text, "off"))      return spdlog::level::off;
            return std::nullopt;
        }

        void ApplyLogLevel()
        {
            // Apply Info even when the INI is missing. Flush follows the configured level, as in WMS.
            spdlog::set_level(logLevel);
            spdlog::default_logger()->flush_on(logLevel);
        }

        void ReadBool(const CSimpleIniA& ini, const char* key, bool& setting)
        {
            const char* configured = ini.GetValue("Behavior", key);
            if (!configured) { return; }
            if (EqualsIgnoreCase(configured, "true")) { setting = true; }
            else if (EqualsIgnoreCase(configured, "false")) { setting = false; }
            else { SKSE::log::warn("Invalid {} \"{}\"; expected true or false. Keeping default.", key, configured); }
        }
    }

    void Load()
    {
        // Missing or invalid entries retain defaults, including when loading is retried.
        logLevel = spdlog::level::info;
        allowCustomDestination = true;
        allowQuestMarkers = false;
        allowUndiscoveredLocations = false;
        ApplyLogLevel();

        // Build this path after SKSE initialization rather than during DLL initialization.
        const auto configPath = std::filesystem::absolute("Data/SKSE/Plugins/ArbitraryFastTravel.ini").string();
        CSimpleIniA ini;
        if (ini.LoadFile(configPath.c_str()) < SI_OK) {
            SKSE::log::info("Could not read {}; using configuration defaults.", configPath);
            return;
        }

        if (const char* configured = ini.GetValue("General", "LogLevel")) {
            if (const auto parsed = ParseLogLevel(configured)) { logLevel = *parsed; }
            else { SKSE::log::warn("Invalid LogLevel \"{}\"; keeping default Info.", configured); }
        }
        ApplyLogLevel();

        ReadBool(ini, "AllowCustomDestination", allowCustomDestination);
        ReadBool(ini, "AllowQuestMarkers", allowQuestMarkers);
        ReadBool(ini, "AllowUndiscoveredLocations", allowUndiscoveredLocations);
        SKSE::log::info(
            "Travel configuration: custom={}, quest={}, undiscovered={}.",
            allowCustomDestination, allowQuestMarkers, allowUndiscoveredLocations
        );
    }

    bool GetAllowCustomDestination() { return allowCustomDestination; }
    bool GetAllowQuestMarkers() { return allowQuestMarkers; }
    bool GetAllowUndiscoveredLocations() { return allowUndiscoveredLocations; }
}
