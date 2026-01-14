#include "encoder_handler.h"

#include "daisy_seed.h"

using namespace kxmx;
using namespace daisy;

void UpdateEncoder(Bluemchen &hw,
                   EncoderState &state,
                   int numPages,
                   int &menuPageIndex)
{
    if (hw.encoder.RisingEdge())
    {
        state.pressTimeMs = System::GetNow();
    }
    if (hw.encoder.FallingEdge())
    {
        menuPageIndex = (menuPageIndex + 1) % numPages;
    }
}
