#pragma once

#include <cstdint>
#include "kxmx_bluemchen.h"

struct EncoderState
{
    uint32_t pressTimeMs = 0;
};

enum class EncoderPress
{
    None,
    Short,
    Long,
};

EncoderPress UpdateEncoder(kxmx::Bluemchen &hw, EncoderState &state);
