/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#ifndef DY4_FILTER_H
#define DY4_FILTER_H

// Add headers as needed
#include <iostream>
#include <vector>
#include "dy4.h"

// Declaration of function prototypes
void impulseResponseLPF(real, real, unsigned short int, std::vector<real> &, real gain = 1.0);
void impulseResponseBPF(real, real, real, unsigned short int, std::vector<real> &);
void convolveFIR(std::vector<real> &, const std::vector<real> &, const std::vector<real> &);

//////////////////////////////////////////////////////////////
// New code as part of benchmarking/testing and the project
//////////////////////////////////////////////////////////////

void convolveFIR_inefficient(std::vector<real> &y, const std::vector<real> &x, const std::vector<real> &h);
void convolveFIR_reference(std::vector<real> &y, const std::vector<real> &x, const std::vector<real> &h);

void blockConvolve_DecimateFast(std::vector<real> &, const std::vector<real> &, const std::vector<real> &, std::vector<real> &, std::vector<real> &, int);
void blockConvolve_DecimateSlow(std::vector<real> &, const std::vector<real> &, const std::vector<real> &, std::vector<real> &, std::vector<real> &, int);

void blockConvolve_ResampleFast(std::vector<real> &, const std::vector<real> &, const std::vector<real> &, std::vector<real> &, std::vector<real> &, int, int);
void blockConvolve_ResampleSlow(std::vector<real> &, const std::vector<real> &, const std::vector<real> &, std::vector<real> &, std::vector<real> &, int, int);

void fmDemodNoArctan(const std::vector<real> &, const std::vector<real> &, real &, real &, std::vector<real> &);

struct PllState {
	real   integrator  = 0.0;
	real   phaseEst    = 0.0;
	real   feedbackI   = 1.0;
	real   feedbackQ   = 0.0;
	double trigOffset  = 0.0;
	real   ncoOut_last = 1.0;
};

void pllBlock(const std::vector<real> &pllIn,
              real freq, real Fs,
              real ncoScale, real phaseAdjust, real normBandwidth,
              PllState &state,
              std::vector<real> &ncoOut);

// PLL variant that outputs both I (cosine) and Q (sine) NCO signals in one pass
void pllBlockIQ(const std::vector<real> &pllIn,
                real freq, real Fs,
                real ncoScale, real phaseAdjust, real normBandwidth,
                PllState &state,
                std::vector<real> &ncoOutI,
                std::vector<real> &ncoOutQ);

// Root-raised-cosine impulse response (T_symbol = 1/2375 s, beta = 0.90)
void impulseResponseRRC(real Fs, int num_taps, std::vector<real> &h);


#endif // DY4_FILTER_H
