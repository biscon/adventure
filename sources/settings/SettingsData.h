//
// Created by Stinus Troels Petersen on 14/06/2025.
//

#ifndef SANDBOX_SETTINGSDATA_H
#define SANDBOX_SETTINGSDATA_H

#include <vector>
#include <string>

enum class DisplayMode {
    Windowed,
    Fullscreen,
    Borderless
};

struct Resolution {
    int width;
    int height;
};

struct SettingsData {
    std::vector<Resolution> availableResolutions;
    int selectedResolutionIndex = 0;

    DisplayMode displayMode = DisplayMode::Windowed;
    DisplayMode originalDisplayMode = DisplayMode::Windowed;
    int originalResolutionIndex = 0;
    int monitor = 0;
    int originalMonitor = 0;

    std::string filename;
    bool needsApply = false;
    bool showFPS = false;
    bool fpsLock = true;
    float exposure = 1.1;
};


#endif //SANDBOX_SETTINGSDATA_H
