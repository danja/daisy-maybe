#include <algorithm>
#include <cmath>

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"
#include "util/PersistentStorage.h"

#include "delay_lines.h"
#include "display.h"
#include "distortion.h"
#include "encoder_handler.h"
#include "filters.h"
#include "menu_system.h"
#include "res_midi.h"
#include "res_sample.h"
#include "resonator_models.h"
#include "sample_voice.h"

using namespace daisy;
using namespace kxmx;

namespace
{
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

    constexpr float kMinFreq = 10.0f;
    constexpr float kMaxFreq = 8000.0f;
    constexpr float kMaxFeed = 0.99f;
    constexpr float kCalibTone = 440.0f;
    constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);

    /* PersistentStorage indexes from the QSPI base, and offset 0 lands inside
     * the 256 KB the Daisy bootloader reserves at 0x90000000 — reading someone
     * else's bytes as calibration, and writing over theirs on save. 1 MB in is
     * clear of both that reserve and the app image at 0x90040000. */
    constexpr uint32_t kCalibStorageOffset = 0x100000;

    struct CalibSettings
    {
        float scale;
        float offset;

        bool operator!=(const CalibSettings &rhs) const
        {
            return scale != rhs.scale || offset != rhs.offset;
        }
    };

    struct DistortionParams
    {
        int folds = 3;
        /* Off by default. The wavefolder is a colour, not the module's voice,
         * and it used to be fully wet the moment you powered up. */
        float foldMix = 0.0f;
        float driveMix = 0.0f;
        /* How hard the folder and overdrive are driven. This used to be Pot 2;
         * Pot 2 is the feedback control now, so it lives on the menu. */
        float depth = 0.5f;
    };

    struct ResonatorParams
    {
        int model = 0;
        float ratio = 1.0f;
        float mix = 1.0f;
        int size = 1;
        int taps = 1;
        /* Pitch of the second delay/channel relative to the first. Separate
         * from `ratio`, which the models read as a timbre control (chord
         * spread, mode stretch, FM ratio) — one knob could not be both. */
        float offset = 1.0f;
        /* Ring time of the struck bodies. Ignored by Delay/ModalBank/
         * SympString/FMRes, which set their decay from Size and feedback. */
        float decay = 0.5f;
    };

    /* What fires a sample. IN 1 doubles as a trigger input because the module
     * has no gate jack; the signal still passes through as excitation. */
    enum SampleTrigMode
    {
        TRIG_OFF = 0,
        TRIG_MIDI,
        TRIG_IN,
        TRIG_BOTH,
        TRIG_COUNT,
    };

    struct SampleParams
    {
        int file = 0;
        float level = 0.7f;
        int trig = TRIG_MIDI;
    };

    /* Internal excitation, so a MIDI note makes a sound with nothing patched
     * and no card in the slot. */
    struct MalletParams
    {
        float level = 0.5f;
    };

    struct FilterParams
    {
        float level = 0.2f;
        float freqRatio = 0.25f;
        float q = 0.7f;
    };

    /* Persisted calibration is two raw floats, and nothing guarantees the
     * bytes behind them are ours — see kCalibStorageOffset. A garbage scale or
     * offset runs through powf() into a NaN frequency, and since std::clamp
     * compares (and every NaN comparison is false) the clamps pass it straight
     * through to a NaN delay time, which silences the entire chain. CAL still
     * works in that state because it overwrites both from the pots, which is
     * exactly the symptom that made this hard to see. */
    bool CalibValid(const CalibSettings &c)
    {
        return std::isfinite(c.scale) && std::isfinite(c.offset) && c.scale >= 0.5f
               && c.scale <= 1.5f && c.offset >= -2.0f && c.offset <= 2.0f;
    }

    float MapExpo(float value, float minVal, float maxVal)
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return minVal * powf(maxVal / minVal, value);
    }

    int GetSizeDivisor(const ResonatorParams &params)
    {
        return std::clamp(params.size, 1, 8);
    }

    int GetTapCount(const ResonatorParams &params)
    {
        return std::clamp(params.taps, 1, 5);
    }
} // namespace

namespace
{
    /* 2 M mono frames = 8 MB of the Seed's 64 MB SDRAM = ~43 s at 48 kHz.
     * Excitation material is short — mallet hits, noise bursts, single drum
     * strikes — so this is deliberately generous rather than tight. */
    constexpr uint32_t kSampleStoreFrames = 2u * 1024u * 1024u;
} // namespace

float DSY_SDRAM_BSS gSampleStore[kSampleStoreFrames];

Bluemchen hw;
DelayLinePair delays;
RingsStyleModels ringsModels;
FeedFilters feedFilters;
DistortionChannel distortionX;
DistortionChannel distortionY;
EncoderState encoderState;
MenuState menuState;

res::SampleStore sampleStore;
res::SampleVoices sampleVoices;
res::ResMidi midi;
res::MidiSettings midiSettings;

DistortionParams distortionParams;
ResonatorParams resonatorParams;
FilterParams filterParams;
SampleParams sampleParams;
MalletParams malletParams;

/* Regeneration around the resonator, from Pot 2 summed with CV 2. Bipolar:
 * centred is none, clockwise regenerates in phase, anticlockwise inverts —
 * which on a delay moves the comb's peaks onto the odd harmonics instead of
 * the even ones, a different tone rather than just less of the same one.
 * There is no cross-channel term at all: X feeds only X, Y only Y. */
float feedAmount = 0.0f;

/* ── Diagnostics ──────────────────────────────────────────────────────────
 * Written by the audio callback, read by the Diag menu page. Each is a
 * word-sized scalar, so a torn read costs at most one stale frame. `diagIn`
 * and `diagOut` are what separates a dead input from a dead output path from
 * a callback that is simply not keeping up. */
float diagCpu = 0.0f;
float diagIn = 0.0f;
float diagOut = 0.0f;
/* The pitch the delay lines are actually given. Reads 0 if it ever goes
 * non-finite again, which is the failure this page exists to catch. */
int diagFreq = 0;
/* Count of MIDI messages received. Stuck at 0 means nothing is reaching the
 * UART; counting up means the firmware is seeing notes and the fault is
 * downstream of that. */
int diagMidi = 0;
/* 1 while the MIDI UART is listening. Midi stuck with MRx 1 means no bytes are
 * reaching the jack; MRx 0 means the peripheral stalled. */
int diagMidiRx = 0;
/* Last note number received. The decisive test: play middle C and this reads
 * 60 if bytes are arriving at all. */
int diagMidiNote = 0;
/* Raw Pot 2 and CV 2, before they are combined into feedAmount. Together with
 * `Feed` these separate "the knob is not moving" from "the knob moves but the
 * model ignores it": an unpatched CV 2 must sit near 50, and P2 must sweep
 * 0..100. If both look right and Feed still reads 0, the model's feed scale is
 * zero — see ModelFeedScale. */
float diagPot2 = 0.0f;
float diagCv2 = 0.0f;
/* Straight off the ADC, below AnalogControl entirely. This is what separates a
 * dead channel from a live one being mis-scaled: if R2/R4 move while P2/C2 do
 * not, the fault is in the control config; if none of them move, nothing is
 * reaching the ADC and it is the pot, the jack or the wiring. Pot 1 and CV 1
 * are shown alongside as the known-good reference — they demonstrably work, so
 * anything true of them and not of 2 is the difference that matters. */
float diagRawP1 = 0.0f;
float diagRawP2 = 0.0f;
float diagRawC1 = 0.0f;
float diagRawC2 = 0.0f;
/* Measured resting point of CV 2, from the boot sample below. Shown on Diag so
 * the auto-zero is visible rather than magic. */
float diagCv2Zero = 0.5f;

CpuLoadMeter cpuMeter;


/* Set from the Load menu row, serviced by the main loop: the load is blocking
 * and touches the SDRAM store, so it can only run outside the callback. */
bool requestLoad = false;
bool requestRescan = false;

void RequestSampleLoad()
{
    requestLoad = true;
}

/* ── Menu value names ─────────────────────────────────────────────────────
 * Five characters is what fits beside a four-character label on the 10-column
 * panel, so the names are abbreviated to that. */
const char *ModelName(int i)
{
    static const char *const kNames[kNumResonatorModels]
        = {"Delay", "Modal", "Strng", "FM", "Beam", "Marim", "Drumh", "Membr", "Plate"};
    return (i >= 0 && i < kNumResonatorModels) ? kNames[i] : "?";
}

const char *TrigName(int i)
{
    static const char *const kNames[TRIG_COUNT] = {"Off", "MIDI", "In", "Both"};
    return (i >= 0 && i < TRIG_COUNT) ? kNames[i] : "?";
}

const char *PitchModeName(int i)
{
    static const char *const kNames[res::MIDI_PITCH_COUNT] = {"Off", "Add", "Set"};
    return (i >= 0 && i < res::MIDI_PITCH_COUNT) ? kNames[i] : "?";
}

const char *ChannelName(int i)
{
    /* Rebuilt per call; the menu renders one row at a time so a single static
     * buffer is enough and costs no flash for 16 string literals. */
    static char buf[4];
    if (i <= 0)
    {
        return "Omni";
    }
    buf[0] = (i >= 10) ? '1' : char('0' + i);
    if (i >= 10)
    {
        buf[1] = char('0' + (i - 10));
        buf[2] = '\0';
    }
    else
    {
        buf[1] = '\0';
    }
    return buf;
}

const char *SampleFileName(int i)
{
    if (sampleStore.Count() <= 0)
    {
        return sampleStore.Status();
    }
    return sampleStore.Name(i);
}

const char *SampleStatus(int)
{
    return sampleStore.Status();
}

MenuItem resonatorItems[] = {
    {"Mode", MenuItemType::Enum, nullptr, &resonatorParams.model, 0.0f,
     static_cast<float>(kNumResonatorModels - 1), 1.0f, ModelName, nullptr},
    {"Ratio", MenuItemType::Ratio, &resonatorParams.ratio, nullptr, 0.25f, 4.0f, 0.05f},
    {"Ofst", MenuItemType::Ratio, &resonatorParams.offset, nullptr, 0.25f, 4.0f, 0.05f},
    {"Mix", MenuItemType::Percent, &resonatorParams.mix, nullptr, 0.0f, 1.0f, 0.02f},
    {"Size", MenuItemType::Int, nullptr, &resonatorParams.size, 1.0f, 8.0f, 1.0f},
    {"Taps", MenuItemType::Int, nullptr, &resonatorParams.taps, 1.0f, 5.0f, 1.0f},
    {"Ring", MenuItemType::Percent, &resonatorParams.decay, nullptr, 0.0f, 1.0f, 0.02f},
};

MenuItem distortionItems[] = {
    {"Fold", MenuItemType::Percent, &distortionParams.foldMix, nullptr, 0.0f, 1.0f, 0.02f},
    {"Drive", MenuItemType::Percent, &distortionParams.driveMix, nullptr, 0.0f, 1.0f, 0.02f},
    {"Dpth", MenuItemType::Percent, &distortionParams.depth, nullptr, 0.0f, 1.0f, 0.02f},
    {"NFold", MenuItemType::Int, nullptr, &distortionParams.folds, 1.0f, 5.0f, 1.0f},
};

MenuItem filterItems[] = {
    {"Level", MenuItemType::Percent, &filterParams.level, nullptr, 0.0f, 1.0f, 0.02f},
    {"Freq", MenuItemType::Ratio, &filterParams.freqRatio, nullptr, 0.25f, 2.0f, 0.05f},
    {"Q", MenuItemType::Ratio, &filterParams.q, nullptr, 0.5f, 2.0f, 0.05f},
};

/* The File row's max is rewritten after every card scan, since it indexes a
 * list whose length is only known at runtime. */
MenuItem sampleItems[] = {
    {"File", MenuItemType::Name, nullptr, &sampleParams.file, 0.0f, 0.0f, 1.0f,
     SampleFileName, nullptr},
    {"Load", MenuItemType::Action, nullptr, nullptr, 0.0f, 0.0f, 0.0f, SampleStatus,
     RequestSampleLoad},
    {"Levl", MenuItemType::Percent, &sampleParams.level, nullptr, 0.0f, 1.0f, 0.02f},
    {"Trig", MenuItemType::Enum, nullptr, &sampleParams.trig, 0.0f,
     static_cast<float>(TRIG_COUNT - 1), 1.0f, TrigName, nullptr},
};

MenuItem midiItems[] = {
    {"Chan", MenuItemType::Enum, nullptr, &midiSettings.channel, 0.0f, 16.0f, 1.0f,
     ChannelName, nullptr},
    {"Ptch", MenuItemType::Enum, nullptr, &midiSettings.pitchMode, 0.0f,
     static_cast<float>(res::MIDI_PITCH_COUNT - 1), 1.0f, PitchModeName, nullptr},
    {"Velo", MenuItemType::Percent, &midiSettings.velAmount, nullptr, 0.0f, 1.0f, 0.02f},
    {"Malt", MenuItemType::Percent, &malletParams.level, nullptr, 0.0f, 1.0f, 0.02f},
};

/* Read-only in practice: the audio callback overwrites these every block, so
 * a stray encoder turn is corrected within a millisecond. */
MenuItem diagItems[] = {
    /* Where Pot 2 and CV 2 have actually landed, signed. The knob has no
     * detent, so this is the only way to find true centre. */
    {"Feed", MenuItemType::Percent, &feedAmount, nullptr, -1.0f, 1.0f, 0.0f},
    {"P2", MenuItemType::Percent, &diagPot2, nullptr, 0.0f, 1.0f, 0.0f},
    {"C2", MenuItemType::Percent, &diagCv2, nullptr, 0.0f, 1.0f, 0.0f},
    {"rP1", MenuItemType::Percent, &diagRawP1, nullptr, 0.0f, 1.0f, 0.0f},
    {"rP2", MenuItemType::Percent, &diagRawP2, nullptr, 0.0f, 1.0f, 0.0f},
    {"rC1", MenuItemType::Percent, &diagRawC1, nullptr, 0.0f, 1.0f, 0.0f},
    {"rC2", MenuItemType::Percent, &diagRawC2, nullptr, 0.0f, 1.0f, 0.0f},
    {"zC2", MenuItemType::Percent, &diagCv2Zero, nullptr, 0.0f, 1.0f, 0.0f},
    {"CPU", MenuItemType::Percent, &diagCpu, nullptr, 0.0f, 1.0f, 0.0f},
    {"In", MenuItemType::Percent, &diagIn, nullptr, 0.0f, 1.0f, 0.0f},
    {"Out", MenuItemType::Percent, &diagOut, nullptr, 0.0f, 1.0f, 0.0f},
    {"Hz", MenuItemType::Int, nullptr, &diagFreq, 0.0f, 0.0f, 0.0f},
    {"Midi", MenuItemType::Int, nullptr, &diagMidi, 0.0f, 0.0f, 0.0f},
    {"MRx", MenuItemType::Int, nullptr, &diagMidiRx, 0.0f, 0.0f, 0.0f},
    {"Note", MenuItemType::Int, nullptr, &diagMidiNote, 0.0f, 0.0f, 0.0f},
};

MenuPage menuPages[] = {
    {"Resonate", resonatorItems, sizeof(resonatorItems) / sizeof(resonatorItems[0])},
    {"Distort", distortionItems, sizeof(distortionItems) / sizeof(distortionItems[0])},
    {"Filter", filterItems, sizeof(filterItems) / sizeof(filterItems[0])},
    {"Sample", sampleItems, sizeof(sampleItems) / sizeof(sampleItems[0])},
    {"MIDI", midiItems, sizeof(midiItems) / sizeof(midiItems[0])},
    {"Diag", diagItems, sizeof(diagItems) / sizeof(diagItems[0])},
};

constexpr size_t kMenuPageCount = sizeof(menuPages) / sizeof(menuPages[0]);

float currentFreq = 440.0f;
float currentFreq2 = 440.0f;
float pitchScale = 1.0f;
float pitchOffset = 0.0f;

bool calibMode = false;
uint32_t lastCalibChangeMs = 0;
bool calibDirty = false;
bool calibSavePending = false;
bool showSaveConfirm = false;
uint32_t saveConfirmUntilMs = 0;
CalibSettings savedCalib = {1.0f, 0.0f};
PersistentStorage<CalibSettings> *calibStorage = nullptr;

/* Internal exciter: a short burst of lowpassed noise, so a MIDI note strikes
 * the body even with no card in the slot and nothing patched to the inputs. */
struct Mallet
{
    void Init(float sr)
    {
        /* ~3 ms to 1/e. Short enough to read as a strike rather than a note. */
        decay_ = std::exp(-1.0f / (0.003f * sr));
        env_ = 0.0f;
        lp_ = 0.0f;
    }

    void Strike(float level) { env_ = std::max(env_, level); }

    float Process()
    {
        if (env_ < 1.0e-5f)
        {
            env_ = 0.0f;
            return 0.0f;
        }
        rng_ = rng_ * 1664525u + 1013904223u;
        const float noise = static_cast<float>(static_cast<int32_t>(rng_)) * (1.0f / 2147483648.0f);
        lp_ += (noise - lp_) * 0.45f;
        const float out = lp_ * env_;
        env_ *= decay_;
        return out;
    }

  private:
    float env_ = 0.0f;
    float lp_ = 0.0f;
    float decay_ = 0.0f;
    uint32_t rng_ = 0x2545F491u;
};

Mallet mallet;

/* IN 1 edge detection runs in the callback but fires voices from the main
 * loop, which keeps powf out of the audio path. One block of latency (~1 ms)
 * is well below the audible threshold for a strike onset. */
volatile int pendingInputStrikes = 0;
bool inputArmed = true;
uint32_t inputLockout = 0;

/* Where CV 2 sits with nothing patched, sampled once at boot. */
float cv2Zero = 0.5f;

float calibPhase = 0.0f;
float sampleRate = 48000.0f;
float modelFeedbackX = 0.0f;
float modelFeedbackY = 0.0f;
int lastAudioModel = 0;
bool heartbeatOn = false;
uint32_t lastHeartbeatMs = 0;
bool ledOn = false;
uint32_t lastLedMs = 0;

void UpdateControls()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    if (calibMode)
    {
        pitchScale = 0.8f + pot1 * 0.4f;
        pitchOffset = (pot2 - 0.5f) * 2.0f;
        const float cvOct = (cv1 - 0.5f) * 10.0f;
        const float base = kCalibTone * powf(2.0f, pitchOffset);
        currentFreq = base * powf(2.0f, cvOct * 0.5f * pitchScale);
        currentFreq2 = currentFreq;

        const uint32_t now = System::GetNow();
        if (fabsf(pitchScale - savedCalib.scale) > 0.0005f || fabsf(pitchOffset - savedCalib.offset) > 0.005f)
        {
            calibDirty = true;
            lastCalibChangeMs = now;
        }
    }
    else
    {
        /* Pitch sources stack: knob, CV, and MIDI. In SET mode the note owns
         * the root outright and the knob drops out, which is what you want
         * when playing from a keyboard; in ADD mode the note is an offset, so
         * the knob still sets the register. */
        float root = MapExpo(pot1, kMinFreq, kMaxFreq);
        float midiOct = 0.0f;
        if (midiSettings.pitchMode == res::MIDI_PITCH_SET && midi.HasNote())
        {
            root = midi.NoteHz();
            midiOct = midi.Bend();
        }
        else if (midiSettings.pitchMode == res::MIDI_PITCH_ADD)
        {
            midiOct = midi.Voct();
        }

        /* CV 1 is bipolar: an unpatched jack sits at mid-scale, so reading it
         * as unipolar (cv1 * 5) added a constant +2.5 octaves and put the
         * pitch floor at 57 Hz instead of 10. CAL has always read it this way
         * — the two disagreeing meant calibration was measured on one scale
         * and played back on another. */
        const float cvOct = (cv1 - 0.5f) * 10.0f * pitchScale;
        const float pitchMultiplier = powf(2.0f, pitchOffset + cvOct + midiOct);
        /* std::clamp cannot filter a NaN (every comparison against one is
         * false), so it is checked explicitly before it can reach a delay
         * time. Nothing downstream recovers from a NaN once it is in the
         * feedback path. */
        const float freq = root * pitchMultiplier;
        currentFreq = std::isfinite(freq) ? std::clamp(freq, kMinFreq, kMaxFreq) : kCalibTone;
        /* Offset, not Ratio: Ratio is the models' timbre control and moving the
         * second channel's pitch with it made every mode change detune the
         * pair. 1.00 is unison. */
        const float freq2 = currentFreq * resonatorParams.offset;
        currentFreq2
            = std::isfinite(freq2) ? std::clamp(freq2, kMinFreq, kMaxFreq) : currentFreq;
        /* Both are bipolar (kxmx_bluemchen's own hardware_test inits CV 1 and
         * CV 2 as -5V..+5V), so an unpatched jack and a centred knob both read
         * 0.5 and contribute nothing. Knob and CV sum, so CV can drive the full
         * range from either end of the knob's travel. */
        /* CV 2 is measured against its own resting point, not an assumed 0.5.
         * An unpatched input that does not sit where you expect saturates this
         * sum and takes the knob with it: the knob's whole travel then lands
         * outside the clamp and Pot 2 looks dead. That is the same failure the
         * wavefolder depth had under a completely different formula, which is
         * what makes the input worth measuring rather than assuming. */
        const float cv2Bipolar = (cv2 - cv2Zero) * 2.0f;
        feedAmount
            = std::clamp((pot2 - 0.5f) * 2.0f + cv2Bipolar, -1.0f, 1.0f);
    }

    diagPot2 = pot2;
    diagCv2 = cv2;
    diagRawP1 = hw.controls[Bluemchen::CTRL_1].GetRawFloat();
    diagRawP2 = hw.controls[Bluemchen::CTRL_2].GetRawFloat();
    diagRawC1 = hw.controls[Bluemchen::CTRL_3].GetRawFloat();
    diagRawC2 = hw.controls[Bluemchen::CTRL_4].GetRawFloat();
    diagFreq = std::isfinite(currentFreq) ? static_cast<int>(currentFreq) : 0;
    diagMidi = static_cast<int>(midi.MessageCount() & 0x7fff);
    diagMidiRx = midi.RxActive(hw) ? 1 : 0;
    diagMidiNote = midi.LastNote();

    const int encInc = hw.encoder.Increment();
    const EncoderPress press = UpdateEncoder(hw, encoderState);

    if (press == EncoderPress::Long)
    {
        if (calibMode)
        {
            calibSavePending = true;
            calibDirty = true;
        }
        calibMode = !calibMode;
    }

    if (!calibMode)
    {
        if (press == EncoderPress::Short)
        {
            MenuPress(menuState, menuPages, kMenuPageCount);
        }
        if (encInc != 0)
        {
            MenuRotate(menuState, encInc, menuPages, kMenuPageCount);
        }
    }

    const float clockDivisor = static_cast<float>(GetSizeDivisor(resonatorParams));
    feedFilters.SetParams(filterParams.level,
                          filterParams.freqRatio,
                          filterParams.q,
                          currentFreq / clockDivisor,
                          currentFreq2 / clockDivisor);
}

/* Blocking SD work. Must run from the main loop: it rewrites the SDRAM store
 * the audio callback reads from. */
void HandleSampleRequests()
{
    if (requestRescan)
    {
        requestRescan = false;
        sampleStore.Scan();
        if (sampleParams.file >= sampleStore.Count())
        {
            sampleParams.file = 0;
        }
        sampleItems[0].max = static_cast<float>(std::max(0, sampleStore.Count() - 1));
    }

    if (requestLoad)
    {
        requestLoad = false;
        if (sampleStore.Count() <= 0)
        {
            requestRescan = true;
            return;
        }
        /* Detach FIRST. The callback keeps running through the load; with a
         * null descriptor every voice reads silence, so no interleaving of the
         * store rewrite can be heard as garbage. */
        sampleVoices.SetSample(res::Sample{});
        res::Sample loaded;
        if (sampleStore.Load(sampleParams.file, sampleRate, loaded))
        {
            sampleVoices.SetSample(loaded);
        }
    }
}

/* Drains MIDI and turns notes and IN 1 edges into strikes. */
void HandleTriggers()
{
    midi.Process(hw, midiSettings);

    const bool midiTrig
        = sampleParams.trig == TRIG_MIDI || sampleParams.trig == TRIG_BOTH;

    res::ResMidi::Strike strike;
    while (midi.TakeStrike(strike))
    {
        if (midiTrig)
        {
            sampleVoices.Trigger(strike.semitones, strike.level);
        }
        mallet.Strike(malletParams.level * strike.level);
    }

    const bool inTrig
        = sampleParams.trig == TRIG_IN || sampleParams.trig == TRIG_BOTH;
    const int inputStrikes = pendingInputStrikes;
    pendingInputStrikes = 0;
    if (inTrig)
    {
        for (int i = 0; i < inputStrikes; ++i)
        {
            /* An audio trigger carries no pitch of its own, so the sample
             * plays at the pitch the rest of the module is tuned to. */
            sampleVoices.Trigger(0.0f, 1.0f);
        }
    }
}

void HandleCalibrationSave()
{
    if (!calibStorage)
    {
        return;
    }

    const uint32_t now = System::GetNow();
    if (calibDirty && (now - lastCalibChangeMs) > 1000)
    {
        calibSavePending = true;
    }

    if (calibSavePending)
    {
        calibStorage->GetSettings().scale = pitchScale;
        calibStorage->GetSettings().offset = pitchOffset;
        calibStorage->Save();
        savedCalib = calibStorage->GetSettings();
        calibDirty = false;
        calibSavePending = false;
        showSaveConfirm = true;
        saveConfirmUntilMs = now + 800;
    }
}

DisplayData BuildDisplayData()
{
    DisplayData data;
    data.isCalib = calibMode;
    data.pitchScale = pitchScale;
    data.pitchOffset = pitchOffset;
    data.currentFreq = currentFreq;
    data.showSaveConfirm = showSaveConfirm;
    data.heartbeatOn = heartbeatOn;

    if (!calibMode)
    {
        const MenuPage &page = menuPages[menuState.pageIndex];
        data.pageTitle = page.title;
        MenuBuildVisibleLines(menuState, page, data.lines, 3, data.lineCount, data.titleSelected);
    }

    return data;
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    cpuMeter.OnBlockStart();

    const float clockDivisor = static_cast<float>(GetSizeDivisor(resonatorParams));
    const float delaySamples1 = (sampleRate * clockDivisor) / std::max(currentFreq, 1.0f);
    const float delaySamples2 = (sampleRate * clockDivisor) / std::max(currentFreq2, 1.0f);

    delays.SetDelayTimes(delaySamples1, delaySamples2);

    const float foldMix = distortionParams.foldMix;
    const float driveMix = distortionParams.driveMix;
    const float waveDepth = std::clamp(distortionParams.depth, 0.0f, 1.0f);
    const float resMix = resonatorParams.mix;
    const float dryMix = 1.0f - resMix;
    const ResonatorModel model = static_cast<ResonatorModel>(
        std::clamp(resonatorParams.model, 0, kNumResonatorModels - 1));
    /* Not every model can take the same feed — see ModelFeedScale. The struck
     * bodies get none at all: their modes are high-Q enough (gain Q/w at
     * resonance, six figures for a 28 s plate tail) that no loop around them
     * survives, which is why Rings-style resonators are terminal blocks. */
    const float feedScale = ModelFeedScale(model);
    const float outGain = ModelOutGain(model);
    /* Block-constant, so the branch below is free. Skipping it at unity matters:
     * model outputs are already soft-clipped, and clipping a bounded signal a
     * second time is pure attenuation. */
    const bool needsMakeup = outGain != 1.0f;
    /* One feed term per channel, straight through. There is deliberately no
     * X-Y or Y-X path: cross-feed made the two outputs collapse into each
     * other, and the two channels are meant to be independent voices. */
    /* FeedTaper, not the raw knob: see its comment. A linear map put the
     * entire audible range in the last few percent of travel, which is what
     * "Pot 2 does nothing" actually was. */
    const float feed = FeedTaper(feedAmount) * kMaxFeed * feedScale;

    /* Excitation is summed into both inputs ahead of the wavefolder, so the
     * distortion stage shapes a struck sample exactly as it shapes live audio. */
    const float sampleLevel = sampleParams.level;
    const bool inputTrig
        = sampleParams.trig == TRIG_IN || sampleParams.trig == TRIG_BOTH;
    const int folds = distortionParams.folds;
    const int tapCount = GetTapCount(resonatorParams);

    if (static_cast<int>(model) != lastAudioModel)
    {
        modelFeedbackX = 0.0f;
        modelFeedbackY = 0.0f;
        lastAudioModel = static_cast<int>(model);
    }

    float inPeakX = 0.0f;
    float inPeakY = 0.0f;
    float outPeakX = 0.0f;
    float outPeakY = 0.0f;
    float rawInPeak = 0.0f;
    float rawOutPeak = 0.0f;

    for (size_t i = 0; i < size; i++)
    {
        rawInPeak = std::max(rawInPeak, std::max(std::fabs(in[0][i]), std::fabs(in[1][i])));

        if (calibMode)
        {
            const float toneFreq = std::clamp(currentFreq, 20.0f, 8000.0f);
            calibPhase += toneFreq / sampleRate;
            if (calibPhase >= 1.0f)
                calibPhase -= 1.0f;
            const float tone = std::sin(calibPhase * kTwoPi) * 0.5f;
            out[0][i] = tone;
            out[1][i] = tone;
            continue;
        }

        if (inputTrig)
        {
            /* Audio inputs are AC-coupled, so a sustained gate droops but a
             * trigger pulse stays a clean edge. Schmitt levels plus a short
             * lockout stop one pulse spawning a burst. */
            const float v = in[0][i];
            if (inputLockout > 0)
            {
                inputLockout--;
            }
            else if (inputArmed && v > 0.15f)
            {
                pendingInputStrikes++;
                inputArmed = false;
                inputLockout = 240; // 5 ms at 48 kHz
            }
            if (!inputArmed && v < 0.05f)
            {
                inputArmed = true;
            }
        }

        const float excite = sampleVoices.Process() * sampleLevel + mallet.Process();
        const float inX = SoftClipSample(in[0][i] + excite);
        const float inY = SoftClipSample(in[1][i] + excite);

        const float delayResX = delays.Read1();
        const float delayResY = delays.Read2();
        const float tapOutX = ReadPrimeSpacedTaps(delays.d1, delaySamples1, tapCount);
        const float tapOutY = ReadPrimeSpacedTaps(delays.d2, delaySamples2, tapCount);
        const float resX = (model == ResonatorModel::Delay) ? delayResX : modelFeedbackX;
        const float resY = (model == ResonatorModel::Delay) ? delayResY : modelFeedbackY;

        const float filteredX = feedFilters.ProcessX(resX);
        const float filteredY = feedFilters.ProcessY(resY);

        /* Feed routing happens before the distortion stage. The feedback term
         * is saturated on its own rather than the sum: that bounds the loop
         * without costing the dry input 6 dB on the way past, which is what
         * left Delay a third of everything else's level. */
        const float preDistX = inX + SoftClipSample(filteredX * feed);
        const float preDistY = inY + SoftClipSample(filteredY * feed);

        const float foldX = ApplyWavefolder(preDistX, waveDepth, folds);
        const float foldY = ApplyWavefolder(preDistY, waveDepth, folds);
        const float foldMixX = preDistX + (foldX - preDistX) * foldMix;
        const float foldMixY = preDistY + (foldY - preDistY) * foldMix;

        const float driveX = ApplyOverdrive(foldMixX, waveDepth);
        const float driveY = ApplyOverdrive(foldMixY, waveDepth);
        const float driveMixX = foldMixX + (driveX - foldMixX) * driveMix;
        const float driveMixY = foldMixY + (driveY - foldMixY) * driveMix;

        inPeakX = std::max(inPeakX, std::fabs(preDistX));
        inPeakY = std::max(inPeakY, std::fabs(preDistY));
        outPeakX = std::max(outPeakX, std::fabs(driveMixX));
        outPeakY = std::max(outPeakY, std::fabs(driveMixY));

        const float makeupX = distortionX.ApplyMakeup(driveMixX);
        const float makeupY = distortionY.ApplyMakeup(driveMixY);

        if (model == ResonatorModel::Delay)
        {
            /* LimitSample, not SoftClipSample: the line's contents are already
             * in range (inX is clipped and the feedback term is saturated
             * above), so a second saturator here only attenuated. */
            delays.Write1(LimitSample(makeupX));
            delays.Write2(LimitSample(makeupY));

            out[0][i] = dryMix * inX + resMix * LimitSample(tapOutX);
            out[1][i] = dryMix * inY + resMix * LimitSample(tapOutY);
        }
        else
        {
            ModelProcessParams params;
            params.freqX = currentFreq;
            params.freqY = currentFreq2;
            params.ratio = resonatorParams.ratio;
            params.size = resonatorParams.size;
            params.taps = tapCount;
            params.decay = resonatorParams.decay;
            params.feed = feedAmount;

            float modelOutX = 0.0f;
            float modelOutY = 0.0f;
            ringsModels.Process(model, SoftClipSample(makeupX), SoftClipSample(makeupY), params, modelOutX, modelOutY);
            modelFeedbackX = modelOutX;
            modelFeedbackY = modelOutY;

            /* The loop above already took modelOut at its natural level; the
             * makeup applies only from here on, so it cannot destabilise the
             * feed. Re-clipped because the gain can push an already bounded
             * signal past full scale. */
            const float mixX
                = needsMakeup ? SoftClipSample(modelOutX * outGain) : modelOutX;
            const float mixY
                = needsMakeup ? SoftClipSample(modelOutY * outGain) : modelOutY;
            out[0][i] = dryMix * inX + resMix * mixX;
            out[1][i] = dryMix * inY + resMix * mixY;
        }

        rawOutPeak
            = std::max(rawOutPeak, std::max(std::fabs(out[0][i]), std::fabs(out[1][i])));
    }

    if (!calibMode)
    {
        distortionX.UpdateMakeup(inPeakX, outPeakX);
        distortionY.UpdateMakeup(inPeakY, outPeakY);
    }

    /* Peaks measured at the jacks, not mid-chain: rawInPeak says whether the
     * codec is delivering an input at all, and rawOutPeak whether anything is
     * reaching the output. Decay slowly so a transient stays readable on a
     * 30 Hz display. */
    diagIn = std::max(rawInPeak, diagIn * 0.85f);
    diagOut = std::max(rawOutPeak, diagOut * 0.85f);

    cpuMeter.OnBlockEnd();
    diagCpu = cpuMeter.GetAvgCpuLoad();
}

int main(void)
{
    hw.Init();
    hw.StartAdc();

    sampleRate = hw.AudioSampleRate();

    delays.Init();
    ringsModels.Init(sampleRate);
    feedFilters.Init(sampleRate);
    sampleVoices.Init(sampleRate);
    mallet.Init(sampleRate);
    cpuMeter.Init(sampleRate, hw.AudioBlockSize());

    distortionX.Reset();
    distortionY.Reset();

    MenuInit(menuState);

    /* The card is optional: with no card the module is exactly what it was,
     * excited by its audio inputs and the internal mallet. */
    sampleStore.Init(gSampleStore, kSampleStoreFrames);
    if (sampleStore.Mount())
    {
        sampleStore.Scan();
    }
    sampleItems[0].max = static_cast<float>(std::max(0, sampleStore.Count() - 1));

    CalibSettings defaults{1.0f, 0.0f};
    static PersistentStorage<CalibSettings> storage(hw.seed.qspi);
    storage.Init(defaults, kCalibStorageOffset);
    calibStorage = &storage;
    savedCalib = storage.GetSettings();
    if (!CalibValid(savedCalib))
    {
        /* Uncalibrated beats silent: fall back rather than propagate garbage
         * into the pitch path. Long-press into CAL to set it properly. */
        savedCalib = defaults;
        storage.GetSettings() = defaults;
    }
    pitchScale = savedCalib.scale;
    pitchOffset = savedCalib.offset;

    /* Sample CV 2's resting point. Only trust it if it holds still: a patched
     * LFO would otherwise have whatever it happened to be doing at boot become
     * "zero". If it moves, fall back to mid-scale, which is the correct answer
     * for a healthy bipolar input anyway. */
    {
        float lo = 1.0f;
        float hi = 0.0f;
        float sum = 0.0f;
        constexpr int kSamples = 64;
        for (int i = 0; i < kSamples; ++i)
        {
            hw.ProcessAnalogControls();
            const float v = hw.GetKnobValue(Bluemchen::CTRL_4);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            sum += v;
            System::Delay(2);
        }
        const float mean = sum / static_cast<float>(kSamples);
        cv2Zero = ((hi - lo) < 0.02f && std::isfinite(mean)) ? mean : 0.5f;
        diagCv2Zero = cv2Zero;
    }

    hw.display.Fill(false);
    hw.display.SetCursor(0, 0);
    hw.display.WriteString("Resonators", Font_6x8, true);
    hw.display.SetCursor(0, 12);
    hw.display.WriteString("Booting...", Font_6x8, true);
    hw.display.Update();

    /* Last, so the UART spends as little time as possible receiving with
     * nothing draining it. Mounting and scanning the card is the slow part of
     * init, and starting reception before it meant the parser had already
     * turned the opto's settling into a queue full of bogus events by the time
     * the main loop came up. ResMidi::Process flushes whatever still gets in. */
    midi.Init(hw);

    hw.StartAudio(AudioCallback);

    uint32_t lastDisplayUpdate = 0;
    while (1)
    {
        /* MIDI first: UpdateControls reads the note pitch, so draining the
         * queue after it would leave every note a loop iteration late. */
        HandleTriggers();
        UpdateControls();
        HandleSampleRequests();
        HandleCalibrationSave();

        const uint32_t now = System::GetNow();
        if (now - lastHeartbeatMs > 250)
        {
            heartbeatOn = !heartbeatOn;
            lastHeartbeatMs = now;
        }
        if (now - lastLedMs > 250)
        {
            ledOn = !ledOn;
            hw.seed.SetLed(ledOn);
            lastLedMs = now;
        }
        if (now - lastDisplayUpdate > 33)
        {
            if (showSaveConfirm && now > saveConfirmUntilMs)
            {
                showSaveConfirm = false;
            }
            RenderDisplay(hw, BuildDisplayData());
            lastDisplayUpdate = now;
        }
    }
}
