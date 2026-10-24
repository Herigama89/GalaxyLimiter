#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "LimiterStyles.h"

// ============================================================================
// Building blocks. All memory is allocated in prepare(), never on the audio thread.
// ============================================================================

// Running maximum over the last `window` samples (monotonic deque in a ring buffer).
class SlidingMax
{
public:
    void allocate (int maxWindow)
    {
        cap = maxWindow + 2;
        vals.assign ((size_t) cap, 0.0f);
        idx.assign ((size_t) cap, 0);
        reset();
    }

    void setWindow (int w) { window = std::max (1, std::min (w, cap - 2)); reset(); }
    void reset() noexcept  { head = 0; size = 0; n = 0; }

    float push (float v) noexcept
    {
        while (size > 0 && vals[(size_t) back()] <= v)
            --size;

        const int pos = (head + size) % cap;
        vals[(size_t) pos] = v;
        idx[(size_t) pos]  = n;
        ++size;

        while (idx[(size_t) head] <= n - window)
        {
            head = (head + 1) % cap;
            --size;
        }

        ++n;
        return vals[(size_t) head];
    }

private:
    int back() const noexcept { return (head + size - 1) % cap; }

    std::vector<float>   vals;
    std::vector<int64_t> idx;
    int cap = 2, head = 0, size = 0, window = 1;
    int64_t n = 0;
};

// Moving average. Averaging the held gain over the attack window gives a
// smooth ramp that is guaranteed to reach the target before the peak arrives.
class Boxcar
{
public:
    void allocate (int maxLen) { buf.assign ((size_t) std::max (1, maxLen), 1.0f); }

    void setLength (int L, float fill) noexcept
    {
        len = std::max (1, std::min (L, (int) buf.size()));
        std::fill (buf.begin(), buf.begin() + len, fill);
        sum = (double) fill * len;
        pos = 0;
    }

    float push (float v) noexcept
    {
        sum += (double) v - (double) buf[(size_t) pos];
        buf[(size_t) pos] = v;
        if (++pos >= len) pos = 0;
        return (float) (sum / len);
    }

private:
    std::vector<float> buf;
    double sum = 1.0;
    int len = 1, pos = 0;
};

class DelayLine
{
public:
    void allocate (int maxDelay) { buf.assign ((size_t) maxDelay + 1, 0.0f); }

    void setDelay (int d) noexcept
    {
        delay = std::max (0, std::min (d, (int) buf.size() - 1));
        std::fill (buf.begin(), buf.end(), 0.0f);
        pos = 0;
    }

    float process (float x) noexcept
    {
        if (delay == 0) return x;
        const float y = buf[(size_t) pos];
        buf[(size_t) pos] = x;
        if (++pos >= delay) pos = 0;
        return y;
    }

private:
    std::vector<float> buf;
    int delay = 0, pos = 0;
};

// ============================================================================
// Limiter engine
//
// Signal flow per channel:
//   required gain reduction (soft-knee gain computer, stereo-linked)
//   -> sliding max over lookahead (hold)
//   -> dual-stage release envelope (fast + program-dependent slow)
//   -> dB to linear -> moving average over attack window (smooth ramp)
//   -> multiply the lookahead-delayed audio
//   -> style colouration (2nd/3rd harmonics) -> DC blocker
//   -> output clipper (soft or hard) that never exceeds the ceiling
// ============================================================================
class LimiterEngine
{
public:
    static constexpr int kMaxChannels = 8;

    void prepare (double maxRate, double maxLookaheadMs, int numChannels)
    {
        numCh = std::max (1, std::min (numChannels, kMaxChannels));
        maxLook = (int) std::ceil (maxRate * maxLookaheadMs * 0.001) + 4;

        for (auto& c : ch)
        {
            c.hold.allocate (maxLook + 1);
            c.box.allocate (maxLook + 1);
            c.delay.allocate (maxLook);
        }
    }

    // Safe to call on the audio thread (no allocation). Resets the state.
    void configure (double sampleRate, int lookaheadSamples, int numChannels) noexcept
    {
        fs = sampleRate;
        numCh = std::max (1, std::min (numChannels, kMaxChannels));
        D = std::max (0, std::min (lookaheadSamples, maxLook));

        for (auto& c : ch)
        {
            c.hold.setWindow (D + 1);
            c.delay.setDelay (D);
            c.fast = c.slow = 0.0f;
            c.dcX = c.dcY = c.dryX = c.dryY = 0.0f;
            c.lastGain = 1.0f;
        }
        attackLen = -1;
    }

    void setParameters (float ceilingLinear, float attackMs, float releaseMs,
                        float link01, int styleIndex, bool deltaMode) noexcept
    {
        const auto& s = kStyles[std::max (0, std::min (styleIndex, kNumStyles - 1))];

        ceiling  = std::max (1.0e-4f, ceilingLinear);
        invCeil  = 1.0f / ceiling;
        link     = std::max (0.0f, std::min (1.0f, link01));
        delta    = deltaMode;

        const float ceilingDb = 20.0f * std::log10 (ceiling);
        limitDb  = ceilingDb + s.overshootDb;
        knee     = s.kneeDb;
        kneeStartLin = dbToLin (limitDb - 0.5f * knee);

        auto coeff = [this] (float ms) { return (float) std::exp (-1.0 / (std::max (0.05f, ms) * 0.001 * fs)); };
        relFast    = coeff (releaseMs * s.releaseScale);
        relSlow    = coeff (releaseMs * s.slowReleaseScale);
        slowAtt    = 1.0f - coeff (s.slowAttackMs);
        slowWeight = s.slowWeight;

        const int A = std::max (1, std::min ((int) std::lround (attackMs * s.attackScale * 0.001 * fs), D + 1));
        if (A != attackLen)
        {
            for (auto& c : ch) c.box.setLength (A, c.lastGain);
            attackLen = A;
        }

        clipT     = ceiling * dbToLin (-3.0f * s.clipSoftness);
        clipRange = ceiling - clipT;
        satEven   = s.satEven;
        satOdd    = s.satOdd;
        dcR       = 1.0f - (float) (2.0 * 3.14159265358979 * 3.0 / fs);
    }

    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        const int nc = std::min (numChannels, numCh);
        const bool colour  = (satEven > 0.0f || satOdd > 0.0f);
        const bool dcBlock = satEven > 0.0f;   // only even harmonics create DC

        for (int i = 0; i < numSamples; ++i)
        {
            float req[kMaxChannels];
            float maxReq = 0.0f;

            for (int c = 0; c < nc; ++c)
            {
                req[c] = reduction (std::abs (data[c][i]));
                maxReq = std::max (maxReq, req[c]);
            }

            for (int c = 0; c < nc; ++c)
            {
                auto& st = ch[(size_t) c];

                // Stereo link in the dB domain: never less reduction than the channel needs.
                const float r    = req[c] + link * (maxReq - req[c]);
                const float held = st.hold.push (r);

                // Dual-stage release
                st.fast = std::max (held, st.fast * relFast);
                st.slow = held > st.slow ? st.slow + (held - st.slow) * slowAtt
                                         : held + (st.slow - held) * relSlow;

                const float grDb = std::max (st.fast, st.slow * slowWeight);
                const float g    = st.box.push (dbToLin (-grDb));
                st.lastGain = g;

                const float dry = st.delay.process (data[c][i]);
                float y = dry * g;

                if (colour)
                {
                    // Colour only appears while the limiter is actually working
                    const float d  = std::min (1.0f, grDb * (1.0f / 6.0f));
                    const float xn = y * invCeil;
                    y = ceiling * (xn + satEven * d * xn * xn - satOdd * d * xn * xn * xn);
                }

                // DC blocker (removes offset from even harmonics). The same filter is run on the
                // dry signal so Delta only contains what the limiter really removed.
                float dryRef = dry;
                if (dcBlock)
                {
                    const float dcOut = y - st.dcX + dcR * st.dcY;
                    st.dcX = y;  st.dcY = dcOut;  y = dcOut;

                    const float dryOut = dry - st.dryX + dcR * st.dryY;
                    st.dryX = dry;  st.dryY = dryOut;  dryRef = dryOut;
                }
                y = clip (y);

                data[c][i] = delta ? (dryRef - y) : y;
                maxGrDb = std::max (maxGrDb, grDb);
            }
        }
    }

    float getAndResetMaxGrDb() noexcept { const float v = maxGrDb; maxGrDb = 0.0f; return v; }

private:
    static float dbToLin (float db) noexcept { return std::exp (db * 0.11512925464970229f); }

    float reduction (float level) const noexcept
    {
        if (level <= kneeStartLin) return 0.0f;

        const float lvlDb = 20.0f * std::log10 (level);
        const float hard  = lvlDb - limitDb;
        if (knee <= 0.0f) return std::max (0.0f, hard);

        const float d    = lvlDb - limitDb + 0.5f * knee;
        const float soft = d < knee ? d * d / (2.0f * knee) : hard;
        return std::max (0.0f, std::max (soft, hard));
    }

    float clip (float x) const noexcept
    {
        const float ax = std::abs (x);
        if (ax <= clipT) return x;
        if (clipRange <= 1.0e-9f) return std::copysign (ceiling, x);
        return std::copysign (clipT + clipRange * std::tanh ((ax - clipT) / clipRange), x);
    }

    struct Channel
    {
        SlidingMax hold;
        Boxcar     box;
        DelayLine  delay;
        float fast = 0.0f, slow = 0.0f, lastGain = 1.0f, dcX = 0.0f, dcY = 0.0f, dryX = 0.0f, dryY = 0.0f;
    };

    Channel ch[kMaxChannels];
    double fs = 44100.0;
    int numCh = 2, maxLook = 0, D = 0, attackLen = -1;

    float ceiling = 1.0f, invCeil = 1.0f, limitDb = 0.0f, knee = 0.0f, kneeStartLin = 1.0f;
    float relFast = 0.99f, relSlow = 0.999f, slowAtt = 0.01f, slowWeight = 1.0f, link = 1.0f;
    float clipT = 1.0f, clipRange = 0.0f, satEven = 0.0f, satOdd = 0.0f, dcR = 0.9995f;
    bool  delta = false;
    float maxGrDb = 0.0f;
};
