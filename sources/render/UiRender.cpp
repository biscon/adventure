#include <sstream>
#include <cmath>
#include "UiRender.h"
#include "scene/SceneHelpers.h"
#include "render/RenderHelpers.h"
#include "adventure/AdventureActorHelpers.h"
#include "adventure/Inventory.h"
#include "adventure/InventoryUi.h"
#include "resources/TextureAsset.h"
#include "adventure/Dialogue.h"

static void DrawShadowedTextLine(
        Font font,
        const std::string& text,
        Vector2 pos,
        float fontSize,
        float spacing,
        Color textColor,
        int shadowOffset)
{
    const Color shadow = Color{0, 0, 0, 170};

    const Vector2 adjustPos = pos;
    pos.x = std::trunc(pos.x);
    pos.y = std::trunc(pos.y);
    DrawTextEx(font, text.c_str(),
               Vector2{adjustPos.x + static_cast<float>(shadowOffset), adjustPos.y + static_cast<float>(shadowOffset)},
               fontSize, spacing, shadow);

    DrawTextEx(font, text.c_str(), adjustPos, fontSize, spacing, textColor);
}

static std::vector<std::string> WrapTextLines(
        Font font,
        const std::string& text,
        float fontSize,
        float spacing,
        float maxWidth)
{
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string paragraph;

    while (std::getline(input, paragraph)) {
        std::istringstream words(paragraph);
        std::string word;
        std::string current;

        while (words >> word) {
            std::string candidate = current.empty() ? word : current + " " + word;
            const Vector2 size = MeasureTextEx(font, candidate.c_str(), fontSize, spacing);

            if (size.x <= maxWidth || current.empty()) {
                current = candidate;
            } else {
                lines.push_back(current);
                current = word;
            }
        }

        if (!current.empty()) {
            lines.push_back(current);
        }

        if (paragraph.empty()) {
            lines.push_back("");
        }
    }

    if (lines.empty()) {
        lines.push_back("");
    }

    return lines;
}

static void DrawBottomCenteredHoverName(const GameState& state)
{
    const ActorInventoryData* inv = GetControlledActorInventory(state);
    const ItemDefinitionData* heldItemDef = nullptr;

    if (inv != nullptr && !inv->heldItemId.empty()) {
        heldItemDef = FindItemDefinitionById(state, inv->heldItemId);
    }

    std::string text;

    if (heldItemDef != nullptr) {
        text = heldItemDef->displayName;

        if (state.adventure.hoverUi.active && !state.adventure.hoverUi.displayName.empty()) {
            text += " -> ";
            text += state.adventure.hoverUi.displayName;
        }
    } else {
        if (!state.adventure.hoverUi.active || state.adventure.hoverUi.displayName.empty()) {
            return;
        }

        text = state.adventure.hoverUi.displayName;
    }

    if (text.empty()) {
        return;
    }

    Font font = GetFontDefault();
    const float fontSize = 48.0f;
    const float spacing = 2.0f;
    const int shadowOffset = 2;

    const Vector2 size = MeasureTextEx(font, text.c_str(), fontSize, spacing);

    Vector2 pos{};
    pos.x = (static_cast<float>(INTERNAL_WIDTH) - size.x) * 0.5f;
    pos.y = static_cast<float>(INTERNAL_HEIGHT) - 90.0f;

    DrawShadowedTextLine(font, text, pos, fontSize, spacing, WHITE, shadowOffset);
}

static void DrawSpeechUi(const GameState& state)
{
    if (!state.adventure.speechUi.active) {
        return;
    }

    Font font = GetFontDefault();
    const float fontSize = 42.0f;
    const float spacing = 2.0f;
    const float maxWidth = 900.0f;
    const float lineHeight = 44.0f;
    const int shadowOffset = 2;

    Rectangle anchorRect{};
    bool haveAnchorRect = false;

    switch (state.adventure.speechUi.anchorType) {
        case SpeechAnchorType::Player:
        {
            const ActorInstance* controlledActor = GetControlledActor(state);
            if (controlledActor == nullptr || !controlledActor->activeInScene || !controlledActor->visible) {
                return;
            }

            anchorRect = GetActorScreenRect(state, *controlledActor);
            haveAnchorRect = true;
            break;
        }
        case SpeechAnchorType::Actor:
        {
            const int actorIndex = state.adventure.speechUi.actorIndex;
            if (actorIndex < 0 ||
                actorIndex >= static_cast<int>(state.adventure.actors.size())) {
                return;
            }

            const ActorInstance& actor = state.adventure.actors[actorIndex];
            if (!actor.activeInScene || !actor.visible) {
                return;
            }

            anchorRect = GetActorScreenRect(state, actor);
            haveAnchorRect = true;
            break;
        }
        case SpeechAnchorType::Prop:
        {
            const int propIndex = state.adventure.speechUi.propIndex;
            if (propIndex < 0 ||
                propIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
                propIndex >= static_cast<int>(state.adventure.props.size())) {
                return;
            }

            const ScenePropData& sceneProp = state.adventure.currentScene.props[propIndex];
            const PropInstance& prop = state.adventure.props[propIndex];

            if (!prop.visible) {
                return;
            }

            anchorRect = GetPropScreenRect(state, sceneProp, prop);
            haveAnchorRect = true;
            break;
        }

        case SpeechAnchorType::Position:
        {
            const Vector2 screenPos{
                    state.adventure.speechUi.worldPos.x - state.adventure.camera.position.x,
                    state.adventure.speechUi.worldPos.y - state.adventure.camera.position.y
            };

            anchorRect.x = screenPos.x - 1.0f;
            anchorRect.y = screenPos.y - 1.0f;
            anchorRect.width = 2.0f;
            anchorRect.height = 2.0f;
            haveAnchorRect = true;
            break;
        }
    }

    if (!haveAnchorRect) {
        return;
    }

    const std::vector<std::string> lines =
            WrapTextLines(font, state.adventure.speechUi.text, fontSize, spacing, maxWidth);

    float widest = 0.0f;
    for (const auto& line : lines) {
        widest = std::max(widest, MeasureTextEx(font, line.c_str(), fontSize, spacing).x);
    }

    const float totalHeight = lineHeight * static_cast<float>(lines.size());

    float x = anchorRect.x + anchorRect.width * 0.5f - widest * 0.5f;
    float y = anchorRect.y - totalHeight - 30.0f;

    if (x < 20.0f) x = 20.0f;
    if (x + widest > static_cast<float>(INTERNAL_WIDTH) - 20.0f) {
        x = static_cast<float>(INTERNAL_WIDTH) - 20.0f - widest;
    }

    if (y < 20.0f) y = 20.0f;

    const Color speechColor = state.adventure.speechUi.color;

    for (size_t i = 0; i < lines.size(); ++i) {
        Vector2 pos{
                x,
                y + lineHeight * static_cast<float>(i)
        };
        DrawShadowedTextLine(font, lines[i], pos, fontSize, spacing, speechColor, shadowOffset);
    }
}

static bool CanInventoryPageBackwardUi(const ActorInventoryData& inv)
{
    return inv.pageStartIndex > 0;
}

static bool CanInventoryPageForwardUi(const ActorInventoryData& inv)
{
    return inv.pageStartIndex + 10 < static_cast<int>(inv.itemIds.size());
}

static void DrawInventoryUi(const GameState& state)
{
    const InventoryUiState& ui = state.adventure.inventoryUi;
    if (ui.openAmount <= 0.0f) {
        return;
    }

    const Rectangle panelRect = GetInventoryPanelRect(state);
    const Rectangle prevRect{
            panelRect.x + 26.0f,
            panelRect.y + 26.0f,
            96.0f,
            96.0f
    };
    const Rectangle nextRect{
            panelRect.x + panelRect.width - 26.0f - 96.0f,
            panelRect.y + 26.0f,
            96.0f,
            96.0f
    };

    DrawRectangleRounded(panelRect, 0.18f, 8, Color{20, 20, 24, 230});
    DrawRectangleRoundedLinesEx(panelRect, 0.18f, 8, 3.0f, Color{130, 130, 150, 255});

    const ActorInventoryData* inv = GetControlledActorInventory(state);
    const bool canPageBackward =
            (inv != nullptr) &&
            (inv->pageStartIndex > 0);
    const bool canPageForward =
            (inv != nullptr) &&
            (inv->pageStartIndex + 10 < static_cast<int>(inv->itemIds.size()));

    const Color prevButtonColor =
            !canPageBackward ? Color{40, 40, 48, 255}
                             : (ui.hoveringPrevPage ? Color{90, 90, 110, 255}
                                                    : Color{60, 60, 75, 255});

    const Color nextButtonColor =
            !canPageForward ? Color{40, 40, 48, 255}
                            : (ui.hoveringNextPage ? Color{90, 90, 110, 255}
                                                   : Color{60, 60, 75, 255});

    const Color prevArrowColor = canPageBackward ? WHITE : Color{120, 120, 130, 255};
    const Color nextArrowColor = canPageForward ? WHITE : Color{120, 120, 130, 255};

    DrawRectangleRounded(prevRect, 0.18f, 6, prevButtonColor);
    DrawRectangleRounded(nextRect, 0.18f, 6, nextButtonColor);

    DrawText("<", static_cast<int>(prevRect.x + 34.0f), static_cast<int>(prevRect.y + 26.0f), 40, prevArrowColor);
    DrawText(">", static_cast<int>(nextRect.x + 34.0f), static_cast<int>(nextRect.y + 26.0f), 40, nextArrowColor);

    if (inv == nullptr) {
        return;
    }

    const float slotsStartX = prevRect.x + prevRect.width + 18.0f;

    for (int visibleSlotIndex = 0; visibleSlotIndex < 10; ++visibleSlotIndex) {
        const int itemIndex = inv->pageStartIndex + visibleSlotIndex;

        Rectangle slotRect{
                slotsStartX + static_cast<float>(visibleSlotIndex) * (96.0f + 12.0f),
                panelRect.y + 26.0f,
                96.0f,
                96.0f
        };

        const bool isHovered = (ui.hoveredSlotIndex == visibleSlotIndex);
        DrawRectangleRounded(slotRect, 0.14f, 6, isHovered ? Color{110, 110, 130, 255} : Color{70, 70, 85, 255});
        DrawRectangleRoundedLinesEx(slotRect, 0.14f, 6, 2.0f, Color{150, 150, 170, 255});

        if (itemIndex < 0 || itemIndex >= static_cast<int>(inv->itemIds.size())) {
            continue;
        }

        const ItemDefinitionData* itemDef = FindItemDefinitionById(state, inv->itemIds[itemIndex]);
        if (itemDef == nullptr || itemDef->iconTextureHandle < 0) {
            continue;
        }

        const TextureResource* texRes =
                FindTextureResource(state.resources, itemDef->iconTextureHandle);
        if (texRes == nullptr || !texRes->loaded) {
            continue;
        }

        Rectangle src{
                0.0f,
                0.0f,
                static_cast<float>(texRes->texture.width),
                static_cast<float>(texRes->texture.height)
        };

        Rectangle dst{
                slotRect.x + 8.0f,
                slotRect.y + 8.0f,
                slotRect.width - 16.0f,
                slotRect.height - 16.0f
        };

        DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);

        if (inv->heldItemId == itemDef->itemId) {
            DrawRectangleRoundedLinesEx(
                    Rectangle{slotRect.x - 2.0f, slotRect.y - 2.0f, slotRect.width + 4.0f, slotRect.height + 4.0f},
                    0.14f, 6, 3.0f, YELLOW);
        }
    }
}

static void DrawInventoryPickupPopup(const GameState& state)
{
    const InventoryPickupPopupState& popup = state.adventure.inventoryUi.pickupPopup;
    if (!popup.active || popup.itemId.empty()) {
        return;
    }

    const ItemDefinitionData* itemDef = FindItemDefinitionById(state, popup.itemId);
    if (itemDef == nullptr || itemDef->iconTextureHandle < 0) {
        return;
    }

    const TextureResource* texRes =
            FindTextureResource(state.resources, itemDef->iconTextureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    float alpha = 1.0f;

    if (popup.timerMs < popup.fadeInMs) {
        alpha = popup.fadeInMs > 0.0f ? (popup.timerMs / popup.fadeInMs) : 1.0f;
    } else if (popup.timerMs < popup.fadeInMs + popup.holdMs) {
        alpha = 1.0f;
    } else {
        const float fadeOutT =
                (popup.timerMs - popup.fadeInMs - popup.holdMs) /
                (popup.fadeOutMs > 0.0f ? popup.fadeOutMs : 1.0f);
        alpha = 1.0f - fadeOutT;
    }

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    const float diameter = 140.0f;
    const float radius = diameter * 0.5f;
    const float padding = 32.0f;

    const Vector2 center{
            static_cast<float>(INTERNAL_WIDTH) - padding - radius,
            padding + radius
    };

    const Color fill{
            20, 20, 24,
            static_cast<unsigned char>(230.0f * alpha)
    };

    const Color border{
            130, 130, 150,
            static_cast<unsigned char>(255.0f * alpha)
    };

    DrawCircleV(center, radius, fill);
    DrawCircleLinesV(center, radius, border);
    DrawCircleLinesV(center, radius - 1.0f, border);

    Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(texRes->texture.width),
            static_cast<float>(texRes->texture.height)
    };

    const float iconSize = 72.0f;

    Rectangle dst{
            center.x - iconSize * 0.5f,
            center.y - iconSize * 0.5f,
            iconSize,
            iconSize
    };

    const Color tint{
            255, 255, 255,
            static_cast<unsigned char>(255.0f * alpha)
    };

    DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void RenderAdventureUi(const GameState& state) {
    if (!state.adventure.currentScene.loaded) {
        return;
    }

    DrawInventoryUi(state);

    if (!IsDialogueUiActive(state) && !state.adventure.speechUi.active) {
        DrawBottomCenteredHoverName(state);
    }

    DrawSpeechUi(state);
    RenderDialogueUi(state);
    DrawInventoryPickupPopup(state);
    RenderHeldInventoryItemCursor(state);
}

