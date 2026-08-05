/* dirac_engine.cpp — time-domain granular synthesizer engine
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ported from Dirac for the ER-301 (src/Dirac.cpp)
 * Copyright 2026 Nicholas Breinich (nickb808) — see LICENSE and NOTICE.
 * Daisy / kxmx_bluemchen port 2026.
 *
 * Grains are read directly from the source (sample at the Playhead, or the live
 * capture ring) with linear interpolation, windowed by a morphable envelope,
 * pitched by playback speed, and overlap-added. Feedback reinjects output into
 * the capture ring for regenerating clouds. See dirac_engine.h. */

#include "dirac_engine.h"

#include <algorithm>

namespace dirac
{

constexpr int DiracEngine::kApSizeL[DiracEngine::kApStages];
constexpr int DiracEngine::kApSizeR[DiracEngine::kApStages];

/* Finiteness guard. A NaN/Inf from a corrupt sample read or a glitchy input
 * poisons the capture ring + feedback loop (tanh(NaN)=NaN) and held grains →
 * non-finite output that escapes downstream. BIT test on the exponent (all 1s
 * ⇒ Inf/NaN) — bit-level because -ffast-math optimizes away `x==x`. */
static inline float Sanitize(float x)
{
    union
    {
        float f;
        uint32_t u;
    } v;
    v.f = x;
    return ((v.u & 0x7F800000u) == 0x7F800000u) ? 0.0f : x;
}

/* Final-stage soft limiter. Level already attenuates per grain, but dense
 * overlap can still sum past ±1.0; transparent below ±0.9, asymptotes to ±1.0
 * above so the output can't hard-clip the DAC. */
static inline float SoftLimit(float x)
{
    const float T = 0.9f;
    const float ax = (x >= 0.0f) ? x : -x;
    if(ax <= T)
        return x;
    const float e = ax - T;
    const float lim = T + (1.0f - T) * (e / (e + (1.0f - T)));
    return (x >= 0.0f) ? lim : -lim;
}

/* Snap a semitone offset to the nearest degree of a musical scale (0 = off).
 * 1 Chromatic · 2 Major · 3 Minor · 4 Penta-maj · 5 Penta-min · 6 Whole-tone. */
static inline float SnapToScale(float semis, int scale)
{
    if(scale <= 0)
        return semis;
    static const int kDeg[6][12] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
        {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0},
        {0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0},
        {0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0},
        {0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0},
        {0, 2, 4, 6, 8, 10, 0, 0, 0, 0, 0, 0},
    };
    static const int kLen[6] = {12, 7, 7, 5, 5, 6};
    if(scale > 6)
        scale = 6;
    const int *deg = kDeg[scale - 1];
    const int n = kLen[scale - 1];
    const float oct = floorf(semis / 12.0f);
    const float frac = semis - oct * 12.0f; // [0, 12)
    float bestD = float(deg[0]), bestDist = fabsf(frac - float(deg[0]));
    for(int i = 1; i < n; ++i)
    {
        const float d = fabsf(frac - float(deg[i]));
        if(d < bestDist)
        {
            bestDist = d;
            bestD = float(deg[i]);
        }
    }
    const float dwrap = fabsf(frac - 12.0f); // nearest may be next-octave root
    if(dwrap < bestDist)
        bestD = 12.0f;
    return oct * 12.0f + bestD;
}

/* ── init ──────────────────────────────────────────────────────────────── */

void DiracEngine::Init(float sampleRate, float *capBuf, int capSize)
{
    mSampleRate = (sampleRate > 1.0f) ? sampleRate : 48000.0f;
    mCapBuf = capBuf;
    mCapSize = capSize;
    mCapMask = capSize - 1;
    mCapWrite = 0;
    for(int i = 0; i < capSize; ++i)
        mCapBuf[i] = 0.0f;
    for(int i = 0; i < kMaxBlock; ++i)
        mFeedBuf[i] = 0.0f;

    int offL = 0, offR = 0;
    for(int k = 0; k < kApStages; ++k)
    {
        mApBufL[k] = mApPoolL + offL;
        mApBufR[k] = mApPoolR + offR;
        offL += kApSizeL[k];
        offR += kApSizeR[k];
        mApIdxL[k] = 0;
        mApIdxR[k] = 0;
    }
    for(int i = 0; i < kApTotalL; ++i)
        mApPoolL[i] = 0.0f;
    for(int i = 0; i < kApTotalR; ++i)
        mApPoolR[i] = 0.0f;

    for(int i = 0; i < kMaxGrains; ++i)
        mGrains[i] = Grain{};

    BuildEnvTables();
}

void DiracEngine::SetSample(const DiracSample &s)
{
    mSample = s;
    if(mSample.frames <= 0)
        mSample.data = nullptr;
    if(mSample.rateRatio <= 0.0f)
        mSample.rateRatio = 1.0f;
}

/* ── PRNG ──────────────────────────────────────────────────────────────── */

float DiracEngine::RandUnipolar()
{
    mRandState ^= mRandState << 13;
    mRandState ^= mRandState >> 17;
    mRandState ^= mRandState << 5;
    return float(mRandState & 0x7FFFFFFFu) / float(0x7FFFFFFFu);
}
float DiracEngine::RandBipolar()
{
    return RandUnipolar() * 2.0f - 1.0f;
}

/* ── envelope tables ───────────────────────────────────────────────────── */
/* Build the THREE morph tables ONCE. Texture (captured per grain at spawn)
 * blends two of them: Percussive (0) → Hann (0.5) → Tukey (1). Read by phase
 * in [0, kEnvTableSize]; the last index is a 0 guard point. */
void DiracEngine::BuildEnvTables()
{
    const float taper = 0.125f; // Tukey cosine-taper fraction at each end
    const float atk = 0.02f;    // percussive attack fraction
    const float rel = 0.04f;    // percussive release-to-zero taper
    const float decayRatio
        = expf(-4.0f / ((1.0f - atk) * float(kEnvTableSize)));
    const float invRel = 1.0f / rel;
    float decayVal = 1.0f;
    bool inDecay = false;
    for(int i = 0; i <= kEnvTableSize; ++i)
    {
        const float t = float(i) / float(kEnvTableSize);

        mHannTable[i] = 0.5f * (1.0f - cosf(kTwoPi * t));

        if(t < taper)
            mTukeyTable[i] = 0.5f * (1.0f - cosf(kPi * t / taper));
        else if(t > 1.0f - taper)
            mTukeyTable[i] = 0.5f * (1.0f - cosf(kPi * (1.0f - t) / taper));
        else
            mTukeyTable[i] = 1.0f;

        float perc;
        if(t < atk)
        {
            perc = t / atk;
        }
        else
        {
            if(!inDecay)
            {
                inDecay = true;
                decayVal = 1.0f;
            }
            else
            {
                decayVal *= decayRatio;
            }
            perc = decayVal;
        }
        if(t > 1.0f - rel)
            perc *= 0.5f * (1.0f - cosf(kPi * (1.0f - t) * invRel));
        mPercTable[i] = perc;
    }
    mPercTable[0] = mHannTable[0] = mTukeyTable[0] = 0.0f;
    mPercTable[kEnvTableSize] = mHannTable[kEnvTableSize]
        = mTukeyTable[kEnvTableSize] = 0.0f;

    /* Window BANKS: 5 variants per fixed shape, built ONCE. Variant 2 is the
     * canonical shape (textr 0.5); the Texture dial blends adjacent variants
     * at render, so in each Window mode Texture is that shape's own macro:
     * bell width | sinc ring density | tri curvature | decay/ramp time |
     * tukey taper | square edge. NO rebuilds at runtime. */
    {
        const float kSigma[kWinVariants] = {0.06f, 0.09f, 0.13f, 0.18f, 0.25f};
        const float kLobes[kWinVariants] = {2.0f, 3.0f, 4.0f, 6.0f, 9.0f};
        const float kTriPow[kWinVariants] = {2.5f, 1.6f, 1.0f, 0.7f, 0.45f};
        const float kFallK[kWinVariants] = {12.0f, 8.5f, 6.0f, 4.0f, 2.5f};
        const float kTaper[kWinVariants] = {0.02f, 0.06f, 0.125f, 0.25f, 0.5f};
        const float kSqEdge[kWinVariants]
            = {0.003f, 0.006f, 0.01f, 0.03f, 0.08f};
        const float kEdge = 0.01f;
        const float invEdge = 1.0f / kEdge;
        for(int v = 0; v < kWinVariants; ++v)
        {
            float *bell = mWinBank[0][v], *sinc = mWinBank[1][v],
                  *tri = mWinBank[2][v], *dec = mWinBank[3][v],
                  *ramp = mWinBank[4][v], *tuk = mWinBank[5][v],
                  *sq = mWinBank[6][v];
            const float sqE = kSqEdge[v], invSqE = 1.0f / sqE;
            const float tap = kTaper[v];
            for(int i = 0; i <= kEnvTableSize; ++i)
            {
                const float t = float(i) / float(kEnvTableSize);
                { // BELL — Gaussian, width by variant
                    const float z = (t - 0.5f) / kSigma[v];
                    bell[i] = expf(-0.5f * z * z);
                }
                { // SINC — SIGNED ringing, lobe count by variant
                    const float x = (t - 0.5f) * 2.0f * kLobes[v];
                    sinc[i] = (fabsf(x) < 1e-6f) ? 1.0f : sinf(kPi * x) / (kPi * x);
                }
                { // TRI — curvature by variant (spike .. linear .. bulge)
                    /* expf(k*logf(x)) rather than powf: powf is ~800 bytes of
                     * flash on its own and expf/logf are already linked. */
                    const float b = 1.0f - fabsf(2.0f * t - 1.0f);
                    tri[i] = (b > 0.0f) ? expf(kTriPow[v] * logf(b)) : 0.0f;
                }
                { // DECAY / RAMP — fall/rise time by variant, tapered edges
                    const float eIn
                        = (t < kEdge) ? 0.5f * (1.0f - cosf(kPi * t * invEdge)) : 1.0f;
                    const float eOut = (t > 1.0f - kEdge)
                                           ? 0.5f * (1.0f - cosf(kPi * (1.0f - t) * invEdge))
                                           : 1.0f;
                    dec[i] = eIn * expf(-kFallK[v] * t);
                    ramp[i] = eOut * expf(-kFallK[v] * (1.0f - t));
                }
                { // TUKEY — taper fraction by variant (near-square .. hann)
                    if(t < tap)
                        tuk[i] = 0.5f * (1.0f - cosf(kPi * t / tap));
                    else if(t > 1.0f - tap)
                        tuk[i] = 0.5f * (1.0f - cosf(kPi * (1.0f - t) / tap));
                    else
                        tuk[i] = 1.0f;
                }
                { // SQUARE — edge hardness by variant
                    const float eIn
                        = (t < sqE) ? 0.5f * (1.0f - cosf(kPi * t * invSqE)) : 1.0f;
                    const float eOut = (t > 1.0f - sqE)
                                           ? 0.5f * (1.0f - cosf(kPi * (1.0f - t) * invSqE))
                                           : 1.0f;
                    sq[i] = eIn * eOut;
                }
            }
            for(int sh = 0; sh < kWinShapes; ++sh)
            { // click-free guarantees
                mWinBank[sh][v][0] = 0.0f;
                mWinBank[sh][v][kEnvTableSize] = 0.0f;
            }
        }
    }
}

/* ── source read (linear interpolation) ────────────────────────────────── */
/* All object state passed in as plain args so the render loop pre-loads the
 * pointers ONCE per grain per block — the out-buffer stores are same-typed
 * float writes, so the compiler cannot hoist the member loads itself. */
inline float DiracEngine::ReadHoisted(const float *cap, int capMask,
                                      const float *sd, int frames, bool live,
                                      float pos)
{
    const float fp = floorf(pos);
    const float f = pos - fp;
    if(live)
    {
        const int i0 = int(fp) & capMask;
        const int i1 = (i0 + 1) & capMask;
        return cap[i0] + f * (cap[i1] - cap[i0]);
    }
    if(!sd || frames <= 0)
        return 0.0f;
    /* pos is kept in [0, frames) by the render loop, so NO per-sample modulo.
     * Defensive clamp: a tiny sample + extreme pitch can leave i0 ≥ frames. */
    int i0 = int(fp);
    if(i0 >= frames)
        i0 = frames - 1;
    else if(i0 < 0)
        i0 = 0;
    const int i1 = (i0 + 1 >= frames) ? 0 : i0 + 1;
    return sd[i0] + f * (sd[i1] - sd[i0]);
}

/* Convenience wrapper for cold paths (spawn-time snap/compress peeks). */
inline float DiracEngine::ReadSrc(float pos, bool live, int frames) const
{
    return ReadHoisted(mCapBuf, mCapMask, mSample.data, frames, live, pos);
}

/* Normalise a read position to [0,1] for the grain field. */
float DiracEngine::NormPos(float p) const
{
    if(mSample.data && mSample.frames > 0)
    {
        const float n = float(mSample.frames);
        const float q = p - n * floorf(p / n);
        return q / n;
    }
    const float n = float(mCapSize);
    float age = float(mCapWrite) - p;
    age -= n * floorf(age / n);
    return 1.0f - age / n;
}

/* ── FindSlot ──────────────────────────────────────────────────────────── */
/* Return a free grain slot, or -1 at capacity. We deliberately do NOT steal an
 * active grain: hard-cutting a still-playing grain is an audible click. At
 * capacity we skip the spawn — density self-limits to the Grains cap. */
int DiracEngine::FindSlot(int grainsCap)
{
    int nActive = 0;
    for(int i = 0; i < kMaxGrains; ++i)
        if(mGrains[i].active)
            nActive++;
    if(nActive >= grainsCap)
        return -1;
    for(int i = 0; i < kMaxGrains; ++i)
        if(!mGrains[i].active)
            return i;
    return -1;
}

/* ── SpawnGrain ────────────────────────────────────────────────────────── */
void DiracEngine::SpawnGrain(float playhead, int frames, float posJtr, float sprd,
                             float detune, float level, bool reverseProb,
                             float psprd, int grainsCap, bool held,
                             float semiShift, int grainDuration, float hopSamples,
                             int sampleOffset, bool liveMode, float compress,
                             float binaural, int scaleSel, int heads,
                             float headSpread, float headTune, float snap,
                             float feedback, float color, float tilt,
                             float tiltSprd)
{
    const int slot = FindSlot(grainsCap);
    if(slot < 0)
        return;
    Grain &g = mGrains[slot];

    /* Sample-mode feedback: regeneration grains. In live mode every grain reads
     * the ring, so feedback regenerates by construction. In sample mode the ring
     * is a regeneration bed and a FEEDBACK-SIZED SHARE of new grains reads it
     * instead of the sample: fdbk 0.3 → ~30% regen grains in the cloud. */
    bool srcLive = liveMode;
    if(!liveMode && feedback > 0.0001f && RandUnipolar() < feedback)
        srcLive = true;

    /* Multi-playhead: deal this grain to the next of N playheads (round robin).
     * Head h reads the SAME buffer, offset in position and transposed by
     * h·HeadTune. heads <= 1 short-circuits to the single-head path. */
    int head = 0;
    float headOff = 0.0f;
    if(heads > 1)
    {
        head = mHeadCycle % heads;
        mHeadCycle = (mHeadCycle + 1) % heads;
        headOff = (float(head) * headSpread) / float(heads);
    }

    const float overlap
        = (hopSamples > 0.0f) ? float(grainDuration) / hopSamples : 1.0f;
    const float densComp = (overlap > 1.0f) ? (1.0f / sqrtf(overlap)) : 1.0f;
    float scale = level * densComp;

    /* Tilt: the envelope phase reaches the TABLE MIDPOINT after tilt·duration
     * samples and the end after the remainder — two rates, one compare at
     * render. TiltSpread draws each grain's own peak position around Tilt;
     * gated so tsprd 0 makes no PRNG draw (preserving the random sequence). */
    float tiltEff = tilt;
    if(tiltSprd > 0.0001f)
    {
        tiltEff += tiltSprd * RandBipolar();
        if(tiltEff < -1.0f)
            tiltEff = -1.0f;
        else if(tiltEff > 1.0f)
            tiltEff = 1.0f;
    }
    const float tiltPos = 0.5f + 0.5f * tiltEff;
    const float tiltCl
        = (tiltPos < 0.02f) ? 0.02f : (tiltPos > 0.98f ? 0.98f : tiltPos);
    const float kHalfTab = float(kEnvTableSize / 2);
    const float envIncrA = kHalfTab / (tiltCl * float(grainDuration));
    const float envIncrB = kHalfTab / ((1.0f - tiltCl) * float(grainDuration));
    const bool detuned = (detune > 0.0001f);
    const float detuneRatio = detuned ? expf(kLog2Over12 * detune) : 1.0f;

    /* Base read position (sample index, or ring index for the live scan).
     *
     * Live reads anchor BEHIND the write head by a delay, so grains pick up the
     * freshly fed-back output and Feedback actually regenerates. Playhead is a
     * DELAY-TIME control in live mode: 0 → freshest feedback (tightest
     * regeneration), 1 → longest delay (long echo / smear). */
    float headGap = 0.0f; // spacing between adjacent playheads (Snap lane width)
    float basePos;
    if(liveMode)
    {
        const float minDelay = float(grainDuration);
        const float maxDelay = float(mCapSize - grainDuration);
        const float delay = minDelay + playhead * (maxDelay - minDelay);
        basePos = float(mCapWrite) - delay; // negative is fine: the read masks
        /* Head offset scaled into the ROOM remaining behind the knob position,
         * so heads always fit and head order stays monotonic near playh = 1. */
        const float room = maxDelay - delay;
        basePos -= headOff * room;
        headGap = (heads > 1) ? (headSpread / float(heads)) * room : 0.0f;
    }
    else if(srcLive)
    {
        /* Sample-mode REGEN grain: tight anchor just behind the write head,
         * where the freshly reinjected wet output lives. The 4800-sample margin
         * keeps posJtr's spray behind the write head; the seam fade does the
         * rest. */
        basePos = float(mCapWrite) - float(grainDuration) - 4800.0f;
    }
    else
    {
        const float span = float(frames > 1 ? frames - 1 : 0);
        basePos = playhead * span + headOff * span;
        if(frames > 0) // wrap the head offset into the sample
            basePos -= float(frames) * floorf(basePos / float(frames));
        headGap = (heads > 1) ? (headSpread / float(heads)) * span : 0.0f;
    }

    /* Pitch offset from the played root = Psprd scatter + this head's chord rung
     * (HeadTune = 7 → root, +7, +14 …). With Scale > 0 the COMBINED offset snaps
     * to the scale: the htune ladder becomes DIATONIC. Quantized BEFORE
     * semiShift, so the ROOT stays free — V/Oct melodies are never snapped. */
    float offSemi = (psprd > 0.0f) ? (RandBipolar() * psprd) : 0.0f;
    offSemi += float(head) * headTune;
    if(scaleSel > 0)
        offSemi = SnapToScale(offSemi, scaleSel);
    const float semis = semiShift + offSemi;
    /* File-rate correction folds into the read speed for sample grains only —
     * regen grains read the ring, which is always at the engine rate. */
    const float srcRatio = srcLive ? 1.0f : mSample.rateRatio;
    const float incL = expf(kLog2Over12 * semis) * srcRatio;

    const float jit = (posJtr > 0.0f) ? (posJtr * 4800.0f * RandBipolar()) : 0.0f;
    float startPos = basePos + jit;

    /* ── SNAP: onset-locked spawning ────────────────────────────────────────
     * Normally a grain reads wherever the metronome puts it, so granulating
     * rhythmic material lands in the gaps as often as on the hits. Snap scans a
     * window around the target for the strongest ENERGY RISE and pulls the read
     * there. Budgeted to kSnapPerBlock searches per block: bounds the worst-case
     * cost at short-grain/high-density settings where snapping is inaudible.
     * Per-spawn (not cached) is deliberate — posJtr sprays each grain to a
     * different region and Snap reels each onto a DIFFERENT hit. */
    if(snap > 0.0001f && mSnapBudget > 0)
    {
        --mSnapBudget;
        /* COARSE → FINE: each stage tests kCand positions across a window, then
         * the next stage re-searches one grid-step around the winner. Three
         * stages of 12 give ~30-sample resolution over ±75 ms for 36 probes. */
        const int kCand = 12;
        const int kTaps = 5;      // energy taps either side
        const float kStep = 40.0f; // tap spacing (samples)
        float win = 3600.0f;       // ±75 ms, shrinks each stage
        /* Keep each head in its OWN lane: the ±75 ms window is wider than the
         * head spacing at moderate HeadSpread, so heads would wander onto each
         * other's transients and the position ladder would collapse. */
        const float kMinLane = 400.0f;
        if(headGap > 0.0f)
        {
            const float lane = headGap * 0.5f;
            if(lane >= kMinLane && lane < win)
                win = lane;
        }
        float bestPos = startPos, bestScore = -1e30f;
        for(int stage = 0; stage < 3; ++stage)
        {
            const float centre = bestPos;
            float stagePos = centre, stageScore = -1e30f;
            for(int c = 0; c < kCand; ++c)
            {
                const float f = (float(c) / float(kCand - 1)) * 2.0f - 1.0f;
                float cand = centre + win * f;
                if(!srcLive && frames > 0) // keep the probe inside the sample
                    cand -= float(frames) * floorf(cand / float(frames));
                float after = 0.0f, before = 0.0f;
                for(int k = 1; k <= kTaps; ++k)
                {
                    const float a = ReadSrc(cand + float(k) * kStep, srcLive, frames);
                    const float b = ReadSrc(cand - float(k) * kStep, srcLive, frames);
                    after += a * a;
                    before += b * b;
                }
                const float score = after - before; // positive = rising edge
                if(score > stageScore)
                {
                    stageScore = score;
                    stagePos = cand;
                }
            }
            bestScore = stageScore;
            bestPos = stagePos;
            win *= 2.0f / float(kCand - 1); // next window = one grid step
        }
        if(bestScore > 0.0f) // only pull toward a real rise
            startPos += snap * (bestPos - startPos);
    }

    /* ── Per-grain leveling ("compression") ──────────────────────────────────
     * Peek the source where this grain will read, estimate its level, and apply
     * a makeup gain toward a target so quiet source bits still speak in sparse
     * clouds. Clamped to ±12 dB, blended by `compress`. 6 reads at spawn. */
    if(compress > 0.0001f)
    {
        const int nPeek = 6;
        const float span = float(grainDuration) * incL;
        float sum = 0.0f;
        for(int k = 0; k < nPeek; ++k)
        {
            float pp = startPos + span * (float(k) / float(nPeek - 1));
            if(!srcLive && frames > 0) // wrap to match the grain's read
                pp -= float(frames) * floorf(pp / float(frames));
            const float sv = ReadSrc(pp, srcLive, frames);
            sum += sv * sv;
        }
        const float localRMS = sqrtf(sum / float(nPeek));
        const float kTarget = 0.3f;
        float makeup = kTarget / std::max(localRMS, 0.02f); // don't boost silence
        if(makeup > 4.0f)
            makeup = 4.0f; // +12 dB ceiling
        else if(makeup < 0.25f)
            makeup = 0.25f; // −12 dB floor
        scale *= 1.0f + compress * (makeup - 1.0f);
    }

    g.duration = grainDuration;
    g.age = 0;
    g.envPhase = 0.0f;
    g.envIncrA = envIncrA;
    g.envIncrB = envIncrB;
    g.envTex = mCurTexture; // captured for this grain's life (no mid-grain jump)
    g.envShape = int8_t(mCurWindow);
    g.reverse = reverseProb;
    g.live = srcLive;
    g.pitchIncrL = incL;
    g.pitchIncrR = incL * detuneRatio;
    g.scale = scale;
    g.held = held;

    // stereo pan (equal-power, centre = unity) — the azimuth proxy
    float pan = RandBipolar() * sprd;
    if(pan < -1.0f)
        pan = -1.0f;
    else if(pan > 1.0f)
        pan = 1.0f;
    g.panPos = pan;
    g.gainL = sqrtf(1.0f - pan);
    g.gainR = sqrtf(1.0f + pan);

    /* Binaural (ITD + head shadow), scaled by `binaural`. Azimuth = pan·90°.
     * The FAR ear reads a delayed position (±31 samples at full side) through a
     * one-pole LP whose cutoff falls 20 kHz→4 kHz. binaural = 0 → plain pan. */
    float delayL = 0.0f, delayR = 0.0f;
    g.lpStateL = 0.0f;
    g.lpStateR = 0.0f;
    g.lpAlphaL = 1.0f;
    g.lpAlphaR = 1.0f;
    if(binaural > 0.0001f)
    {
        const float sinAz = sinf(pan * (kPi * 0.5f));
        const float itd = kMaxITDSamples * sinAz * binaural;
        delayL = (itd > 0.0f) ? itd : 0.0f; // source right → left delayed
        delayR = (itd < 0.0f) ? -itd : 0.0f;
        const float shadow = fabsf(sinAz) * binaural;
        const float fc = 20000.0f - 16000.0f * shadow;
        const float alpha = 1.0f - expf(-kTwoPi * fc / mSampleRate);
        if(sinAz >= 0.0f)
            g.lpAlphaL = alpha; // left is the far (shadowed) ear
        else
            g.lpAlphaR = alpha;
        g.stereo = true; // ITD needs the separate R read
    }
    else
    {
        g.stereo = detuned;
    }

    /* COLOR: per-grain cutoff scatter — the spectral Psprd. Each grain draws a
     * random low-pass cutoff, log-uniform from 20 kHz down to ~600 Hz at
     * Color = 1. Reuses the head-shadow one-pole (min-combined per ear). */
    if(color > 0.0001f)
    {
        const float u = RandUnipolar();
        const float fc = 20000.0f * expf(-3.5f * color * u);
        const float aC = 1.0f - expf(-kTwoPi * fc / mSampleRate);
        if(g.lpAlphaL > aC)
            g.lpAlphaL = aC;
        if(g.lpAlphaR > aC)
            g.lpAlphaR = aC;
    }

    g.readPosL = startPos - delayL;
    g.readPos0L = g.readPosL;
    g.readPosR = startPos - delayR;
    g.readPos0R = g.readPosR;

    g.startDelay = sampleOffset;
    g.pitchSemi = semis; // includes the head transpose → heads stack in the viz
    g.active = true;
}

/* ── process ───────────────────────────────────────────────────────────── */
void DiracEngine::Process(const float *in, const float *trig, float *outL,
                          float *outR, int n, const DiracParams &p)
{
    const int blockSize = (n > kMaxBlock) ? kMaxBlock : n;

    for(int s = 0; s < blockSize; ++s)
    {
        outL[s] = 0.0f;
        outR[s] = 0.0f;
    }

    const int frames = (mSample.data != nullptr) ? mSample.frames : 0;
    const bool liveMode = (frames <= 0);

    /* ── Parameters ─────────────────────────────────────────────────────
     * Density = target grain overlap. 0 = trigger-only; otherwise the spawn
     * rate is derived from grain length so grains overlap. */
    const float dens = std::max(0.0f, std::min(p.density, 16.0f));
    const float playhead = std::max(0.0f, std::min(p.playhead, 1.0f));
    const float posJtr = std::max(0.0f, std::min(p.posJtr, 1.0f));
    const float sprd = std::max(0.0f, std::min(p.spread, 1.0f));
    const float detune = std::max(0.0f, std::min(p.detune, 2.0f));
    const float level = std::max(0.0f, std::min(p.level, 1.0f));
    const float mix = std::max(0.0f, std::min(p.mix, 1.0f));
    const float feedback = std::max(0.0f, std::min(p.feedback, 1.0f));
    const float compress = std::max(0.0f, std::min(p.compress, 1.0f));
    const float binaural = std::max(0.0f, std::min(p.binaural, 1.0f));
    const int scale = std::max(0, std::min(p.scale, 6));
    const float revprob = std::max(0.0f, std::min(p.revProb, 1.0f));
    const float psprd = std::max(0.0f, std::min(p.psprd, 12.0f));
    const float texture = std::max(0.0f, std::min(p.texture, 1.0f));
    const int grains = std::max(1, std::min(p.grains, kMaxGrains));
    const bool holdOn = p.hold;
    /* Transpose = SemiShift (coarse, integer semitones) + V/Oct (smooth).
     * Clamped to ±4 octaves so extreme reads don't alias into garbage. */
    const float semiKnob = roundf(std::max(-24.0f, std::min(p.semiShift, 24.0f)));
    const float semiShift = std::max(
        -kMaxTranspose, std::min(semiKnob + p.voctOct * 12.0f, kMaxTranspose));
    const float speed = std::max(-4.0f, std::min(p.speed, 4.0f));
    /* Multi-playhead. The option hard-bypasses the feature when off, so a stray
     * Heads CV can't engage it and the spawn path stays on the classic branch. */
    const int heads
        = p.multiHead ? std::max(1, std::min(p.heads, kMaxHeads)) : 1;
    const float headSpread = std::max(0.0f, std::min(p.headSpread, 1.0f));
    const float headTune = std::max(-12.0f, std::min(p.headTune, 12.0f));
    const float snap = std::max(0.0f, std::min(p.snap, 1.0f));
    const float color = std::max(0.0f, std::min(p.color, 1.0f));
    const float tilt = std::max(-1.0f, std::min(p.tilt, 1.0f));
    const float tiltSprd = std::max(0.0f, std::min(p.tiltSprd, 1.0f));
    const float grainLenSec = std::max(
        0.001f, std::min(p.grainLenSec, float(kMaxGrainSamp) / mSampleRate));
    const int grainDuration = std::max(
        64, std::min(int(grainLenSec * mSampleRate), kMaxGrainSamp));

    /* ── Capture into the ring (+ feedback reinjection) ──────────────────
     * Live mode: grains read the ring, so feedback regenerates by construction.
     * Sample mode: the ring is the REGENERATION BED — wet output is written
     * here and a feedback-sized share of grains reads it instead of the sample.
     * Freeze stops ALL writes: the ring locks while every control stays live —
     * the source freezes, not the cloud (that's Hold). */
    if(!p.freeze)
    {
        if(feedback > 0.0001f)
        {
            if(liveMode)
            {
                for(int s = 0; s < blockSize; ++s)
                {
                    mCapBuf[mCapWrite] = Sanitize(in[s] + feedback * mFeedBuf[s]);
                    mCapWrite = (mCapWrite + 1) & mCapMask;
                }
            }
            else
            {
                /* Sample mode: the ring is a PURE regeneration bed — feedback
                 * only, NO input. Selecting a sample must isolate the unit from
                 * its input; regen self-bootstraps from the sample grains' own
                 * wet output. */
                for(int s = 0; s < blockSize; ++s)
                {
                    mCapBuf[mCapWrite] = Sanitize(feedback * mFeedBuf[s]);
                    mCapWrite = (mCapWrite + 1) & mCapMask;
                }
            }
        }
        else
        {
            for(int s = 0; s < blockSize; ++s)
            {
                mCapBuf[mCapWrite] = Sanitize(in[s]);
                mCapWrite = (mCapWrite + 1) & mCapMask;
            }
        }
    }

    /* Envelope tables are static; each grain captures Texture and Window at
     * spawn and blends its own shape at render, so modulating them never
     * disturbs a playing grain (no clicks) and never rebuilds anything. */
    mCurTexture = texture;
    mCurWindow = (p.window < 0) ? 0 : (p.window > kWinShapes ? kWinShapes : p.window);

    /* ── Playhead scan: Speed decouples TIME from PITCH (sample mode).
     * Grain pitch is per-grain read speed; Speed moves WHERE grains spawn:
     * 0 = parked (the knob is the position), ±1 = original tempo
     * forward/reverse, wrapping at the ends. Moving the Playhead knob re-seats
     * the scan. Live mode ignores Speed. Block-rate only. */
    float playheadEff = playhead;
    if(!liveMode)
    {
        const float fn = float(frames);
        if(speed != 0.0f && frames > 1)
        {
            if(fabsf(playhead - mLastPlayheadKnob) > 0.0005f)
                mScanPos = playhead * fn; // knob touched → re-seat scan
            /* Accumulated in FRAMES rather than normalised, and in float rather
             * than the original's double: a float holds integers exactly up to
             * 2^24 (16.7 M frames, well past the 8 M sample store), and the
             * per-block increment is at most speed*blockSize = 192 frames, so
             * it never vanishes into the accumulator. Normalised double was the
             * ER-301's answer to the same precision problem; this one costs no
             * soft-float double routines, which the flash budget cares about. */
            mScanPos += speed * float(blockSize);
            mScanPos -= fn * floorf(mScanPos / fn);
            playheadEff = mScanPos / fn;
        }
        else
        {
            mScanPos = playhead * fn; // parked: follow the knob
        }
        mLastPlayheadKnob = playhead;
    }
    mPlayheadEff = playheadEff;

    /* Density → spawn period. overlap = grains deep; hop = grainLen / overlap.
     * Floored at kDensFloor so even the lowest setting spawns regularly.
     * dens < 0.05 → trigger-only (free-run off). */
    const bool freeRun = (dens >= 0.05f);
    const float overlap = (dens > kDensFloor) ? dens : kDensFloor;
    const float hopSamples
        = freeRun ? (float(grainDuration) / overlap) : float(grainDuration);

    /* ── Spawn scheduling: free-run at the density + trig edges (decoupled) ── */
    mSnapBudget = kSnapPerBlock;
    if(freeRun)
    {
        mSpawnTimer -= float(blockSize);
        while(mSpawnTimer <= 0.0f)
        {
            int spawnAt = blockSize + int(mSpawnTimer);
            if(spawnAt < 0)
                spawnAt = 0;
            if(spawnAt >= blockSize)
                spawnAt = blockSize - 1;
            SpawnGrain(playheadEff, frames, posJtr, sprd, detune, level,
                       (revprob > 0.0f) && (RandUnipolar() < revprob), psprd,
                       grains, holdOn, semiShift, grainDuration, hopSamples,
                       spawnAt, liveMode, compress, binaural, scale, heads,
                       headSpread, headTune, snap, feedback, color, tilt,
                       tiltSprd);
            mSpawnTimer += hopSamples;
        }
    }
    else
    {
        mSpawnTimer = 0.0f;
    }
    if(trig)
    {
        float last = mLastFire;
        for(int s = 0; s < blockSize; ++s)
        {
            if(trig[s] > 0.5f && last <= 0.5f)
            {
                SpawnGrain(playheadEff, frames, posJtr, sprd, detune, level,
                           (revprob > 0.0f) && (RandUnipolar() < revprob), psprd,
                           grains, holdOn, semiShift, grainDuration, hopSamples,
                           s, liveMode, compress, binaural, scale, heads,
                           headSpread, headTune, snap, feedback, color, tilt,
                           tiltSprd);
            }
            last = trig[s];
        }
        mLastFire = last;
    }

    /* ── Render active grains (time-domain overlap-add) ──────────────────── */
    const float *capData = mCapBuf;
    const int capMask = mCapMask;
    const float *smpData = mSample.data;

    for(int i = 0; i < kMaxGrains; ++i)
    {
        Grain &g = mGrains[i];
        if(!g.active)
            continue;

        /* startDelay is the within-block spawn offset (always < blockSize), so
         * it takes effect this same block; it never carries across blocks. */
        const int outStart = g.startDelay;
        const int toRender = std::min(g.duration - g.age, blockSize - outStart);
        if(toRender <= 0)
            continue;
        const bool live = g.live;
        const bool st = g.stereo;
        const float gL = g.gainL * g.scale;
        const float gR = g.gainR * g.scale;
        const float incL = g.reverse ? -g.pitchIncrL : g.pitchIncrL;
        const float incR = g.reverse ? -g.pitchIncrR : g.pitchIncrR;
        float readL = g.readPosL, readR = g.readPosR, envPh = g.envPhase;
        const float envIncA = g.envIncrA, envIncB = g.envIncrB;
        const float kHalfT = float(kEnvTableSize / 2);
        const float lpAL = g.lpAlphaL, lpAR = g.lpAlphaR;
        float lpL = g.lpStateL, lpR = g.lpStateR;
        const bool doShadow = (lpAL < 0.9999f) || (lpAR < 0.9999f);
        /* This grain's envelope = blend of two static tables by its captured
         * Texture. At an exact node (Texture 0 or the 0.5 default) ew == 0 →
         * skip the second table lerp entirely. */
        const float *eA, *eB;
        float ew;
        if(g.envShape > 0)
        { // fixed Window: Texture blends the shape's BANK
            const float pos = g.envTex * float(kWinVariants - 1);
            int k = int(pos);
            if(k > kWinVariants - 2)
                k = kWinVariants - 2;
            if(k < 0)
                k = 0;
            eA = mWinBank[g.envShape - 1][k];
            eB = mWinBank[g.envShape - 1][k + 1];
            ew = pos - float(k);
        }
        else if(g.envTex < 0.5f)
        {
            eA = mPercTable;
            eB = mHannTable;
            ew = g.envTex * 2.0f;
        }
        else
        {
            eA = mHannTable;
            eB = mTukeyTable;
            ew = (g.envTex - 0.5f) * 2.0f;
        }
        const bool blend = (ew != 0.0f);

        /* Edge fade (anti-click): fade the grain as its read nears a SEAM — the
         * write head in live mode (read/write collision guard) or the sample
         * loop boundary in sample mode. In a dense cloud the overlapping grains
         * cover the dip, so it reads as a crossfade. */
        const float period = live ? float(mCapSize) : float(frames);
        const float halfP = 0.5f * period;
        const bool doSeam = live ? true : (frames > int(4.0f * kSeamGuard));
        /* The R read gets its OWN seam distance: with Detune it runs at a
         * different speed and drifts far from L, and could cross a seam
         * unfaded (R-channel click). */
        float relSeamL = 0.0f, relSeamR = 0.0f;
        if(doSeam)
        {
            const float basePL = live ? (readL - float(mCapWrite)) : readL;
            relSeamL = basePL - period * floorf(basePL / period);
            if(st)
            {
                const float basePR = live ? (readR - float(mCapWrite)) : readR;
                relSeamR = basePR - period * floorf(basePR / period);
            }
        }

        /* Sample mode: keep read positions wrapped to [0, frames) so the
         * per-sample read uses a compare/subtract instead of a modulo. Live
         * mode masks in the read. */
        const float wN = float(frames);
        if(!live)
        {
            readL -= wN * floorf(readL / wN);
            readR -= wN * floorf(readR / wN);
        }

        /* Gate the per-sample edge fade: only run it when the read can actually
         * come within kSeamGuard of the seam THIS block. */
        bool seamActive = false;
        if(doSeam)
        {
            const float distL = (relSeamL < halfP) ? relSeamL : (period - relSeamL);
            const float travL = ((incL >= 0.0f) ? incL : -incL) * float(toRender);
            seamActive = (distL <= kSeamGuard + travL + 1.0f);
            if(st && !seamActive)
            {
                const float distR
                    = (relSeamR < halfP) ? relSeamR : (period - relSeamR);
                const float travR = ((incR >= 0.0f) ? incR : -incR) * float(toRender);
                seamActive = (distR <= kSeamGuard + travR + 1.0f);
            }
        }

        for(int s = 0; s < toRender; ++s)
        {
            int ei = int(envPh);
            /* envPh is a float ACCUMULATOR; on very long grains the rounding
             * drift can nudge the final reads up to kEnvTableSize, and
             * eA[ei + 1] would then read past the table. */
            if(ei > kEnvTableSize - 1)
                ei = kEnvTableSize - 1;
            else if(ei < 0)
                ei = 0;
            const float fr = envPh - float(ei);
            float env = eA[ei] + fr * (eA[ei + 1] - eA[ei]);
            if(blend)
            {
                const float eb = eB[ei] + fr * (eB[ei + 1] - eB[ei]);
                env += ew * (eb - env);
            }
            float fadeL = 1.0f, fadeR = 1.0f;
            if(seamActive)
            {
                const float distL
                    = (relSeamL < halfP) ? relSeamL : (period - relSeamL);
                if(distL < kSeamGuard)
                    fadeL = distL * kInvSeamGuard;
                relSeamL += incL;
                if(relSeamL >= period)
                    relSeamL -= period;
                else if(relSeamL < 0.0f)
                    relSeamL += period;
                if(st)
                { // R fades on its own seam distance
                    const float distR
                        = (relSeamR < halfP) ? relSeamR : (period - relSeamR);
                    if(distR < kSeamGuard)
                        fadeR = distR * kInvSeamGuard;
                    relSeamR += incR;
                    if(relSeamR >= period)
                        relSeamR -= period;
                    else if(relSeamR < 0.0f)
                        relSeamR += period;
                }
                else
                {
                    fadeR = fadeL;
                }
            }
            const float eL = env * fadeL;
            const float eR = env * fadeR;
            const float sL
                = ReadHoisted(capData, capMask, smpData, frames, live, readL);
            const float sR
                = st ? ReadHoisted(capData, capMask, smpData, frames, live, readR)
                     : sL;
            if(doShadow)
            { // binaural / colour: one-pole LP
                lpL += lpAL * (sL - lpL);
                lpR += lpAR * (sR - lpR);
                outL[outStart + s] += gL * eL * lpL;
                outR[outStart + s] += gR * eR * lpR;
            }
            else
            { // common case: no filter, no extra work
                outL[outStart + s] += gL * eL * sL;
                outR[outStart + s] += gR * eR * sR;
            }
            readL += incL;
            readR += incR;
            envPh += (envPh < kHalfT) ? envIncA : envIncB; // Tilt: peak-relative
            if(!live)
            {
                if(readL >= wN)
                    readL -= wN;
                else if(readL < 0.0f)
                    readL += wN;
                if(readR >= wN)
                    readR -= wN;
                else if(readR < 0.0f)
                    readR += wN;
            }
        }
        g.readPosL = readL;
        g.readPosR = readR;
        g.envPhase = envPh;
        g.lpStateL = lpL;
        g.lpStateR = lpR;
        g.age += toRender;
        g.startDelay = 0;
        if(g.age >= g.duration)
        {
            if(g.held && holdOn)
            {
                g.age = 0;
                g.envPhase = 0.0f;
                g.readPosL = g.readPos0L;
                g.readPosR = g.readPos0R;
            }
            else
            {
                g.active = false;
            }
        }
    }

    /* ── Diffuse: wet-path allpass halo ──────────────────────────────────
     * 4 cascaded Schroeder allpasses per channel (mutually-prime delays,
     * L ≠ R for stereo decorrelation, ~49 ms total path) smear grain edges into
     * a lush tail. Runs BEFORE the feedback tap so regeneration is diffused
     * too. Off = hard bypass, and the delay lines clear on Off so re-enabling
     * never replays a stale tail. */
    if(p.diffuse)
    {
        for(int s = 0; s < blockSize; ++s)
        {
            float xl = Sanitize(outL[s]), xr = Sanitize(outR[s]);
            for(int k = 0; k < kApStages; ++k)
            {
                {
                    const int j = mApIdxL[k];
                    const float z = mApBufL[k][j];
                    const float y = z - kApGain * xl;
                    mApBufL[k][j] = xl + kApGain * y;
                    if(++mApIdxL[k] >= kApSizeL[k])
                        mApIdxL[k] = 0;
                    xl = y;
                }
                {
                    const int j = mApIdxR[k];
                    const float z = mApBufR[k][j];
                    const float y = z - kApGain * xr;
                    mApBufR[k][j] = xr + kApGain * y;
                    if(++mApIdxR[k] >= kApSizeR[k])
                        mApIdxR[k] = 0;
                    xr = y;
                }
            }
            outL[s] = xl;
            outR[s] = xr;
        }
        mDiffusePrimed = true;
    }
    else if(mDiffusePrimed)
    {
        for(int i = 0; i < kApTotalL; ++i)
            mApPoolL[i] = 0.0f;
        for(int i = 0; i < kApTotalR; ++i)
            mApPoolR[i] = 0.0f;
        for(int k = 0; k < kApStages; ++k)
        {
            mApIdxL[k] = 0;
            mApIdxR[k] = 0;
        }
        mDiffusePrimed = false;
    }

    /* ── Feedback source: soft-clipped wet mono ──────────────────────────
     * `drive` is a makeup gain INSIDE the tanh: the round trip loses ~8 dB
     * (densComp + mono-sum + envelope average), so without it the loop never
     * sustains even at feedback ≈ 1. tanh still bounds the result → saturates
     * into a stable drone, no runaway. */
    if(feedback > 0.0001f && !p.freeze)
    {
        const float drive = 2.5f;
        /* Loop high-pass, fc ≈ 120 Hz @ 48 kHz. Not just a DC blocker: the tanh
         * RE-CREATES low-frequency intermodulation every pass, and with multiple
         * heads the audio band combs while LF keeps winning — the pedestal rails
         * the tanh and squeezes the audio out ("fades like an overload"). Same
         * corner class as the Clouds feedback path. */
        const float aHp = 0.98429f;
        for(int s = 0; s < blockSize; ++s)
        {
            const float mono = 0.5f * (outL[s] + outR[s]);
            const float hp = mono - mFbHpX + aHp * mFbHpY; // y = x − x₁ + a·y₁
            mFbHpX = mono;
            mFbHpY = hp;
            mFeedBuf[s] = Sanitize(FastTanh(drive * hp));
        }
        mFeedPrimed = true;
    }
    else if(mFeedPrimed)
    {
        /* Leaving the feedback path: clear so re-entering doesn't inject one
         * block of stale audio from whenever feedback last ran. */
        for(int s = 0; s < kMaxBlock; ++s)
            mFeedBuf[s] = 0.0f;
        mFbHpX = 0.0f;
        mFbHpY = 0.0f;
        mFeedPrimed = false;
    }

    /* ── Wet/dry Mix (LIVE mode only; dry = unity input passthrough) ────── */
    if(liveMode && mix < 0.99995f)
    {
        const float dry = 1.0f - mix;
        for(int s = 0; s < blockSize; ++s)
        {
            const float d = in[s] * dry;
            outL[s] = outL[s] * mix + d;
            outR[s] = outR[s] * mix + d;
        }
    }

    /* Final stage: scrub non-finite (downstream protection) then soft-limit so
     * dense-overlap spikes never hard-clip the DAC. Sanitize FIRST (SoftLimit's
     * compares are no-ops on NaN under -ffast-math). */
    for(int s = 0; s < blockSize; ++s)
    {
        outL[s] = SoftLimit(Sanitize(outL[s]));
        outR[s] = SoftLimit(Sanitize(outR[s]));
    }
}

} // namespace dirac
