/**
 * Formant Synthesis Class
 *
 * Implements cascaded bandpass filter formant synthesis
 * for vowel-like vocal timbres, based on Chatterbox
 */

#pragma once

#include <math.h>
#include "Filters/svf.h"
#include "Synthesis/oscillator.h"
#include "Noise/whitenoise.h"

using namespace daisysp;

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

class FormantSynth {
public:
    enum VowelPreset {
        VOWEL_A,  // "ah" as in "father"
        VOWEL_E,  // "eh" as in "bed"
        VOWEL_I,  // "ee" as in "see"
        VOWEL_O,  // "oh" as in "go"
        VOWEL_U   // "oo" as in "food"
    };

    FormantSynth() {
        f1_freq_ = 730.0f;   // Default to 'A'
        f2_freq_ = 1090.0f;
        f3_freq_ = 2440.0f;
        f4_freq_ = 3200.0f;
        pitch_ = 110.0f;
        excitationEnabled_ = false;
        useExternalInput_ = true;
        sampleRate_ = 48000.0f;
    }

    void Init(float sampleRate) {
        sampleRate_ = sampleRate;

        // Initialize all 4 formant filters
        formant1_.Init(sampleRate);
        formant2_.Init(sampleRate);
        formant3_.Init(sampleRate);
        formant4_.Init(sampleRate);

        // Initialize excitation sources
        larynx_.Init(sampleRate);
        larynx_.SetWaveform(Oscillator::WAVE_SAW);
        larynx_.SetFreq(pitch_);

        noise_.Init();

        // Set initial formant frequencies and bandwidths
        UpdateFormants();
    }

    float Process(float audioInput = 0.0f) {
        // Determine excitation source
        float excitation;
        if (useExternalInput_) {
            excitation = audioInput;
        } else if (excitationEnabled_) {
            // Mix sawtooth and noise for voiced excitation
            excitation = larynx_.Process() * 0.7f + noise_.Process() * 0.3f;
        } else {
            excitation = 0.0f;
        }

        // Cascade through 4 formants (bandpass filters)
        formant1_.Process(excitation);
        float sig = formant1_.Band();

        formant2_.Process(sig);
        sig = formant2_.Band();

        formant3_.Process(sig);
        sig = formant3_.Band();

        formant4_.Process(sig);
        sig = formant4_.Band();

        // Apply makeup gain for cascade losses
        return sig * CASCADE_MAKEUP_GAIN;
    }

    // Formant frequency setters
    void SetF1(float freq) {
        f1_freq_ = Clamp(freq, 200.0f, 1000.0f);
        UpdateFormants();
    }

    void SetF2(float freq) {
        f2_freq_ = Clamp(freq, 500.0f, 3000.0f);
        UpdateFormants();
    }

    void SetF3(float freq) {
        f3_freq_ = Clamp(freq, 1500.0f, 4000.0f);
        UpdateFormants();
    }

    void SetF4(float freq) {
        f4_freq_ = Clamp(freq, 2500.0f, 4500.0f);
        UpdateFormants();
    }

    void SetVowelPreset(VowelPreset vowel) {
        switch(vowel) {
            case VOWEL_A:  // "ah"
                f1_freq_ = 730.0f;
                f2_freq_ = 1090.0f;
                f3_freq_ = 2440.0f;
                f4_freq_ = 3200.0f;
                break;
            case VOWEL_E:  // "eh"
                f1_freq_ = 530.0f;
                f2_freq_ = 1840.0f;
                f3_freq_ = 2480.0f;
                f4_freq_ = 3500.0f;
                break;
            case VOWEL_I:  // "ee"
                f1_freq_ = 270.0f;
                f2_freq_ = 2290.0f;
                f3_freq_ = 3010.0f;
                f4_freq_ = 3500.0f;
                break;
            case VOWEL_O:  // "oh"
                f1_freq_ = 570.0f;
                f2_freq_ = 840.0f;
                f3_freq_ = 2410.0f;
                f4_freq_ = 3200.0f;
                break;
            case VOWEL_U:  // "oo"
                f1_freq_ = 300.0f;
                f2_freq_ = 870.0f;
                f3_freq_ = 2240.0f;
                f4_freq_ = 3200.0f;
                break;
        }
        UpdateFormants();
    }

    void SetPitch(float freq) {
        pitch_ = freq;
        larynx_.SetFreq(pitch_);
    }

    void SetExcitationEnabled(bool en) {
        excitationEnabled_ = en;
    }

    void SetExternalInput(bool useExt) {
        useExternalInput_ = useExt;
    }

    // Getters
    float GetF1() const { return f1_freq_; }
    float GetF2() const { return f2_freq_; }
    float GetF3() const { return f3_freq_; }
    float GetF4() const { return f4_freq_; }
    float GetPitch() const { return pitch_; }
    bool IsExcitationEnabled() const { return excitationEnabled_; }
    bool IsUsingExternalInput() const { return useExternalInput_; }

private:
    // Formant filters (4 cascaded bandpass filters)
    Svf formant1_, formant2_, formant3_, formant4_;

    // Excitation sources
    Oscillator larynx_;    // Sawtooth oscillator for pitched excitation
    WhiteNoise noise_;     // Noise source for breathiness

    // Formant frequencies
    float f1_freq_, f2_freq_, f3_freq_, f4_freq_;
    float pitch_;
    float sampleRate_;

    // State
    bool excitationEnabled_;
    bool useExternalInput_;

    // Constants
    static constexpr float CASCADE_MAKEUP_GAIN = 3.0f;
    static constexpr float BW1 = 80.0f;   // Bandwidth for F1
    static constexpr float BW2 = 120.0f;  // Bandwidth for F2
    static constexpr float BW3 = 150.0f;  // Bandwidth for F3
    static constexpr float BW4 = 200.0f;  // Bandwidth for F4

    void UpdateFormants() {
        // Update filter frequencies and resonances
        formant1_.SetFreq(f1_freq_);
        formant1_.SetRes(CalculateRes(f1_freq_, BW1));

        formant2_.SetFreq(f2_freq_);
        formant2_.SetRes(CalculateRes(f2_freq_, BW2));

        formant3_.SetFreq(f3_freq_);
        formant3_.SetRes(CalculateRes(f3_freq_, BW3));

        formant4_.SetFreq(f4_freq_);
        formant4_.SetRes(CalculateRes(f4_freq_, BW4));
    }

    /**
     * Calculate resonance value from center frequency and bandwidth
     * Q = f_center / bandwidth
     * res = 1 - (1/Q), capped for stability
     */
    float CalculateRes(float centerFreq, float bandwidth) {
        float q = centerFreq / bandwidth;
        float res = 1.0f - (1.0f / fmaxf(q, 1.0f));
        return fminf(res, 0.90f);  // Cap at 0.90 for stability
    }

    float Clamp(float value, float min, float max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
};
