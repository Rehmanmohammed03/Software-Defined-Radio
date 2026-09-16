/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// Unit tests for convolution, decimation, and resampling
// using the Google Test framework.

#include <limits.h>
#include "dy4.h"
#include "iofunc.h"
#include "filter.h"
#include "gtest/gtest.h"

namespace {

	class Convolution_Fixture: public ::testing::Test {

		public:

			const int N = 1024;	// signal size
			const int M = 101;	// kernel size
			const int lower_bound = -1;
			const int upper_bound = 1;
			const real EPSILON = 1e-4;

			std::vector<real> x, h, y_reference, y_test;

			Convolution_Fixture() {
				x.resize(N);
				h.resize(M);
				y_reference.resize(N + M - 1);
				y_test.resize(N + M - 1);
			}

			void SetUp() {
				generate_random_values(x, lower_bound, upper_bound);
				generate_random_values(h, lower_bound, upper_bound);
				convolveFIR_reference(y_reference, x, h);
			}

			void TearDown() {
			}

			~Convolution_Fixture() {
			}
	};

	TEST_F(Convolution_Fixture, convolveFIR_inefficient_NEAR) {

		convolveFIR_inefficient(y_test, x, h);

		ASSERT_EQ(y_reference.size(), y_test.size()) << "Output vector sizes for convolveFIR_reference and convolveFIR_inefficient are unequal";

		for (int i = 0; i < (int)y_reference.size(); i++) {
			EXPECT_NEAR(y_reference[i], y_test[i], EPSILON) << "Original/convolveFIR_inefficient vectors differ at index " << i;
		}
	}

	///////////////////////////// DECIMATOR TESTS /////////////////////////////
	// These tests compare the fast and slow decimator versions.
	// Both should give the same final output.

	class DecimateBlock_Fixture : public ::testing::Test {
	public:
		// This block size is similar to a realistic mono path case
		const int N = 9600;
		const int M = 101;
		const int D = 5;
		const real EPSILON = 1e-3f;

		std::vector<real> x, h;

		DecimateBlock_Fixture() { x.resize(N); h.resize(M); }

		void SetUp() override {
			generate_random_values(x, -1.0f, 1.0f);
			generate_random_values(h, -1.0f, 1.0f);
		}
	};

	// Test one block
	TEST_F(DecimateBlock_Fixture, single_block_fast_matches_slow) {
		std::vector<real> state_fast(M - 1, 0.0f), state_slow(M - 1, 0.0f);
		std::vector<real> comb_fast(M - 1 + N, 0.0f), comb_slow(M - 1 + N, 0.0f);

		// Fast version outputs only the kept samples
		std::vector<real> y_fast(N / D, 0.0f);
		// Slow version uses a full block first, then resizes inside
		std::vector<real> y_slow(N, 0.0f);

		blockConvolve_DecimateFast(y_fast, x, h, state_fast, comb_fast, D);
		blockConvolve_DecimateSlow(y_slow, x, h, state_slow, comb_slow, D);

		ASSERT_EQ(y_fast.size(), y_slow.size())
			<< "Output sizes differ: fast=" << y_fast.size()
			<< " slow=" << y_slow.size();
		for (int i = 0; i < (int)y_fast.size(); i++)
			EXPECT_NEAR(y_fast[i], y_slow[i], EPSILON)
				<< "Mismatch at output index " << i;
	}

	// Test multiple blocks in a row
	TEST_F(DecimateBlock_Fixture, multi_block_state_propagation) {
		std::vector<real> state_fast(M - 1, 0.0f), state_slow(M - 1, 0.0f);
		std::vector<real> comb_fast(M - 1 + N, 0.0f), comb_slow(M - 1 + N, 0.0f);

		for (int blk = 0; blk < 3; blk++) {
			// New random block each time
			generate_random_values(x, -1.0f, 1.0f);

			std::vector<real> y_fast(N / D, 0.0f);
			std::vector<real> y_slow(N, 0.0f);

			blockConvolve_DecimateFast(y_fast, x, h, state_fast, comb_fast, D);
			blockConvolve_DecimateSlow(y_slow, x, h, state_slow, comb_slow, D);

			ASSERT_EQ(y_fast.size(), y_slow.size())
				<< "Block " << blk << ": output sizes differ";
			for (int i = 0; i < (int)y_fast.size(); i++)
				EXPECT_NEAR(y_fast[i], y_slow[i], EPSILON)
					<< "Block " << blk << ", index " << i;
		}
	}

	///////////////////////////// RESAMPLER TESTS /////////////////////////////
	// These tests compare the fast and slow resampler versions.
	// Both should give the same final resampled output.

	class ResampleBlock_Fixture : public ::testing::Test {
	public:
		// Smaller values are used here so the slow version stays manageable
		const int N = 256;    // input block size
		const int taps = 11;  // taps before multiplying by U
		const int U = 5;      // upsample factor
		const int D = 7;      // decimation factor
		// Final filter length
		int M;
		const real EPSILON = 1e-3f;

		std::vector<real> x, h;

		ResampleBlock_Fixture() : M(taps * U) {
			x.resize(N);
			h.resize(M);
		}

		void SetUp() override {
			generate_random_values(x, -1.0f, 1.0f);
			// Scale the filter values down a bit
			generate_random_values(h, -1.0f / U, 1.0f / U);
		}
	};

	// Test one block
	TEST_F(ResampleBlock_Fixture, single_block_fast_matches_slow) {
		// Fast version uses input-rate buffers
		std::vector<real> state_fast(M - 1, 0.0f);
		std::vector<real> comb_fast(M - 1 + N, 0.0f);
		std::vector<real> y_fast(N * U / D, 0.0f);

		// Slow version uses the full upsampled block
		std::vector<real> state_slow(M - 1, 0.0f);
		std::vector<real> comb_slow(M - 1 + N * U, 0.0f);
		std::vector<real> y_slow(N * U, 0.0f);  // slow resizes internally

		blockConvolve_ResampleFast(y_fast, x, h, state_fast, comb_fast, D, U);
		blockConvolve_ResampleSlow(y_slow, x, h, state_slow, comb_slow, D, U);

		ASSERT_EQ(y_fast.size(), y_slow.size())
			<< "Output sizes differ: fast=" << y_fast.size()
			<< " slow=" << y_slow.size();
		for (int i = 0; i < (int)y_fast.size(); i++)
			EXPECT_NEAR(y_fast[i], y_slow[i], EPSILON)
				<< "Mismatch at output index " << i;
	}

	// Test multiple blocks in a row
	TEST_F(ResampleBlock_Fixture, multi_block_state_propagation) {
		std::vector<real> state_fast(M - 1, 0.0f);
		std::vector<real> comb_fast(M - 1 + N, 0.0f);

		std::vector<real> state_slow(M - 1, 0.0f);
		std::vector<real> comb_slow(M - 1 + N * U, 0.0f);

		for (int blk = 0; blk < 3; blk++) {
			generate_random_values(x, -1.0f, 1.0f);

			std::vector<real> y_fast(N * U / D, 0.0f);
			std::vector<real> y_slow(N * U, 0.0f);

			blockConvolve_ResampleFast(y_fast, x, h, state_fast, comb_fast, D, U);
			blockConvolve_ResampleSlow(y_slow, x, h, state_slow, comb_slow, D, U);

			ASSERT_EQ(y_fast.size(), y_slow.size())
				<< "Block " << blk << ": output sizes differ";
			for (int i = 0; i < (int)y_fast.size(); i++)
				EXPECT_NEAR(y_fast[i], y_slow[i], EPSILON)
					<< "Block " << blk << ", index " << i;
		}
	}

} // end of namespace
