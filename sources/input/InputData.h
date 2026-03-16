//
// Created by bison on 18-11-25.
//

#pragma once
#include "raylib.h"
#include <vector>

enum class InputEventType {
    MouseClick,
    KeyPressed,
    KeyReleased,
    TextInput,
    Any
};

struct MouseClickEvent {
    Vector2 pos;
    int button;       // MOUSE_LEFT_BUTTON, etc
    bool doubleClick;
};

struct KeyEvent {
    int key;          // KEY_A, KEY_SPACE, etc
};

struct TextInputEvent {
    unsigned int codepoint = 0;
};

struct InputEvent {
    InputEventType type;
    bool handled = false;  // ← consumer sets this

    union {
        MouseClickEvent mouse{};
        KeyEvent key;
        TextInputEvent text;
    };
};

struct InputData {
    std::vector<InputEvent> events;

    // double-click logic
    float lastClickTime = -1.0f;
    float doubleClickThreshold = 0.3f;
};

