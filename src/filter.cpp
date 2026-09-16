/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#include "dy4.h"
#include "filter.h"
#include <cmath>


void impulseResponseLPF(real Fs, real Fc, unsigned short int num_taps, std::vector<real> &h, real gain)
{
	h.clear();
	h.resize(num_taps, 0.0);

	int center = (num_taps - 1)/2;
	real scale_factor = 0.0;
	int n;

	for (int k = 0; k < (int)h.size(); k++){
		n = k - center;
		if (n == 0)
			h[k] = (2*Fc/Fs);
		else
			h[k] = (std::sin(2*PI*Fc*n/Fs))/(PI*n);

		h[k] = h[k] * (0.5 - (0.5)*std::cos(2*PI*k/(num_taps - 1)));
		scale_factor = scale_factor + h[k];
	}

	for (int k = 0; k < (int)h.size(); k++){
		h[k] = (h[k]/scale_factor) * gain;
	}
}

void impulseResponseBPF(real Fs, real Flo, real Fhi, unsigned short int num_taps, std::vector<real> &h)
{
	h.clear();
	h.resize(num_taps, 0.0);
	int center = (num_taps - 1) / 2;
	real Fmid  = (Flo + Fhi) / 2.0;

	for (int k = 0; k < (int)h.size(); k++) {
		int  n = k - center;
		real hlo, hhi;
		if (n == 0) {
			hlo = 2.0 * Flo / Fs;
			hhi = 2.0 * Fhi / Fs;
		} else {
			hlo = std::sin(2.0 * PI * Flo * n / Fs) / (PI * n);
			hhi = std::sin(2.0 * PI * Fhi * n / Fs) / (PI * n);
		}
		h[k] = (hhi - hlo) * (0.5 - 0.5 * std::cos(2.0 * PI * k / (num_taps - 1)));
	}

	real scale_factor = 0.0;
	for (int k = 0; k < (int)h.size(); k++) {
		int n = k - center;
		scale_factor += h[k] * std::cos(2.0 * PI * Fmid / Fs * n);
	}

	for (int k = 0; k < (int)h.size(); k++)
		h[k] /= scale_factor;
}

void convolveFIR(std::vector<real> &y, const std::vector<real> &x, const std::vector<real> &h)
{
	y.clear();
	y.resize(x.size() + h.size() - 1, 0.0);

	for (int n = 0; n < (int)y.size(); n++){
		for (int k = 0; k < (int) h.size(); k++) {
			if (n >= k && (n - k) < (int)x.size())
				y[n] += h[k] * x[n - k];
		}
	}
}

//////////////////////////////////////////////////////////////
// New code as part of benchmarking/testing and the project
//////////////////////////////////////////////////////////////

void convolveFIR_inefficient(std::vector<real> &y, const std::vector<real> &x, const std::vector<real> &h) {
    y.clear();
    y.resize(int(x.size() + h.size()-1), 0.0);
    for (int n = 0; n < (int)y.size(); n++) {
        for (int k = 0; k < (int)x.size(); k++) {
            if ((n - k >= 0) && (n - k) < (int)h.size())
                y[n] += x[k] * h[n - k];
        }
    }
}

void convolveFIR_reference(std::vector<real> &y, const std::vector<real> &x, const std::vector<real> &h) {
    y.clear();
    y.resize(int(x.size() + h.size()-1), 0.0);

    for (int n = 0; n < (int)y.size(); n++) {
        for (int k = 0; k < (int)h.size(); k++) {
            if ((n - k >= 0) && (n - k) < (int)x.size())
                y[n] += h[k] * x[n - k];
        }
    }
}

void blockConvolve_DecimateFast(std::vector<real> &yb, const std::vector<real> &xb, const std::vector<real> &h, std::vector<real> &state, std::vector<real> &combined, int decimation)
{
	int M = h.size();
	int unrolled_limit = M - (M % 4);

	for (int i = 0; i < M - 1; i++)
		combined[i] = state[i];

	for (int i = 0; i < (int)xb.size(); i++)
		combined[i + (M-1)] = xb[i];

	int idx = 0;
    for (int n = 0; n < (int)xb.size(); n+=decimation){
		int base  = n + (M-1);
		yb[idx] = 0;

        for (int k = 0; k < unrolled_limit; k+=4)
		{
            yb[idx] += h[k] * combined[base - k] + h[k + 1] * combined[base - (k+1)] + h[k + 2] * combined[base - (k+2)] + h[k + 3] * combined[base - (k+3)];
		}

		for (int k = unrolled_limit; k < M; k++)
			yb[idx] += h[k] * combined[base - k];

		idx++;
	}

    state.assign(combined.end() - (M-1), combined.end());
}

void blockConvolve_DecimateSlow(std::vector<real> &yb, const std::vector<real> &xb, const std::vector<real> &h, std::vector<real> &state, std::vector<real> &combined, int decimation)
{
	int M = h.size();

	for (int i = 0; i < M - 1; i++)
		combined[i] = state[i];

	for (int i = 0; i < (int)xb.size(); i++)
		combined[i + (M-1)] = xb[i];

	int idx = 0;
    for (int n = 0; n < (int)xb.size(); n++){
		int base  = n + (M-1);
		yb[idx] = 0;

        for (int k = 0; k < M; k++)
		{
            yb[idx] += h[k] * combined[base - k];
		}

		idx++;
	}

	for (int n = 0; n < (int)yb.size()/decimation; n++)
	{
		yb[n] = yb[n*decimation];
	}

	yb.resize(int(yb.size()/decimation));
	yb.shrink_to_fit();
    state.assign(combined.end() - (M-1), combined.end());
}

void blockConvolve_ResampleSlow(std::vector<real> &yb, const std::vector<real> &xb, const std::vector<real> &h, std::vector<real> &state, std::vector<real> &combined, int decimation, int upsampling)
{
	int M = h.size();

	std::vector<real> xb_u((int)xb.size() * upsampling, 0.0);

	int index_u = 0;

	for (int i = 0; i < (int)xb.size() * upsampling; i++)
	{
		if (i % upsampling == 0)
			xb_u[i] = xb[index_u++];
		else
			xb_u[i] = 0.0;
	}

	for (int i = 0; i < M - 1; i++)
		combined[i] = state[i];

	for (int i = 0; i < (int)xb_u.size(); i++)
		combined[i + (M-1)] = xb_u[i];

	int idx = 0;
    for (int n = 0; n < (int)xb_u.size(); n++){
		int base  = n + (M-1);
		yb[idx] = 0;
        for (int k = 0; k < M; k++)
		{
            yb[idx] += h[k] * combined[base - k];
		}
		idx++;
	}

	for (int t = 0; t < (int)(yb.size()/decimation); t++)
	{
		yb[t] = yb[t*decimation];
	}
	yb.resize(int(yb.size()/decimation));
	yb.shrink_to_fit();

    state.assign(combined.end() - (M-1), combined.end());
}

void blockConvolve_ResampleFast(std::vector<real> &yb, const std::vector<real> &xb, const std::vector<real> &h, std::vector<real> &state, std::vector<real> &combined, int decimation, int upsampling)
{
	int M = h.size();
	int M_offset = M-1;

	for (int i = 0; i < M_offset; i++)
		combined[i] = state[i];

	for (int i = 0; i < (int)xb.size(); i++)
		combined[i + (M_offset)] = xb[i];

	int output_size = ((int)xb.size()*upsampling)/decimation;
	int unrolled_limit = 0;

	int idx = 0;
	int original = 0;
	int phase = 0;
	int start = 0;
    for (int n = 0; n < (int)output_size; n++){
		yb[idx] = 0;
		phase = original % upsampling;
		start = (original - phase)/upsampling;
		int base  = start + (M_offset);
		int tap_count = (M - phase) / upsampling;
		int unrolled_taps = tap_count - (tap_count % 4);
		unrolled_limit = phase + unrolled_taps * upsampling;
        for (int k = phase; k < unrolled_limit; k+=(upsampling*4))
		{
            yb[idx] += h[k] * combined[base] +
						h[k + 1*upsampling] * combined[base-1] +
						h[k + 2*upsampling] * combined[base-2] +
						h[k + 3*upsampling] * combined[base-3];
			base -= 4;
		}

		for (int k = unrolled_limit; k < M; k+=upsampling)
			yb[idx] += h[k] * combined[base--];
		original += decimation;
		idx++;
	}

    state.assign(combined.end() - (M_offset), combined.end());
}

void fmDemodNoArctan(const std::vector<real> &I, const std::vector<real> &Q, real &previous_I, real &previous_Q, std::vector<real> &fm_demod)
{
	real current_I = 0.0;
	real current_Q = 0.0;
	real diff_I = 0.0;
	real diff_Q = 0.0;
	real numerator = 0.0;
	real denominator = 0.0;
	for (int k = 0; k < (int)I.size(); k++)
	{
		current_I = I[k];
		current_Q = Q[k];

		diff_Q = current_Q - previous_Q;
		diff_I = current_I - previous_I;

		numerator = (current_I * diff_Q) - (current_Q * diff_I);
		denominator = (current_I * current_I) + (current_Q * current_Q);

		if (denominator != 0)
			fm_demod[k] = numerator / denominator;
		else
			fm_demod[k] = 0.0;

		previous_I = current_I;
		previous_Q = current_Q;
	}
}

void pllBlockIQ(const std::vector<real> &pllIn,
                real freq, real Fs,
                real ncoScale, real phaseAdjust, real normBandwidth,
                PllState &state,
                std::vector<real> &ncoOutI,
                std::vector<real> &ncoOutQ)
{
	const real Cp = 2.666;
	const real Ci = 3.555;
	const real Kp = normBandwidth * Cp;
	const real Ki = normBandwidth * normBandwidth * Ci;

	ncoOutI.resize(pllIn.size());
	ncoOutQ.resize(pllIn.size());

	for (int k = 0; k < (int)pllIn.size(); k++) {
		real errorI = pllIn[k] * (+state.feedbackI);
		real errorQ = pllIn[k] * (-state.feedbackQ);
		real errorD = std::atan2(errorQ, errorI);

		state.integrator += Ki * errorD;
		state.phaseEst   += Kp * errorD + state.integrator;

		state.trigOffset += 1.0;
		double trigArg    = 2.0 * PI * (freq / Fs) * state.trigOffset + state.phaseEst;
		state.feedbackI   = std::cos(trigArg);
		state.feedbackQ   = std::sin(trigArg);
		ncoOutI[k]        = std::cos(trigArg * ncoScale + phaseAdjust);
		ncoOutQ[k]        = std::sin(trigArg * ncoScale + phaseAdjust);
	}
}

void impulseResponseRRC(real Fs, int num_taps, std::vector<real> &h)
{
	
	const double T_sym = 1.0 / 2375.0;
	const double beta  = 0.90;

	h.resize(num_taps);
	for (int k = 0; k < num_taps; k++) {
		double t = (double)(k - num_taps / 2) / (double)Fs;
		double val;
		if (t == 0.0) {
			val = 1.0 + beta * (4.0 / PI - 1.0);
		} else if (std::abs(std::abs(t) - T_sym / (4.0 * beta)) < 1e-10) {
			val = (beta / std::sqrt(2.0)) * (
				(1.0 + 2.0/PI) * std::sin(PI / (4.0 * beta)) +
				(1.0 - 2.0/PI) * std::cos(PI / (4.0 * beta))
			);
		} else {
			double tN  = t / T_sym;
			double num = std::sin(PI * tN * (1.0 - beta))
			           + 4.0 * beta * tN * std::cos(PI * tN * (1.0 + beta));
			double den = PI * tN * (1.0 - (4.0 * beta * tN) * (4.0 * beta * tN));
			val = num / den;
		}
		h[k] = (real)val;
	}
}

void pllBlock(const std::vector<real> &pllIn,
              real freq, real Fs,
              real ncoScale, real phaseAdjust, real normBandwidth,
              PllState &state,
              std::vector<real> &ncoOut)
{
	const real Cp = 2.666;
	const real Ci = 3.555;
	const real Kp = normBandwidth * Cp;
	const real Ki = normBandwidth * normBandwidth * Ci;

	ncoOut.resize(pllIn.size());

	for (int k = 0; k < (int)pllIn.size(); k++) {
		real errorI = pllIn[k] * (+state.feedbackI);
		real errorQ = pllIn[k] * (-state.feedbackQ);
		real errorD = std::atan2(errorQ, errorI);

		state.integrator += Ki * errorD;
		state.phaseEst   += Kp * errorD + state.integrator;

		state.trigOffset += 1.0;
		double trigArg    = 2.0 * PI * (freq / Fs) * state.trigOffset + state.phaseEst;
		state.feedbackI   = std::cos(trigArg);
		state.feedbackQ   = std::sin(trigArg);
		ncoOut[k]         = std::cos(trigArg * ncoScale + phaseAdjust);
	}
}
