#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

// Detector de pitch monofonico (variante de YIN). Analiza en bloques
// (windowSize) con solapamiento (hopSize), asi el costo de CPU queda
// concentrado en cada "hop" en vez de correr algo pesado por muestra.
class PitchTracker
{
public:
    void prepare (double sampleRateIn, int windowSizeIn, int hopSizeIn, float minFreqHzIn, float maxFreqHzIn)
    {
        sampleRate = sampleRateIn;
        windowSize = windowSizeIn;
        hopSize    = hopSizeIn;
        minFreqHz  = minFreqHzIn;
        maxFreqHz  = maxFreqHzIn;

        tauMin = std::max (2, (int) (sampleRate / (double) maxFreqHz));
        tauMax = std::min (windowSize / 2, (int) (sampleRate / (double) minFreqHz));

        buffer.assign ((size_t) windowSize, 0.0f);
        diffFunction.assign ((size_t) tauMax + 1, 0.0f);
        cmndf.assign ((size_t) tauMax + 1, 1.0f);
        fillPos = 0;

        newPitchAvailable = false;
        lastFrequencyHz = 0.0f;
        lastRms = 0.0f;
        confirmedFrequencyHz = 0.0f;
        previousRawFrequencyHz = 0.0f;

        // pasa-altos de una sola muestra para sacar ruido de baja frecuencia
        // (ruido de piso, golpes del cuerpo) antes de analizar el pitch
        const float cutoffHz = minFreqHz * 0.5f;
        hpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
        hpX1 = hpY1 = 0.0f;
    }

    void pushSample (float sample) noexcept
    {
        const float filtered = sample - hpX1 + hpCoeff * hpY1;
        hpX1 = sample;
        hpY1 = filtered;

        if (fillPos < windowSize)
            buffer[(size_t) fillPos++] = filtered;

        if (fillPos >= windowSize)
        {
            analyze();

            const int keep = windowSize - hopSize;
            if (keep > 0)
                std::copy (buffer.begin() + hopSize, buffer.end(), buffer.begin());
            fillPos = std::max (0, keep);
        }
    }

    // Devuelve true una sola vez por cada estimacion nueva de pitch.
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
    void analyze()
    {
        float sumSq = 0.0f;
        for (float s : buffer)
            sumSq += s * s;
        lastRms = std::sqrt (sumSq / (float) buffer.size());

        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            float sum = 0.0f;
            const int n = windowSize - tau;
            for (int j = 0; j < n; ++j)
            {
                const float diff = buffer[(size_t) j] - buffer[(size_t) (j + tau)];
                sum += diff * diff;
            }
            diffFunction[(size_t) tau] = sum;
        }

        float runningSum = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            runningSum += diffFunction[(size_t) tau];
            cmndf[(size_t) tau] = (runningSum > 0.0f)
                ? diffFunction[(size_t) tau] * (float) tau / runningSum
                : 1.0f;
        }

        constexpr float threshold = 0.12f;
        int bestTau = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmndf[(size_t) tau] < threshold && cmndf[(size_t) tau] < cmndf[(size_t) (tau + 1)])
            {
                bestTau = tau;
                break;
            }
        }

        // si no hubo ningun dip claro por debajo del umbral estricto, nos
        // quedamos con el minimo global de todos modos (siempre que sea
        // razonablemente periodico); evita cortes de tracking en notas
        // con timbre mas complejo, a costa de algo de precision en ese caso
        if (bestTau < 0)
        {
            float minValue = 1.0f;
            int minTau = -1;
            for (int tau = tauMin; tau <= tauMax; ++tau)
            {
                if (cmndf[(size_t) tau] < minValue)
                {
                    minValue = cmndf[(size_t) tau];
                    minTau = tau;
                }
            }

            if (minTau > 0 && minValue < 0.35f)
                bestTau = minTau;
        }

        if (bestTau < 0)
        {
            lastFrequencyHz = 0.0f;
            newPitchAvailable = true;
            return;
        }

        // interpolacion parabolica para afinar la estimacion entre muestras enteras de tau
        float refinedTau = (float) bestTau;
        if (bestTau > tauMin && bestTau < tauMax)
        {
            const float s0 = cmndf[(size_t) (bestTau - 1)];
            const float s1 = cmndf[(size_t) bestTau];
            const float s2 = cmndf[(size_t) (bestTau + 1)];
            const float denom = s0 - 2.0f * s1 + s2;
            if (std::abs (denom) > 1.0e-9f)
                refinedTau += 0.5f * (s0 - s2) / denom;
        }

        const float rawFrequencyHz = (refinedTau > 0.0f) ? (float) (sampleRate / refinedTau) : 0.0f;

        // debounce: solo confirmamos una nota nueva si dos hops seguidos
        // coinciden dentro de +-6% (~1 semitono). Esto filtra los errores
        // de octava y el jitter de un solo hop que suelen venir de pulsaciones
        // ruidosas, a costa de un hop extra de latencia en notas nuevas.
        bool accept = false;
        if (confirmedFrequencyHz <= 0.0f)
        {
            accept = true; // primera deteccion: sin nota previa, no hay nada que debounce-ar
        }
        else if (previousRawFrequencyHz > 0.0f)
        {
            const float ratio = rawFrequencyHz / previousRawFrequencyHz;
            if (ratio > 0.94f && ratio < 1.06f)
                accept = true;
        }

        if (accept)
            confirmedFrequencyHz = rawFrequencyHz;

        previousRawFrequencyHz = rawFrequencyHz;
        lastFrequencyHz = confirmedFrequencyHz;
        newPitchAvailable = true;
    }

    double sampleRate = 44100.0;
    int windowSize = 2048, hopSize = 1024;
    float minFreqHz = 70.0f, maxFreqHz = 1200.0f;
    int tauMin = 2, tauMax = 512;

    std::vector<float> buffer, diffFunction, cmndf;
    int fillPos = 0;

    bool newPitchAvailable = false;
    float lastFrequencyHz = 0.0f;
    float lastRms = 0.0f;

    // debounce de octava/jitter
    float confirmedFrequencyHz = 0.0f;
    float previousRawFrequencyHz = 0.0f;

    // pasa-altos de pre-filtrado
    float hpCoeff = 0.0f, hpX1 = 0.0f, hpY1 = 0.0f;
};
