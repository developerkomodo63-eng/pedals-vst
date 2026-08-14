#pragma once

#include <cmath>

// Osciladores clasicos (saw, square, triangle) con correccion PolyBLEP:
// suaviza las discontinuidades para que no aliasen tanto como un
// diente de sierra "naive", sin el costo de una wavetable.
class BlepOscillator
{
public:
    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void setFrequency (float freqHz) noexcept
    {
        frequency = freqHz;
        phaseInc = frequency / (float) sampleRate;
    }

    void setWaveform (int type) noexcept { waveform = type; } // 0=saw, 1=square, 2=triangle

    void reset() noexcept { phase = 0.0f; triState = 0.0f; }

    float getNextSample() noexcept
    {
        float value = 0.0f;

        switch (waveform)
        {
            case 0:  value = renderSaw();      break;
            case 1:  value = renderSquare();   break;
            default: value = renderTriangle(); break;
        }

        phase += phaseInc;
        if (phase >= 1.0f)
            phase -= 1.0f;

        return value;
    }

private:
    static float polyBlep (float t, float dt) noexcept
    {
        if (dt <= 0.0f)
            return 0.0f;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float renderSaw() noexcept
    {
        float value = (2.0f * phase) - 1.0f;
        value -= polyBlep (phase, phaseInc);
        return value;
    }

    float renderSquare() noexcept
    {
        float value = (phase < 0.5f) ? 1.0f : -1.0f;
        value += polyBlep (phase, phaseInc);

        float t2 = phase + 0.5f;
        if (t2 >= 1.0f)
            t2 -= 1.0f;
        value -= polyBlep (t2, phaseInc);

        return value;
    }

    float renderTriangle() noexcept
    {
        // integracion con fuga del cuadrado: triangulo barato y sin DC drift notable
        const float sq = renderSquare();
        triState += 4.0f * phaseInc * sq;
        triState *= 0.999f;
        return triState;
    }

    double sampleRate = 44100.0;
    float frequency = 220.0f;
    float phase = 0.0f;
    float phaseInc = 0.0f;
    float triState = 0.0f;
    int waveform = 0;
};
