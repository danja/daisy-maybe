#pragma once

#include "kxmx_bluemchen.h"

struct EncoderState
{
    uint32_t pressTimeMs = 0;
};

void UpdateEncoder(kxmx::Bluemchen &hw,
                   EncoderState &state,
                   int numPages,
                   int &menuPageIndex);
