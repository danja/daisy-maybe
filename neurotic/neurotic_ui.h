#pragma once

#include "display.h"
#include "encoder_handler.h"
#include "menu_system.h"
#include "neurotic_state.h"

class NeuroticUi
{
public:
    void Init(kxmx::Bluemchen &hw, NeuroticState &state);
    void Update(kxmx::Bluemchen &hw, NeuroticState &state);
    void RenderIfNeeded(kxmx::Bluemchen &hw, const NeuroticState &state, bool heartbeatOn, uint32_t nowMs);

private:
    void UpdateAlgoLabels(int algoIndex);

    MenuState menuState_{};
    EncoderState encoderState_{};

    MenuItem masterItems_[4]{};
    MenuItem algoItems_[2]{};
    MenuPage pages_[2]{};

    uint32_t lastDisplayUpdateMs_ = 0;
    int algoIndex_ = 0;
};
