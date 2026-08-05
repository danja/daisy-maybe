/* main.cpp — Dirac granular synthesizer for kxmx_bluemchen.
 * SPDX-License-Identifier: Apache-2.0
 * Ported from Dirac for the ER-301, © 2026 Nicholas Breinich (nickb808).
 * See LICENSE and NOTICE. */

#include "dirac_app.h"

namespace
{
dirac::DiracApp app;
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

    while(1)
    {
        app.Update();
    }
}
