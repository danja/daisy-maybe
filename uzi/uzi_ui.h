#pragma once

#include "display.h"
#include "encoder_handler.h"
#include "menu_system.h"
#include "uzi_state.h"

class UziUi
{
public:
    void Init(kxmx::Bluemchen &hw, UziState &state);
    void Update(kxmx::Bluemchen &hw);
    void RenderIfNeeded(kxmx::Bluemchen &hw, const UziState &state, const UziRuntime &runtime, bool heartbeatOn, uint32_t nowMs);

private:
    MenuState menuState_{};
    EncoderState encoderState_{};

    MenuItem masterItems_[4]{};
    MenuItem distortionItems_[2]{};
    MenuItem fftItems_[4]{};
    MenuPage pages_[3]{};

    uint32_t lastDisplayUpdateMs_ = 0;
};
