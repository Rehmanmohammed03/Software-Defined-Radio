#
# Comp Eng 3DY4 (Computer Systems Integration Project)
#
# Department of Electrical and Computer Engineering
# McMaster University
# Ontario, Canada
#

import numpy as np
import math

def fmPll(pllIn, freq, Fs, ncoScale = 1.0, phaseAdjust = 0.0, normBandwidth = 0.01, state = None, return_quadrature = False):

	"""

	pllIn 	 		array of floats
					input signal to the PLL (assume known frequency)

	freq 			float
					reference frequency to which the PLL locks

	Fs  			float
					sampling rate for the input/output signals

	ncoScale		float
					frequency scale factor for the NCO output

	phaseAdjust		float
					phase adjust to be added to the NCO output only

	normBandwidth	float
					normalized bandwidth for the loop filter
					(relative to the sampling rate)

	state 			dictionary with keys integrator, phaseEst, feedbackI, feedbackQ,
					trigOffset, ncoOut_last; pass None for the first block

	return_quadrature  bool
					if True, also return the quadrature (sin) NCO output in a
					second array and include 'ncoOutQ_last' in the state dict

	"""

	# scale factors for proportional/integrator terms
	# these scale factors were derived assuming the following:
	# damping factor of 0.707 (1 over square root of 2)
	# there is no oscillator gain and no phase detector gain
	Cp = 2.666
	Ci = 3.555

	# gain for the proportional term
	Kp = (normBandwidth)*Cp

	# gain for the integrator term
	Ki = (normBandwidth*normBandwidth)*Ci

	# output array for the NCO
	ncoOut = np.empty(len(pllIn)+1)
	if return_quadrature:
		ncoOutQ = np.empty(len(pllIn)+1)

	# initialize internal state
	if state is None:
		integrator = 0.0
		phaseEst = 0.0
		feedbackI = 1.0
		feedbackQ = 0.0
		ncoOut[0] = 1.0
		if return_quadrature:
			ncoOutQ[0] = 0.0
		trigOffset = 0
	else:
		integrator = state['integrator']
		phaseEst   = state['phaseEst']
		feedbackI  = state['feedbackI']
		feedbackQ  = state['feedbackQ']
		trigOffset = state['trigOffset']
		ncoOut[0]  = state['ncoOut_last']
		if return_quadrature:
			ncoOutQ[0] = state.get('ncoOutQ_last', 0.0)

	# note: state saving will be needed for block processing
	for k in range(len(pllIn)):

		# phase detector
		errorI = pllIn[k] * (+feedbackI)  # complex conjugate of the
		errorQ = pllIn[k] * (-feedbackQ)  # feedback complex exponential

		# four-quadrant arctangent discriminator for phase error detection
		errorD = math.atan2(errorQ, errorI)

		# loop filter
		integrator = integrator + Ki*errorD

		# update phase estimate
		phaseEst = phaseEst + Kp*errorD + integrator

		# internal oscillator
		trigOffset += 1
		trigArg = 2*math.pi*(freq/Fs)*(trigOffset) + phaseEst
		feedbackI = math.cos(trigArg)
		feedbackQ = math.sin(trigArg)
		ncoOut[k+1] = math.cos(trigArg*ncoScale + phaseAdjust)
		if return_quadrature:
			ncoOutQ[k+1] = math.sin(trigArg*ncoScale + phaseAdjust)

	# for stereo only the in-phase NCO component should be returned
	# for block processing you should also return the state

	state_out = {
		'integrator':  integrator,
		'phaseEst':    phaseEst,
		'feedbackI':   feedbackI,
		'feedbackQ':   feedbackQ,
		'trigOffset':  trigOffset,
		'ncoOut_last': ncoOut[-1],
	}
	if return_quadrature:
		state_out['ncoOutQ_last'] = ncoOutQ[-1]
		return ncoOut, ncoOutQ, state_out

	return ncoOut, state_out

if __name__ == "__main__":

	pass
