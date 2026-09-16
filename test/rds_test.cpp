/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// Unit tests for RDS-related functions:
//   impulseResponseBPF, impulseResponseRRC,
//   fmDemodNoArctan, pllBlock, pllBlockIQ
// using the Google Test framework.

#include <limits.h>
#include <cmath>
#include "dy4.h"
#include "filter.h"
#include "iofunc.h"
#include "gtest/gtest.h"

namespace {

// ─────────────────────────────────────────────────────────────────────────────
//  BPF impulse-response tests
//  The RDS carrier sits at 57 kHz; the BPF is centred there.
// ─────────────────────────────────────────────────────────────────────────────

class BPF_Fixture : public ::testing::Test {
public:
    const real Fs       = 240000.0f;
    const real Flo      =  54000.0f;
    const real Fhi      =  60000.0f;
    const int  num_taps =    101;
    const real EPSILON  =    1e-4f;

    std::vector<real> h;

    void SetUp() override {
        impulseResponseBPF(Fs, Flo, Fhi, num_taps, h);
    }
};

// Filter must have exactly the requested number of taps.
TEST_F(BPF_Fixture, correct_length) {
    ASSERT_EQ((int)h.size(), num_taps)
        << "BPF output size should equal num_taps";
}

// A linear-phase FIR must be symmetric: h[k] ≈ h[N-1-k].
TEST_F(BPF_Fixture, symmetry) {
    for (int k = 0; k < num_taps / 2; k++) {
        EXPECT_NEAR(h[k], h[num_taps - 1 - k], EPSILON)
            << "BPF not symmetric at index " << k;
    }
}

// The centre of the passband (57 kHz) should have a large frequency response;
// DC (0 Hz) should be strongly attenuated.
TEST_F(BPF_Fixture, passband_vs_stopband) {
    real Fmid  = (Flo + Fhi) / 2.0f;

    // Compute |H(e^{j2πf/Fs})| at fmid and at DC by direct DFT summation.
    real pass_re = 0.0f, pass_im = 0.0f;
    real dc_re   = 0.0f;

    for (int k = 0; k < num_taps; k++) {
        real angle = 2.0f * PI * Fmid / Fs * k;
        pass_re   += h[k] * std::cos(angle);
        pass_im   += h[k] * std::sin(angle);
        dc_re     += h[k];   // e^0 = 1
    }

    real pass_mag = std::sqrt(pass_re * pass_re + pass_im * pass_im);
    real dc_mag   = std::abs(dc_re);

    EXPECT_GT(pass_mag, 0.5f) << "BPF passband gain should be close to 1";
    EXPECT_LT(dc_mag,   0.1f) << "BPF should strongly attenuate DC";
}


// ─────────────────────────────────────────────────────────────────────────────
//  RRC impulse-response tests
//  The RRC filter is used for matched filtering of the BPSK RDS symbols.
// ─────────────────────────────────────────────────────────────────────────────

class RRC_Fixture : public ::testing::Test {
public:
    const real Fs       = 57000.0f;   // RDS channel rate
    const int  num_taps =    151;
    const real EPSILON  =    1e-4f;

    std::vector<real> h;

    void SetUp() override {
        impulseResponseRRC(Fs, num_taps, h);
    }
};

// Size check.
TEST_F(RRC_Fixture, correct_length) {
    ASSERT_EQ((int)h.size(), num_taps)
        << "RRC output size should equal num_taps";
}

// RRC is an even-symmetric filter: h[k] == h[N-1-k] (up to floating-point).
TEST_F(RRC_Fixture, symmetry) {
    for (int k = 0; k < num_taps / 2; k++) {
        EXPECT_NEAR(h[k], h[num_taps - 1 - k], EPSILON)
            << "RRC not symmetric at index " << k;
    }
}

// The peak of the RRC should be at the centre tap (the largest magnitude).
TEST_F(RRC_Fixture, peak_at_centre) {
    int centre = num_taps / 2;
    real peak  = std::abs(h[centre]);
    for (int k = 0; k < num_taps; k++) {
        if (k == centre) continue;
        EXPECT_LE(std::abs(h[k]), peak + EPSILON)
            << "RRC: tap " << k << " exceeds the centre tap magnitude";
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  fmDemodNoArctan tests
// ─────────────────────────────────────────────────────────────────────────────

class FMDemod_Fixture : public ::testing::Test {
public:
    const real EPSILON = 1e-4f;
};

// A constant-phase phasor (I=1, Q=0 throughout) has zero instantaneous
// frequency, so every demodulated sample should be 0.
TEST_F(FMDemod_Fixture, constant_phasor_gives_zero) {
    const int N = 256;
    std::vector<real> I(N, 1.0f), Q(N, 0.0f);
    std::vector<real> out(N, 0.0f);
    real prev_I = 1.0f, prev_Q = 0.0f;

    fmDemodNoArctan(I, Q, prev_I, prev_Q, out);

    for (int k = 0; k < N; k++)
        EXPECT_NEAR(out[k], 0.0f, EPSILON)
            << "Constant phasor should demodulate to 0 at index " << k;
}

// A phasor rotating at frequency f has constant instantaneous frequency.
// fmDemodNoArctan approximates (I·dQ - Q·dI) / (I²+Q²).
// For a unit circle rotating by Δθ = 2π·f/Fs per sample the result should
// be roughly sin(Δθ) ≈ Δθ for small Δθ.
TEST_F(FMDemod_Fixture, rotating_phasor_nonzero) {
    const int  N   = 256;
    const real Fs  = 240000.0f;
    const real f   =   1000.0f;      // 1 kHz tone
    const real dth = 2.0f * PI * f / Fs;

    std::vector<real> I(N), Q(N);
    for (int k = 0; k < N; k++) {
        I[k] = std::cos(dth * k);
        Q[k] = std::sin(dth * k);
    }

    std::vector<real> out(N, 0.0f);
    real prev_I = std::cos(-dth), prev_Q = std::sin(-dth);

    fmDemodNoArctan(I, Q, prev_I, prev_Q, out);

    // After the first sample every output should be approximately sin(Δθ).
    real expected = std::sin(dth);
    for (int k = 0; k < N; k++)
        EXPECT_NEAR(out[k], expected, 1e-3f)
            << "Rotating phasor demod mismatch at index " << k;
}

// Zero denominator (I=0, Q=0) must not produce NaN or Inf.
TEST_F(FMDemod_Fixture, zero_phasor_is_safe) {
    const int N = 32;
    std::vector<real> I(N, 0.0f), Q(N, 0.0f);
    std::vector<real> out(N, -1.0f);
    real prev_I = 0.0f, prev_Q = 0.0f;

    fmDemodNoArctan(I, Q, prev_I, prev_Q, out);

    for (int k = 0; k < N; k++) {
        EXPECT_FALSE(std::isnan(out[k])) << "NaN at index " << k;
        EXPECT_FALSE(std::isinf(out[k])) << "Inf at index " << k;
        EXPECT_NEAR(out[k], 0.0f, EPSILON) << "Expected 0 for zero phasor at " << k;
    }
}


// ─────────────────────────────────────────────────────────────────────────────
//  pllBlock tests (single-output NCO used for stereo pilot / RDS carrier)
// ─────────────────────────────────────────────────────────────────────────────

class PllBlock_Fixture : public ::testing::Test {
public:
    const real Fs            = 240000.0f;
    const real freq          =  19000.0f;   // stereo pilot
    const real ncoScale      =      1.0f;
    const real phaseAdjust   =      0.0f;
    const real normBandwidth =    0.01f;
    const real EPSILON       =    1e-3f;

    PllState state;

    void SetUp() override {
        // Default-constructed PllState already zero-initialised via filter.h
    }
};

// Output size must equal input size.
TEST_F(PllBlock_Fixture, output_size_matches_input) {
    const int N = 512;
    std::vector<real> pllIn(N, 0.0f);
    std::vector<real> ncoOut;

    pllBlock(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth, state, ncoOut);

    ASSERT_EQ((int)ncoOut.size(), N)
        << "pllBlock: output size should match input size";
}

// NCO output must always stay in [-1, 1] (it is a cosine).
TEST_F(PllBlock_Fixture, output_bounded) {
    const int N = 1024;
    std::vector<real> pllIn(N, 0.0f);
    std::vector<real> ncoOut;

    pllBlock(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth, state, ncoOut);

    for (int k = 0; k < N; k++)
        EXPECT_LE(std::abs(ncoOut[k]), 1.0f + EPSILON)
            << "pllBlock NCO output out of range at index " << k;
}

// Feed the PLL a pure sine at exactly its target frequency.
// After some settling the NCO should lock and produce values near ±1.
TEST_F(PllBlock_Fixture, locks_to_tone) {
    const int N = 4800;   // 20 ms at 240 kHz — enough to settle
    std::vector<real> pllIn(N);
    for (int k = 0; k < N; k++)
        pllIn[k] = std::cos(2.0f * PI * freq / Fs * k);

    std::vector<real> ncoOut;
    pllBlock(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth, state, ncoOut);

    // Check the last 10 % of samples — the PLL should have settled.
    int settle = (int)(N * 0.9);
    real max_abs = 0.0f;
    for (int k = settle; k < N; k++)
        max_abs = std::max(max_abs, std::abs(ncoOut[k]));

    EXPECT_GT(max_abs, 0.5f)
        << "pllBlock NCO amplitude too small after settling";
}


// ─────────────────────────────────────────────────────────────────────────────
//  pllBlockIQ tests (IQ NCO used to generate the RDS complex carrier)
// ─────────────────────────────────────────────────────────────────────────────

class PllBlockIQ_Fixture : public ::testing::Test {
public:
    const real Fs            = 240000.0f;
    const real freq          =  57000.0f;   // RDS carrier
    const real ncoScale      =      1.0f;
    const real phaseAdjust   =      0.0f;
    const real normBandwidth =    0.01f;
    const real EPSILON       =    1e-3f;

    PllState state;
};

// Both I and Q outputs must have the same length as the input.
TEST_F(PllBlockIQ_Fixture, output_sizes_match_input) {
    const int N = 512;
    std::vector<real> pllIn(N, 0.0f);
    std::vector<real> ncoOutI, ncoOutQ;

    pllBlockIQ(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth,
               state, ncoOutI, ncoOutQ);

    ASSERT_EQ((int)ncoOutI.size(), N) << "pllBlockIQ: I output size mismatch";
    ASSERT_EQ((int)ncoOutQ.size(), N) << "pllBlockIQ: Q output size mismatch";
}

// The I/Q outputs should satisfy I²+Q² ≈ 1 (unit circle) for every sample.
TEST_F(PllBlockIQ_Fixture, unit_circle) {
    const int N = 1024;
    std::vector<real> pllIn(N, 0.0f);
    std::vector<real> ncoOutI, ncoOutQ;

    pllBlockIQ(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth,
               state, ncoOutI, ncoOutQ);

    for (int k = 0; k < N; k++) {
        real mag = ncoOutI[k] * ncoOutI[k] + ncoOutQ[k] * ncoOutQ[k];
        EXPECT_NEAR(mag, 1.0f, EPSILON)
            << "pllBlockIQ: |I|^2 + |Q|^2 != 1 at index " << k;
    }
}

// The I and Q outputs must be 90° apart (quadrature).
// For a cosine I and sine Q: I·Q_next - Q·I_next ≈ sin(Δθ) per step.
// A simpler check: the cross-correlation at lag-0 must be ~0 for a locked NCO
// running at a fixed frequency with zero input.
TEST_F(PllBlockIQ_Fixture, i_q_quadrature) {
    const int N = 1024;
    std::vector<real> pllIn(N, 0.0f);
    std::vector<real> ncoOutI, ncoOutQ;

    pllBlockIQ(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth,
               state, ncoOutI, ncoOutQ);

    // Mean dot product should be near zero for I ⊥ Q.
    real dot = 0.0f;
    for (int k = 0; k < N; k++)
        dot += ncoOutI[k] * ncoOutQ[k];
    dot /= N;

    EXPECT_NEAR(dot, 0.0f, 0.05f)
        << "pllBlockIQ: I and Q outputs are not approximately orthogonal";
}

// Feed in a tone at the target frequency; after settling the NCO should lock.
TEST_F(PllBlockIQ_Fixture, locks_to_rds_carrier) {
    const int N = 4800;
    std::vector<real> pllIn(N);
    for (int k = 0; k < N; k++)
        pllIn[k] = std::cos(2.0f * PI * freq / Fs * k);

    std::vector<real> ncoOutI, ncoOutQ;
    pllBlockIQ(pllIn, freq, Fs, ncoScale, phaseAdjust, normBandwidth,
               state, ncoOutI, ncoOutQ);

    int settle = (int)(N * 0.9);
    real max_abs = 0.0f;
    for (int k = settle; k < N; k++)
        max_abs = std::max(max_abs, std::abs(ncoOutI[k]));

    EXPECT_GT(max_abs, 0.5f)
        << "pllBlockIQ NCO amplitude too small after settling";
}

} // end of namespace
