#include "encoder_handler.h"

#include "daisy_seed.h"

using namespace kxmx;
using namespace daisy;

EncoderPress UpdateEncoder(Bluemchen &hw, EncoderState &state)
{
    if (hw.encoder.RisingEdge())
    {
        state.pressTimeMs = System::GetNow();
    }
    if (hw.encoder.FallingEdge())
    {
        const uint32_t pressDuration = System::GetNow() - state.pressTimeMs;
        if (pressDuration > 500)
        {
            return EncoderPress::Long;
        }
        return EncoderPress::Short;
    }
    return EncoderPress::None;
}
