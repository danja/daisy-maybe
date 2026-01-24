#pragma once

#include "kxmx_bluemchen.h"
#include "neurotic_state.h"

class NeuroticParams
{
public:
    void Update(kxmx::Bluemchen &hw, const NeuroticState &state, NeuroticRuntime &runtime);

private:
    float c1Smooth_ = 0.0f;
    float c2Smooth_ = 0.0f;
};
