#pragma once

#include "kxmx_bluemchen.h"
#include "uzi_state.h"

class UziParams
{
public:
    void Update(kxmx::Bluemchen &hw, const UziState &state, UziRuntime &runtime);

private:
    float MapExpo(float value, float minVal, float maxVal) const;
};
