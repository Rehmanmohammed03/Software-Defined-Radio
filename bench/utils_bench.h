/*
   Comp Eng 3DY4 (Computer Systems Integration Project)

   Department of Electrical and Computer Engineering
   McMaster University
   Ontario, Canada
*/

// This file defines some utilities to be used together with Google C++ benchmark framework.
// (it is based on https://github.com/google/benchmark/blob/main/docs/user_guide.md)

#ifndef DY4_UTILS_BENCH_H
#define DY4_UTILS_BENCH_H

#include <benchmark/benchmark.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <limits.h>
#include <complex>

class CustomReporter : public benchmark::ConsoleReporter {
public:
	CustomReporter() : ConsoleReporter() {}

	void ReportRuns(const std::vector<Run>& runs) override {
		if (!header_printed) {
			PrintCustomHeader();
			header_printed = true;
		}

		for (const auto& run : runs) {
			std::cout << std::fixed << std::setprecision(2);
			std::cout << std::setw(70) << std::left << run.benchmark_name()
					  << std::setw(10) << std::right << run.GetAdjustedCPUTime() / 1e6  // Convert ns -> ms
					  << std::endl;
		}
	}

private:
	bool header_printed = false;

	void PrintCustomHeader() {
		std::cout << "--------------------------------------------------------------------------------\n";
		std::cout << std::setw(65) << std::left << "Benchmark"
				  << std::setw(15) << std::right << "CPU Time (ms)"
				  << std::endl;
		std::cout << "--------------------------------------------------------------------------------\n";
	}
};

#endif // DY4_UTILS_BENCH_H
