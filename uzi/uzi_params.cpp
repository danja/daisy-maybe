#include "uzi_params.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kNotchMin = 0.01f;
constexpr float kNotchMax = 3.0f;
}

float UziParams::MapExpo(float value, float minVal, float maxVal) const
{
    value = std::clamp(value, 0.0f, 1.0f);
    return minVal * powf(maxVal / minVal, value);
}

void UziParams::Update(kxmx::Bluemchen &hw, const UziState &state, UziRuntime &runtime)
{
    const float pot1 = hw.GetKnobValue(kxmx::Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(kxmx::Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(kxmx::Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(kxmx::Bluemchen::CTRL_4);
    runtime.rawK1 = hw.controls[kxmx::Bluemchen::CTRL_1].GetRawValue();
    runtime.rawK2 = hw.controls[kxmx::Bluemchen::CTRL_2].GetRawValue();
    runtime.rawCv1 = hw.controls[kxmx::Bluemchen::CTRL_3].GetRawValue();
    runtime.rawCv2 = hw.controls[kxmx::Bluemchen::CTRL_4].GetRawValue();

    const float pot1Bipolar = (pot1 - 0.5f) * 2.0f;
    const float pot2Bipolar = (pot2 - 0.5f) * 2.0f;
    const float cv1Bipolar = (cv1 - 0.5f) * 2.0f;
    const float cv2Bipolar = (cv2 - 0.5f) * 2.0f;

    const float notchControl = std::clamp(0.5f + 0.5f * (pot1Bipolar + cv1Bipolar), 0.0f, 1.0f);
    const float phaseControl = std::clamp(0.5f + 0.5f * (pot2Bipolar + cv2Bipolar), 0.0f, 1.0f);

    runtime.mix = state.mix;
    runtime.xmix = state.xmix;
    runtime.lfoDepth = state.lfoDepth;
    runtime.lfoFreq = state.lfoFreq;
    runtime.wave = state.wave;
    runtime.overdrive = state.overdrive;
    runtime.crossover = state.crossover;
    runtime.blur = state.blur;
    runtime.binRounding = state.binRounding;
    runtime.blockSize = std::clamp(state.blockSize, 0, 2);

    runtime.notchDistance = MapExpo(notchControl, kNotchMin, kNotchMax) * 2.0f;
    runtime.phaseOffset = (phaseControl * 2.0f - 1.0f) * 4.0f;
}
