#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace fx
{
    constexpr float pi = 3.14159265358979f;

    // Circular buffer with fractional (4-point Hermite) reads.
    class DelayLine
    {
    public:
        void prepare (int minSize)
        {
            size = juce::nextPowerOfTwo (minSize + 8);
            mask = size - 1;
            buffer.assign ((size_t) size, 0.0f);
            writePos = 0;
        }

        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

        void push (float x)
        {
            buffer[(size_t) writePos] = x;
            writePos = (writePos + 1) & mask;
        }

        // delaySamples >= 1 reads data written before the most recent push.
        float read (float delaySamples) const
        {
            delaySamples = juce::jlimit (1.0f, (float) (size - 4), delaySamples);
            const float pos = (float) writePos - delaySamples;
            const float fl = std::floor (pos);
            const float t = pos - fl;
            const int i = (int) fl;

            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * t + c2) * t + c1) * t + x0;
        }

    private:
        float at (int i) const { return buffer[(size_t) (i & mask)]; }

        std::vector<float> buffer;
        int size = 0, mask = 0, writePos = 0;
    };

    // Topology-preserving state variable filter (Cytomic / Zavalishin).
    class SVF
    {
    public:
        void set (float cutoff, float sampleRate, float q = 0.7071f)
        {
            cutoff = juce::jlimit (10.0f, sampleRate * 0.45f, cutoff);
            g = std::tan (pi * cutoff / sampleRate);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() { ic1 = ic2 = 0.0f; }

        float lowpass (float v0)  { float lp, hp; process (v0, lp, hp); return lp; }
        float highpass (float v0) { float lp, hp; process (v0, lp, hp); return hp; }

    private:
        void process (float v0, float& lp, float& hp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2;
            hp = v0 - k * v1 - v2;
        }

        float g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    };

    class DCBlocker
    {
    public:
        float process (float x)
        {
            const float y = x - x1 + 0.995f * y1;
            x1 = x;
            y1 = y;
            return y;
        }
        void reset() { x1 = y1 = 0.0f; }

    private:
        float x1 = 0, y1 = 0;
    };

    // Saturation stage. Small-signal gain is ~1 so it can sit inside the feedback loop
    // without causing runaway; loudness is made up on the wet output instead (see makeup()).
    class Saturator
    {
    public:
        void reset() { dc.reset(); holdCounter = 0; held = 0.0f; }

        static float drivenGain (float drive01) { return 1.0f + 24.0f * drive01 * drive01; }

        static float makeup (int type, float drive01)
        {
            if (type == 3) return 1.0f + 0.3f * drive01;
            return std::pow (drivenGain (drive01), 0.45f);
        }

        float process (float x, int type, float drive01)
        {
            if (drive01 <= 0.001f)
                return x;

            const float k = drivenGain (drive01);

            switch (type)
            {
                case 0: // Tape: smooth symmetric compression
                    return std::tanh (k * x) / k;

                case 1: // Tube: asymmetric -> even harmonics
                {
                    const float biased = k * x + 0.25f * drive01;
                    const float y = biased >= 0.0f ? std::tanh (biased)
                                                   : std::tanh (0.6f * biased) / 0.6f * 0.8f;
                    return dc.process (y / k);
                }

                case 2: // Fuzz: hard clipping
                {
                    const float y = juce::jlimit (-1.0f, 1.0f, 1.5f * k * x);
                    return y / (1.5f * k);
                }

                default: // Lo-Fi: bit + sample-rate reduction
                {
                    const int holdLength = 1 + (int) (drive01 * 11.0f);
                    if (++holdCounter >= holdLength)
                    {
                        holdCounter = 0;
                        const float levels = std::pow (2.0f, 14.0f - drive01 * 10.5f);
                        held = std::round (std::tanh (x) * levels) / levels;
                    }
                    return held;
                }
            }
        }

    private:
        DCBlocker dc;
        int holdCounter = 0;
        float held = 0.0f;
    };

    // Two-tap granular pitch shifter, delay-line based.
    class PitchShifter
    {
    public:
        void prepare (double sampleRate)
        {
            window = (float) (sampleRate * 0.045);
            line.prepare ((int) (window * 2.0f) + 16);
            phase = 0.0f;
        }

        void reset() { line.clear(); phase = 0.0f; }

        float process (float x, float ratio)
        {
            line.push (x);

            const float phaseB = std::fmod (phase + 0.5f, 1.0f);
            const float gA = std::sin (pi * phase);
            const float gB = std::sin (pi * phaseB);
            const float y = gA * gA * line.read (1.0f + phase * window)
                          + gB * gB * line.read (1.0f + phaseB * window);

            phase += (1.0f - ratio) / window;
            phase -= std::floor (phase);
            return y;
        }

    private:
        DelayLine line;
        float window = 2000.0f, phase = 0.0f;
    };
}
