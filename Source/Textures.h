#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Procedurally generated surface textures (no image files needed).
namespace tex
{
    inline uint32_t hash32 (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

    inline float hash2 (int x, int y, uint32_t seed)
    {
        const uint32_t h = hash32 ((uint32_t) x * 73856093U ^ (uint32_t) y * 19349663U ^ seed * 83492791U);
        return (float) (h & 0xffffffU) / 16777215.0f;
    }

    inline float valueNoise (float x, float y, uint32_t seed)
    {
        const float fx0 = std::floor (x), fy0 = std::floor (y);
        const int xi = (int) fx0, yi = (int) fy0;
        const float fx = x - fx0, fy = y - fy0;
        const float u = fx * fx * (3.0f - 2.0f * fx);
        const float v = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash2 (xi, yi, seed), b = hash2 (xi + 1, yi, seed);
        const float c = hash2 (xi, yi + 1, seed), d = hash2 (xi + 1, yi + 1, seed);
        return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
    }

    inline float fbm (float x, float y, uint32_t seed, int octaves)
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f, f = 1.0f;
        for (int o = 0; o < octaves; ++o)
        {
            sum += amp * valueNoise (x * f, y * f, seed + (uint32_t) o * 101U);
            norm += amp;
            amp *= 0.5f;
            f *= 2.0f;
        }
        return sum / norm;
    }

    inline juce::Colour scaled (juce::Colour c, float k)
    {
        return juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, c.getFloatRed() * k),
                                            juce::jlimit (0.0f, 1.0f, c.getFloatGreen() * k),
                                            juce::jlimit (0.0f, 1.0f, c.getFloatBlue() * k), 1.0f);
    }

    // Horizontally brushed aluminium.
    inline juce::Image brushedMetal (int w, int h, juce::Colour base, uint32_t seed)
    {
        juce::Image img (juce::Image::RGB, w, h, false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float streak = 0.55f * valueNoise ((float) x / 180.0f, (float) y / 1.4f, seed)
                                   + 0.30f * valueNoise ((float) x / 30.0f, (float) y / 0.9f, seed + 7)
                                   + 0.15f * hash2 (x, y, seed + 13);
                bd.setPixelColour (x, y, scaled (base, 0.86f + 0.24f * streak));
            }
        return img;
    }

    // Painted steel: soft mottling, grain, specks and fine scratches.
    inline juce::Image paintedMetal (int w, int h, juce::Colour base, uint32_t seed, float wear = 1.0f)
    {
        juce::Image img (juce::Image::RGB, w, h, false);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float mottle = fbm ((float) x / 110.0f, (float) y / 110.0f, seed, 3);
                    const float grain = hash2 (x, y, seed + 3);
                    float k = 0.92f + 0.13f * mottle + 0.07f * (grain - 0.5f);
                    if (grain > 0.9975f) k += 0.18f * wear;
                    bd.setPixelColour (x, y, scaled (base, k));
                }
        }

        juce::Graphics g (img);
        juce::Random rng ((juce::int64) seed);
        const int numScratches = (int) (w * h / 9000 * wear);
        for (int i = 0; i < numScratches; ++i)
        {
            const float x = rng.nextFloat() * (float) w, y = rng.nextFloat() * (float) h;
            const float len = 6.0f + rng.nextFloat() * 50.0f;
            const float a = (rng.nextFloat() - 0.5f) * 0.9f + (rng.nextBool() ? 0.0f : 1.4f);
            g.setColour (juce::Colours::white.withAlpha (0.025f + rng.nextFloat() * 0.06f));
            g.drawLine (x, y, x + len * std::cos (a), y + len * std::sin (a), 0.6f);
        }
        return img;
    }

    // Vertical wood grain for the side cheeks.
    inline juce::Image wood (int w, int h, uint32_t seed)
    {
        const juce::Colour dark (0xff3e1f0e), light (0xffa2602f);
        juce::Image img (juce::Image::RGB, w, h, false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float v = (float) x * 0.9f + 22.0f * fbm ((float) x / 60.0f, (float) y / 420.0f, seed, 3);
                const float rings = 0.5f + 0.5f * std::sin (v * 0.42f + 7.0f * fbm ((float) x / 25.0f, (float) y / 260.0f, seed + 3, 2));
                const float fiber = valueNoise ((float) x / 1.3f, (float) y / 45.0f, seed + 5);
                float t = 0.62f * std::pow (rings, 1.6f) + 0.38f * fiber;
                t = juce::jlimit (0.0f, 1.0f, t);
                bd.setPixelColour (x, y, scaled (dark.interpolatedWith (light, t), 0.94f + 0.08f * hash2 (x, y, seed + 9)));
            }
        return img;
    }
}
