/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// This file shows how to write convolution benchmark functions, using Google C++ benchmark framework.
// (it is based on https://github.com/google/benchmark/blob/main/docs/user_guide.md)

#include <benchmark/benchmark.h>
#include "utils_bench.h"
#include "dy4.h"
#include "iofunc.h"
#include "filter.h"

#define RANGE_MULTIPLIER 2
#define MIN_INPUT_SIZE 32768
#define MAX_INPUT_SIZE (4 * MIN_INPUT_SIZE)
#define MIN_FILTER_SIZE 101
#define MAX_FILTER_SIZE (1 * MIN_FILTER_SIZE)

const int lower_bound = -1;
const int upper_bound = 1;

static void Bench_convolveFIR_reference(benchmark::State& state) {
	int N = state.range(0);
	int M = state.range(1);

	std::vector<real> x(N);
	std::vector<real> h(M);
	std::vector<real> y(N + M -1);

	generate_random_values(x, lower_bound, upper_bound);
	generate_random_values(h, lower_bound, upper_bound);

	for (auto _ : state) {
		convolveFIR_reference(y, x, h);
	}
}

// register benchmark Bench_convolveFIR_reference //

BENCHMARK(Bench_convolveFIR_reference)
    ->ArgsProduct({benchmark::CreateRange(MIN_INPUT_SIZE, MAX_INPUT_SIZE, RANGE_MULTIPLIER),
                   benchmark::CreateRange(MIN_FILTER_SIZE, MAX_FILTER_SIZE, RANGE_MULTIPLIER)});

////////////////////////////////////////////////////

static void Bench_convolveFIR_inefficient(benchmark::State& state) {
	int N = state.range(0);
	int M = state.range(1);

	std::vector<real> x(N);
	std::vector<real> h(M);
	std::vector<real> y(N + M -1);

	generate_random_values(x, lower_bound, upper_bound);
	generate_random_values(h, lower_bound, upper_bound);

	for (auto _ : state) {
		convolveFIR_inefficient(y, x, h);
	}
}

// register benchmark Bench_convolveFIR_inefficient //

BENCHMARK(Bench_convolveFIR_inefficient)
    ->ArgsProduct({benchmark::CreateRange(MIN_INPUT_SIZE, MAX_INPUT_SIZE, RANGE_MULTIPLIER),
                   benchmark::CreateRange(MIN_FILTER_SIZE, MAX_FILTER_SIZE, RANGE_MULTIPLIER)});

//////////////////////////////////////////////////////
