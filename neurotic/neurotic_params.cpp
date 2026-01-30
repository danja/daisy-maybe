#include "neurotic_params.h"

#include <algorithm>

void NeuroticParams::Update(kxmx::Bluemchen &hw, const NeuroticState &state, NeuroticRuntime &runtime)
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

    const float c1 = std::clamp(0.5f + 0.5f * (pot1Bipolar + cv1Bipolar), 0.0f, 1.0f);
    const float c2 = std::clamp(0.5f + 0.5f * (pot2Bipolar + cv2Bipolar), 0.0f, 1.0f);

    runtime.mix = state.mix;
    runtime.fb = state.fb;
    runtime.outTrim = 1.0f;
    runtime.algoIndex = state.algoIndex;
    runtime.c3 = state.c3;
    runtime.c4 = state.c4;
    runtime.lfoDepth = (state.lfoDepth < 0.005f) ? 0.0f : state.lfoDepth;
    runtime.lfoRate = state.lfoRate;

    runtime.c1 = c1;
    runtime.c2 = c2;
}
