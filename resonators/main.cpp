#include <algorithm>
#include <cmath>

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"
#include "util/PersistentStorage.h"

#include "delay_lines.h"
#include "display.h"
#include "distortion.h"
#include "encoder_handler.h"
#include "filters.h"
#include "menu_system.h"

using namespace daisy;
using namespace kxmx;

namespace
{
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

    constexpr float kMinFreq = 10.0f;
    constexpr float kMaxFreq = 8000.0f;
    constexpr float kMaxFeed = 0.99f;
    constexpr float kCalibTone = 440.0f;
    constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);

    struct CalibSettings
    {
        float scale;
        float offset;

        bool operator!=(const CalibSettings &rhs) const
        {
            return scale != rhs.scale || offset != rhs.offset;
        }
    };

    struct MasterParams
    {
        float waveMix = 0.0f;
        float resonatorMix = 1.0f;
        float feedXX = 0.7f;
        float feedYY = 0.7f;
        float feedXY = 0.0f;
        float feedYX = 0.0f;
    };

    struct DistortionParams
    {
        int folds = 2;
        float overdrive = 0.0f;
    };

    struct ResonatorParams
    {
        float ratio = 1.0f;
        float dampX = 0.0f;
        float dampY = 0.0f;
    };

    float MapExpo(float value, float minVal, float maxVal)
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return minVal * powf(maxVal / minVal, value);
    }
} // namespace

Bluemchen hw;
DelayLinePair delays;
FeedFilters feedFilters;
DistortionChannel distortionX;
DistortionChannel distortionY;
EncoderState encoderState;
MenuState menuState;

MasterParams masterParams;
DistortionParams distortionParams;
ResonatorParams resonatorParams;

MenuItem masterItems[] = {
    {"DMIX", MenuItemType::Percent, &masterParams.waveMix, nullptr, 0.0f, 1.0f, 0.02f},
    {"RMIX", MenuItemType::Percent, &masterParams.resonatorMix, nullptr, 0.0f, 1.0f, 0.02f},
    {"FXX", MenuItemType::Percent, &masterParams.feedXX, nullptr, 0.0f, kMaxFeed, 0.02f},
    {"FYY", MenuItemType::Percent, &masterParams.feedYY, nullptr, 0.0f, kMaxFeed, 0.02f},
    {"FXY", MenuItemType::Percent, &masterParams.feedXY, nullptr, 0.0f, kMaxFeed, 0.02f},
    {"FYX", MenuItemType::Percent, &masterParams.feedYX, nullptr, 0.0f, kMaxFeed, 0.02f},
};

MenuItem distortionItems[] = {
    {"FOLD", MenuItemType::Int, nullptr, &distortionParams.folds, 1.0f, 5.0f, 1.0f},
    {"ODRV", MenuItemType::Percent, &distortionParams.overdrive, nullptr, 0.0f, 1.0f, 0.02f},
};

MenuItem resonatorItems[] = {
    {"RAT", MenuItemType::Ratio, &resonatorParams.ratio, nullptr, 0.25f, 4.0f, 0.01f},
    {"DMX", MenuItemType::Percent, &resonatorParams.dampX, nullptr, 0.0f, 1.0f, 0.02f},
    {"DMY", MenuItemType::Percent, &resonatorParams.dampY, nullptr, 0.0f, 1.0f, 0.02f},
};

MenuPage menuPages[] = {
    {"Master", masterItems, sizeof(masterItems) / sizeof(masterItems[0])},
    {"Dist", distortionItems, sizeof(distortionItems) / sizeof(distortionItems[0])},
    {"Res", resonatorItems, sizeof(resonatorItems) / sizeof(resonatorItems[0])},
};

constexpr size_t kMenuPageCount = sizeof(menuPages) / sizeof(menuPages[0]);

float currentFreq = 440.0f;
float currentFreq2 = 440.0f;
float pitchScale = 1.0f;
float pitchOffset = 0.0f;
float waveDepth = 0.0f;

bool calibMode = false;
uint32_t lastCalibChangeMs = 0;
bool calibDirty = false;
bool calibSavePending = false;
bool showSaveConfirm = false;
uint32_t saveConfirmUntilMs = 0;
CalibSettings savedCalib = {1.0f, 0.0f};
PersistentStorage<CalibSettings> *calibStorage = nullptr;

float calibPhase = 0.0f;
float sampleRate = 48000.0f;
bool heartbeatOn = false;
uint32_t lastHeartbeatMs = 0;
bool ledOn = false;
uint32_t lastLedMs = 0;

void UpdateControls()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    if (calibMode)
    {
        pitchScale = 0.8f + pot1 * 0.4f;
        pitchOffset = (pot2 - 0.5f) * 2.0f;
        const float cvOct = (cv1 - 0.5f) * 10.0f;
        const float base = kCalibTone * powf(2.0f, pitchOffset);
        currentFreq = base * powf(2.0f, cvOct * 0.5f * pitchScale);
        currentFreq2 = currentFreq;

        const uint32_t now = System::GetNow();
        if (fabsf(pitchScale - savedCalib.scale) > 0.0005f || fabsf(pitchOffset - savedCalib.offset) > 0.005f)
        {
            calibDirty = true;
            lastCalibChangeMs = now;
        }
    }
    else
    {
        const float baseFreq = MapExpo(pot1, kMinFreq, kMaxFreq);
        const float cvOct = cv1 * 5.0f * pitchScale;
        const float pitchMultiplier = powf(2.0f, pitchOffset + cvOct);
        currentFreq = std::clamp(baseFreq * pitchMultiplier, kMinFreq, kMaxFreq);
        currentFreq2 = std::clamp(currentFreq * resonatorParams.ratio, kMinFreq, kMaxFreq);
        waveDepth = std::clamp(pot2 + (cv2 - 0.5f), 0.0f, 1.0f);
    }

    const int encInc = hw.encoder.Increment();
    const EncoderPress press = UpdateEncoder(hw, encoderState);

    if (press == EncoderPress::Long)
    {
        if (calibMode)
        {
            calibSavePending = true;
            calibDirty = true;
        }
        calibMode = !calibMode;
    }

    if (!calibMode)
    {
        if (press == EncoderPress::Short)
        {
            MenuPress(menuState, menuPages, kMenuPageCount);
        }
        if (encInc != 0)
        {
            MenuRotate(menuState, encInc, menuPages, kMenuPageCount);
        }
    }

    feedFilters.SetDampX(resonatorParams.dampX);
    feedFilters.SetDampY(resonatorParams.dampY);
}

void HandleCalibrationSave()
{
    if (!calibStorage)
    {
        return;
    }

    const uint32_t now = System::GetNow();
    if (calibDirty && (now - lastCalibChangeMs) > 1000)
    {
        calibSavePending = true;
    }

    if (calibSavePending)
    {
        calibStorage->GetSettings().scale = pitchScale;
        calibStorage->GetSettings().offset = pitchOffset;
        calibStorage->Save();
        savedCalib = calibStorage->GetSettings();
        calibDirty = false;
        calibSavePending = false;
        showSaveConfirm = true;
        saveConfirmUntilMs = now + 800;
    }
}

DisplayData BuildDisplayData()
{
    DisplayData data;
    data.isCalib = calibMode;
    data.pitchScale = pitchScale;
    data.pitchOffset = pitchOffset;
    data.currentFreq = currentFreq;
    data.showSaveConfirm = showSaveConfirm;
    data.heartbeatOn = heartbeatOn;

    if (!calibMode)
    {
        const MenuPage &page = menuPages[menuState.pageIndex];
        data.pageTitle = page.title;
        MenuBuildVisibleLines(menuState, page, data.lines, 3, data.lineCount, data.titleSelected);
    }

    return data;
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    const float delaySamples1 = sampleRate / std::max(currentFreq, 1.0f);
    const float delaySamples2 = sampleRate / std::max(currentFreq2, 1.0f);

    delays.SetDelayTimes(delaySamples1, delaySamples2);

    const float waveMix = masterParams.waveMix;
    const float resMix = masterParams.resonatorMix;
    const float dryMix = 1.0f - resMix;
    const float feedXX = masterParams.feedXX;
    const float feedYY = masterParams.feedYY;
    const float feedXY = masterParams.feedXY;
    const float feedYX = masterParams.feedYX;

    DistortionSettings distSettings{waveDepth, distortionParams.folds, distortionParams.overdrive};

    float inPeakX = 0.0f;
    float inPeakY = 0.0f;
    float outPeakX = 0.0f;
    float outPeakY = 0.0f;

    for (size_t i = 0; i < size; i++)
    {
        if (calibMode)
        {
            const float toneFreq = std::clamp(currentFreq, 20.0f, 8000.0f);
            calibPhase += toneFreq / sampleRate;
            if (calibPhase >= 1.0f)
                calibPhase -= 1.0f;
            const float tone = std::sin(calibPhase * kTwoPi) * 0.5f;
            out[0][i] = tone;
            out[1][i] = tone;
            continue;
        }

        const float inX = SoftClipSample(in[0][i]);
        const float inY = SoftClipSample(in[1][i]);

        const float resX = delays.Read1();
        const float resY = delays.Read2();

        const float filteredX = feedFilters.ProcessX(resX);
        const float filteredY = feedFilters.ProcessY(resY);

        // Feed routing happens before the distortion stage.
        const float preDistX = inX + filteredX * feedXX + filteredY * feedYX;
        const float preDistY = inY + filteredY * feedYY + filteredX * feedXY;

        const float distX = distortionX.ProcessSample(preDistX, distSettings, inPeakX, outPeakX);
        const float distY = distortionY.ProcessSample(preDistY, distSettings, inPeakY, outPeakY);

        // Blend dry and folded/overdriven signals before the resonators.
        const float driveX = preDistX + (distX - preDistX) * waveMix;
        const float driveY = preDistY + (distY - preDistY) * waveMix;

        delays.Write1(SoftClipSample(driveX));
        delays.Write2(SoftClipSample(driveY));

        out[0][i] = dryMix * inX + resMix * resX;
        out[1][i] = dryMix * inY + resMix * resY;
    }

    if (!calibMode)
    {
        distortionX.UpdateMakeup(inPeakX, outPeakX);
        distortionY.UpdateMakeup(inPeakY, outPeakY);
    }
}

int main(void)
{
    hw.Init();
    hw.StartAdc();

    sampleRate = hw.AudioSampleRate();

    delays.Init();
    feedFilters.Init();
    feedFilters.SetDampX(resonatorParams.dampX);
    feedFilters.SetDampY(resonatorParams.dampY);

    distortionX.Reset();
    distortionY.Reset();

    MenuInit(menuState);

    CalibSettings defaults{1.0f, 0.0f};
    static PersistentStorage<CalibSettings> storage(hw.seed.qspi);
    storage.Init(defaults);
    calibStorage = &storage;
    savedCalib = storage.GetSettings();
    pitchScale = savedCalib.scale;
    pitchOffset = savedCalib.offset;

    hw.display.Fill(false);
    hw.display.SetCursor(0, 0);
    hw.display.WriteString("Resonators", Font_6x8, true);
    hw.display.SetCursor(0, 12);
    hw.display.WriteString("Booting...", Font_6x8, true);
    hw.display.Update();

    hw.StartAudio(AudioCallback);

    uint32_t lastDisplayUpdate = 0;
    while (1)
    {
        UpdateControls();
        HandleCalibrationSave();

        const uint32_t now = System::GetNow();
        if (now - lastHeartbeatMs > 250)
        {
            heartbeatOn = !heartbeatOn;
            lastHeartbeatMs = now;
        }
        if (now - lastLedMs > 250)
        {
            ledOn = !ledOn;
            hw.seed.SetLed(ledOn);
            lastLedMs = now;
        }
        if (now - lastDisplayUpdate > 33)
        {
            if (showSaveConfirm && now > saveConfirmUntilMs)
            {
                showSaveConfirm = false;
            }
            RenderDisplay(hw, BuildDisplayData());
            lastDisplayUpdate = now;
        }
    }
}
