#pragma once

#include <algorithm>
#include <cmath>

namespace disyn {

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
constexpr float kEpsilon = 1e-8f;

inline float StepPhase(float currentPhase, float frequency, float sampleRate) {
    const float next = currentPhase + frequency / sampleRate;
    return next - std::floor(next);
}

inline float ExpoMap(float value, float min, float max) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return min * std::pow(max / min, clamped);
}

inline float ComputeDSFComponent(float w, float t, float decay) {
    const float denominator = 1.0f - 2.0f * decay * std::cos(t) + decay * decay;
    if (std::abs(denominator) < kEpsilon) {
        return 0.0f;
    }

    const float numerator = std::sin(w) - decay * std::sin(w - t);
    const float normalise = std::sqrt(1.0f - decay * decay);
    return (numerator / denominator) * normalise;
}

inline float ProcessAsymmetricFM(float param1, float param2, float frequency,
                                 float sampleRate, float& carrierPhaseRef, float& modPhaseRef) {
    const float k = ExpoMap(param1, 0.01f, 10.0f);
    const float r = ExpoMap(param2, 0.5f, 2.0f);
    const float modFreq = frequency;

    carrierPhaseRef = StepPhase(carrierPhaseRef, frequency, sampleRate);
    modPhaseRef = StepPhase(modPhaseRef, modFreq, sampleRate);

    const float modulator = std::sin(kTwoPi * modPhaseRef);
    const float asymmetry = std::exp(k * (r - 1.0f / r) * std::cos(kTwoPi * modPhaseRef) / 2.0f);
    const float carrier = std::cos(kTwoPi * carrierPhaseRef + k * modulator);

    return carrier * asymmetry * 0.5f;
}

inline float WrapAngle(float x) {
    float wrapped = x;
    while (wrapped > static_cast<float>(M_PI)) {
        wrapped -= kTwoPi;
    }
    while (wrapped < -static_cast<float>(M_PI)) {
        wrapped += kTwoPi;
    }
    return wrapped;
}

inline float ComputeTaylorSine(float x, int numTerms) {
    const float wrapped = WrapAngle(x);

    float result = 0.0f;
    float term = wrapped;
    const float xSquared = wrapped * wrapped;

    for (int n = 0; n < numTerms; n++) {
        result += term;
        const float denominator = static_cast<float>((2 * n + 2) * (2 * n + 3));
        term *= -xSquared / denominator;
    }

    return std::clamp(result, -1.5f, 1.5f);
}

} // namespace disyn
