#pragma once

#include <cstddef>

enum class MenuItemType
{
    Percent,
    Ratio,
    Int,
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
