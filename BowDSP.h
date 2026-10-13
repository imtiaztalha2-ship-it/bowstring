#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace bow
{

// Coefficient for a one-pole smoother with the given time constant
inline float onePoleCoeff (float ms, double sampleRate)
{
    if (ms <= 0.0f)
        return 0.0f;
    return (float) std::exp (-1.0 / (0.001 * (double) ms * sampleRate));
}

//==============================================================================
// Monophonic pitch tracker (YIN algorithm) running on a ~12 kHz decimated signal.
// Window ~21 ms + lag range ~21 ms, analysed every ~4 ms, median-of-3 smoothing.
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decim   = juce::jmax (1, (int) std::round (sampleRate / 12000.0));
        dsRate  = (float) (sampleRate / decim);
        lpCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 1800.0 / sampleRate);

        ring.fill (0.0f);
        frame.fill (0.0f);
        cmnd.fill (1.0f);
        history.fill (0.0f);
        writePos = accCount = hopCount = histIdx = 0;
        acc = lp1 = lp2 = 0.0f;
        pitch = 0.0f;
        voiced = false;
    }

    void push (float x) noexcept
    {
        // two one-pole low-passes as a cheap anti-alias before decimating
        lp1 = x   + (lp1 - x)   * lpCoeff;
        lp2 = lp1 + (lp2 - lp1) * lpCoeff;
        acc += lp2;

        if (++accCount >= decim)
        {
            ring[(size_t) writePos] = acc / (float) decim;
            writePos = (writePos + 1) % N;
            acc = 0.0f;
            accCount = 0;

            if (++hopCount >= hop)
            {
                hopCount = 0;
                analyse();
            }
        }
    }

    bool  isVoiced() const noexcept { return voiced; }
    float getPitch() const noexcept { return pitch; }

private:
    static constexpr int   W = 256, tauMax = 256, N = W + tauMax, hop = 48;
    static constexpr float threshold = 0.15f;

    void analyse() noexcept
    {
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % N)];

        float energy = 0.0f;
        for (int i = 0; i < W; ++i)
            energy += frame[(size_t) i] * frame[(size_t) i];

        if (energy < 1.0e-7f * (float) W)
        {
            voiced = false;
            return;
        }

        // cumulative mean normalised difference function
        cmnd[0] = 1.0f;
        float running = 0.0f;

        for (int tau = 1; tau < tauMax; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float d = frame[(size_t) j] - frame[(size_t) (j + tau)];
                sum += d * d;
            }
            running += sum;
            cmnd[(size_t) tau] = running > 0.0f ? sum * (float) tau / running : 1.0f;
        }

        const int tauMin = juce::jmax (2, (int) (dsRate / 1600.0f));
        int found = -1;

        for (int tau = tauMin; tau < tauMax - 1; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax - 1 && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                found = tau;
                break;
            }
        }

        if (found < 0)
        {
            voiced = false;
            return;
        }

        // parabolic interpolation around the minimum
        const float s0 = cmnd[(size_t) found - 1], s1 = cmnd[(size_t) found], s2 = cmnd[(size_t) found + 1];
        const float denom = s0 - 2.0f * s1 + s2;
        float shift = std::abs (denom) > 1.0e-9f ? 0.5f * (s0 - s2) / denom : 0.0f;
        shift = juce::jlimit (-1.0f, 1.0f, shift);

        const float f = dsRate / ((float) found + shift);

        history[(size_t) histIdx] = f;
        histIdx = (histIdx + 1) % 3;

        float a = history[0], b = history[1], c = history[2];
        if (a <= 0.0f || b <= 0.0f || c <= 0.0f)
        {
            pitch = f;
        }
        else
        {
            if (a > b) std::swap (a, b);
            if (b > c) std::swap (b, c);
            if (a > b) std::swap (a, b);
            pitch = b; // median of last three estimates
        }

        voiced = true;
    }

    std::array<float, N>      ring {}, frame {};
    std::array<float, tauMax> cmnd {};
    std::array<float, 3>      history {};
    int   decim = 4, writePos = 0, accCount = 0, hopCount = 0, histIdx = 0;
    float dsRate = 12000.0f, lpCoeff = 0.0f, lp1 = 0.0f, lp2 = 0.0f, acc = 0.0f;
    float pitch = 0.0f;
    bool  voiced = false;
};

//==============================================================================
// A "bowed string" voice: anti-aliased sawtooth (Helmholtz-like motion),
// bow noise, slow bow envelope, glide, delayed vibrato and pressure-dependent brightness.
class BowedVoice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        phase = 0.0f;
        logFreq = targetLogFreq = std::log (220.0f);
        amp = targetAmp = 0.0f;
        vibPhase = 0.0f;
        noteTime = 0.0f;

        juce::dsp::ProcessSpec spec { sr, 512, 1 };
        noiseFilter.prepare (spec);
        noiseFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        noiseFilter.setCutoffFrequency (2800.0f);
        noiseFilter.setResonance (0.9f);

        toneFilter.prepare (spec);
        toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        toneFilter.setResonance (0.6f);
        toneFilter.setCutoffFrequency (4000.0f);
    }

    void setParams (float attackMs, float releaseMs, float glideMs,
                    float vibRateHz, float vibDepthCents, float noiseAmount, float brightness) noexcept
    {
        attCoeff   = onePoleCoeff (attackMs,  sampleRate);
        relCoeff   = onePoleCoeff (releaseMs, sampleRate);
        glideCoeff = onePoleCoeff (glideMs,   sampleRate);
        vibInc     = (float) (juce::MathConstants<double>::twoPi * vibRateHz / sampleRate);
        vibDepth   = vibDepthCents;
        noiseAmt   = noiseAmount;
        bright     = brightness;
    }

    void setTarget (float hz, bool jump) noexcept
    {
        targetLogFreq = std::log (juce::jlimit (30.0f, 5000.0f, hz));
        if (jump)
            logFreq = targetLogFreq;
    }

    void  setGate (float a) noexcept { targetAmp = a; }
    float getAmp() const noexcept    { return amp; }

    // New pick attack while a note is sounding: restart vibrato and dip the bow
    void rebow() noexcept
    {
        noteTime = 0.0f;
        amp *= 0.7f;
    }

    float tick() noexcept
    {
        amp     = targetAmp     + (amp - targetAmp) * (targetAmp > amp ? attCoeff : relCoeff);
        logFreq = targetLogFreq + (logFreq - targetLogFreq) * glideCoeff;

        if (amp < 0.0005f) noteTime = 0.0f;
        else               noteTime += (float) (1.0 / sampleRate);

        // vibrato fades in after the note starts, like a real player
        const float fade = juce::jlimit (0.0f, 1.0f, (noteTime - 0.18f) / 0.5f);

        vibPhase += vibInc;
        if (vibPhase > juce::MathConstants<float>::twoPi)
            vibPhase -= juce::MathConstants<float>::twoPi;

        const float cents = std::sin (vibPhase) * vibDepth * fade;
        const float f  = std::exp (logFreq) * std::exp2 (cents / 1200.0f);
        const float dt = juce::jmin (0.45f, f / (float) sampleRate);

        float saw = 2.0f * phase - 1.0f - polyBlep (phase, dt);
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;

        // bow hair noise, strongest during the attack
        float n = (random.nextFloat() * 2.0f - 1.0f) * noiseAmt * (0.35f + 0.65f * (1.0f - fade));
        n = noiseFilter.processSample (0, n);

        if (++ctrlCount >= 32)
        {
            ctrlCount = 0;
            const float cutoff = 900.0f + bright * bright * 9000.0f * (0.4f + 0.6f * juce::jmin (1.0f, amp));
            toneFilter.setCutoffFrequency (juce::jmin (cutoff, (float) sampleRate * 0.45f));
        }

        const float out = toneFilter.processSample (0, saw * 0.6f + n);
        return out * amp;
    }

private:
    static float polyBlep (float t, float dt) noexcept
    {
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

    double sampleRate = 44100.0;
    float phase = 0.0f, logFreq = 0.0f, targetLogFreq = 0.0f;
    float amp = 0.0f, targetAmp = 0.0f;
    float attCoeff = 0.0f, relCoeff = 0.0f, glideCoeff = 0.0f;
    float vibPhase = 0.0f, vibInc = 0.0f, vibDepth = 0.0f, noteTime = 0.0f;
    float noiseAmt = 0.0f, bright = 0.5f;
    int   ctrlCount = 0;
    juce::Random random { 1234 };
    juce::dsp::StateVariableTPTFilter<float> noiseFilter, toneFilter;
};

//==============================================================================
// Sarangi-style sympathetic strings: tuned comb resonators (Sa, Pa, Sa', Re', Ga', Pa', Sa'')
class SympatheticStrings
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& c : combs)
        {
            c.buf.assign ((size_t) (sr / 40.0) + 4, 0.0f);
            c.write = 0;
            c.lp = 0.0f;
        }
        currentKey = -1;
        setKey (0);
    }

    void setKey (int semitone) noexcept
    {
        if (semitone == currentKey)
            return;
        currentKey = semitone;

        static constexpr float ratios[numStrings] = { 1.0f, 1.5f, 2.0f, 2.25f, 2.5f, 3.0f, 4.0f };
        const float sa = 130.81f * std::exp2 ((float) semitone / 12.0f); // Sa around C3

        for (int i = 0; i < numStrings; ++i)
            combs[(size_t) i].delay = (float) (sampleRate / (sa * ratios[i]));
    }

    float process (float in) noexcept
    {
        float sum = 0.0f;
        for (auto& c : combs)
        {
            const int len = (int) c.buf.size();
            float rp = (float) c.write - c.delay;
            if (rp < 0.0f) rp += (float) len;
            const int   i0 = (int) rp;
            const int   i1 = (i0 + 1) % len;
            const float fr = rp - (float) i0;
            const float d  = c.buf[(size_t) i0] + (c.buf[(size_t) i1] - c.buf[(size_t) i0]) * fr;

            c.lp = d * 0.7f + c.lp * 0.3f;            // damping inside the loop
            c.buf[(size_t) c.write] = in * 0.05f + c.lp * 0.985f;
            c.write = (c.write + 1) % len;
            sum += d;
        }
        return sum * 0.35f;
    }

private:
    static constexpr int numStrings = 7;
    struct Comb { std::vector<float> buf; int write = 0; float delay = 100.0f, lp = 0.0f; };
    std::array<Comb, numStrings> combs;
    double sampleRate = 44100.0;
    int currentKey = -1;
};

//==============================================================================
// Stereo ping-pong delay with smoothed time and a darkened feedback path
class PingPongDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        len = (int) (sr * 2.0) + 4;
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        write = 0;
        dampL = dampR = 0.0f;
        timeSamples.reset (sr, 0.08);
        timeSamples.setCurrentAndTargetValue ((float) (sr * 0.38));
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        dampL = dampR = 0.0f;
    }

    void setParams (float ms, float fb, float wet) noexcept
    {
        timeSamples.setTargetValue (juce::jlimit (1.0f, (float) (len - 3), ms * 0.001f * (float) sampleRate));
        feedback = fb;
        mix = wet;
    }

    void process (float& l, float& r) noexcept
    {
        const float d  = timeSamples.getNextValue();
        const float dL = read (bufL, d);
        const float dR = read (bufR, d);

        dampL = dL * 0.6f + dampL * 0.4f;
        dampR = dR * 0.6f + dampR * 0.4f;

        const float in = 0.5f * (l + r);
        bufL[(size_t) write] = in + dampR * feedback;
        bufR[(size_t) write] = dampL * feedback;
        write = (write + 1) % len;

        l += dL * mix;
        r += dR * mix;
    }

private:
    float read (const std::vector<float>& b, float delay) const noexcept
    {
        float rp = (float) write - delay;
        if (rp < 0.0f) rp += (float) len;
        const int   i0 = (int) rp;
        const int   i1 = (i0 + 1) % len;
        const float fr = rp - (float) i0;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
    }

    std::vector<float> bufL, bufR;
    juce::SmoothedValue<float> timeSamples;
    double sampleRate = 44100.0;
    int len = 1, write = 0;
    float feedback = 0.3f, mix = 0.2f, dampL = 0.0f, dampR = 0.0f;
};

} // namespace bow
