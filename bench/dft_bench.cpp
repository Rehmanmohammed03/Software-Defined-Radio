/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// This file shows how to write DFT benchmark functions, using Google C++ benchmark framework.
// (it is based on https://github.com/google/benchmark/blob/main/docs/user_guide.md)

#include <benchmark/benchmark.h>
#include "utils_bench.h"
#include "dy4.h"
#include "iofunc.h"
#include "fourier.h"

#define RANGE_MULTIPLIER 2
#define MIN_INPUT_SIZE 256
#define MAX_INPUT_SIZE (8 * MIN_INPUT_SIZE)

const int lower_bound = -1;
const int upper_bound = 1;

static void Bench_DFT_reference(benchmark::State& state) {
	int N = state.range(0);

	std::vector<real> x(N);
	std::vector<std::complex<real>> Xf(N);
	generate_random_values(x, lower_bound, upper_bound);

	for (auto _ : state) {
		DFT_reference(x, Xf);
	}
}

// register benchmark Bench_DFT_reference //

BENCHMARK(Bench_DFT_reference)->RangeMultiplier(RANGE_MULTIPLIER)->Range(MIN_INPUT_SIZE, MAX_INPUT_SIZE);

////////////////////////////////////////////

static void Bench_DFT_init_bins(benchmark::State& state) {
	int N = state.range(0);

	std::vector<real> x(N);
	std::vector<std::complex<real>> Xf(N);
	generate_random_values(x, lower_bound, upper_bound);

	for (auto _ : state) {
		DFT_init_bins(x, Xf);
	}
}

// register benchmark Bench_DFT_init_bins //

BENCHMARK(Bench_DFT_init_bins)->RangeMultiplier(RANGE_MULTIPLIER)->Range(MIN_INPUT_SIZE, MAX_INPUT_SIZE);

////////////////////////////////////////////

