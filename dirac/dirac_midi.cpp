/* dirac_midi.cpp — MIDI notes, CC, program change, and clock sync.
 * SPDX-License-Identifier: Apache-2.0 */

#include "dirac_midi.h"

namespace dirac
{

namespace
{
/* Ticks per trigger at 24 ppqn, indexed by the clk setting:
 * off, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32. */
const int kClkTicks[7] = {0, 96, 48, 24, 12, 6, 3};
} // namespace

void DiracMidi::Init(kxmx::Bluemchen &hw)
{
    hw.midi.StartReceive();
}

void DiracMidi::ApplyPitch(uint8_t note)
{
    /* Middle C (60) is unity: the sample plays at its stored pitch. */
    voct_ = (float(note) - 60.0f) / 12.0f;
}

void DiracMidi::NoteOn(uint8_t note, uint8_t vel, ParamState &st)
{
    if(heldCount_ < kMaxHeld)
        held_[heldCount_++] = note;
    ApplyPitch(note);
    level_ = float(vel) / 127.0f;
    /* Retrig fires one grain per note; gate and latch let density free-run. */
    if(st.setting[S_NOTE_MODE] == NOTE_RETRIG)
        triggers_++;
}

void DiracMidi::NoteOff(uint8_t note, ParamState &st)
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
    /* Last-note priority: fall back to whatever is still held. Latch mode
     * keeps the last pitch after release. */
    if(heldCount_ > 0)
        ApplyPitch(held_[heldCount_ - 1]);
    /* Pitch persists after release in every mode; whether the cloud keeps
     * running is the note mode's business (see BuildEngineParams). */
}

bool DiracMidi::Process(kxmx::Bluemchen &hw, ParamState &st, int selectedParam)
{
    bool changed = false;
    hw.midi.Listen();

    while(hw.midi.HasEvents())
    {
        daisy::MidiEvent e = hw.midi.PopEvent();

        /* Channel filter: setting 0 = omni, 1..16 = that channel. */
        const int chSet = st.setting[S_MIDI_CH];
        const bool channelMsg
            = (e.type == daisy::NoteOn || e.type == daisy::NoteOff
               || e.type == daisy::ControlChange || e.type == daisy::ProgramChange
               || e.type == daisy::PitchBend);
        if(channelMsg && chSet != 0 && e.channel != (chSet - 1))
            continue;

        switch(e.type)
        {
            case daisy::NoteOn:
            {
                const auto m = e.AsNoteOn();
                if(m.velocity == 0)
                    NoteOff(m.note, st); // running-status note-off
                else
                    NoteOn(m.note, m.velocity, st);
                break;
            }
            case daisy::NoteOff: NoteOff(e.AsNoteOff().note, st); break;

            case daisy::ControlChange:
            {
                const auto m = e.AsControlChange();
                lastCc_ = m.control_number;
                /* CC learn: bind the next CC to the selected menu parameter. */
                if(st.setting[S_CC_LEARN] == 1 && selectedParam >= 0
                   && selectedParam < P_COUNT)
                {
                    st.ccMap[m.control_number] = uint8_t(selectedParam + 1);
                    st.setting[S_CC_LEARN] = 0;
                    changed = true;
                    break;
                }
                const uint8_t dest = st.ccMap[m.control_number];
                if(dest)
                {
                    st.SetNorm(dest - 1, float(m.value) / 127.0f);
                    /* A CC that owns a knob's destination would fight the knob;
                     * drop the knob out of pickup so the CC wins until the knob
                     * is moved back across the value. */
                    for(int k = 0; k < 2; ++k)
                        if(st.setting[S_K1_DEST + k] == dest - 1)
                            st.knobLive[k] = false;
                    changed = true;
                }
                break;
            }

            case daisy::ProgramChange:
                program_ = e.AsProgramChange().program;
                break;

            case daisy::PitchBend:
            {
                /* +/- 2 semitones, the General MIDI default. Held separately
                 * from the note pitch so repeated bend messages replace the
                 * offset instead of accumulating it. */
                const float b = float(e.AsPitchBend().value) / 8192.0f;
                bend_ = b * (2.0f / 12.0f);
                break;
            }

            case daisy::SystemRealTime:
                switch(e.srt_type)
                {
                    case daisy::TimingClock:
                        if(clockRunning_)
                        {
                            const int ticks
                                = kClkTicks[(st.setting[S_CLK_DIV] < 0
                                             || st.setting[S_CLK_DIV] > 6)
                                                ? 0
                                                : st.setting[S_CLK_DIV]];
                            if(ticks > 0)
                            {
                                if(++clockTicks_ >= ticks)
                                {
                                    clockTicks_ = 0;
                                    triggers_++;
                                }
                            }
                        }
                        break;
                    case daisy::Start:
                    case daisy::Continue:
                        clockRunning_ = true;
                        clockTicks_ = 0;
                        break;
                    case daisy::Stop: clockRunning_ = false; break;
                    default: break;
                }
                break;

            default: break;
        }
    }
    return changed;
}

void DiracMidi::UpdateClock(ParamState &st)
{
    /* Selecting "off" mid-run must not leave a partial tick count behind. */
    if(st.setting[S_CLK_DIV] == 0)
        clockTicks_ = 0;
}

} // namespace dirac
