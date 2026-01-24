#pragma once

#include "daisy_seed.h"
#include "distortion.h"
#include "uzi_spectral.h"
#include "uzi_state.h"

class UziDsp
{
public:
    void Init(float sampleRate);
    void Process(daisy::AudioHandle::InputBuffer in,
                 daisy::AudioHandle::OutputBuffer out,
                 size_t size,
                 const UziRuntime &runtime);

private:
    float sampleRate_ = 48000.0f;
    float lfoPhase_ = 0.0f;
    float feedbackL_ = 0.0f;
    float feedbackR_ = 0.0f;
    DistortionChannel distortionLeft_{};
    DistortionChannel distortionRight_{};
    UziSpectralStereo spectral_{};
};
