/* res_midi.cpp — MIDI note pitch, velocity, and sample triggering.
 * SPDX-License-Identifier: Apache-2.0 */

#include "res_midi.h"

#include <cmath>

namespace res
{

void ResMidi::Init(kxmx::Bluemchen &hw)
{
    hw.midi.StartReceive();
}

void ResMidi::ApplyPitch(uint8_t note)
{
    lastNote_ = note;
    /* Middle C (60) is unity in ADD mode; A4 (69) is 440 Hz in SET mode. Both
     * offsets are derived here so the mode can change without a note retrigger. */
    voct_ = (static_cast<float>(note) - 60.0f) / 12.0f;
    noteHz_ = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
    hasNote_ = true;
}

void ResMidi::NoteOn(uint8_t note, uint8_t vel, const MidiSettings &cfg)
{
    if(heldCount_ < kMaxHeld)
        held_[heldCount_++] = note;
    ApplyPitch(note);

    const float vNorm = static_cast<float>(vel) / 127.0f;
    /* velAmount 0 flattens every note to full level, which is what you want
     * when the controller has no velocity sensor. */
    level_ = 1.0f - cfg.velAmount * (1.0f - vNorm);

    if(pendingCount_ < kMaxPending)
    {
        pending_[pendingCount_].level = level_;
        pending_[pendingCount_].semitones = static_cast<float>(note) - 60.0f;
        pendingCount_++;
    }
}

void ResMidi::NoteOff(uint8_t note)
{
    for(int i = 0; i < heldCount_; ++i)
    {
        if(held_[i] == note)
        {
            for(int j = i; j < heldCount_ - 1; ++j)
                held_[j] = held_[j + 1];
            heldCount_--;
            break;
        }
    }
    /* Last-note priority. The pitch persists after the final release: a body
     * that is still ringing must not be retuned out from under its own tail. */
    if(heldCount_ > 0)
        ApplyPitch(held_[heldCount_ - 1]);
}

bool ResMidi::TakeStrike(Strike &out)
{
    if(pendingCount_ <= 0)
        return false;
    out = pending_[0];
    for(int i = 0; i < pendingCount_ - 1; ++i)
        pending_[i] = pending_[i + 1];
    pendingCount_--;
    return true;
}

void ResMidi::Process(kxmx::Bluemchen &hw, const MidiSettings &cfg)
{
    /* First pass only: throw away everything the UART accumulated during init.
     *
     * StartReceive runs before the SD card is mounted and scanned, and nothing
     * drains the queue until the main loop starts — so the opto's power-up
     * settling gets framed into a backlog of bogus events that all arrive in
     * the first Process() call. That backlog is what the Diag counter was
     * reporting: it read a fixed number, then never moved again, which looked
     * like a stuck peripheral rather than a stale queue.
     *
     * A wall-clock settle window could not fix it — if init ran past the
     * window, the whole backlog was already counted by the time it closed.
     * Flushing on the first pass is exact regardless of how long init takes.
     *
     * Only the queue is emptied. Calling StartReceive again to "re-arm" looks
     * tempting and is worse than doing nothing: HAL_UART_Receive_DMA returns
     * BUSY while reception is already running, so the DMA keeps its real
     * position while DmaListenStart has already reset the read cursor to zero
     * — and the next IDLE interrupt replays the stale buffer as fresh bytes. */
    if(!flushed_)
    {
        while(hw.midi.HasEvents())
            hw.midi.PopEvent();
        flushed_ = true;
        return;
    }

    hw.midi.Listen();

    while(hw.midi.HasEvents())
    {
        daisy::MidiEvent e = hw.midi.PopEvent();
        messages_++;

        const bool channelMsg
            = (e.type == daisy::NoteOn || e.type == daisy::NoteOff
               || e.type == daisy::PitchBend);
        if(channelMsg && cfg.channel != 0 && e.channel != (cfg.channel - 1))
            continue;

        switch(e.type)
        {
            case daisy::NoteOn:
            {
                const auto m = e.AsNoteOn();
                if(m.velocity == 0)
                    NoteOff(m.note); // running-status note-off
                else
                    NoteOn(m.note, m.velocity, cfg);
                break;
            }
            case daisy::NoteOff: NoteOff(e.AsNoteOff().note); break;

            case daisy::PitchBend:
            {
                /* +/- 2 semitones, the General MIDI default. Held separately
                 * from the note pitch so repeated bend messages replace the
                 * offset instead of accumulating it. */
                const float b = static_cast<float>(e.AsPitchBend().value) / 8192.0f;
                bend_ = b * (2.0f / 12.0f);
                break;
            }

            default: break;
        }
    }
}

} // namespace res
