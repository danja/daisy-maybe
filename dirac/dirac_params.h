/* dirac_params.h — parameter table, settings, and the modulation matrix.
 * SPDX-License-Identifier: Apache-2.0
 *
 * One flat table drives everything: the menu rows, the knob/CV assignment
 * destinations, and the MIDI CC map. Adding a parameter means adding one row.
 *
 * Platform-free so the host harness can exercise the mapping. */

#pragma once

#include <cstdint>

#include "dirac_engine.h"

namespace dirac
{

/* ── Parameters (DSP-facing, assignable, CC-controllable) ───────────────── */
enum ParamId : uint8_t
{
    P_DENS,    // density / target overlap
    P_GLEN,    // grain length
    P_GRAINS,  // polyphony cap
    P_PLAYH,   // playhead
    P_SPEED,   // playhead scan rate
    P_SEMI,    // coarse transpose
    P_VOCT,    // continuous pitch, octaves
    P_PSPRD,   // pitch scatter
    P_SCALE,   // scatter quantization
    P_DETUNE,  // stereo detune
    P_WINDOW,  // envelope shape
    P_TEXTR,   // shape morph / macro
    P_TILT,    // envelope peak position
    P_TSPRD,   // tilt scatter
    P_SPRD,    // stereo spread
    P_BINAU,   // binaural depth
    P_PJTR,    // read-position jitter
    P_REV,     // reverse probability
    P_COLOR,   // per-grain cutoff scatter
    P_FDBK,    // feedback
    P_MIX,     // wet/dry
    P_LEVEL,   // output level
    P_COMP,    // per-grain compression
    P_DIFFUSE, // allpass halo
    P_MULTI,   // multi-playhead enable
    P_HEADS,   // playhead count
    P_HSPRD,   // head spread
    P_HTUNE,   // head interval
    P_SNAP,    // onset lock
    P_HOLD,    // freeze the cloud
    P_FREEZE,  // freeze the capture ring
    P_COUNT
};

/* How a value is shown in 4 characters and stepped by the encoder. */
enum ParamFmt : uint8_t
{
    F_PCT,   // 0..1        -> "  47"  (percent)
    F_SPCT,  // -1..1       -> " -47"  (signed percent)
    F_DEC1,  // one decimal -> " 3.0"
    F_SDEC1, // signed one decimal
    F_INT,   // integer
    F_SINT,  // signed integer
    F_MS,    // seconds -> milliseconds, integer
    F_BOOL,  // off / on
    F_ENUM,  // named
};

struct ParamDesc
{
    const char *name; // <= 5 chars
    float min, max, def;
    float step; // encoder increment
    ParamFmt fmt;
    const char *const *names; // F_ENUM only
};

extern const ParamDesc kParams[P_COUNT];

/* ── Settings (host-facing: routing, MIDI, calibration) ─────────────────── */
enum SettingId : uint8_t
{
    S_K1_DEST,
    S_K2_DEST,
    S_CV1_DEST,
    S_CV1_DEPTH, // -100..100 %
    S_CV2_DEST,
    S_CV2_DEPTH,
    S_MOD_DEST,  // IN-R envelope follower destination (INR_MOD only)
    S_MOD_DEPTH,
    S_INR_MODE,  // capture / trigger / modulator
    S_OUT_MODE,  // stereo / wet+dry / mono
    S_MIDI_CH,   // 0 = omni, 1..16
    S_NOTE_MODE, // retrig / gate / latch
    S_CLK_DIV,   // off, 1/1 .. 1/32
    S_CC_LEARN,  // off / arm
    S_CAL_SCALE, // V/Oct tracking, per mille (1000 = 1.000)
    S_CAL_OFS,   // V/Oct offset, cents
    S_COUNT
};

struct SettingDesc
{
    const char *name;
    int min, max, def;
    ParamFmt fmt;
    const char *const *names;
};

extern const SettingDesc kSettings[S_COUNT];

enum InRMode : uint8_t
{
    INR_CAPTURE,
    INR_TRIGGER,
    INR_MOD
};
enum OutMode : uint8_t
{
    OUT_STEREO,
    OUT_WETDRY,
    OUT_MONO
};
enum NoteMode : uint8_t
{
    NOTE_RETRIG,
    NOTE_GATE,
    NOTE_LATCH
};

/* ── Live parameter state ───────────────────────────────────────────────── */
struct ParamState
{
    float value[P_COUNT];
    int setting[S_COUNT];

    /* Knob pickup: a knob is inert after a re-assignment (or a menu edit of its
     * destination) until it crosses the stored value, so nothing jumps. */
    bool knobLive[2] = {false, false};
    float knobLast[2] = {-1.0f, -1.0f};

    /* MIDI CC map: cc[n] = destination parameter + 1, 0 = unmapped. */
    uint8_t ccMap[128] = {0};

    void Reset();
    /* Normalised [0,1] position of a parameter, for knob pickup + display. */
    float Norm(int id) const;
    void SetNorm(int id, float n);
    void Step(int id, int clicks);
    void StepSetting(int id, int clicks);
    void FormatValue(int id, char *buf, int len) const;
    void FormatSetting(int id, char *buf, int len) const;
};

/* ── Modulation ─────────────────────────────────────────────────────────── */
struct ModInputs
{
    float knob[2] = {0.0f, 0.0f}; // 0..1
    /* Raw 0..1 from the CV ADC. Read two ways: as 0..5 V for a V/Oct
     * destination (with the tracking calibration applied), and as a bipolar
     * -1..1 around the mid point for every other destination. */
    float cv[2] = {0.0f, 0.0f};
    float midiVoct = 0.0f;        // octaves, from note number
    float midiLevel = 1.0f;       // velocity gain
    bool midiGate = false;        // note held (NOTE_GATE mode)
    float modEnv = 0.0f;          // IN-R envelope follower, 0..1
};

/* Applies knobs (with pickup), CV (bipolar depth), and MIDI onto the stored
 * values, then fills the engine's block-rate parameter struct. */
void BuildEngineParams(ParamState &st, const ModInputs &mod, DiracParams &out);

/* Default CC assignment: parameters in table order starting at CC 16, plus the
 * conventional CC 1 / 7 / 64. */
void InstallDefaultCcMap(ParamState &st);

} // namespace dirac
