#include "menu_system.h"

#include <algorithm>

namespace
{
void ClampMenuItem(MenuItem &item, int inc)
{
    if (item.type == MenuItemType::Int && item.intValue)
    {
        const int minVal = static_cast<int>(item.min);
        const int maxVal = static_cast<int>(item.max);
        int next = *item.intValue + inc * static_cast<int>(item.step);
        next = std::clamp(next, minVal, maxVal);
        *item.intValue = next;
        return;
    }

    if (item.value)
    {
        float next = *item.value + static_cast<float>(inc) * item.step;
        next = std::clamp(next, item.min, item.max);
        *item.value = next;
    }
}

void EnsureVisible(MenuState &state, const MenuPage &page, int maxLines)
{
    if (state.selectedIndex <= 0)
    {
        state.scrollIndex = 0;
        return;
    }

    const int itemIndex = state.selectedIndex - 1;
    if (itemIndex < state.scrollIndex)
    {
        state.scrollIndex = itemIndex;
    }
    else if (itemIndex >= state.scrollIndex + maxLines)
    {
        state.scrollIndex = itemIndex - (maxLines - 1);
    }

    const int maxScroll = std::max(0, static_cast<int>(page.itemCount) - maxLines);
    state.scrollIndex = std::clamp(state.scrollIndex, 0, maxScroll);
}
} // namespace

void MenuInit(MenuState &state)
{
    state.pageIndex = 0;
    state.selectedIndex = 0;
    state.scrollIndex = 0;
}

void MenuRotate(MenuState &state, int inc, MenuPage *pages, size_t pageCount)
{
    if (pageCount == 0 || inc == 0)
    {
        return;
    }

    MenuPage &page = pages[state.pageIndex];
    if (state.selectedIndex == 0)
    {
        const int nextPage = (state.pageIndex + inc + static_cast<int>(pageCount)) % static_cast<int>(pageCount);
        state.pageIndex = nextPage;
        state.selectedIndex = 0;
        state.scrollIndex = 0;
        return;
    }

    const int itemIndex = state.selectedIndex - 1;
    if (itemIndex < 0 || itemIndex >= static_cast<int>(page.itemCount))
    {
        return;
    }

    ClampMenuItem(page.items[itemIndex], inc);
}

void MenuPress(MenuState &state, MenuPage *pages, size_t pageCount)
{
    if (pageCount == 0)
    {
        return;
    }

    const MenuPage &page = pages[state.pageIndex];
    const int maxIndex = static_cast<int>(page.itemCount);
    state.selectedIndex = (state.selectedIndex + 1) % (maxIndex + 1);
    EnsureVisible(state, page, 3);
}

void MenuBuildVisibleLines(const MenuState &state,
                           const MenuPage &page,
                           MenuLine *lines,
                           int maxLines,
                           int &outCount,
                           bool &titleSelected)
{
    outCount = 0;
    titleSelected = (state.selectedIndex == 0);

    const int maxItems = std::min(static_cast<int>(page.itemCount), maxLines);
    for (int i = 0; i < maxItems; ++i)
    {
        const int itemIndex = state.scrollIndex + i;
        if (itemIndex >= static_cast<int>(page.itemCount))
        {
            break;
        }

        const MenuItem &item = page.items[itemIndex];
        MenuLine &line = lines[outCount];
        line.label = item.label;
        line.type = item.type;
        line.selected = (state.selectedIndex - 1) == itemIndex;
        if (item.type == MenuItemType::Int && item.intValue)
        {
            line.intValue = *item.intValue;
        }
        else if (item.value)
        {
            line.value = *item.value;
        }
        ++outCount;
    }
}
