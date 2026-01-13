#include "encoder_handler.h"

#include "daisy_seed.h"

using namespace kxmx;
using namespace daisy;

void UpdateEncoder(Bluemchen &hw,
                   EncoderState &state,
                   int numPages,
                   int &menuPageIndex,
                   bool &encoderLongPress)
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
            encoderLongPress = !encoderLongPress;
        }
        else
        {
            menuPageIndex = (menuPageIndex + 1) % numPages;
        }
    }
}
