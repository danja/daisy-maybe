#include "neurotic_ui.h"

#include "daisy_seed.h"
#include "neurotic_config.h"

#include <algorithm>

namespace
{
constexpr int kAlgoCount = (kNeuroticEnableReverb ? (kNeuroticEnableEuDelay ? 15 : 14) : 13);
constexpr int kSmearIndex = 10;
constexpr int kReverbIndex = 13;
constexpr int kEuDelayIndex = 14;
constexpr int kSmearPolesItemIndex = 4;
constexpr int kReverbTapsItemIndex = 5;
constexpr int kEuDelayStepsItemIndex = 4;
constexpr int kEuDelayBpmItemIndex = 5;
constexpr int kEuDelayScaleItemIndex = 6;

const char *kAlgoParamLabels[][3] = {
    {"Damp", "Asym", "Res"},    // 0 NCR
    {"Form", "Trans", ""},      // 1 LSB
    {"Head", "FB", ""},         // 2 NTH
    {"Dist", "Spin", ""},       // 3 BGM
    {"Artic", "Breath", "Res"}, // 4 NFF
    {"Color", "Grain", ""},     // 5 NDM
    {"Glue", "Bias", ""},       // 6 NES
    {"Inharm", "Sparse", ""},   // 7 NHC
    {"Swirl", "Tilt", ""},      // 8 NPL
    {"Drift", "Scatt", ""},     // 9 NMG
    {"Poles", "FB", ""},        // 10 NSM
    {"Atk", "Dec", ""},         // 11 NCE
    {"Win", "Stereo", ""},      // 12 NPS
    {"Cross", "Taps", "Tilt"},  // 13 NRV
    {"Steps", "BPM", "Scale"},  // 14 EUD
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
    "CompExp",
    "PitchShift",
    "Reverb",
    "EuDelay",
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
    algoItems_[6] = {kAlgoParamLabels[0][2], MenuItemType::Percent, &state.c5, nullptr, 0.0f, 1.0f, 0.02f};

    pages_[0] = {kAlgoNames[0], algoItems_, sizeof(algoItems_) / sizeof(algoItems_[0])};

    MenuInit(menuState_);
    menuState_.selectedIndex = 0;
    UpdateAlgoLabels(state);
    lastDisplayUpdateMs_ = daisy::System::GetNow();

    (void)hw;
}

void NeuroticUi::UpdateAlgoLabels(NeuroticState &state)
{
    const int clamped = std::clamp(state.algoIndex, 0, kAlgoCount - 1);
    algoItems_[4].label = kAlgoParamLabels[clamped][0];
    algoItems_[5].label = kAlgoParamLabels[clamped][1];
    algoItems_[6].label = kAlgoParamLabels[clamped][2];
    pages_[0].title = kAlgoNames[clamped];
    pages_[0].itemCount = (kAlgoParamLabels[clamped][2][0] == '\0')
        ? (sizeof(algoItems_) / sizeof(algoItems_[0]) - 1)
        : (sizeof(algoItems_) / sizeof(algoItems_[0]));

    algoItems_[4].type = MenuItemType::Percent;
    algoItems_[4].value = &state.c3;
    algoItems_[4].intValue = nullptr;
    algoItems_[4].min = 0.0f;
    algoItems_[4].max = 1.0f;
    algoItems_[4].step = 0.02f;

    algoItems_[5].type = MenuItemType::Percent;
    algoItems_[5].value = &state.c4;
    algoItems_[5].intValue = nullptr;
    algoItems_[5].min = 0.0f;
    algoItems_[5].max = 1.0f;
    algoItems_[5].step = 0.02f;

    algoItems_[6].type = MenuItemType::Percent;
    algoItems_[6].value = &state.c5;
    algoItems_[6].intValue = nullptr;
    algoItems_[6].min = 0.0f;
    algoItems_[6].max = 1.0f;
    algoItems_[6].step = 0.02f;

    if (clamped == kSmearIndex)
    {
        if (menuState_.selectedIndex - 1 != kSmearPolesItemIndex)
        {
            smearPoles_ = std::clamp(2 + static_cast<int>(state.c3 * 126.0f + 0.5f), 2, 128);
        }
        algoItems_[4].type = MenuItemType::Int;
        algoItems_[4].value = nullptr;
        algoItems_[4].intValue = &smearPoles_;
        algoItems_[4].min = 2.0f;
        algoItems_[4].max = 128.0f;
        algoItems_[4].step = 1.0f;
    }

    if (kNeuroticEnableReverb && clamped == kReverbIndex)
    {
        reverbTaps_ = std::clamp(1 + static_cast<int>(state.c4 * 9.0f + 0.5f), 1, 10);
        algoItems_[5].type = MenuItemType::Int;
        algoItems_[5].value = nullptr;
        algoItems_[5].intValue = &reverbTaps_;
        algoItems_[5].min = 1.0f;
        algoItems_[5].max = 10.0f;
        algoItems_[5].step = 1.0f;
    }

    if (kNeuroticEnableEuDelay && clamped == kEuDelayIndex)
    {
        eudelayScale_ = 0.25f + state.c5 * 3.75f;
        if (menuState_.selectedIndex - 1 != kEuDelayStepsItemIndex)
        {
            eudelaySteps_ = std::clamp(2 + static_cast<int>(state.c3 * 14.0f + 0.5f), 2, 16);
        }
        if (menuState_.selectedIndex - 1 != kEuDelayBpmItemIndex)
        {
            eudelayBpm_ = std::clamp(80 + static_cast<int>(state.c4 * 120.0f + 0.5f), 80, 200);
        }
        algoItems_[4].type = MenuItemType::Int;
        algoItems_[4].value = nullptr;
        algoItems_[4].intValue = &eudelaySteps_;
        algoItems_[4].min = 2.0f;
        algoItems_[4].max = 16.0f;
        algoItems_[4].step = 1.0f;

        algoItems_[5].type = MenuItemType::Int;
        algoItems_[5].value = nullptr;
        algoItems_[5].intValue = &eudelayBpm_;
        algoItems_[5].min = 80.0f;
        algoItems_[5].max = 200.0f;
        algoItems_[5].step = 1.0f;

        algoItems_[6].type = MenuItemType::Ratio;
        algoItems_[6].value = &eudelayScale_;
        algoItems_[6].intValue = nullptr;
        algoItems_[6].min = 0.25f;
        algoItems_[6].max = 4.0f;
        algoItems_[6].step = 0.05f;
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
            const int next = (state.algoIndex + encInc + kAlgoCount) % kAlgoCount;
            state.algoIndex = next;
            UpdateAlgoLabels(state);
        }
        else
        {
            MenuRotate(menuState_, encInc, pages_, sizeof(pages_) / sizeof(pages_[0]));
        }
    }

    if (state.algoIndex == kSmearIndex)
    {
        const int poles = std::clamp(smearPoles_, 2, 128);
        state.c3 = static_cast<float>(poles - 2) / 126.0f;
    }

    if (kNeuroticEnableReverb && state.algoIndex == kReverbIndex)
    {
        algoItems_[5].type = MenuItemType::Int;
        algoItems_[5].value = nullptr;
        algoItems_[5].intValue = &reverbTaps_;
        algoItems_[5].min = 1.0f;
        algoItems_[5].max = 10.0f;
        algoItems_[5].step = 1.0f;

        if (menuState_.selectedIndex - 1 != kReverbTapsItemIndex)
        {
            reverbTaps_ = std::clamp(1 + static_cast<int>(state.c4 * 9.0f + 0.5f), 1, 10);
        }
        const int taps = std::clamp(reverbTaps_, 1, 10);
        state.c4 = static_cast<float>(taps - 1) / 9.0f;
    }

    if (kNeuroticEnableEuDelay && state.algoIndex == kEuDelayIndex)
    {
        algoItems_[6].type = MenuItemType::Ratio;
        algoItems_[6].value = &eudelayScale_;
        algoItems_[6].intValue = nullptr;
        algoItems_[6].min = 0.25f;
        algoItems_[6].max = 4.0f;
        algoItems_[6].step = 0.05f;

        if (menuState_.selectedIndex - 1 != kEuDelayStepsItemIndex)
        {
            eudelaySteps_ = std::clamp(2 + static_cast<int>(state.c3 * 14.0f + 0.5f), 2, 16);
        }
        if (menuState_.selectedIndex - 1 != kEuDelayBpmItemIndex)
        {
            eudelayBpm_ = std::clamp(80 + static_cast<int>(state.c4 * 120.0f + 0.5f), 80, 200);
        }
        if (menuState_.selectedIndex - 1 != kEuDelayScaleItemIndex)
        {
            eudelayScale_ = 0.25f + state.c5 * 3.75f;
        }
        const int steps = std::clamp(eudelaySteps_, 2, 16);
        const int bpm = std::clamp(eudelayBpm_, 80, 200);
        state.c3 = static_cast<float>(steps - 2) / 14.0f;
        state.c4 = static_cast<float>(bpm - 80) / 120.0f;
        const float scale = std::clamp(eudelayScale_, 0.25f, 4.0f);
        state.c5 = (scale - 0.25f) / 3.75f;
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
    data.pageTitle = kNeuroticForceMute ? "MUTE" : page.title;
    data.heartbeatOn = heartbeatOn;
    MenuBuildVisibleLines(menuState_, page, data.lines, 3, data.lineCount, data.titleSelected);
    RenderDisplay(hw, data);

    lastDisplayUpdateMs_ = nowMs;
}
