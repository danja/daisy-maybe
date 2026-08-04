/* dirac_ui.cpp — menu navigation and 64x32 OLED rendering.
 * SPDX-License-Identifier: Apache-2.0 */

#include "dirac_ui.h"

#include <cstring>

#include "dirac_fmt.h"

namespace dirac
{

namespace
{
#define P(x) MenuItem{MI_PARAM, x}
#define S(x) MenuItem{MI_SETTING, x}
#define A(x) MenuItem{MI_ACTION, x}
#define I(x) MenuItem{MI_INFO, x}

const MenuItem kGrain[] = {P(P_DENS), P(P_GLEN), P(P_GRAINS), P(P_PLAYH),
                           P(P_SPEED)};
const MenuItem kPitch[] = {P(P_SEMI), P(P_VOCT), P(P_PSPRD), P(P_SCALE),
                           P(P_DETUNE)};
const MenuItem kShape[] = {P(P_WINDOW), P(P_TEXTR), P(P_TILT), P(P_TSPRD)};
const MenuItem kSpace[] = {P(P_SPRD), P(P_BINAU), P(P_PJTR), P(P_REV),
                           P(P_COLOR)};
const MenuItem kFdbk[] = {P(P_FDBK), P(P_MIX), P(P_LEVEL), P(P_COMP),
                          P(P_DIFFUSE)};
const MenuItem kHeads[] = {P(P_MULTI), P(P_HEADS), P(P_HSPRD), P(P_HTUNE),
                           P(P_SNAP)};
/* Order matters: an action fires on press and then advances, so nothing
 * destructive may sit directly below another action. `file` (load) is followed
 * by plain parameters — otherwise two presses in a row would load a sample and
 * then instantly detach it. `detc` -> `scan` is fine: rescanning is harmless. */
const MenuItem kSmpl[] = {A(A_FILE),  P(P_FREEZE), P(P_HOLD), A(A_DETACH),
                          A(A_RESCAN), I(I_LEN),   I(I_SD)};
const MenuItem kMod[] = {S(S_K1_DEST),  S(S_K2_DEST),  S(S_CV1_DEST),
                         S(S_CV1_DEPTH), S(S_CV2_DEST), S(S_CV2_DEPTH),
                         S(S_MOD_DEST), S(S_MOD_DEPTH)};
const MenuItem kMidi[] = {S(S_MIDI_CH), S(S_NOTE_MODE), S(S_CLK_DIV),
                          S(S_CC_LEARN)};
const MenuItem kIo[] = {S(S_INR_MODE), S(S_OUT_MODE), S(S_CAL_SCALE),
                        S(S_CAL_OFS)};
const MenuItem kSys[] = {I(I_CPU), I(I_GRAINS), I(I_LEN), I(I_VER)};

const MenuPage kPages[] = {
    {"GRAIN", kGrain, 5}, {"PITCH", kPitch, 5}, {"SHAPE", kShape, 4},
    {"SPACE", kSpace, 5}, {"FDBK", kFdbk, 5},   {"HEADS", kHeads, 5},
    {"SMPL", kSmpl, 7},   {"MOD", kMod, 8},     {"MIDI", kMidi, 4},
    {"IO", kIo, 4},       {"SYS", kSys, 4},
};
constexpr int kPageCount = int(sizeof(kPages) / sizeof(kPages[0]));
constexpr int kVisibleRows = 3;

#undef P
#undef S
#undef A
#undef I

inline int Clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}
} // namespace

void DiracUi::Init()
{
    page_ = 0;
    sel_ = 0;
    scroll_ = 0;
    memset(phosphor_, 0, sizeof(phosphor_));
}

int DiracUi::SelectedParam(const UiContext &ctx) const
{
    (void)ctx;
    if(sel_ <= 0)
        return -1;
    const MenuPage &pg = kPages[page_];
    if(sel_ > pg.count)
        return -1;
    const MenuItem &it = pg.items[sel_ - 1];
    return (it.kind == MI_PARAM) ? int(it.id) : -1;
}

void DiracUi::OnRotate(int clicks, UiContext &ctx)
{
    if(clicks == 0)
        return;

    if(fieldView_)
    {
        /* In the field, the encoder still edits whatever the menu had
         * selected — so a cloud can be shaped while watching it. */
        if(sel_ > 0 && sel_ <= kPages[page_].count)
        {
            const MenuItem &it = kPages[page_].items[sel_ - 1];
            if(it.kind == MI_PARAM)
                ctx.params->Step(it.id, clicks);
            else if(it.kind == MI_SETTING)
                ctx.params->StepSetting(it.id, clicks);
        }
        return;
    }

    if(sel_ == 0)
    {
        /* Title row: rotate through pages. */
        page_ = (page_ + clicks) % kPageCount;
        if(page_ < 0)
            page_ += kPageCount;
        scroll_ = 0;
        return;
    }

    const MenuPage &pg = kPages[page_];
    const MenuItem &it = pg.items[sel_ - 1];
    switch(it.kind)
    {
        case MI_PARAM: ctx.params->Step(it.id, clicks); break;
        case MI_SETTING: ctx.params->StepSetting(it.id, clicks); break;
        case MI_ACTION:
            if(it.id == A_FILE && ctx.store->Count() > 0)
                ctx.fileSel
                    = Clampi(ctx.fileSel + clicks, 0, ctx.store->Count() - 1);
            break;
        case MI_INFO: break;
    }
}

void DiracUi::OnShortPress(UiContext &ctx)
{
    if(fieldView_)
    {
        fieldPosAxis_ = !fieldPosAxis_; // flip the X axis: pan <-> read position
        memset(phosphor_, 0, sizeof(phosphor_));
        return;
    }

    /* An action row fires on press AND still advances the selection. Firing
     * without advancing would strand the cursor: press is the only way down a
     * page, so an action row that swallowed the press would make every row
     * below it unreachable. The SMPL page is ordered so that the row landed on
     * after an action is never itself destructive. */
    if(sel_ > 0 && sel_ <= kPages[page_].count)
    {
        const MenuItem &it = kPages[page_].items[sel_ - 1];
        if(it.kind == MI_ACTION)
        {
            switch(it.id)
            {
                case A_FILE: ctx.requestLoad = true; break;
                case A_DETACH: ctx.requestDetach = true; break;
                case A_RESCAN: ctx.requestRescan = true; break;
            }
        }
    }

    sel_++;
    if(sel_ > kPages[page_].count)
        sel_ = 0; // wrap back to the title
    /* Keep the selection inside the three visible rows. */
    const int item = sel_ - 1;
    if(sel_ == 0)
        scroll_ = 0;
    else if(item < scroll_)
        scroll_ = item;
    else if(item >= scroll_ + kVisibleRows)
        scroll_ = item - kVisibleRows + 1;
}

void DiracUi::OnLongPress(UiContext &ctx)
{
    (void)ctx;
    fieldView_ = !fieldView_;
    memset(phosphor_, 0, sizeof(phosphor_));
}

void DiracUi::FormatItem(const MenuItem &it, const UiContext &ctx, char *label,
                         int labelLen, char *value, int valueLen) const
{
    switch(it.kind)
    {
        case MI_PARAM:
            fmt::Str(label, labelLen, kParams[it.id].name);
            ctx.params->FormatValue(it.id, value, valueLen);
            break;
        case MI_SETTING:
            fmt::Str(label, labelLen, kSettings[it.id].name);
            ctx.params->FormatSetting(it.id, value, valueLen);
            break;
        case MI_ACTION:
            switch(it.id)
            {
                case A_FILE:
                    fmt::Str(label, labelLen, "file");
                    /* Only the leading characters of a name fit in the value
                     * field; the full name is not shown anywhere. */
                    fmt::Str(value, valueLen, ctx.store->Name(ctx.fileSel));
                    break;
                case A_DETACH:
                    fmt::Str(label, labelLen, "detc");
                    fmt::Str(value, valueLen,
                             ctx.engine->IsSampleMode() ? "smpl" : "live");
                    break;
                case A_RESCAN:
                    fmt::Str(label, labelLen, "scan");
                    fmt::Int(value, valueLen, ctx.store->Count());
                    break;
            }
            break;
        case MI_INFO:
            switch(it.id)
            {
                case I_CPU:
                {
                    fmt::Str(label, labelLen, "cpu");
                    const int n = fmt::Int(value, valueLen,
                                           int(ctx.cpuLoad * 100.0f + 0.5f));
                    if(n < valueLen - 1)
                    {
                        value[n] = '%';
                        value[n + 1] = '\0';
                    }
                    break;
                }
                case I_GRAINS:
                    fmt::Str(label, labelLen, "grn");
                    fmt::Int(value, valueLen, ctx.engine->ActiveGrains());
                    break;
                case I_LEN:
                    fmt::Str(label, labelLen, "len");
                    /* Loaded length in tenths of a second. */
                    fmt::Int(value, valueLen,
                             int(ctx.store->LoadedFrames() / 4800u));
                    break;
                case I_SD:
                    fmt::Str(label, labelLen, "sd");
                    fmt::Str(value, valueLen, ctx.store->Status());
                    break;
                case I_VER:
                    fmt::Str(label, labelLen, "ver");
                    fmt::Str(value, valueLen, "0.1");
                    break;
            }
            break;
    }
}

void DiracUi::RenderMenu(kxmx::Bluemchen &hw, UiContext &ctx)
{
    /* Ten columns of Font_6x8. Every row is assembled by hand into a fixed
     * layout: marker, 4-char label, 5-char right-aligned value. */
    char line[12], label[10], value[10];
    const MenuPage &pg = kPages[page_];

    line[0] = (sel_ == 0) ? '*' : ' ';
    fmt::Field(line + 1, 5, pg.title, false);
    line[6] = ' ';
    line[7] = ctx.engine->IsSampleMode() ? 'S' : ' ';
    line[8] = ' ';
    line[9] = ctx.heartbeat ? '.' : ' ';
    line[10] = '\0';
    hw.display.SetCursor(0, 0);
    hw.display.WriteString(line, Font_6x8, true);

    for(int row = 0; row < kVisibleRows; ++row)
    {
        const int idx = scroll_ + row;
        if(idx >= pg.count)
            break;
        FormatItem(pg.items[idx], ctx, label, sizeof(label), value,
                   sizeof(value));
        line[0] = (sel_ == idx + 1) ? '*' : ' ';
        fmt::Field(line + 1, 4, label, false);
        fmt::Field(line + 5, 5, value, true);
        line[10] = '\0';
        hw.display.SetCursor(0, 8 + row * 8);
        hw.display.WriteString(line, Font_6x8, true);
    }
}

void DiracUi::RenderField(kxmx::Bluemchen &hw, UiContext &ctx)
{
    /* Header: which axis the X position means, and the live grain count. */
    char line[12], num[6];
    fmt::Field(line, 4, fieldPosAxis_ ? "pos" : "pan", false);
    fmt::Int(num, sizeof(num), ctx.engine->ActiveGrains());
    fmt::Field(line + 4, 3, num, true);
    line[7] = ' ';
    line[8] = ctx.heartbeat ? '.' : ' ';
    line[9] = '\0';
    hw.display.SetCursor(0, 0);
    hw.display.WriteString(line, Font_6x8, true);

    /* Phosphor decay. On a 1-bit panel a grain's envelope cannot modulate
     * brightness, so it becomes persistence instead: a bright grain writes a
     * high age value and lingers as it fades. */
    for(int x = 0; x < kFieldW; ++x)
        for(int y = 0; y < kFieldH; ++y)
            if(phosphor_[x][y] > 0)
                phosphor_[x][y] = uint8_t(phosphor_[x][y] > 24
                                              ? phosphor_[x][y] - 24
                                              : 0);

    DiracEngine &eng = *ctx.engine;
    for(int i = 0; i < eng.GetGrainCount(); ++i)
    {
        if(!eng.IsGrainActive(i))
            continue;

        /* X: stereo position, or where the grain is reading in the source. */
        float nx;
        if(fieldPosAxis_)
            nx = eng.GetGrainReadPos01(i);
        else
            nx = 0.5f * (eng.GetGrainPan(i) + 1.0f);
        int x = int(nx * float(kFieldW - 1) + 0.5f);
        x = Clampi(x, 0, kFieldW - 1);

        /* Y: pitch, +/-24 semitones across the field. */
        const float semis = eng.GetGrainPitch(i);
        int y = int(float(kFieldH - 1) * (0.5f - semis / 48.0f) + 0.5f);
        y = Clampi(y, 0, kFieldH - 1);

        /* Loudness -> persistence. */
        const float amp = eng.GetGrainEnvelope(i) * eng.GetGrainGain(i);
        const int bright = Clampi(int(64.0f + amp * 400.0f), 64, 255);
        if(phosphor_[x][y] < bright)
            phosphor_[x][y] = uint8_t(bright);

        /* Grain length is the stem height: short grains are bare points, long
         * grains stand tall. Reverse grains hang the stem the other way. */
        const int stem = Clampi(eng.GetGrainDuration(i) / 6000, 0, 6);
        const int dir = eng.GetGrainReverse(i) ? -1 : 1;
        for(int s = 1; s <= stem; ++s)
        {
            const int yy = y + dir * s;
            if(yy < 0 || yy >= kFieldH)
                break;
            if(phosphor_[x][yy] < bright / 2)
                phosphor_[x][yy] = uint8_t(bright / 2);
        }
    }

    for(int x = 0; x < kFieldW; ++x)
        for(int y = 0; y < kFieldH; ++y)
            if(phosphor_[x][y] > 96)
                hw.display.DrawPixel(uint_fast8_t(x), uint_fast8_t(8 + y), true);
}

void DiracUi::Render(kxmx::Bluemchen &hw, UiContext &ctx)
{
    hw.display.Fill(false);
    if(fieldView_)
        RenderField(hw, ctx);
    else
        RenderMenu(hw, ctx);
    hw.display.Update();
}

} // namespace dirac
