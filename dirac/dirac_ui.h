/* dirac_ui.h — menu navigation and 64x32 OLED rendering.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Interaction follows docs/menu-system.md: the encoder press walks the
 * selection down the page and wraps back to the title row; rotating on the
 * title row changes page; rotating on an item edits it. A long press swaps
 * between the menu and the grain field.
 *
 * The screen is 64x32 — with Font_6x8 that is 10 characters by 4 rows, so a
 * page shows its title plus three items and scrolls. */

#pragma once

#include <cstdint>

#include "dirac_engine.h"
#include "dirac_params.h"
#include "dirac_sample.h"
#include "kxmx_bluemchen.h"

namespace dirac
{

enum ItemKind : uint8_t
{
    MI_PARAM,
    MI_SETTING,
    MI_ACTION,
    MI_INFO,
};

enum ActionId : uint8_t
{
    A_FILE,   // rotate = choose, press = load
    A_DETACH, // press = back to live input
    A_RESCAN, // press = re-read the card
};

enum InfoId : uint8_t
{
    I_CPU,
    I_GRAINS,
    I_LEN,
    I_SD,
    I_VER,
};

struct MenuItem
{
    ItemKind kind;
    uint8_t id;
};

struct MenuPage
{
    const char *title;
    const MenuItem *items;
    uint8_t count;
};

/* Everything the UI needs to render and act on. */
struct UiContext
{
    ParamState *params = nullptr;
    SampleStore *store = nullptr;
    DiracEngine *engine = nullptr;
    float cpuLoad = 0.0f;
    int fileSel = 0;
    bool heartbeat = false;
    /* Set by the UI, consumed by the app. */
    bool requestLoad = false;
    bool requestDetach = false;
    bool requestRescan = false;
};

class DiracUi
{
  public:
    void Init();

    /* Encoder input, called from the main loop. */
    void OnRotate(int clicks, UiContext &ctx);
    void OnShortPress(UiContext &ctx);
    void OnLongPress(UiContext &ctx);

    void Render(kxmx::Bluemchen &hw, UiContext &ctx);

    /* The parameter under the cursor, or -1 — used as the CC learn target. */
    int SelectedParam(const UiContext &ctx) const;

    bool FieldView() const { return fieldView_; }

  private:
    int page_ = 0;
    int sel_ = 0;    // 0 = title row, 1..count = items
    int scroll_ = 0; // first visible item
    bool fieldView_ = false;
    bool fieldPosAxis_ = false; // X = read position instead of pan

    /* Grain-field phosphor: the panel is 1-bit, so a grain's decay is drawn as
     * a fading trail through an age buffer rather than as brightness. */
    static constexpr int kFieldW = 64;
    static constexpr int kFieldH = 24;
    uint8_t phosphor_[kFieldW][kFieldH] = {{0}};

    void RenderMenu(kxmx::Bluemchen &hw, UiContext &ctx);
    void RenderField(kxmx::Bluemchen &hw, UiContext &ctx);
    void FormatItem(const MenuItem &it, const UiContext &ctx, char *label,
                    int labelLen, char *value, int valueLen) const;
};

} // namespace dirac
