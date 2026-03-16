#include "Menu.h"
#include "raylib.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <stack>
#include <cmath>
#include "settings/Settings.h"
#include "input/Input.h"
#include "adventure/Adventure.h"
#include "save/SaveGame.h"

static GameState* game;

const Color MENU_BG_COLOR = Color{25, 25, 25, 255};

static constexpr int SAVE_SLOT_COUNT = 8;

static std::string menuToastText;
static float menuToastTimer = 0.0f;
static float menuToastDuration = 0.0f;

static void ShowMenuToast(const std::string& text, float durationSeconds = 1.5f) {
    menuToastText = text;
    menuToastDuration = durationSeconds;
    menuToastTimer = durationSeconds;
}

// Forward declaration
struct Menu;
static std::shared_ptr<Menu> createMainMenu();

using MenuBuilder = std::function<std::shared_ptr<Menu>()>;

struct MenuItem {
    std::string text;
    bool isSubmenu = false;
    bool isSlider = false;
    std::function<void()> action;               // only used if !isSubmenu
    std::function<float()> getValue;            // slider value getter
    std::function<void(float)> setValue;        // slider value setter
    float sliderMin = 0.0f;  // slider range
    float sliderMax = 1.0f;
    MenuBuilder submenuBuilder = nullptr;
    Color color = LIGHTGRAY;
    bool enabled = true;
};

struct Menu {
    std::string title;
    std::string hint;
    std::vector<MenuItem> items;
    int selected = 0;
};


static void startNewGame() {
    AdventureQueueLoadScene(*game, "basement_demo");
}

//static std::stack<std::shared_ptr<Menu>> menuStack;
static std::stack<std::function<std::shared_ptr<Menu>()>> menuStack;

static std::shared_ptr<Menu> createResolutionMenu() {
    RefreshResolutions(game->settings);
    // Resolution submenu
    auto resolutionMenu = std::make_shared<Menu>();
    resolutionMenu->title = "Resolution";

    for (size_t i = 0; i < game->settings.availableResolutions.size(); ++i) {
        auto& availRes = game->settings.availableResolutions[i];
        MenuItem res;
        bool selected = (int)i == game->settings.selectedResolutionIndex;
        res.text = (selected ? "< " : "  ") +
                   std::to_string(availRes.width) + " x " + std::to_string(availRes.height) + (selected ? " > " : "");

        res.color = selected ? WHITE : LIGHTGRAY;
        res.isSubmenu = false;
        res.action = [i] {
            SettingsData& settings = game->settings;
            settings.selectedResolutionIndex = (int) i;
            settings.needsApply = true;
            ApplySettings(settings);
            SaveSettings(settings);
        };
        resolutionMenu->items.push_back(res);
    }

    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        if (!menuStack.empty()) menuStack.pop();
    };
    resolutionMenu->items.push_back(back);
    return resolutionMenu;
}

static std::shared_ptr<Menu> createDisplayModeMenu() {
    // Resolution submenu
    auto menu = std::make_shared<Menu>();
    menu->title = "Display Mode";
    menu->hint = "Fullscreen is buggy AF, consider yourself warned.";
    MenuItem i1;
    i1.text = game->settings.displayMode == DisplayMode::Windowed ? "< Windowed >" : "Windowed";
    i1.color = game->settings.displayMode == DisplayMode::Windowed ? WHITE : LIGHTGRAY;
    i1.isSubmenu = false;
    i1.action = [] {
        game->settings.displayMode = DisplayMode::Windowed;
        game->settings.needsApply = true;
        ApplySettings(game->settings);
        SaveSettings(game->settings);
    };
    menu->items.push_back(i1);

    MenuItem i2;
    i2.text = game->settings.displayMode == DisplayMode::Fullscreen ? "< Fullscreen >" : "Fullscreen";
    i2.color = game->settings.displayMode == DisplayMode::Fullscreen ? WHITE : LIGHTGRAY;
    i2.isSubmenu = false;
    i2.action = [] {
        game->settings.displayMode = DisplayMode::Fullscreen;
        game->settings.needsApply = true;
        ApplySettings(game->settings);
        SaveSettings(game->settings);
    };
    i2.enabled = true;
    menu->items.push_back(i2);

    MenuItem i3;
    i3.text = game->settings.displayMode == DisplayMode::Borderless ? "< Borderless >" : "Borderless";
    i3.color = game->settings.displayMode == DisplayMode::Borderless ? WHITE : LIGHTGRAY;
    i3.isSubmenu = false;
    i3.action = [] {
        game->settings.displayMode = DisplayMode::Borderless;
        game->settings.needsApply = true;
        ApplySettings(game->settings);
        SaveSettings(game->settings);
    };
    menu->items.push_back(i3);

    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        if (!menuStack.empty()) menuStack.pop();
    };
    menu->items.push_back(back);
    return menu;
}

static std::shared_ptr<Menu> createDebugMenu() {
    auto debugMenu = std::make_shared<Menu>();
    debugMenu->title = "Debug Options";

    MenuItem toggleFPS;
    if(game->settings.showFPS) {
        toggleFPS.text = "Disable FPS";
    } else {
        toggleFPS.text = "Enable FPS";
    }
    toggleFPS.isSubmenu = false;
    toggleFPS.action = [] {
        game->settings.showFPS = !game->settings.showFPS;
        SaveSettings(game->settings);
    };
    debugMenu->items.push_back(toggleFPS);

    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        if (!menuStack.empty()) menuStack.pop();
    };
    debugMenu->items.push_back(back);
    return debugMenu;
}

static std::shared_ptr<Menu> createExposureMenu() {
    auto menu = std::make_shared<Menu>();
    menu->title = "Exposure";
    menu->hint = "Increase to make the game world look brighter.";

    MenuItem exposureSlider;
    exposureSlider.text = "Exposure";
    exposureSlider.isSlider = true;
    exposureSlider.sliderMin = 0.5;
    exposureSlider.sliderMax = 2.5;
    exposureSlider.getValue = []() { return game->settings.exposure; };
    exposureSlider.setValue = [](float v) {
        game->settings.exposure = v;
        TraceLog(LOG_INFO, "exposure = %f", game->settings.exposure);
    };
    menu->items.push_back(exposureSlider);

    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        SaveSettings(game->settings);
        if (!menuStack.empty()) menuStack.pop();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createSettingsMenu() {
    // Settings submenu
    auto settingsMenu = std::make_shared<Menu>();
    settingsMenu->title = "Settings";
    MenuItem resolution;
    resolution.text = "Resolution";
    resolution.isSubmenu = true;
    resolution.submenuBuilder = createResolutionMenu;
    settingsMenu->items.push_back(resolution);

    MenuItem displayMode;
    displayMode.text = "Display Mode";
    displayMode.isSubmenu = true;
    displayMode.submenuBuilder = createDisplayModeMenu;
    settingsMenu->items.push_back(displayMode);

    MenuItem exposure;
    exposure.text = "Exposure";
    exposure.isSubmenu = true;
    exposure.submenuBuilder = createExposureMenu;
    settingsMenu->items.push_back(exposure);

    MenuItem debugOptions;
    debugOptions.text = "Debug Options";
    debugOptions.isSubmenu = true;
    debugOptions.submenuBuilder = createDebugMenu;
    settingsMenu->items.push_back(debugOptions);


    MenuItem toggleFPSLock;
    if(game->settings.fpsLock) {
        toggleFPSLock.text = "Unlock FPS";
    } else {
        toggleFPSLock.text = "Lock FPS (60)";
    }
    toggleFPSLock.isSubmenu = false;
    toggleFPSLock.action = [] {
        game->settings.fpsLock = !game->settings.fpsLock;
        ApplySettings(game->settings);
        SaveSettings(game->settings);
    };
    settingsMenu->items.push_back(toggleFPSLock);


    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        if (!menuStack.empty()) menuStack.pop();
    };
    settingsMenu->items.push_back(back);
    return settingsMenu;
}

static std::shared_ptr<Menu> createSaveMenu() {
    auto menu = std::make_shared<Menu>();
    menu->title = "Save Game";
    menu->hint = "Select a slot to overwrite.";

    for (int slot = 1; slot <= SAVE_SLOT_COUNT; ++slot) {
        MenuItem item;
        item.text = "Slot " + std::to_string(slot) + " - " + GetSaveSlotSummary(slot);
        item.isSubmenu = false;
        item.enabled = game->adventure.currentScene.loaded;
        item.action = [slot] {
            if (SaveGameToSlot(*game, slot)) {
                TraceLog(LOG_INFO, "Saved game to slot %d", slot);
                ShowMenuToast("Game Saved");

                while (!menuStack.empty()) {
                    menuStack.pop();
                }
                menuStack.push(&createMainMenu);

                game->mode = GameMode::Game;
            } else {
                TraceLog(LOG_ERROR, "Failed saving game to slot %d", slot);
                ShowMenuToast("Save Failed");
            }
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        if (!menuStack.empty()) menuStack.pop();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createLoadMenu() {
    auto menu = std::make_shared<Menu>();
    menu->title = "Load Game";
    menu->hint = "Select a save slot to load.";

    for (int slot = 1; slot <= SAVE_SLOT_COUNT; ++slot) {
        MenuItem item;
        item.text = "Slot " + std::to_string(slot) + " - " + GetSaveSlotSummary(slot);
        item.isSubmenu = false;
        item.enabled = DoesSaveSlotExist(slot);
        item.action = [slot] {
            if (LoadGameFromSlot(*game, slot)) {
                TraceLog(LOG_INFO, "Loaded game from slot %d", slot);
                ShowMenuToast("Game Loaded");
                while (!menuStack.empty()) {
                    menuStack.pop();
                }
                menuStack.push(&createMainMenu);
            } else {
                TraceLog(LOG_ERROR, "Failed loading game from slot %d", slot);
                ShowMenuToast("Load Failed");
            }
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.isSubmenu = false;
    back.action = [] {
        if (!menuStack.empty()) menuStack.pop();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createMainMenu() {
    auto menu = std::make_shared<Menu>();
    menu->title = "Main Menu";

    const bool hasGameLoaded = game->adventure.currentScene.loaded;

    if (!hasGameLoaded) {
        MenuItem item;
        item.text = "Start New Game";
        item.isSubmenu = false;
        item.action = startNewGame;
        menu->items.push_back(item);
    } else {
        MenuItem resume;
        resume.text = "Resume";
        resume.isSubmenu = false;
        resume.action = [] {
            game->mode = GameMode::Game;
            TraceLog(LOG_DEBUG, "Resume selected");
        };
        menu->items.push_back(resume);

        MenuItem save;
        save.text = "Save Game";
        save.isSubmenu = true;
        save.submenuBuilder = createSaveMenu;
        menu->items.push_back(save);
    }

    MenuItem load;
    load.text = "Load Game";
    load.isSubmenu = true;
    load.submenuBuilder = createLoadMenu;
    menu->items.push_back(load);

    MenuItem settings;
    settings.text = "Settings";
    settings.isSubmenu = true;
    settings.submenuBuilder = createSettingsMenu;
    menu->items.push_back(settings);

    MenuItem quit;
    quit.text = "Quit";
    quit.isSubmenu = false;
    quit.action = [] {
        TraceLog(LOG_INFO, "main menu quit");
        game->mode = GameMode::Quit;
    };
    menu->items.push_back(quit);

    return menu;
}

void MenuInit(GameState* gameState) {
    game = gameState;
    menuStack = std::stack<std::function<std::shared_ptr<Menu>()>>();
    menuStack.push(&createMainMenu);
}

void MenuUpdate(float dt) {
    if (menuToastTimer > 0.0f) {
        menuToastTimer -= dt;
        if (menuToastTimer < 0.0f) {
            menuToastTimer = 0.0f;
            menuToastText.clear();
        }
    }
}

void MenuRenderUi(GameState& state) {
    ClearBackground(MENU_BG_COLOR);
    if (menuStack.empty()) return;

    std::shared_ptr<Menu> menu = menuStack.top()(); // call the builder
    float menuX = INTERNAL_WIDTH / 2.0f;
    float menuY = INTERNAL_HEIGHT / 2.0f;
    float spacing = 40.0f;
    float itemHeight = 36.0f;

    float itemWidth = 0.0f;
    for (const MenuItem& item : menu->items) {
        const int textWidth = MeasureText(item.text.c_str(), 20);
        if (static_cast<float>(textWidth) > itemWidth) {
            itemWidth = static_cast<float>(textWidth);
        }
    }

    itemWidth += 80.0f; // left/right padding
    if (itemWidth < 400.0f) {
        itemWidth = 400.0f;
    }

    if(!menu->title.empty()) {
        DrawText(menu->title.c_str(), menuX - (MeasureText(menu->title.c_str(), 40) / 2), 10, 40, WHITE);
    }

    if(!menu->hint.empty()) {
        DrawText(menu->hint.c_str(), menuX - (MeasureText(menu->hint.c_str(), 30) / 2), 255, 30, LIGHTGRAY);
    }
    static bool dragging = false;

    for (int i = 0; i < (int)menu->items.size(); ++i) {
        bool enabled = menu->items[i].enabled;
        float x = menuX - itemWidth / 2;
        float y = menuY + (i - menu->items.size() / 2.0f) * spacing;
        Rectangle rect = {x, y, itemWidth, itemHeight};

        bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), rect);

        bool clicked = false;
        for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
            if(CheckCollisionPointRec(ev.mouse.pos, rect) && ev.mouse.button == MOUSE_LEFT_BUTTON) {
                clicked = true;
                ConsumeEvent(ev);
            }
        }

        //bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        if (!menu->items[i].isSlider) {
            DrawRectangleRec(rect, hovered ? Fade(WHITE, 0.1f) : Fade(WHITE, 0.05f));
            DrawRectangleLinesEx(rect, 1.0f, hovered ? YELLOW : DARKGRAY);
            if (enabled) {
                DrawText(menu->items[i].text.c_str(), (int) (x + 10), (int) (y + 8), 20,
                         hovered ? YELLOW : menu->items[i].color);
            } else {
                DrawText(menu->items[i].text.c_str(), (int) (x + 10), (int) (y + 8), 20, DARKGRAY);
            }

            if (clicked) {
                auto &item = menu->items[i];
                if (item.isSubmenu && item.submenuBuilder) {
                    menuStack.push(item.submenuBuilder);
                } else if (item.action) {
                    item.action();
                }
            }
        }

        if (menu->items[i].isSlider) {
            float sliderWidth = itemWidth - 20;
            float sliderHeight = 6;
            float sliderX = x + 10;
            float sliderY = y;

            // normalize value 0..1
            float rawValue = menu->items[i].getValue();
            float value = (rawValue - menu->items[i].sliderMin) / (menu->items[i].sliderMax - menu->items[i].sliderMin);
            value = fminf(fmaxf(value, 0.0f), 1.0f);

            float knobX = sliderX + value * sliderWidth;

            // track
            DrawRectangle(sliderX, sliderY, sliderWidth, sliderHeight, DARKGRAY);
            DrawRectangle(knobX - 5, sliderY - 2, 10, sliderHeight + 4, YELLOW);

            // numeric value
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", rawValue);
            DrawText(buf, sliderX + sliderWidth + 10, sliderY-2, 20, WHITE);

            // mouse interaction
            if (CheckCollisionPointRec(GetMousePosition(), {sliderX, sliderY - 4, sliderWidth, sliderHeight + 8})
                && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !dragging) {
                dragging = true;
            }
            if(dragging) {
                float newValue = (GetMousePosition().x - sliderX) / sliderWidth;
                newValue = fminf(fmaxf(newValue, 0.0f), 1.0f);
                // map back to raw range
                newValue = menu->items[i].sliderMin + newValue * (menu->items[i].sliderMax - menu->items[i].sliderMin);
                menu->items[i].setValue(newValue);
                if(!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    dragging = false;
                }
            }
        }
    }
}

void MenuRenderOverlay() {
    if (menuToastTimer <= 0.0f || menuToastText.empty()) {
        return;
    }

    float alpha = 1.0f;
    if (menuToastDuration > 0.0f && menuToastTimer < 0.35f) {
        alpha = menuToastTimer / 0.35f;
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
    }

    const int fontSize = 24;
    const int paddingX = 18;
    const int paddingY = 12;

    const int textWidth = MeasureText(menuToastText.c_str(), fontSize);
    const float boxWidth = static_cast<float>(textWidth + paddingX * 2);
    const float boxHeight = static_cast<float>(fontSize + paddingY * 2);

    const float x = static_cast<float>(INTERNAL_WIDTH) - boxWidth - 24.0f;
    const float y = 24.0f;

    const Color bg = Color{20, 20, 24, static_cast<unsigned char>(220.0f * alpha)};
    const Color border = Color{180, 180, 200, static_cast<unsigned char>(255.0f * alpha)};
    const Color textColor = Color{255, 255, 255, static_cast<unsigned char>(255.0f * alpha)};

    DrawRectangleRounded(Rectangle{x, y, boxWidth, boxHeight}, 0.15f, 6, bg);
    DrawRectangleRoundedLinesEx(Rectangle{x, y, boxWidth, boxHeight}, 0.15f, 6, 2.0f, border);
    DrawText(menuToastText.c_str(),
             static_cast<int>(x + paddingX),
             static_cast<int>(y + paddingY),
             fontSize,
             textColor);
}

void MenuHandleInput(GameState& state) {
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if(ev.key.key == KEY_ESCAPE) {
            if (menuStack.size() > 1) {
                menuStack.pop();
            } else {
                state.mode = GameMode::Game;
                TraceLog(LOG_DEBUG, "closing menu");
            }
            ConsumeEvent(ev);
        }
    }
}
