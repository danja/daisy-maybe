#pragma once

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"

#include "neurotic_dsp.h"
#include "neurotic_params.h"
#include "neurotic_ui.h"
#include "neurotic_state.h"

class NeuroticApp
{
public:
    void Init();
    void StartAudio(daisy::AudioHandle::AudioCallback cb);
    void Update();
    void ProcessAudio(daisy::AudioHandle::InputBuffer in,
                      daisy::AudioHandle::OutputBuffer out,
                      size_t size);

private:
    kxmx::Bluemchen hw_{};
    NeuroticState state_{};
    NeuroticRuntime runtime_{};
    NeuroticParams params_{};
    NeuroticUi ui_{};
    NeuroticDsp dsp_{};

    bool heartbeatOn_ = false;
    uint32_t lastHeartbeatMs_ = 0;
};
