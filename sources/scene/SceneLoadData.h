#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "raylib.h"
#include "scene/SceneData.h"

enum class SceneLoadJobState {
    Idle,
    Running,
    Succeeded,
    Failed
};

struct PreparedSceneImageData {
    std::string path;
    Image image{};
};

struct PreparedSceneLoadData {
    SceneData scene{};
    std::string sceneDir;
    std::string scriptPath;
    std::string requestedSpawnId;

    std::vector<PreparedSceneImageData> images;

    bool success = false;
    std::string errorMessage;
};

struct SceneLoadJobData {
    SceneLoadJobState state = SceneLoadJobState::Idle;

    std::string sceneId;
    std::string spawnId;

    PreparedSceneLoadData prepared{};

    std::thread worker;
    std::mutex mutex;
};
