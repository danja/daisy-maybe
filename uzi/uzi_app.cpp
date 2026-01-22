#include "uzi_app.h"

void UziApp::Init()
{
    hw_.Init();
    hw_.StartAdc();

    const float sampleRate = hw_.AudioSampleRate();
    dsp_.Init(sampleRate);
    ui_.Init(hw_, state_);

    lastHeartbeatMs_ = daisy::System::GetNow();
}

void UziApp::StartAudio(daisy::AudioHandle::AudioCallback cb)
{
    hw_.StartAudio(cb);
}

void UziApp::Update()
{
    hw_.ProcessAnalogControls();
    hw_.ProcessDigitalControls();

    params_.Update(hw_, state_, runtime_);
    ui_.Update(hw_);

    const uint32_t now = daisy::System::GetNow();
    if (now - lastHeartbeatMs_ > 250)
    {
        heartbeatOn_ = !heartbeatOn_;
        lastHeartbeatMs_ = now;
    }

    ui_.RenderIfNeeded(hw_, state_, runtime_, heartbeatOn_, now);
}

void UziApp::ProcessAudio(daisy::AudioHandle::InputBuffer in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t size)
{
    dsp_.Process(in, out, size, runtime_);
}
