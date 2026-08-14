#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

// Lightweight FFT autocorrelation pitch tracker for monophonic guitar/bass.
// The previous implementation evaluated a full YIN difference function for
// every possible lag on every hop. That is accurate but unnecessarily costly
// for a real-time pedal. This version uses an FFT autocorrelation, then applies
// harmonic-aware octave correction and temporal hysteresis.
class PitchTracker
{
public:
    void prepare (double sampleRateIn, int windowSizeIn, int hopSizeIn,
                  float minFreqHzIn, float maxFreqHzIn)
    {
        sampleRate = sampleRateIn;
        windowSize = juce::jmax (256, windowSizeIn);
        hopSize = juce::jlimit (64, windowSize - 1, hopSizeIn);
        minFreqHz = juce::jmax (12.0f, minFreqHzIn);
        maxFreqHz = juce::jmin ((float) sampleRate * 0.45f, juce::jmax (80.0f, maxFreqHzIn));

        tauMin = juce::jmax (2, (int) std::floor (sampleRate / (double) maxFreqHz));
        tauMax = juce::jmin (windowSize / 2, (int) std::ceil (sampleRate / (double) minFreqHz));
        tauMax = juce::jmax (tauMin + 2, tauMax);

        fftOrder = 1;
        while ((1 << fftOrder) < windowSize * 2)
            ++fftOrder;
        fftSize = 1 << fftOrder;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        buffer.assign ((size_t) windowSize, 0.0f);
        window.assign ((size_t) windowSize, 0.0f);
        fftData.assign ((size_t) 2 * (size_t) fftSize, 0.0f);
        autocorrelation.assign ((size_t) tauMax + 2, 0.0f);

        fillPos = 0;
        newPitchAvailable = false;
        lastFrequencyHz = 0.0f;
        lastRms = 0.0f;
        confirmedFrequencyHz = 0.0f;
        previousRawFrequencyHz = 0.0f;
        hpCoeff = 0.0f;
        hpX1 = hpY1 = lpY1 = 0.0f;

        for (int i = 0; i < windowSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (
                2.0f * juce::MathConstants<float>::pi * (float) i
                / (float) juce::jmax (1, windowSize - 1));

        // Very gentle DC/rumble removal. Keep the cutoff comfortably below
        // the lowest fundamental we want to track.
        const float cutoffHz = juce::jmax (6.0f, minFreqHz * 0.20f);
        hpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi
                            * cutoffHz / (float) sampleRate);

        // Detector-only low-pass: enough bandwidth for guitar harmonics, while
        // reducing high harmonic dominance that can cause octave-up errors.
        const float detectorCutoff = juce::jmin (maxFreqHz * 2.0f,
                                                  (float) sampleRate * 0.40f);
        lpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi
                            * detectorCutoff / (float) sampleRate);
    }

    void pushSample (float sample) noexcept
    {
        const float highPassed = sample - hpX1 + hpCoeff * hpY1;
        hpX1 = sample;
        hpY1 = highPassed;

        const float filtered = (1.0f - lpCoeff) * highPassed + lpCoeff * lpY1;
        lpY1 = filtered;

        if (fillPos < windowSize)
            buffer[(size_t) fillPos++] = filtered;

        if (fillPos >= windowSize)
        {
            analyze();

            const int keep = windowSize - hopSize;
            if (keep > 0)
                std::memmove (buffer.data(), buffer.data() + hopSize,
                              (size_t) keep * sizeof (float));
            fillPos = juce::jmax (0, keep);
        }
    }

    bool consumeNewPitch (float& frequencyHzOut, float& rmsOut) noexcept
    {
        if (! newPitchAvailable)
            return false;

        frequencyHzOut = lastFrequencyHz;
        rmsOut = lastRms;
        newPitchAvailable = false;
        return true;
    }

private:
    float correlationAt (int lag) const noexcept
    {
        if (lag <= 0 || lag >= (int) autocorrelation.size())
            return 0.0f;

        const float r0 = autocorrelation[0];
        return r0 > 1.0e-10f ? autocorrelation[(size_t) lag] / r0 : 0.0f;
    }

    void analyze() noexcept
    {
        double sumSq = 0.0;

        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < windowSize; ++i)
        {
            const float s = buffer[(size_t) i] * window[(size_t) i];
            fftData[(size_t) i] = s;
            sumSq += (double) s * s;
        }

        lastRms = std::sqrt ((float) (sumSq / (double) windowSize));

        if (lastRms < 1.0e-5f)
        {
            lastFrequencyHz = 0.0f;
            newPitchAvailable = true;
            return;
        }

        fft->performRealOnlyForwardTransform (fftData.data());

        // Convert the power spectrum into a real autocorrelation spectrum.
        // JUCE stores the DC/Nyquist bins specially in real-only format.
        fftData[0] *= fftData[0];
        fftData[1] *= fftData[1];

        for (int k = 1; k < fftSize / 2; ++k)
        {
            const size_t reIndex = (size_t) 2 * (size_t) k;
            const size_t imIndex = reIndex + 1;
            const float re = fftData[reIndex];
            const float im = fftData[imIndex];
            const float power = re * re + im * im;
            fftData[reIndex] = power;
            fftData[imIndex] = 0.0f;
        }

        fft->performRealOnlyInverseTransform (fftData.data());

        for (int tau = 0; tau <= tauMax; ++tau)
            autocorrelation[(size_t) tau] = fftData[(size_t) tau];

        const float r0 = autocorrelation[0];
        if (r0 <= 1.0e-10f)
        {
            lastFrequencyHz = 0.0f;
            newPitchAvailable = true;
            return;
        }

        // Find local maxima. We keep the strongest candidates rather than
        // blindly selecting the first peak; this is important for bass notes
        // with a strong second harmonic.
        int bestTau = -1;
        float bestScore = 0.0f;

        for (int tau = tauMin + 1; tau < tauMax - 1; ++tau)
        {
            const float c = correlationAt (tau);
            if (c < 0.08f)
                continue;

            if (c >= correlationAt (tau - 1) && c >= correlationAt (tau + 1))
            {
                // Harmonic-aware score. A true fundamental tends to preserve
                // periodicity at 2T/3T; this helps recover low notes whose
                // second harmonic is stronger than the fundamental.
                float score = c;
                if (tau * 2 <= tauMax)
                    score += 0.18f * juce::jmax (0.0f, correlationAt (tau * 2));
                if (tau * 3 <= tauMax)
                    score += 0.08f * juce::jmax (0.0f, correlationAt (tau * 3));

                if (score > bestScore)
                {
                    bestScore = score;
                    bestTau = tau;
                }
            }
        }

        if (bestTau < 0)
        {
            lastFrequencyHz = 0.0f;
            newPitchAvailable = true;
            return;
        }

        // For low-register instruments, prefer a true fundamental when a
        // strong second harmonic is the selected peak. Limit this correction
        // to the low register so a clean A2/E3 is not incorrectly pulled down
        // an octave. Very strong candidates require stronger evidence before
        // moving down.
        const float bestCorr = correlationAt (bestTau);
        const float candidateFrequency = (float) (sampleRate / (double) bestTau);
        if (candidateFrequency > minFreqHz * 1.8f && candidateFrequency <= 120.0f)
        {
            const float requiredRatio = candidateFrequency <= 70.0f ? 0.62f
                                          : (bestCorr > 0.90f ? 0.90f : 0.78f);
            for (int divisor = 2; divisor <= 3; ++divisor)
            {
                const int lowerTau = bestTau * divisor;
                if (lowerTau > tauMax)
                    break;

                const float lowerCorr = correlationAt (lowerTau);
                if (lowerCorr >= bestCorr * requiredRatio && lowerCorr > 0.12f)
                {
                    bestTau = lowerTau;
                    break;
                }
            }
        }

        // Conversely, reject an accidental subharmonic when the half-period
        // has substantially stronger evidence.
        if (bestTau > tauMin * 2)
        {
            const int upperTau = bestTau / 2;
            const float currentCorr = correlationAt (bestTau);
            const float upperCorr = correlationAt (upperTau);
            if (upperCorr > currentCorr * 1.28f)
                bestTau = upperTau;
        }

        float refinedTau = (float) bestTau;
        if (bestTau > tauMin && bestTau < tauMax)
        {
            const float left = correlationAt (bestTau - 1);
            const float centre = correlationAt (bestTau);
            const float right = correlationAt (bestTau + 1);
            const float denom = left - 2.0f * centre + right;

            if (std::abs (denom) > 1.0e-6f)
                refinedTau += 0.5f * (left - right) / denom;
        }

        const float rawFrequencyHz = refinedTau > 0.0f
            ? (float) (sampleRate / refinedTau)
            : 0.0f;

        if (rawFrequencyHz < minFreqHz * 0.75f
            || rawFrequencyHz > maxFreqHz * 1.20f)
        {
            lastFrequencyHz = 0.0f;
            newPitchAvailable = true;
            return;
        }

        const float confidence = correlationAt (bestTau);
        bool accept = false;

        if (confirmedFrequencyHz <= 0.0f)
        {
            accept = confidence >= 0.18f;
        }
        else
        {
            const float ratio = rawFrequencyHz / juce::jmax (confirmedFrequencyHz, 1.0e-6f);
            const float cents = 1200.0f * std::log2 (juce::jmax (ratio, 1.0e-6f));

            if (std::abs (cents) < 160.0f)
                accept = confidence >= 0.10f;
            else
                accept = confidence >= 0.28f;
        }

        if (accept)
            confirmedFrequencyHz = rawFrequencyHz;

        previousRawFrequencyHz = rawFrequencyHz;
        lastFrequencyHz = confirmedFrequencyHz;
        newPitchAvailable = true;
    }

    double sampleRate = 44100.0;
    int windowSize = 2048;
    int hopSize = 512;
    float minFreqHz = 70.0f;
    float maxFreqHz = 1200.0f;

    int tauMin = 2;
    int tauMax = 512;
    int fftOrder = 11;
    int fftSize = 2048;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> buffer;
    std::vector<float> window;
    std::vector<float> fftData;
    std::vector<float> autocorrelation;
    int fillPos = 0;

    bool newPitchAvailable = false;
    float lastFrequencyHz = 0.0f;
    float lastRms = 0.0f;
    float confirmedFrequencyHz = 0.0f;
    float previousRawFrequencyHz = 0.0f;

    float hpCoeff = 0.0f;
    float hpX1 = 0.0f;
    float hpY1 = 0.0f;
    float lpCoeff = 0.0f;
    float lpY1 = 0.0f;
};
