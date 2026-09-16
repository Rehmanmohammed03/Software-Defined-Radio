/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// This file is the main function for benchmarking, using Google C++ benchmark framework.
// (it is based on https://github.com/google/benchmark/blob/main/docs/user_guide.md)

// After compilation you can benchmark only a subset of registered functions as shown below
// (we assume the executable is called "bench_primitives" and
// we have registered benchmarks with "DFT" in their name)
//
// ./bench_primitives --benchmark_filter=DFT.*
//

#include "utils_bench.h"

int main(int argc, char** argv) {
	benchmark::Initialize(&argc, argv);
	if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;

	// Use the custom reporter
	CustomReporter reporter;
	benchmark::RunSpecifiedBenchmarks(&reporter);

	return 0;
}
