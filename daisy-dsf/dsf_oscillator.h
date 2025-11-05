/**
 * DSF Oscillator Class
 * 
 * Implements various Discrete Summation Formula algorithms
 * for band-limited waveform synthesis
 */

#pragma once

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef M_TWOPI
#define M_TWOPI (2.0f * M_PI)
#endif

class DSFOscillator {
public:
    enum Algorithm {
        CLASSIC_DSF,      // Classic Moorer DSF
        MODIFIED_FM,      // FM-style DSF
        WAVESHAPE,        // DSF with waveshaping
        COMPLEX_DSF       // Complex multi-term DSF
    };
    
    DSFOscillator() {
        phase_ = 0.0f;
        freq_ = 440.0f;
        baseFreq_ = 440.0f;
        sampleRate_ = 48000.0f;
        numHarmonics_ = 20;
        alpha_ = 0.5f;
        algorithm_ = CLASSIC_DSF;
        phaseInc_ = 0.0f;
        throughZero_ = false;
        phaseReversed_ = false;
        currentAmplitude_ = 0.0f;
    }
    
    void Init(float sampleRate) {
        sampleRate_ = sampleRate;
        phase_ = 0.0f;
        UpdatePhaseIncrement();
    }
    
    float Process() {
        float output = 0.0f;
        
        switch(algorithm_) {
            case CLASSIC_DSF:
                output = ProcessClassicDSF();
                break;
            case MODIFIED_FM:
                output = ProcessModifiedFM();
                break;
            case WAVESHAPE:
                output = ProcessWaveshape();
                break;
            case COMPLEX_DSF:
                output = ProcessComplexDSF();
                break;
        }
        
        // Store amplitude for external processing
        currentAmplitude_ = fabsf(output);
        
        // Handle through-zero (phase reversal)
        if (throughZero_ && phaseReversed_) {
            output = -output;
        }
        
        // Advance phase
        phase_ += phaseInc_;
        if (phase_ >= M_TWOPI) {
            phase_ -= M_TWOPI;
            phaseReversed_ = !phaseReversed_;  // Toggle phase on zero crossing
        }
        if (phase_ < 0.0f) {
            phase_ += M_TWOPI;
        }
        
        return output;
    }
    
    // Setters
    void SetFreq(float freq) {
        freq_ = freq;
        UpdatePhaseIncrement();
    }
    
    void SetBaseFreq(float freq) {
        baseFreq_ = freq;
        SetFreq(freq);
    }
    
    void SetNumHarmonics(int n) {
        if (n < 1) n = 1;
        if (n > 100) n = 100;
        numHarmonics_ = n;
    }
    
    void SetAlpha(float alpha) {
        // Clamp to reasonable range
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 0.99f) alpha = 0.99f;
        alpha_ = alpha;
    }
    
    void SetAlgorithm(Algorithm alg) {
        algorithm_ = alg;
    }
    
    void SetThroughZero(bool enable) {
        throughZero_ = enable;
        if (!enable) {
            phaseReversed_ = false;
        }
    }
    
    // Getters
    float GetFreq() const { return freq_; }
    float GetBaseFreq() const { return baseFreq_; }
    int GetNumHarmonics() const { return numHarmonics_; }
    float GetAlpha() const { return alpha_; }
    float GetPhase() const { return phase_; }
    float GetCurrentAmplitude() const { return currentAmplitude_; }
    Algorithm GetCurrentAlgorithm() const { return algorithm_; }
    
private:
    float phase_;
    float freq_;
    float baseFreq_;
    float sampleRate_;
    int numHarmonics_;
    float alpha_;
    Algorithm algorithm_;
    float phaseInc_;
    bool throughZero_;
    bool phaseReversed_;
    float currentAmplitude_;
    
    void UpdatePhaseIncrement() {
        phaseInc_ = (M_TWOPI * freq_) / sampleRate_;
    }
    
    /**
     * Classic DSF - Moorer 1976
     * Produces a sawtooth-like waveform with controlled rolloff
     */
    float ProcessClassicDSF() {
        float N = (float)numHarmonics_;
        float a = alpha_;
        
        // Avoid division by zero
        float denominator = 1.0f + a*a - 2.0f*a*cosf(N * phase_);
        if (fabsf(denominator) < 1e-10f) {
            return 0.0f;
        }
        
        float numerator = sinf(phase_) - a * sinf(phase_ - N * phase_);
        
        return numerator / denominator;
    }
    
    /**
     * Modified FM approach
     * Uses DSF formula with modulation index control
     */
    float ProcessModifiedFM() {
        float N = (float)numHarmonics_;
        float beta = alpha_ * 10.0f; // FM modulation index
        
        // Carrier and modulator phases
        float modPhase = phase_ * N;
        float modulated = phase_ + beta * sinf(modPhase);
        
        float a = alpha_;
        float denominator = 1.0f + a*a - 2.0f*a*cosf(modPhase);
        
        if (fabsf(denominator) < 1e-10f) {
            return 0.0f;
        }
        
        float numerator = sinf(modulated) - a * sinf(modulated - modPhase);
        
        return numerator / denominator;
    }
    
    /**
     * DSF with waveshaping
     * Applies nonlinear distortion to the DSF output
     */
    float ProcessWaveshape() {
        float dsf = ProcessClassicDSF();
        
        // Soft clipping waveshaper
        float shaped = Waveshape(dsf, alpha_ * 5.0f);
        
        return shaped;
    }
    
    /**
     * Complex DSF with multiple terms
     * Based on generalized summation formulas
     */
    float ProcessComplexDSF() {
        float N = (float)numHarmonics_;
        float a = alpha_;
        
        // First term (fundamental)
        float denom1 = 1.0f + a*a - 2.0f*a*cosf(N * phase_);
        float term1 = 0.0f;
        if (fabsf(denom1) > 1e-10f) {
            term1 = (sinf(phase_) - a * sinf(phase_ - N * phase_)) / denom1;
        }
        
        // Second term (harmonic series shift)
        float phase2 = phase_ * 2.0f;
        float denom2 = 1.0f + a*a - 2.0f*a*cosf(N * phase2 / 2.0f);
        float term2 = 0.0f;
        if (fabsf(denom2) > 1e-10f) {
            term2 = (sinf(phase2) - a * sinf(phase2 - N * phase2 / 2.0f)) / denom2;
        }
        
        // Mix terms
        return term1 + 0.5f * term2;
    }
    
    /**
     * Soft clipping waveshaper
     */
    float Waveshape(float x, float gain) {
        x *= gain;
        
        // Tanh-like soft clipper
        if (x > 1.0f) {
            return 2.0f / 3.0f + (x - 1.0f) / 3.0f;
        } else if (x < -1.0f) {
            return -2.0f / 3.0f + (x + 1.0f) / 3.0f;
        } else {
            return x - (x * x * x) / 3.0f;
        }
    }
};
