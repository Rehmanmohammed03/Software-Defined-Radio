/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#ifndef DY4_DY4_H
#define DY4_DY4_H

// Macro for conditional compilation
#ifdef DOUBLE
	typedef double real;
#else
	typedef float real;
#endif

// Some general and reusable stuff
// Our beloved PI constant
#define PI 3.14159265358979323846

// Although we use DFT (no FFT ... yet), the number of points for a
// Fourier transform is defined as NFFT (same as matplotlib)
#define NFFT 512

#endif // DY4_DY4_H
