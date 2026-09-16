/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// This file shows how to write DFT unit tests, using Google C++ test framework.
// (it is based on https://github.com/google/googletest/blob/main/docs/index.md)

#include <limits.h>
#include "dy4.h"
#include "iofunc.h"
#include "fourier.h"
#include "gtest/gtest.h"

namespace {

	class DFT_Fixture: public ::testing::Test {

		public:

			const int N = 1024;
			const int lower_bound = -1;
			const int upper_bound = 1;
			const real EPSILON = 1e-4;

			std::vector<real> x;
			std::vector<std::complex<real>> Xf_reference, Xf_test;
			// Twiddles will be used by the functions you will develop in-lab
			std::vector<std::complex<real>> Twiddle1D;
			std::vector<std::vector<std::complex<real>>> Twiddle2D;

			DFT_Fixture( ) {
				x.resize(N);
				Xf_reference.resize(N);
				Xf_test.resize(N);
				Twiddle1D.resize(N);
				Twiddle2D.resize(N, std::vector<std::complex<real>>(N));
			}

			void SetUp( ) {
				generate_random_values(x, lower_bound, upper_bound);
				generate_DFT_matrix(N, Twiddle2D);
				generate_DFT_twiddles(N, Twiddle1D);
				DFT_reference(x, Xf_reference);
			}

			void TearDown( ) {
			}

			~DFT_Fixture( )  {
			}
	};

	TEST_F(DFT_Fixture, DFT_init_bins_NEAR) { // to disable: TEST_F(DFT_Fixture, DISABLED_DFT_init_bins_NEAR)

		DFT_init_bins(x, Xf_test);

		ASSERT_EQ(Xf_reference.size(), Xf_test.size()) << "The output vector sizes for DFT_reference and DFT_init_bins are of unequal length";

		for (int i = 0; i < (int)Xf_reference.size(); i++) {
			EXPECT_NEAR(std::abs(Xf_reference[i]), std::abs(Xf_test[i]), EPSILON) << "Original/DFT_init_bins vectors differ at index " << i;
		}
	}


} // end of namespace

