//
// Created by Stinus Troels Petersen on 14/06/2025.
//

#include <fstream>
#include "Settings.h"
//#include "raygui.h"
#include "raylib.h"
#include "utils/json.hpp"
#include "data/GameState.h"

void ApplySettings(SettingsData& settings) {
    Resolution res = settings.availableResolutions[settings.selectedResolutionIndex];
    settings.monitor = GetCurrentMonitor();
    switch (settings.displayMode) {
        case DisplayMode::Windowed: {
            ClearWindowState(FLAG_FULLSCREEN_MODE);
            ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            SetWindowSize(res.width, res.height);

            // Manually center on primary monitor (assumes monitor starts at 0,0)
            /*
            int monitorWidth = GetMonitorWidth(settings.monitor);
            int monitorHeight = GetMonitorHeight(settings.monitor);
            int winX = (monitorWidth - res.width) / 2;
            int winY = (monitorHeight - res.height) / 2;
            SetWindowPosition(winX, winY);
             */
            break;
        }

        case DisplayMode::Fullscreen: {
            ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            SetWindowSize(res.width, res.height);
            SetWindowState(FLAG_FULLSCREEN_MODE);
            break;
        }

        case DisplayMode::Borderless: {
            ClearWindowState(FLAG_FULLSCREEN_MODE);
            SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            SetWindowSize(GetMonitorWidth(settings.monitor), GetMonitorHeight(settings.monitor));
            break;
        }
    }
    SetWindowMonitor(settings.monitor);

    settings.originalResolutionIndex = settings.selectedResolutionIndex;
    settings.originalDisplayMode = settings.displayMode;

    if(settings.fpsLock) {
        SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
    } else {
        SetTargetFPS(0);
    }

    settings.needsApply = false;
}

void SaveSettings(const SettingsData& settings) {
    nlohmann::json j;
    j["resolutionIndex"] = settings.selectedResolutionIndex;
    j["displayMode"] = static_cast<int>(settings.displayMode);
    j["monitor"] = settings.monitor;
    j["showFPS"] = settings.showFPS;
    j["lockFPS"] = settings.fpsLock;
    j["exposure"] = settings.exposure;
    j["soundVolume"] = settings.soundVolume;
    j["musicVolume"] = settings.musicVolume;

    std::ofstream file(settings.filename);
    if (file) {
        file << j.dump(2);
    }
}

void RefreshResolutions(SettingsData& data) {
    int monitor = GetCurrentMonitor();
    data.monitor = monitor;
    // Populate available resolutions
    data.availableResolutions.clear();

    int monWidth = GetMonitorWidth(monitor);
    int monHeight = GetMonitorHeight(monitor);

    if(monWidth >= 1920 && monHeight >= 1080) {
        data.availableResolutions.push_back({1920, 1080});
    }
    if(monWidth >= 2560 && monHeight >= 1440) {
        data.availableResolutions.push_back({2560, 1440});
    }

    if (data.selectedResolutionIndex >= data.availableResolutions.size()) {
        data.selectedResolutionIndex = 0;
    }
}

void InitSettings(SettingsData& data, const std::string &filename) {
    data.filename = filename;
    int monitor = GetCurrentMonitor();

    // Load from JSON
    std::ifstream file(filename);
    if (file) {
        nlohmann::json j;
        file >> j;

        data.selectedResolutionIndex = j.value("resolutionIndex", 0);
        data.displayMode = static_cast<DisplayMode>(j.value("displayMode", 0));
        if(j["showFPS"] != nullptr) {
            j["showFPS"].get_to(data.showFPS);
        }
        if(j.contains("lockFPS")) {
            j["lockFPS"].get_to(data.fpsLock);
        }
        if(j.contains("monitor")) {
            j["monitor"].get_to(data.monitor);
        }
        if(j.contains("exposure")) {
            j["exposure"].get_to(data.exposure);
        }
        if (j.contains("soundVolume")) {
            j["soundVolume"].get_to(data.soundVolume);
        }
        if (j.contains("musicVolume")) {
            j["musicVolume"].get_to(data.musicVolume);
        }
    }

    data.originalResolutionIndex = data.selectedResolutionIndex;
    data.originalDisplayMode = data.displayMode;
    data.originalMonitor = data.monitor;

    RefreshResolutions(data);
}

static Color GuiIntToColor(int colInt) {
    Color c;
    c.r = (colInt >> 0) & 0xFF;
    c.g = (colInt >> 8) & 0xFF;
    c.b = (colInt >> 16) & 0xFF;
    c.a = (colInt >> 24) & 0xFF;
    return c;
}
