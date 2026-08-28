/* res_midi.h — MIDI note pitch, velocity, and sample triggering.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Events are drained in the main loop (libDaisy parses the UART into a queue),
 * so a note's timing is block-quantised — about 1 ms at 48 kHz, well under the
 * audible threshold for a strike onset.
 *
 * A resonator body is passive, so a note is not a "voice": it sets the pitch,
 * scales how hard the excitation hits, and fires one strike. What is struck
 * (an SD sample, the audio input, an internal mallet) is decided downstream. */

#pragma once

#include <cstdint>

#include "kxmx_bluemchen.h"

namespace res
{

/* How a MIDI note relates to the knob/CV pitch. */
enum MidiPitchMode
{
    MIDI_PITCH_OFF = 0, // notes trigger but never move the pitch
    MIDI_PITCH_ADD,     // note 60 is unity; offsets the knob/CV pitch
    MIDI_PITCH_SET,     // note 69 is 440 Hz; overrides the knob/CV pitch
    MIDI_PITCH_COUNT,
};

struct MidiSettings
{
    /* 0 = omni, 1..16 = that channel only. */
    int channel = 0;
    int pitchMode = MIDI_PITCH_ADD;
    /* How much velocity scales the strike: 0 = every note full level. */
    float velAmount = 1.0f;
};

class ResMidi
{
  public:
    void Init(kxmx::Bluemchen &hw);

    /* True while the UART is actually listening. If this is false the
     * peripheral has disabled itself (usually an overrun) and Listen() has not
     * recovered it; if it is true and MessageCount() never moves, no bytes are
     * arriving and the problem is upstream of the firmware. */
    bool RxActive(kxmx::Bluemchen &hw) const { return hw.midi.RxActive(); }

    /* Drains the queue. Call once per main loop. */
    void Process(kxmx::Bluemchen &hw, const MidiSettings &cfg);

    /* Octave offset from note 60, plus pitch bend. Meaningful in ADD mode. */
    float Voct() const { return voct_ + bend_; }
    /* Absolute pitch of the last note in Hz. Meaningful in SET mode. */
    float NoteHz() const { return noteHz_; }
    /* Pitch bend alone, in octaves — SET mode needs it without the note. */
    float Bend() const { return bend_; }
    bool HasNote() const { return hasNote_; }
    bool Gate() const { return heldCount_ > 0; }
    /* 0..1, velocity of the most recent note-on already scaled by velAmount. */
    float Level() const { return level_; }

    /* Consumes pending strikes: one per note-on. Each carries the level it was
     * struck at, so a fast run does not lose its dynamics to the last note. */
    struct Strike
    {
        float level;
        float semitones; // offset from note 60, for sample playback rate
    };
    bool TakeStrike(Strike &out);

    int LastNote() const { return lastNote_; }
    /* Every channel-voice message seen, before any filtering. Zero means
     * nothing is arriving at the UART at all — a cable, TRS-type or wiring
     * problem, not a firmware one. */
    uint32_t MessageCount() const { return messages_; }

  private:
    static constexpr int kMaxHeld = 8;
    static constexpr int kMaxPending = 8;

    float voct_ = 0.0f;
    float bend_ = 0.0f;
    float noteHz_ = 440.0f;
    float level_ = 1.0f;
    bool hasNote_ = false;
    int lastNote_ = 0; // 0 until a real note arrives, so Diag starts clean

    uint32_t messages_ = 0;
    /* False until the first Process() call has emptied the boot backlog and
     * re-armed reception. Until then the queue holds whatever the opto framed
     * while it was settling, none of which is real traffic. */
    bool flushed_ = false;

    uint8_t held_[kMaxHeld] = {0};
    int heldCount_ = 0;

    Strike pending_[kMaxPending];
    int pendingCount_ = 0;

    void NoteOn(uint8_t note, uint8_t vel, const MidiSettings &cfg);
    void NoteOff(uint8_t note);
    void ApplyPitch(uint8_t note);
};

} // namespace res
