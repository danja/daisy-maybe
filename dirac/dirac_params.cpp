/* dirac_params.cpp — parameter table, formatting, and the modulation matrix.
 * SPDX-License-Identifier: Apache-2.0 */

#include "dirac_params.h"

#include <cmath>

#include "dirac_fmt.h"

namespace dirac
{

namespace
{
const char *const kScaleNames[] = {"off", "chrm", "maj", "min",
                                   "pnt5", "pntm", "whol"};
const char *const kWindowNames[] = {"norm", "bell", "sinc", "tri",
                                    "decy", "ramp", "tuky", "sqr"};
const char *const kInrNames[] = {"capt", "trig", "mod"};
const char *const kOutNames[] = {"st", "w+d", "mono"};
const char *const kNoteNames[] = {"trig", "gate", "ltch"};
const char *const kClkNames[] = {"off", "1/1", "1/2", "1/4",
                                 "1/8", "/16", "/32"};
const char *const kLearnNames[] = {"off", "arm"};

inline float Clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}
inline int Clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

/* Grain length spans 1 ms .. 2 s — three decades. Both the encoder step and the
 * knob mapping are logarithmic, or the bottom two decades would be unreachable. */
inline bool IsLog(int id)
{
    return id == P_GLEN;
}

void FormatFmt(ParamFmt fmt, float v, const char *const *names, char *buf,
               int len)
{
    const float half = (v < 0.0f) ? -0.5f : 0.5f;
    switch(fmt)
    {
        case F_PCT:
        case F_SPCT: fmt::Int(buf, len, int(v * 100.0f + half)); break;
        case F_DEC1:
        case F_SDEC1: fmt::Dec1(buf, len, int(v * 10.0f + half)); break;
        case F_INT:
        case F_SINT: fmt::Int(buf, len, int(v + half)); break;
        case F_MS: fmt::Int(buf, len, int(v * 1000.0f + 0.5f)); break;
        case F_BOOL: fmt::Str(buf, len, (v >= 0.5f) ? "on" : "off"); break;
        case F_ENUM:
        {
            const int i = int(v + 0.5f);
            fmt::Str(buf, len, names ? names[i] : "?");
            break;
        }
    }
}
} // namespace

/* name    min     max    def   step   fmt      names */
const ParamDesc kParams[P_COUNT] = {
    {"dens", 0.0f, 16.0f, 3.0f, 0.25f, F_DEC1, nullptr},   // P_DENS
    {"glen", 0.001f, 2.0f, 0.05f, 0.0f, F_MS, nullptr},    // P_GLEN (log)
    {"grns", 1.0f, 16.0f, 12.0f, 1.0f, F_INT, nullptr},    // P_GRAINS
    {"plyh", 0.0f, 1.0f, 0.0f, 0.01f, F_PCT, nullptr},     // P_PLAYH
    {"sped", -4.0f, 4.0f, 0.0f, 0.1f, F_SDEC1, nullptr},   // P_SPEED
    {"semi", -24.0f, 24.0f, 0.0f, 1.0f, F_SINT, nullptr},  // P_SEMI
    {"voct", -4.0f, 4.0f, 0.0f, 0.02f, F_SDEC1, nullptr},  // P_VOCT
    {"pspr", 0.0f, 12.0f, 0.0f, 0.25f, F_DEC1, nullptr},   // P_PSPRD
    {"scal", 0.0f, 6.0f, 0.0f, 1.0f, F_ENUM, kScaleNames}, // P_SCALE
    {"detu", 0.0f, 2.0f, 0.0f, 0.02f, F_DEC1, nullptr},    // P_DETUNE
    {"wind", 0.0f, 7.0f, 0.0f, 1.0f, F_ENUM, kWindowNames},// P_WINDOW
    {"text", 0.0f, 1.0f, 0.5f, 0.02f, F_PCT, nullptr},     // P_TEXTR
    {"tilt", -1.0f, 1.0f, 0.0f, 0.02f, F_SPCT, nullptr},   // P_TILT
    {"tspr", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_TSPRD
    {"sprd", 0.0f, 1.0f, 0.5f, 0.02f, F_PCT, nullptr},     // P_SPRD
    {"bin", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},      // P_BINAU
    {"pjtr", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_PJTR
    {"rev", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},      // P_REV
    {"colr", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_COLOR
    {"fdbk", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_FDBK
    {"mix", 0.0f, 1.0f, 1.0f, 0.02f, F_PCT, nullptr},      // P_MIX
    {"levl", 0.0f, 1.0f, 0.7f, 0.02f, F_PCT, nullptr},     // P_LEVEL
    {"comp", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_COMP
    {"difu", 0.0f, 1.0f, 0.0f, 1.0f, F_BOOL, nullptr},     // P_DIFFUSE
    {"mult", 0.0f, 1.0f, 0.0f, 1.0f, F_BOOL, nullptr},     // P_MULTI
    {"head", 1.0f, 4.0f, 1.0f, 1.0f, F_INT, nullptr},      // P_HEADS
    {"hspr", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_HSPRD
    {"htun", -12.0f, 12.0f, 0.0f, 1.0f, F_SINT, nullptr},  // P_HTUNE
    {"snap", 0.0f, 1.0f, 0.0f, 0.02f, F_PCT, nullptr},     // P_SNAP
    {"hold", 0.0f, 1.0f, 0.0f, 1.0f, F_BOOL, nullptr},     // P_HOLD
    {"frez", 0.0f, 1.0f, 0.0f, 1.0f, F_BOOL, nullptr},     // P_FREEZE
};

const SettingDesc kSettings[S_COUNT] = {
    {"K1", 0, P_COUNT - 1, P_PLAYH, F_ENUM, nullptr},   // S_K1_DEST
    {"K2", 0, P_COUNT - 1, P_DENS, F_ENUM, nullptr},    // S_K2_DEST
    {"CV1", 0, P_COUNT - 1, P_VOCT, F_ENUM, nullptr},   // S_CV1_DEST
    {"cv1d", -100, 100, 100, F_SINT, nullptr},          // S_CV1_DEPTH
    {"CV2", 0, P_COUNT - 1, P_PLAYH, F_ENUM, nullptr},  // S_CV2_DEST
    {"cv2d", -100, 100, 50, F_SINT, nullptr},           // S_CV2_DEPTH
    {"MOD", 0, P_COUNT - 1, P_DENS, F_ENUM, nullptr},   // S_MOD_DEST
    {"modd", -100, 100, 0, F_SINT, nullptr},            // S_MOD_DEPTH
    {"in-R", 0, 2, INR_CAPTURE, F_ENUM, kInrNames},     // S_INR_MODE
    {"out", 0, 2, OUT_STEREO, F_ENUM, kOutNames},       // S_OUT_MODE
    {"mch", 0, 16, 0, F_INT, nullptr},                  // S_MIDI_CH (0 = omni)
    {"note", 0, 2, NOTE_RETRIG, F_ENUM, kNoteNames},    // S_NOTE_MODE
    {"clk", 0, 6, 0, F_ENUM, kClkNames},                // S_CLK_DIV
    {"cc", 0, 1, 0, F_ENUM, kLearnNames},               // S_CC_LEARN
    {"cscl", 800, 1200, 1000, F_INT, nullptr},          // S_CAL_SCALE
    {"cofs", -1200, 1200, 0, F_INT, nullptr},           // S_CAL_OFS
};

/* ── ParamState ─────────────────────────────────────────────────────────── */

void ParamState::Reset()
{
    for(int i = 0; i < P_COUNT; ++i)
        value[i] = kParams[i].def;
    for(int i = 0; i < S_COUNT; ++i)
        setting[i] = kSettings[i].def;
    knobLive[0] = knobLive[1] = false;
    knobLast[0] = knobLast[1] = -1.0f;
    InstallDefaultCcMap(*this);
}

float ParamState::Norm(int id) const
{
    const ParamDesc &d = kParams[id];
    if(IsLog(id))
        return logf(value[id] / d.min) / logf(d.max / d.min);
    return (value[id] - d.min) / (d.max - d.min);
}

void ParamState::SetNorm(int id, float n)
{
    const ParamDesc &d = kParams[id];
    n = Clampf(n, 0.0f, 1.0f);
    float v;
    if(IsLog(id))
        v = d.min * expf(n * logf(d.max / d.min));
    else
        v = d.min + n * (d.max - d.min);
    /* Quantize the discrete kinds so a knob can't park between values. */
    if(d.fmt == F_INT || d.fmt == F_SINT || d.fmt == F_ENUM || d.fmt == F_BOOL)
        v = floorf(v + 0.5f);
    value[id] = Clampf(v, d.min, d.max);
}

void ParamState::Step(int id, int clicks)
{
    const ParamDesc &d = kParams[id];
    if(IsLog(id))
    {
        /* Multiplicative: ~1.12x per click gives ~67 clicks over 1 ms .. 2 s.
         * expf(n*ln 1.12) instead of powf — see the note in BuildEnvTables. */
        value[id] *= expf(0.1133287f * float(clicks));
    }
    else
    {
        value[id] += d.step * float(clicks);
    }
    value[id] = Clampf(value[id], d.min, d.max);
}

void ParamState::StepSetting(int id, int clicks)
{
    const SettingDesc &d = kSettings[id];
    setting[id] = Clampi(setting[id] + clicks, d.min, d.max);
    /* Re-assigning a knob drops it out of pickup, so the new destination does
     * not jump to wherever the knob happens to be sitting. */
    if(id == S_K1_DEST)
        knobLive[0] = false;
    if(id == S_K2_DEST)
        knobLive[1] = false;
}

void ParamState::FormatValue(int id, char *buf, int len) const
{
    FormatFmt(kParams[id].fmt, value[id], kParams[id].names, buf, len);
}

void ParamState::FormatSetting(int id, char *buf, int len) const
{
    const SettingDesc &d = kSettings[id];
    /* Destination settings display the parameter's own name. */
    if(d.names == nullptr && d.fmt == F_ENUM)
    {
        const int p = Clampi(setting[id], 0, P_COUNT - 1);
        fmt::Str(buf, len, kParams[p].name);
        return;
    }
    FormatFmt(d.fmt, float(setting[id]), d.names, buf, len);
}

/* ── CC map ─────────────────────────────────────────────────────────────── */

void InstallDefaultCcMap(ParamState &st)
{
    for(int i = 0; i < 128; ++i)
        st.ccMap[i] = 0;
    /* Parameters in table order from CC 16 — 31 params lands on CC 16..46. */
    for(int i = 0; i < P_COUNT; ++i)
    {
        const int cc = 16 + i;
        if(cc < 128)
            st.ccMap[cc] = uint8_t(i + 1);
    }
    /* Conventional assignments on top. */
    st.ccMap[1] = P_PLAYH + 1;  // mod wheel
    st.ccMap[7] = P_LEVEL + 1;  // channel volume
    st.ccMap[64] = P_HOLD + 1;  // sustain pedal
}

/* ── Modulation matrix ──────────────────────────────────────────────────── */

void BuildEngineParams(ParamState &st, const ModInputs &mod, DiracParams &out)
{
    /* ── Knobs: absolute, with pickup ───────────────────────────────────
     * A knob takes over its destination only once it crosses the stored
     * value, so re-assigning it (or editing the value in the menu) never
     * makes the sound jump to the knob's physical position. */
    for(int k = 0; k < 2; ++k)
    {
        const int dest = Clampi(st.setting[S_K1_DEST + k], 0, P_COUNT - 1);
        const float kv = Clampf(mod.knob[k], 0.0f, 1.0f);
        if(!st.knobLive[k])
        {
            if(fabsf(kv - st.Norm(dest)) < 0.02f)
                st.knobLive[k] = true;
        }
        if(st.knobLive[k])
            st.SetNorm(dest, kv);
        st.knobLast[k] = kv;
    }

    /* ── CV and the IN-R envelope follower: bipolar offsets in normalised
     * space, so one depth setting works for every destination range. The
     * stored value is never overwritten — CV is an offset, not an edit. */
    float eff[P_COUNT];
    for(int i = 0; i < P_COUNT; ++i)
        eff[i] = st.value[i];

    struct Src
    {
        int destSetting, depthSetting;
        float amount;  // raw 0..1 (volts, for V/Oct)
        float bipolar; // -1..1 around the mid point, for everything else
    };
    const float env = (st.setting[S_INR_MODE] == INR_MOD) ? mod.modEnv : 0.0f;
    const Src srcs[3] = {
        {S_CV1_DEST, S_CV1_DEPTH, mod.cv[0], (mod.cv[0] - 0.5f) * 2.0f},
        {S_CV2_DEST, S_CV2_DEPTH, mod.cv[1], (mod.cv[1] - 0.5f) * 2.0f},
        /* The envelope follower is unipolar — it only ever adds. */
        {S_MOD_DEST, S_MOD_DEPTH, env, env},
    };
    for(const Src &s : srcs)
    {
        const float depth = float(st.setting[s.depthSetting]) * 0.01f;
        if(depth == 0.0f)
            continue;
        const int dest = Clampi(st.setting[s.destSetting], 0, P_COUNT - 1);

        /* V/Oct is not a normalised offset — it is volts. The CV reads 0..1
         * over the input's 0..5 V, so an octave is 0.2 of the range, scaled by
         * the tracking calibration and shifted by the offset (in cents). */
        if(dest == P_VOCT)
        {
            const float trk = float(st.setting[S_CAL_SCALE]) * 0.001f;
            const float ofs = float(st.setting[S_CAL_OFS]) / 1200.0f;
            eff[P_VOCT] = Clampf(
                st.value[P_VOCT] + depth * (s.amount * 5.0f * trk) + ofs, -4.0f,
                4.0f);
            continue;
        }

        if(s.bipolar == 0.0f)
            continue;
        const ParamDesc &d = kParams[dest];
        const float base = st.Norm(dest);
        const float n = Clampf(base + depth * s.bipolar, 0.0f, 1.0f);
        float v = IsLog(dest) ? d.min * expf(n * logf(d.max / d.min))
                              : d.min + n * (d.max - d.min);
        if(d.fmt == F_INT || d.fmt == F_SINT || d.fmt == F_ENUM
           || d.fmt == F_BOOL)
            v = floorf(v + 0.5f);
        eff[dest] = Clampf(v, d.min, d.max);
    }

    /* ── MIDI: note pitch adds to V/Oct, velocity scales level ───────── */
    eff[P_VOCT] = Clampf(eff[P_VOCT] + mod.midiVoct, -4.0f, 4.0f);
    eff[P_LEVEL] = Clampf(eff[P_LEVEL] * mod.midiLevel, 0.0f, 1.0f);
    /* Gate mode: the cloud only free-runs while a note is held. */
    if(st.setting[S_NOTE_MODE] == NOTE_GATE && !mod.midiGate)
        eff[P_DENS] = 0.0f;

    /* ── Map onto the engine's block-rate struct ─────────────────────── */
    out.density = eff[P_DENS];
    out.grainLenSec = eff[P_GLEN];
    out.grains = int(eff[P_GRAINS] + 0.5f);
    out.playhead = eff[P_PLAYH];
    out.speed = eff[P_SPEED];
    out.semiShift = eff[P_SEMI];
    out.voctOct = eff[P_VOCT];
    out.psprd = eff[P_PSPRD];
    out.scale = int(eff[P_SCALE] + 0.5f);
    out.detune = eff[P_DETUNE];
    out.window = int(eff[P_WINDOW] + 0.5f);
    out.texture = eff[P_TEXTR];
    out.tilt = eff[P_TILT];
    out.tiltSprd = eff[P_TSPRD];
    out.spread = eff[P_SPRD];
    out.binaural = eff[P_BINAU];
    out.posJtr = eff[P_PJTR];
    out.revProb = eff[P_REV];
    out.color = eff[P_COLOR];
    out.feedback = eff[P_FDBK];
    out.mix = eff[P_MIX];
    out.level = eff[P_LEVEL];
    out.compress = eff[P_COMP];
    out.diffuse = eff[P_DIFFUSE] >= 0.5f;
    out.multiHead = eff[P_MULTI] >= 0.5f;
    out.heads = int(eff[P_HEADS] + 0.5f);
    out.headSpread = eff[P_HSPRD];
    out.headTune = eff[P_HTUNE];
    out.snap = eff[P_SNAP];
    out.hold = eff[P_HOLD] >= 0.5f;
    out.freeze = eff[P_FREEZE] >= 0.5f;
}

} // namespace dirac
