//
// Created by bison on 18-11-25.
//

#include <algorithm>
#include "Input.h"

void InitInput(InputData& input) {
    input.events.clear();
    input.lastClickTime = -1.0f;
    input.doubleClickThreshold = 0.3f;
}

static void AddEvent(InputData& input, const InputEvent& evt) {
    input.events.push_back(evt);
}

void UpdateInput(InputData& input)
{
    input.events.clear();
    // 1. Remove handled events from previous frame
    // Remove handled events (C++17 version)
    /*
    input.events.erase(
            std::remove_if(input.events.begin(), input.events.end(),
                           [](const InputEvent& e) { return e.handled; }),
            input.events.end()
    );
    */

    Vector2 mouseScreen = GetMousePosition();

    float now = (float)GetTime();

    // ---- MOUSE CLICKS ----------------------------------------------------------------

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {

        bool dbl = (now - input.lastClickTime <= input.doubleClickThreshold);
        input.lastClickTime = now;

        InputEvent evt;
        evt.type = InputEventType::MouseClick;
        evt.handled = false;

        evt.mouse.pos = mouseScreen;
        evt.mouse.button = MOUSE_LEFT_BUTTON;
        evt.mouse.doubleClick = dbl;

        AddEvent(input, evt);
    }

    if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {

        bool dbl = (now - input.lastClickTime <= input.doubleClickThreshold);
        input.lastClickTime = now;

        InputEvent evt;
        evt.type = InputEventType::MouseClick;
        evt.handled = false;

        evt.mouse.pos = mouseScreen;
        evt.mouse.button = MOUSE_RIGHT_BUTTON;
        evt.mouse.doubleClick = dbl;

        AddEvent(input, evt);
    }

    // ---- KEYBOARD -----------------------------------------------------

    for (;;) {
        const int key = GetKeyPressed();
        if (key == 0) {
            break;
        }

        InputEvent evt;
        evt.type = InputEventType::KeyPressed;
        evt.handled = false;
        evt.key.key = key;
        AddEvent(input, evt);
    }

    // ---- TEXT INPUT ---------------------------------------------------

    for (;;) {
        const int codepoint = GetCharPressed();
        if (codepoint == 0) {
            break;
        }

        InputEvent evt;
        evt.type = InputEventType::TextInput;
        evt.handled = false;
        evt.text.codepoint = static_cast<unsigned int>(codepoint);
        AddEvent(input, evt);
    }

    // Key release (raylib does not track releases for all keys)
    // You can expand this if needed.
}


