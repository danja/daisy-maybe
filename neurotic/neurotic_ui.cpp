#include "neurotic_ui.h"

#include "daisy_seed.h"

#include <algorithm>

namespace
{
const char *kAlgoParamLabels[][2] = {
    {"MASS", "ASYM"},    // 0 NCR
    {"FORM", "TRANS"},   // 1 LSB
    {"HEAD", "FDBK"},    // 2 NTH
    {"DIST", "SPIN"},    // 3 BGM
    {"ARTIC", "BREATH"}, // 4 NFF
    {"COLOR", "GRAIN"},  // 5 NDM
    {"GLUE", "BIAS"},    // 6 NES
    {"INHARM", "SPARSE"},// 7 NHC
    {"SWIRL", "TILT"},   // 8 NPL
    {"DRIFT", "SCATTR"}, // 9 NMG
};

const char *kAlgoNames[] = {
    "CrossRes",
    "Braid",
    "TapeHyd",
    "Binaural",
    "Formant",
    "Diffusion",
    "Energy",
    "Harmonic",
    "PhaseLoom",
    "MicroGran",
};
}

void NeuroticUi::Init(kxmx::Bluemchen &hw, NeuroticState &state)
{
    algoItems_[0] = {"MIX", MenuItemType::Percent, &state.mix, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[1] = {"LDEPTH", MenuItemType::Percent, &state.lfoDepth, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[2] = {"LRATE", MenuItemType::Percent, &state.lfoRate, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[3] = {"OUT", MenuItemType::Percent, &state.outTrim, nullptr, 0.0f, 1.5f, 0.02f};
    algoItems_[4] = {kAlgoParamLabels[0][0], MenuItemType::Percent, &state.c3, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[5] = {kAlgoParamLabels[0][1], MenuItemType::Percent, &state.c4, nullptr, 0.0f, 1.0f, 0.02f};

    pages_[0] = {kAlgoNames[0], algoItems_, sizeof(algoItems_) / sizeof(algoItems_[0])};

    MenuInit(menuState_);
    menuState_.selectedIndex = 0;
    UpdateAlgoLabels(state.algoIndex);
    lastDisplayUpdateMs_ = daisy::System::GetNow();

    (void)hw;
}

void NeuroticUi::UpdateAlgoLabels(int algoIndex)
{
    const int clamped = std::clamp(algoIndex, 0, 9);
    algoItems_[4].label = kAlgoParamLabels[clamped][0];
    algoItems_[5].label = kAlgoParamLabels[clamped][1];
    pages_[0].title = kAlgoNames[clamped];
    algoIndex_ = clamped;
}

void NeuroticUi::Update(kxmx::Bluemchen &hw, NeuroticState &state)
{
    hw.ProcessDigitalControls();
    const int encInc = hw.encoder.Increment();
    const EncoderPress press = UpdateEncoder(hw, encoderState_);

    if (press == EncoderPress::Short)
    {
        MenuPress(menuState_, pages_, sizeof(pages_) / sizeof(pages_[0]));
    }

    if (encInc != 0)
    {
        if (menuState_.selectedIndex == 0)
        {
            const int next = (state.algoIndex + encInc + 10) % 10;
            state.algoIndex = next;
            UpdateAlgoLabels(state.algoIndex);
        }
        else
        {
            MenuRotate(menuState_, encInc, pages_, sizeof(pages_) / sizeof(pages_[0]));
        }
    }

    if (state.algoIndex != algoIndex_)
    {
        UpdateAlgoLabels(state.algoIndex);
    }
}

void NeuroticUi::RenderIfNeeded(kxmx::Bluemchen &hw, const NeuroticState &state, bool heartbeatOn, uint32_t nowMs)
{
    (void)state;
    if (nowMs - lastDisplayUpdateMs_ < 33)
    {
        return;
    }

    const MenuPage &page = pages_[menuState_.pageIndex];
    DisplayData data;
    data.pageTitle = page.title;
    data.heartbeatOn = heartbeatOn;
    MenuBuildVisibleLines(menuState_, page, data.lines, 3, data.lineCount, data.titleSelected);
    RenderDisplay(hw, data);

    lastDisplayUpdateMs_ = nowMs;
}
