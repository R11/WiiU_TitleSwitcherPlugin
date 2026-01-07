/**
 * Help Overlay Implementation
 * Displays context-sensitive controls help.
 */

#include "help_overlay.h"
#include "../menu.h"
#include "../../render/renderer.h"
#include "../../input/buttons.h"
#include "../../storage/settings.h"

namespace Menu {
namespace HelpOverlay {

namespace {

bool sVisible = false;
Context sCurrentContext = Context::BROWSE;

constexpr int OVERLAY_START_COL = 20;
constexpr int OVERLAY_END_COL = 80;
constexpr int OVERLAY_START_ROW = 3;
constexpr int OVERLAY_TITLE_ROW = 4;
constexpr int OVERLAY_CONTENT_ROW = 6;
constexpr int OVERLAY_FOOTER_ROW = 17;

constexpr uint32_t OVERLAY_BG_COLOR = 0x181825FF;
constexpr uint32_t OVERLAY_BORDER_COLOR = 0x45475aFF;
constexpr uint32_t OVERLAY_TITLE_COLOR = 0x89b4faFF;
constexpr uint32_t OVERLAY_KEY_COLOR = 0xf9e2afFF;
constexpr uint32_t OVERLAY_TEXT_COLOR = 0xcdd6f4FF;
constexpr uint32_t OVERLAY_HINT_COLOR = 0x6c7086FF;

struct HelpEntry {
    const char* key;
    const char* action;
};

void drawOverlayBackground()
{
    for (int row = OVERLAY_START_ROW; row <= OVERLAY_FOOTER_ROW + 1; row++) {
        for (int col = OVERLAY_START_COL; col < OVERLAY_END_COL; col++) {
            Renderer::DrawText(col, row, " ", OVERLAY_BG_COLOR);
        }
    }

    for (int col = OVERLAY_START_COL; col < OVERLAY_END_COL; col++) {
        Renderer::DrawText(col, OVERLAY_START_ROW, "-", OVERLAY_BORDER_COLOR);
        Renderer::DrawText(col, OVERLAY_FOOTER_ROW + 1, "-", OVERLAY_BORDER_COLOR);
    }
    for (int row = OVERLAY_START_ROW; row <= OVERLAY_FOOTER_ROW + 1; row++) {
        Renderer::DrawText(OVERLAY_START_COL, row, "|", OVERLAY_BORDER_COLOR);
        Renderer::DrawText(OVERLAY_END_COL - 1, row, "|", OVERLAY_BORDER_COLOR);
    }
}

void drawHelpEntries(const HelpEntry* entries, int count, int startRow)
{
    int row = startRow;
    int leftCol = OVERLAY_START_COL + 3;
    int rightCol = OVERLAY_START_COL + 32;

    for (int i = 0; i < count && row < OVERLAY_FOOTER_ROW; i++) {
        int col = (i % 2 == 0) ? leftCol : rightCol;
        if (i % 2 == 1) {
        } else if (i > 0) {
            row++;
        }

        Renderer::DrawTextF(col, row, OVERLAY_KEY_COLOR, "%s", entries[i].key);
        int keyLen = 0;
        for (const char* p = entries[i].key; *p; p++) keyLen++;
        Renderer::DrawTextF(col + keyLen + 1, row, OVERLAY_TEXT_COLOR, "%s", entries[i].action);

        if (i % 2 == 1) {
            row++;
        }
    }
}

void renderBrowseHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "BROWSE CONTROLS", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Navigate list"},
        {"Left/Right", "Skip 5 titles"},
        {"L/R", "Page up/down"},
        {"ZL/ZR", "Change category"},
        {"A", "Launch title"},
        {"B", "Close menu"},
        {"Y", "Toggle favorite"},
        {"X", "Edit categories"},
        {"+", "Open settings"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderEditHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "EDIT CATEGORIES", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Navigate categories"},
        {"A", "Toggle category"},
        {"B", "Save and go back"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderSettingsMainHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "SETTINGS", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Navigate options"},
        {"A", "Edit / Select"},
        {"B", "Save and go back"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderManageCatsHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "MANAGE CATEGORIES", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Navigate categories"},
        {"L/R", "Move category up/down"},
        {"A", "Rename category"},
        {"Y", "Toggle visibility"},
        {"X", "Delete category"},
        {"+", "Add new category"},
        {"B", "Go back"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderSystemAppsHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "SYSTEM APPS", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Navigate apps"},
        {"A", "Launch app"},
        {"B", "Go back"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderColorsHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "CUSTOMIZE COLORS", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Navigate colors"},
        {"A", "Edit color value"},
        {"B", "Go back"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderColorInputHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "EDIT COLOR VALUE", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Change character"},
        {"A/B", "Move cursor right/left"},
        {"X", "Delete character"},
        {"+", "Confirm"},
        {"-", "Cancel"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderNameInputHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "ENTER NAME", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"Up/Down", "Change character"},
        {"A/B", "Move cursor right/left"},
        {"X", "Delete character"},
        {"+", "Confirm"},
        {"-", "Cancel"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

void renderDebugGridHelp()
{
    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_TITLE_ROW, "DEBUG GRID", OVERLAY_TITLE_COLOR);

    static const HelpEntry entries[] = {
        {"B", "Go back"},
        {"-", "Show/hide help"},
    };

    drawHelpEntries(entries, sizeof(entries) / sizeof(entries[0]), OVERLAY_CONTENT_ROW);
}

}

bool IsVisible()
{
    return sVisible;
}

void Show(Context context)
{
    sCurrentContext = context;
    sVisible = true;
}

void Hide()
{
    sVisible = false;
}

void Toggle(Context context)
{
    if (sVisible) {
        Hide();
    } else {
        Show(context);
    }
}

void Render()
{
    if (!sVisible) return;

    drawOverlayBackground();

    switch (sCurrentContext) {
        case Context::BROWSE:
            renderBrowseHelp();
            break;
        case Context::EDIT:
            renderEditHelp();
            break;
        case Context::SETTINGS_MAIN:
            renderSettingsMainHelp();
            break;
        case Context::SETTINGS_MANAGE_CATS:
            renderManageCatsHelp();
            break;
        case Context::SETTINGS_SYSTEM_APPS:
            renderSystemAppsHelp();
            break;
        case Context::SETTINGS_COLORS:
            renderColorsHelp();
            break;
        case Context::SETTINGS_COLOR_INPUT:
            renderColorInputHelp();
            break;
        case Context::SETTINGS_NAME_INPUT:
            renderNameInputHelp();
            break;
        case Context::DEBUG_GRID:
            renderDebugGridHelp();
            break;
    }

    Renderer::DrawText(OVERLAY_START_COL + 2, OVERLAY_FOOTER_ROW, "Press - to close", OVERLAY_HINT_COLOR);
}

bool HandleInput(uint32_t pressed)
{
    if (!sVisible) return false;

    if (Buttons::Actions::HELP.Pressed(pressed)) {
        Hide();
        return true;
    }

    return true;
}

void DrawHelpIndicator()
{
    if (sVisible) return;

    int col = Renderer::GetGridWidth() - 4;
    int row = Renderer::GetFooterRow();

    Renderer::DrawText(col, row, "[?]", OVERLAY_HINT_COLOR);
}

}
}
