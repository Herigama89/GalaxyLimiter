#pragma once

// Each "style" is a voicing of the same lookahead limiter core.
// The numbers change how the gain computer, envelopes, output clipper
// and harmonic colouration behave, which is what gives each mode its sound.
struct LimiterStyle
{
    const char* name;
    const char* description;

    float attackScale;       // multiplies the user Attack (length of the gain-smoothing window)
    float releaseScale;      // multiplies the user Release for the fast release stage
    float slowReleaseScale;  // multiplies the user Release for the slow (program-dependent) stage
    float slowAttackMs;      // how quickly sustained loudness builds up the slow stage
    float slowWeight;        // 0..1 how strongly the slow stage holds gain reduction
    float kneeDb;            // soft-knee width in dB (0 = hard knee)
    float overshootDb;       // transients allowed past the limiter into the output clipper
    float clipSoftness;      // 0 = hard clip, 1 = soft clip starting 3 dB below the ceiling
    float satEven;           // 2nd-harmonic colour (tube-like), scaled by gain reduction
    float satOdd;            // 3rd-harmonic colour (tape/transistor-like), scaled by gain reduction
};

inline constexpr int kNumStyles = 10;

inline const LimiterStyle kStyles[kNumStyles] =
{
    //  name            description
    //  atk   rel   slowRel slowAtk w     knee  over  clip  even    odd
    { "Transparent",  "Clean brickwall with smooth attack and program-dependent release. Lowest distortion.",
        1.0f, 1.0f, 3.0f,   80.f,  0.6f, 0.0f, 0.0f, 0.15f, 0.000f, 0.000f },

    { "Punchy",       "Shorter attack; lets transients poke through into a soft clipper for extra snap.",
        0.5f, 0.8f, 2.5f,   40.f,  0.7f, 0.0f, 1.5f, 0.60f, 0.000f, 0.002f },

    { "Dynamic",      "Strong program-dependent release that keeps micro-dynamics and groove alive.",
        1.2f, 0.5f, 4.0f,  120.f,  0.9f, 1.0f, 0.5f, 0.50f, 0.000f, 0.000f },

    { "Aggressive",   "Fast and loud. Heavy transient clipping for maximum density.",
        0.3f, 0.4f, 1.5f,   20.f,  0.5f, 0.0f, 3.0f, 0.90f, 0.003f, 0.008f },

    { "Modern",       "Balanced dual-stage release for loud, clean modern masters.",
        0.8f, 0.7f, 3.0f,   50.f,  0.8f, 0.5f, 0.8f, 0.50f, 0.000f, 0.002f },

    { "Safe",         "Slow and careful. Avoids distortion even with heavy gain reduction.",
        1.5f, 1.2f, 4.0f,  100.f,  0.9f, 0.0f, 0.0f, 0.00f, 0.000f, 0.000f },

    { "Bus Glue",     "Wide soft knee and slow release. Behaves like a gentle bus compressor into a wall.",
        1.3f, 1.5f, 5.0f,  150.f,  1.0f, 6.0f, 0.3f, 0.70f, 0.001f, 0.001f },

    { "Vari-Mu Tube", "Wide knee, slow program release and warm 2nd-harmonic colour.",
        1.5f, 1.8f, 6.0f,  200.f,  1.0f, 8.0f, 0.5f, 0.80f, 0.012f, 0.003f },

    { "FET Analog",   "Very fast attack with edgy odd-harmonic bite, like a pushed FET limiter.",
        0.2f, 0.5f, 2.0f,   15.f,  0.6f, 1.0f, 2.0f, 0.80f, 0.004f, 0.012f },

    { "Opto",         "Smooth optical-style release that gets slower the harder you push it.",
        1.4f, 2.0f, 8.0f,  300.f,  1.0f, 5.0f, 0.3f, 0.70f, 0.006f, 0.002f },
};
