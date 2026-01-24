#include "uzi_ui.h"

#include "daisy_seed.h"

#include <algorithm>

void UziUi::Init(kxmx::Bluemchen &hw, UziState &state)
{
    cutoffHzInt_ = static_cast<int>(state.cutoffHz + 0.5f);
    masterItems_[0] = {"MIX", MenuItemType::Percent, &state.mix, nullptr, 0.0f, 1.0f, 0.02f};
    masterItems_[1] = {"FBK", MenuItemType::Percent, &state.feedback, nullptr, 0.0f, 1.0f, 0.02f};
    masterItems_[2] = {"X", MenuItemType::Percent, &state.xmix, nullptr, 0.0f, 1.0f, 0.02f};
    masterItems_[3] = {"CUT", MenuItemType::Int, nullptr, &cutoffHzInt_, 0.0f, 300.0f, 20.0f};
    masterItems_[4] = {"LFD", MenuItemType::Percent, &state.lfoDepth, nullptr, 0.0f, 1.0f, 0.02f};
    masterItems_[5] = {"LFR", MenuItemType::Percent, &state.lfoFreq, nullptr, 0.0f, 1.0f, 0.02f};

    distortionItems_[0] = {"WAVE", MenuItemType::Percent, &state.wave, nullptr, 0.0f, 1.0f, 0.02f};
    distortionItems_[1] = {"ODRV", MenuItemType::Percent, &state.overdrive, nullptr, 0.0f, 1.0f, 0.02f};

    fftItems_[0] = {"XOVR", MenuItemType::Percent, &state.crossover, nullptr, 0.0f, 1.0f, 0.02f};
    fftItems_[1] = {"BLUR", MenuItemType::Percent, &state.blur, nullptr, 0.0f, 1.0f, 0.02f};
    fftItems_[2] = {"BINS", MenuItemType::Percent, &state.binRounding, nullptr, 0.0f, 1.0f, 0.02f};
    fftItems_[3] = {"BLK", MenuItemType::Int, nullptr, &state.blockSize, 0.0f, 2.0f, 1.0f};

    pages_[0] = {"Master", masterItems_, sizeof(masterItems_) / sizeof(masterItems_[0])};
    pages_[1] = {"Dist", distortionItems_, sizeof(distortionItems_) / sizeof(distortionItems_[0])};
    pages_[2] = {"FFT", fftItems_, sizeof(fftItems_) / sizeof(fftItems_[0])};

    MenuInit(menuState_);
    lastDisplayUpdateMs_ = daisy::System::GetNow();

    (void)hw;
}

void UziUi::Update(kxmx::Bluemchen &hw, UziState &state)
{
    const int encInc = hw.encoder.Increment();
    const EncoderPress press = UpdateEncoder(hw, encoderState_);

    if (press == EncoderPress::Short)
    {
        MenuPress(menuState_, pages_, sizeof(pages_) / sizeof(pages_[0]));
    }

    if (encInc != 0)
    {
        MenuRotate(menuState_, encInc, pages_, sizeof(pages_) / sizeof(pages_[0]));
    }

    state.cutoffHz = std::clamp(static_cast<float>(cutoffHzInt_), 0.0f, 300.0f);
}

void UziUi::RenderIfNeeded(kxmx::Bluemchen &hw, const UziState &state, const UziRuntime &runtime, bool heartbeatOn, uint32_t nowMs)
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
    if (menuState_.pageIndex >= 3)
    {
        data.debug = true;
        data.debugPage = menuState_.pageIndex - 3;
        data.rawK1 = runtime.rawK1;
        data.rawK2 = runtime.rawK2;
        data.rawCv1 = runtime.rawCv1;
        data.rawCv2 = runtime.rawCv2;
        data.notchDistance = runtime.notchDistance;
        data.phaseOffset = runtime.phaseOffset;
    }
    RenderDisplay(hw, data);

    lastDisplayUpdateMs_ = nowMs;
}
