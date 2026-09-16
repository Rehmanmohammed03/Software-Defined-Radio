/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#include "dy4.h"
#include "genfunc.h"

// Some basic support functions for signal generation from the previous lab
void generateSin(std::vector<real> &t, std::vector<real> &x, real Fs, real interval, real frequency = 7.0, real amplitude = 5.0, real phase = 0.0)
{
	t.clear();
	x.clear();
	real dt = 1 / Fs;
	for (real i = 0.0; i < interval; i += dt) {
		t.push_back(i);
		x.push_back(amplitude * std::sin(2 * PI * frequency * i + phase));
	}
}

void addSin(const std::vector<std::vector<real>> &sv, std::vector<real> &added)
{
	// Assumes at least one sine passed to this function and all input sines are of the same size
	for (int i = 0; i < (int)sv[0].size(); i++) {
		real addval = 0.0;
		// Note: sv.size() returns the number of sines (or rows in 2D repr)
		// sv[0].size() returns the number of samples in a sine (or cols in 2D repr)
		for (int k = 0; k < (int)sv.size(); k++) {
			addval += sv[k][i];
		}
		added.push_back(addval);
	}
}

void generateRandomSamples(std::vector<real> &x, unsigned int N, unsigned short int max, unsigned char precision) {
	x.clear();
	x.resize(N);
	int int_random_max = 2 * (max * int(pow(10, precision)));
	for (int i = 0; i < (int)x.size(); i++) {
		x[i] = int(std::rand() % int_random_max);
		x[i] = (x[i] - (int_random_max / 2)) / pow(10, precision);
	}
}
