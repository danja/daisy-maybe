/* dirac_midi.h — MIDI notes, CC, program change, and clock sync.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Events are drained in the main loop (libDaisy parses the UART into a queue),
 * so trigger timing is block-quantised — about 1 ms at 48 kHz with a 48-frame
 * block, which is well below the audible threshold for grain onsets. */

#pragma once

#include <cstdint>

#include "dirac_params.h"
#include "kxmx_bluemchen.h"

namespace dirac
{

class DiracMidi
{
  public:
    void Init(kxmx::Bluemchen &hw);

    /* Drains the queue. `selectedParam` is the menu's current parameter, used
     * as the target when CC learn is armed (-1 when the selection is not a
     * parameter row). Returns true if a parameter value changed. */
    bool Process(kxmx::Bluemchen &hw, ParamState &st, int selectedParam);

    /* Advances the clock-derived trigger generator. Call once per main loop. */
    void UpdateClock(ParamState &st);

    float Voct() const { return voct_ + bend_; }
    float Level() const { return level_; }
    bool Gate() const { return heldCount_ > 0; }
    /* Consumes any pending triggers (one grain each). */
    int TakeTriggers()
    {
        const int t = triggers_;
        triggers_ = 0;
        return t;
    }
    /* Program change requests a sample slot; -1 when nothing is pending. */
    int TakeProgramChange()
    {
        const int p = program_;
        program_ = -1;
        return p;
    }
    bool ClockRunning() const { return clockRunning_; }
    int LastCc() const { return lastCc_; }

  private:
    static constexpr int kMaxHeld = 8;

    float voct_ = 0.0f;
    float bend_ = 0.0f;
    float level_ = 1.0f;
    int triggers_ = 0;
    int program_ = -1;
    int lastCc_ = -1;

    uint8_t held_[kMaxHeld] = {0};
    int heldCount_ = 0;

    bool clockRunning_ = false;
    int clockTicks_ = 0;

    void NoteOn(uint8_t note, uint8_t vel, ParamState &st);
    void NoteOff(uint8_t note, ParamState &st);
    void ApplyPitch(uint8_t note);
};

} // namespace dirac
