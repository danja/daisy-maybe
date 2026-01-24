#include "neurotic_app.h"

namespace
{
NeuroticApp app;
}

void AudioCallback(daisy::AudioHandle::InputBuffer in,
                   daisy::AudioHandle::OutputBuffer out,
                   size_t size)
{
    app.ProcessAudio(in, out, size);
}

int main(void)
{
    app.Init();
    app.StartAudio(AudioCallback);

    while (1)
    {
        app.Update();
    }
}
