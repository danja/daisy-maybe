#pragma once

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"

#include "uzi_dsp.h"
#include "uzi_params.h"
#include "uzi_ui.h"
#include "uzi_state.h"

class UziApp
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
    UziState state_{};
    UziRuntime runtime_{};
    UziParams params_{};
    UziUi ui_{};
    UziDsp dsp_{};

    bool heartbeatOn_ = false;
    uint32_t lastHeartbeatMs_ = 0;
};
