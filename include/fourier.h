/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#ifndef DY4_FOURIER_H
#define DY4_FOURIER_H

// Add headers as needed
#include <iostream>
#include <vector>
#include <complex>
#include <cmath>

// Declaration of function prototypes
void DFT(const std::vector<real> &,
	std::vector<std::complex<real>> &);

// You should add your own IDFT
// time-permitting you can build your own function for FFT

void computeVectorMagnitude(const std::vector<std::complex<real>> &,
	std::vector<real> &);

// Provide the prototype to estimate PSD
// ...

//////////////////////////////////////////////////////////////
// New code as part of benchmarking/testing and the project
//////////////////////////////////////////////////////////////

void DFT_reference(const std::vector<real>& x, std::vector<std::complex<real>>& Xf);
void DFT_init_bins(const std::vector<real>& x, std::vector<std::complex<real>>& Xf);

void generate_DFT_twiddles(const int& N, std::vector<std::complex<real>> &Twiddle1D);
void generate_DFT_matrix(const int& N, std::vector<std::vector<std::complex<real>>>& Twiddle2D);

#endif // DY4_FOURIER_H
