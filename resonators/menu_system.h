#pragma once

#include <cstddef>

enum class MenuItemType
{
    Percent,
    Ratio,
    Int,
    /* Int whose value is shown as a name from `nameFn` — resonator model,
     * trigger source. */
    Enum,
    /* Like Enum, but the name is the whole row: no label, full display width.
     * For values where the name *is* the identity, e.g. an SD filename. */
    Name,
    /* Rotating the encoder runs `action` instead of changing a value. The row
     * shows its label plus, if `nameFn` is set, nameFn(0) as a status. */
    Action,
};

struct MenuItem
{
    const char *label = "";
    MenuItemType type = MenuItemType::Percent;
    float *value = nullptr;
    int *intValue = nullptr;
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.01f;
    /* Enum/Name: value -> display name. Action: ignores the argument and
     * returns a status string. */
    const char *(*nameFn)(int) = nullptr;
    void (*action)() = nullptr;
};

struct MenuPage
{
    const char *title = "";
    MenuItem *items = nullptr;
    size_t itemCount = 0;
};

struct MenuState
{
    int pageIndex = 0;
    int selectedIndex = 0; // 0 = title, 1..itemCount = items
    int scrollIndex = 0;
};

struct MenuLine
{
    const char *label = "";
    MenuItemType type = MenuItemType::Percent;
    float value = 0.0f;
    int intValue = 0;
    const char *text = "";
    bool selected = false;
};

void MenuInit(MenuState &state);
void MenuRotate(MenuState &state, int inc, MenuPage *pages, size_t pageCount);
void MenuPress(MenuState &state, MenuPage *pages, size_t pageCount);
void MenuBuildVisibleLines(const MenuState &state,
                           const MenuPage &page,
                           MenuLine *lines,
                           int maxLines,
                           int &outCount,
                           bool &titleSelected);
