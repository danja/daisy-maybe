/* dirac_engine.h — time-domain granular synthesizer engine
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported from Dirac for the ER-301 (src/Dirac.h, src/Dirac.cpp)
 * Copyright 2026 Nicholas Breinich (nickb808) — see LICENSE and NOTICE.
 * Daisy / kxmx_bluemchen port 2026.
 *
 * Grains read a short window of the source with linear interpolation, multiply
 * it by a windowed envelope, and overlap-add. No FFT: crisp transients, classic
 * formant-shifting pitch, low CPU.
 *
 * SOURCE (dual, automatic):
 *   • a loaded sample (DiracSample) read at a movable Playhead, OR
 *   • the LIVE input captured into a rolling ring when no sample is attached.
 *
 * This file is deliberately free of libDaisy: it compiles on the host so the
 * DSP can be tested without hardware (see host_dsp/dirac_fixture.cpp).
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace dirac
{

/* Feedback saturator. libm's tanhf drags in expm1f — around 800 bytes of a
 * 128 KB flash budget for one call site. This is the order-7/6 Padé
 * approximant of tanh, clamped to the asymptote beyond |x| = 4 (where tanh is
 * already 0.99933). Declared here so the host harness can check it against
 * libm tanhf; see the fixture's `tanh` mode. It is also faster than libm. */
inline float FastTanh(float x)
{
    if(x > 4.0f)
        return 1.0f;
    if(x < -4.0f)
        return -1.0f;
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return num / den;
}

/* ── Source sample descriptor ──────────────────────────────────────────────
 * `data` is mono float32 owned by the caller (SDRAM on hardware). nullptr
 * means LIVE mode: grains read the capture ring instead. */
struct DiracSample
{
    const float *data = nullptr;
    int frames = 0;
    /* file rate / engine rate. Folded into the grain pitch increment so a
     * 44.1 kHz file plays at the right speed with no resampler. */
    float rateRatio = 1.0f;
};

/* ── Block-rate parameters ─────────────────────────────────────────────────
 * One field per ER-301 inlet. Filled once per block by the host layer; the
 * original read every inlet at block rate too, so this changes nothing. */
struct DiracParams
{
    float playhead = 0.0f;    // sample: read position [0,1]; live: feedback delay
    float density = 3.0f;     // target grain overlap [0,16]; 0 = trigger-only
    float grainLenSec = 0.05f;// grain duration, seconds
    int grains = 12;          // polyphony cap [1,16]
    float speed = 0.0f;       // playhead scan rate [-4,4]; 0 = parked

    float semiShift = 0.0f;   // coarse transpose [-24,+24] st
    float voctOct = 0.0f;     // 1 V/oct pitch, in OCTAVES
    float psprd = 0.0f;       // per-grain pitch scatter [0,12] st
    int scale = 0;            // quantize scatter to a scale (0 = off)
    float detune = 0.0f;      // stereo pitch detune, R vs L [st]

    int window = 0;           // envelope shape: 0 = Texture morph, 1..7 fixed
    float texture = 0.5f;     // shape morph / per-shape macro
    float tilt = 0.0f;        // envelope peak position [-1,+1]
    float tiltSprd = 0.0f;    // per-grain tilt scatter [0,1]

    float spread = 0.5f;      // stereo position scatter [0,1]
    float binaural = 0.0f;    // ITD + head shadow [0,1]
    float posJtr = 0.0f;      // read-position spray [0,1]
    float revProb = 0.0f;     // reverse-grain probability [0,1]
    float color = 0.0f;       // per-grain LP cutoff scatter [0,1]

    float feedback = 0.0f;    // output → capture reinjection [0,1]
    float mix = 1.0f;         // wet/dry (live mode only)
    float level = 0.7f;       // output gain [0,1]
    float compress = 0.0f;    // per-grain leveling [0,1]
    bool diffuse = false;     // 4 cascaded allpasses per channel

    bool multiHead = false;   // enable the Heads controls (hard bypass when off)
    int heads = 1;            // playhead count [1,4]
    float headSpread = 0.0f;  // position offset between heads [0,1]
    float headTune = 0.0f;    // pitch interval between heads [-12,+12] st
    float snap = 0.0f;        // pull grains onto transients [0,1]

    bool hold = false;        // freeze the current grains into a looping cloud
    bool freeze = false;      // stop WRITING the capture ring (freeze the source)
};

class DiracEngine
{
  public:
    static constexpr int kMaxGrains = 16;
    static constexpr int kMaxHeads = 4;
    static constexpr int kMaxBlock = 128;

    /* capBuf/capSize: caller-owned capture ring. capSize MUST be a power of
     * two (the render loop masks rather than wraps). */
    void Init(float sampleRate, float *capBuf, int capSize);

    void SetSample(const DiracSample &s);
    bool IsSampleMode() const { return mSample.data != nullptr && mSample.frames > 0; }

    /* in:   mono live input, `n` frames (never null)
     * trig: per-sample gate; rising edges spawn one grain. May be null. */
    void Process(const float *in, const float *trig, float *outL, float *outR,
                 int n, const DiracParams &p);

    /* ── Grain-field / display accessors (read-only, display rate) ───────── */
    int GetGrainCount() const { return kMaxGrains; }
    bool IsGrainActive(int i) const { return mGrains[i].active; }
    float GetGrainEnvelope(int i) const
    {
        const Grain &g = mGrains[i];
        if(g.duration <= 0)
            return 0.0f;
        float p = float(g.age) / float(g.duration);
        if(p < 0.0f)
            p = 0.0f;
        else if(p > 1.0f)
            p = 1.0f;
        return sinf(kPi * p);
    }
    float GetGrainPan(int i) const { return mGrains[i].panPos; }
    float GetGrainPitch(int i) const { return mGrains[i].pitchSemi; }
    int GetGrainDuration(int i) const { return mGrains[i].duration; }
    bool GetGrainReverse(int i) const { return mGrains[i].reverse; }
    bool GetGrainLive(int i) const { return mGrains[i].live; }
    float GetGrainGain(int i) const
    {
        const Grain &g = mGrains[i];
        return g.scale * 0.5f * (g.gainL + g.gainR);
    }
    float GetGrainAlphaMin(int i) const
    {
        const Grain &g = mGrains[i];
        return (g.lpAlphaL < g.lpAlphaR) ? g.lpAlphaL : g.lpAlphaR;
    }
    /* Where this grain reads in the source, normalised to [0,1] — the
     * alternate X axis for the grain field. Live: 0 = oldest, 1 = write head. */
    float GetGrainReadPos01(int i) const { return NormPos(mGrains[i].readPosL); }
    float GetGrainSpawnPos01(int i) const { return NormPos(mGrains[i].readPos0L); }
    float GetVizTexture() const
    {
        if(mCurWindow > 0)
        {
            static const float kWinTex[7] = {0.5f, 0.5f, 0.5f, 0.0f, 0.15f, 1.0f, 1.0f};
            return kWinTex[mCurWindow - 1];
        }
        return mCurTexture;
    }
    int GetCapWritePos() const { return mCapWrite; }
    float GetPlayheadEff() const { return mPlayheadEff; }
    int ActiveGrains() const
    {
        int n = 0;
        for(int i = 0; i < kMaxGrains; ++i)
            if(mGrains[i].active)
                n++;
        return n;
    }
    /* Harness-facing */
    float GetGrainSpawnPos(int i) const { return mGrains[i].readPos0L; }
    float GetGrainGainL(int i) const { return mGrains[i].gainL; }
    float GetGrainGainR(int i) const { return mGrains[i].gainR; }

  private:
    /* ── Constants ─────────────────────────────────────────────────────── */
    static constexpr int kMaxGrainSamp = 96000;  // grain length ceiling (2 s)
    /* 512 (not the ER-301's 1024): halves the window bank to ~78 KB so it
     * stays in fast AXI SRAM instead of SDRAM. The table is interpolated, so
     * the difference is inaudible. */
    static constexpr int kEnvTableSize = 512;
    static constexpr float kDensFloor = 0.25f;
    static constexpr float kSeamGuard = 256.0f;
    static constexpr float kInvSeamGuard = 1.0f / 256.0f;
    static constexpr float kPi = 3.14159265358979f;
    static constexpr float kTwoPi = 6.28318530717959f;
    static constexpr float kLog2Over12 = 0.05776226504f;
    static constexpr float kMaxITDSamples = 31.2f;
    static constexpr float kMaxTranspose = 48.0f;  // ±4 octaves
    static constexpr int kSnapPerBlock = 2;
    static constexpr int kWinShapes = 7;
    static constexpr int kWinVariants = 5;

    struct Grain
    {
        bool active = false;
        bool held = false;
        bool reverse = false;
        bool stereo = false;
        int duration = 0;
        int age = 0;
        int startDelay = 0;
        float readPosL = 0.0f, readPosR = 0.0f;
        float readPos0L = 0.0f, readPos0R = 0.0f;
        float pitchIncrL = 1.0f, pitchIncrR = 1.0f;
        float envPhase = 0.0f;
        float envIncrA = 0.0f, envIncrB = 0.0f;
        float envTex = 0.5f;
        float gainL = 1.0f, gainR = 1.0f;
        float lpStateL = 0.0f, lpStateR = 0.0f;
        float lpAlphaL = 1.0f, lpAlphaR = 1.0f;
        float scale = 1.0f;
        int8_t envShape = 0;
        bool live = false;
        float panPos = 0.0f;
        float pitchSemi = 0.0f;
    };
    Grain mGrains[kMaxGrains];

    float mSampleRate = 48000.0f;
    DiracSample mSample{};

    /* ── Live capture ring + feedback ──────────────────────────────────── */
    float *mCapBuf = nullptr;
    int mCapSize = 0;
    int mCapMask = 0;
    int mCapWrite = 0;
    float mFeedBuf[kMaxBlock] = {0.0f};
    bool mFeedPrimed = false;
    float mFbHpX = 0.0f, mFbHpY = 0.0f;

    /* ── Envelope tables (built once, never rebuilt) ────────────────────── */
    float mPercTable[kEnvTableSize + 1];
    float mHannTable[kEnvTableSize + 1];
    float mTukeyTable[kEnvTableSize + 1];
    float mWinBank[kWinShapes][kWinVariants][kEnvTableSize + 1];
    int mCurWindow = 0;
    float mCurTexture = 0.5f;
    void BuildEnvTables();

    /* ── Spawn timing / scan ────────────────────────────────────────────── */
    float mSpawnTimer = 0.0f;
    float mLastFire = 0.0f;
    float mScanPos = 0.0f; // in SOURCE FRAMES, not normalised — see Process()
    float mLastPlayheadKnob = -1.0f;
    float mPlayheadEff = 0.0f;
    int mHeadCycle = 0;
    int mSnapBudget = 0;

    /* ── Diffuser: 4 cascaded Schroeder allpasses per channel ───────────── */
    static constexpr int kApStages = 4;
    static constexpr int kApSizeL[kApStages] = {113, 337, 719, 1201};
    static constexpr int kApSizeR[kApStages] = {127, 347, 743, 1213};
    static constexpr int kApTotalL = 113 + 337 + 719 + 1201;
    static constexpr int kApTotalR = 127 + 347 + 743 + 1213;
    static constexpr float kApGain = 0.6f;
    float mApPoolL[kApTotalL];
    float mApPoolR[kApTotalR];
    float *mApBufL[kApStages] = {nullptr};
    float *mApBufR[kApStages] = {nullptr};
    int mApIdxL[kApStages] = {0}, mApIdxR[kApStages] = {0};
    bool mDiffusePrimed = false;

    /* ── PRNG ───────────────────────────────────────────────────────────── */
    uint32_t mRandState = 0x1234abcdu;
    float RandUnipolar();
    float RandBipolar();

    /* ── Helpers ────────────────────────────────────────────────────────── */
    static inline float ReadHoisted(const float *cap, int capMask, const float *sd,
                                    int frames, bool live, float pos);
    inline float ReadSrc(float pos, bool live, int frames) const;
    float NormPos(float p) const;
    void SpawnGrain(float playhead, int frames, float posJtr, float sprd,
                    float detune, float level, bool reverseProb, float psprd,
                    int grainsCap, bool held, float semiShift, int grainDuration,
                    float hopSamples, int sampleOffset, bool liveMode,
                    float compress, float binaural, int scaleSel, int heads,
                    float headSpread, float headTune, float snap, float feedback,
                    float color, float tilt, float tiltSprd);
    int FindSlot(int grainsCap);
};

} // namespace dirac
