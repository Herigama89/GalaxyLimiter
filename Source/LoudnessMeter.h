#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// ITU-R BS.1770-4 / EBU R128 loudness: momentary (400 ms), short-term (3 s),
// and gated integrated loudness. Integrated uses a 0.1 LU histogram so memory is fixed.
class LoudnessMeter
{
public:
    LoudnessMeter()
    {
        momentary.store (-100.0f);
        shortTerm.store (-100.0f);
        integrated.store (-100.0f);
    }

    void prepare (double sampleRate, int numChannels)
    {
        fs  = sampleRate;
        nCh = std::max (1, std::min (numChannels, kMaxCh));
        designFilters();
        blockLen = std::max (1, (int) std::lround (fs * 0.1));
        resetInternal();
    }

    void requestReset() noexcept { resetFlag.store (true); }

    void process (const float* const* data, int numChannels, int n) noexcept
    {
        if (resetFlag.exchange (false))
            resetInternal();

        const int chs = std::min (numChannels, nCh);

        for (int i = 0; i < n; ++i)
        {
            double sq = 0.0;
            for (int c = 0; c < chs; ++c)
            {
                double v = shelf[(size_t) c].process ((double) data[c][i]);
                v = highpass[(size_t) c].process (v);
                sq += v * v;
            }
            blockSum += sq;
            if (++blockCount >= blockLen)
                finishBlock();
        }
    }

    std::atomic<float> momentary, shortTerm, integrated;

private:
    static constexpr int kMaxCh = 8;
    static constexpr int kSub   = 30;   // 30 x 100 ms = 3 s
    static constexpr int kBins  = 800;  // -70 .. +10 LUFS at 0.1 LU

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static float toLufs (double p) noexcept { return p > 0.0 ? (float) (-0.691 + 10.0 * std::log10 (p)) : -100.0f; }

    void designFilters()
    {
        const double pi = 3.14159265358979323846;

        // Stage 1: high shelf (head effects)
        double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        double K  = std::tan (pi * f0 / fs);
        double Vh = std::pow (10.0, G / 20.0);
        double Vb = std::pow (Vh, 0.4996667741545416);
        double a0 = 1.0 + K / Q + K * K;
        Biquad s;
        s.b0 = (Vh + Vb * K / Q + K * K) / a0;
        s.b1 = 2.0 * (K * K - Vh) / a0;
        s.b2 = (Vh - Vb * K / Q + K * K) / a0;
        s.a1 = 2.0 * (K * K - 1.0) / a0;
        s.a2 = (1.0 - K / Q + K * K) / a0;

        // Stage 2: RLB high-pass
        f0 = 38.13547087602444; Q = 0.5003270373238773;
        K  = std::tan (pi * f0 / fs);
        a0 = 1.0 + K / Q + K * K;
        Biquad h;
        h.b0 = 1.0; h.b1 = -2.0; h.b2 = 1.0;
        h.a1 = 2.0 * (K * K - 1.0) / a0;
        h.a2 = (1.0 - K / Q + K * K) / a0;

        shelf.fill (s);
        highpass.fill (h);
    }

    void resetInternal() noexcept
    {
        for (auto& b : shelf)    b.z1 = b.z2 = 0.0;
        for (auto& b : highpass) b.z1 = b.z2 = 0.0;
        sub.fill (0.0); subIdx = 0; subFilled = 0;
        histE.fill (0.0); histN.fill (0);
        blockSum = 0.0; blockCount = 0;
        momentary.store (-100.0f); shortTerm.store (-100.0f); integrated.store (-100.0f);
    }

    double meanLast (int k) const noexcept
    {
        k = std::min (k, subFilled);
        if (k <= 0) return 0.0;
        double s = 0.0;
        for (int j = 0; j < k; ++j)
            s += sub[(size_t) ((subIdx - 1 - j + kSub) % kSub)];
        return s / k;
    }

    void finishBlock() noexcept
    {
        sub[(size_t) subIdx] = blockSum / blockCount;
        subIdx = (subIdx + 1) % kSub;
        if (subFilled < kSub) ++subFilled;
        blockSum = 0.0; blockCount = 0;

        if (subFilled >= 4)
        {
            const double pm = meanLast (4);
            const float  lm = toLufs (pm);
            momentary.store (lm);

            if (lm > -70.0f)   // absolute gate
            {
                const int bin = std::max (0, std::min ((int) ((lm + 70.0f) * 10.0f), kBins - 1));
                histE[(size_t) bin] += pm;
                histN[(size_t) bin] += 1;
            }
        }

        if (subFilled >= kSub)
            shortTerm.store (toLufs (meanLast (kSub)));

        computeIntegrated();
    }

    void computeIntegrated() noexcept
    {
        double e = 0.0; uint64_t n = 0;
        for (int b = 0; b < kBins; ++b) { e += histE[(size_t) b]; n += histN[(size_t) b]; }
        if (n == 0) { integrated.store (-100.0f); return; }

        const float gate = toLufs (e / (double) n) - 10.0f;   // relative gate
        const int startBin = std::max (0, std::min ((int) std::ceil ((gate + 70.0f) * 10.0f), kBins));

        double e2 = 0.0; uint64_t n2 = 0;
        for (int b = startBin; b < kBins; ++b) { e2 += histE[(size_t) b]; n2 += histN[(size_t) b]; }
        integrated.store (n2 > 0 ? toLufs (e2 / (double) n2) : -100.0f);
    }

    double fs = 48000.0;
    int nCh = 2, blockLen = 4800, blockCount = 0;
    double blockSum = 0.0;
    std::array<Biquad, kMaxCh> shelf, highpass;
    std::array<double, kSub> sub {};
    int subIdx = 0, subFilled = 0;
    std::array<double, kBins>   histE {};
    std::array<uint32_t, kBins> histN {};
    std::atomic<bool> resetFlag { false };
};
