#include "neurotic_ui.h"

#include "daisy_seed.h"

#include <algorithm>

namespace
{
const char *kAlgoParamLabels[][2] = {
    {"Mass", "Asym"},   // 0 NCR
    {"Form", "Trans"},  // 1 LSB
    {"Head", "FB"},     // 2 NTH
    {"Dist", "Spin"},   // 3 BGM
    {"Artic", "Breath"},// 4 NFF
    {"Color", "Grain"}, // 5 NDM
    {"Glue", "Bias"},   // 6 NES
    {"Inharm", "Sparse"},// 7 NHC
    {"Swirl", "Tilt"},  // 8 NPL
    {"Drift", "Scatt"}, // 9 NMG
    {"Poles", "FB"},    // 10 NSM
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
    "Smear",
};
}

void NeuroticUi::Init(kxmx::Bluemchen &hw, NeuroticState &state)
{
    algoItems_[0] = {"Mix", MenuItemType::Percent, &state.mix, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[1] = {"Feed", MenuItemType::Percent, &state.fb, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[2] = {"Mod", MenuItemType::Percent, &state.lfoDepth, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[3] = {"Rate", MenuItemType::Hz, &state.lfoRate, nullptr, 0.0f, 1.0f, 0.0102041f};
    algoItems_[4] = {kAlgoParamLabels[0][0], MenuItemType::Percent, &state.c3, nullptr, 0.0f, 1.0f, 0.02f};
    algoItems_[5] = {kAlgoParamLabels[0][1], MenuItemType::Percent, &state.c4, nullptr, 0.0f, 1.0f, 0.02f};

    pages_[0] = {kAlgoNames[0], algoItems_, sizeof(algoItems_) / sizeof(algoItems_[0])};

    MenuInit(menuState_);
    menuState_.selectedIndex = 0;
    UpdateAlgoLabels(state);
    lastDisplayUpdateMs_ = daisy::System::GetNow();

    (void)hw;
}

void NeuroticUi::UpdateAlgoLabels(NeuroticState &state)
{
    const int clamped = std::clamp(state.algoIndex, 0, 10);
    algoItems_[4].label = kAlgoParamLabels[clamped][0];
    algoItems_[5].label = kAlgoParamLabels[clamped][1];
    pages_[0].title = kAlgoNames[clamped];

    if (clamped == 10)
    {
        smearPoles_ = std::clamp(2 + static_cast<int>(state.c3 * 126.0f + 0.5f), 2, 128);
        algoItems_[4].type = MenuItemType::Int;
        algoItems_[4].value = nullptr;
        algoItems_[4].intValue = &smearPoles_;
        algoItems_[4].min = 2.0f;
        algoItems_[4].max = 128.0f;
        algoItems_[4].step = 1.0f;
    }
    else
    {
        algoItems_[4].type = MenuItemType::Percent;
        algoItems_[4].value = &state.c3;
        algoItems_[4].intValue = nullptr;
        algoItems_[4].min = 0.0f;
        algoItems_[4].max = 1.0f;
        algoItems_[4].step = 0.02f;
    }

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
            const int next = (state.algoIndex + encInc + 11) % 11;
            state.algoIndex = next;
            UpdateAlgoLabels(state);
        }
        else
        {
            MenuRotate(menuState_, encInc, pages_, sizeof(pages_) / sizeof(pages_[0]));
        }
    }

    if (state.algoIndex == 10)
    {
        const int poles = std::clamp(smearPoles_, 2, 128);
        state.c3 = static_cast<float>(poles - 2) / 126.0f;
    }

    if (state.algoIndex != algoIndex_)
    {
        UpdateAlgoLabels(state);
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
