#
# Comp Eng 3DY4 (Computer Systems Integration Project)
#
# Department of Electrical and Computer Engineering
# McMaster University
# Ontario, Canada
#

import matplotlib.animation as animation
import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy import signal
import numpy as np
import sys

from fmSupportLib import fmDemodArctan, fmPlotPSD
# this animation is optional and hence there is no take-home exercise
# the in-lab changes (if done) are conceptually identical to what is needed
# to change for the in-lab part in fmMonoBlock.py

rf_Fs = 2.4e6
rf_Fc = 100e3
rf_taps = 101
rf_decim = 10

audio_Fs = 48e3
audio_decim = 5
# add other settings for audio, like filter taps, ...

# we need a dummy init for animate to avoid calling
# the main update function (animate_update) twice at the start
def animate_init():
	pass

# this is the main animation function called by "animation.FuncAnimation"
# the first argument is mandatory and it keeps track of how many times
# this function has been called
# the subsequent arguments are custom to this particular application
def animate_update(block_count, iq_data, block_size, rf_coeff, audio_coeff):

	global audio_data, state_i_lpf_100k, state_q_lpf_100k, state_I_last_sample, state_Q_last_sample

	if (block_count + 1) * block_size > len(iq_data):
		print('Finished processing all the blocks from the recorded I/Q samples')
		out_fname = "../data/fmMonoAnim.wav"
		wavfile.write(out_fname, int(audio_Fs), np.int16((audio_data / 2) * 32767))
		print("Written audio samples to \"" + out_fname + "\" in signed 16-bit format")
		sys.exit()

	# filter to extract the FM channel (I samples are even, Q samples are odd)
	i_filt, state_i_lpf_100k = signal.lfilter(rf_coeff, 1.0, \
			iq_data[block_count * block_size:(block_count + 1) * block_size:2],
			zi=state_i_lpf_100k)
	q_filt, state_q_lpf_100k = signal.lfilter(rf_coeff, 1.0, \
			iq_data[block_count * block_size + 1:(block_count + 1) * block_size:2],
			zi=state_q_lpf_100k)

	# downsample the FM channel
	i_ds = i_filt[::rf_decim]
	q_ds = q_filt[::rf_decim]

	# FM demodulator
	fm_demod, state_I_last_sample, state_Q_last_sample = fmDemodArctan(i_ds, q_ds, state_I_last_sample, state_Q_last_sample)

	# plot PSD after FM demodulation
	# (for easier visualization purposes we divide Fs by 1e3 to imply the kHz units on the x-axis)
	# (this scales the y axis of the PSD, but not the relative strength of different frequencies)
	ax0.clear()
	fmPlotPSD(ax0, fm_demod, (rf_Fs / rf_decim) / 1e3, subfig_height[0], \
					'Demodulated FM (block ' + str(block_count) + ')')

	# extract the mono audio data through filtering
	# audio_filt = ... change as needed

	# plot PSD after extracting mono audio
	# ... change as needed

	# downsample audio data
	# audio_block = ... change as needed

	# plot PSD after downsampling mono audio
	# ... change as needed

	# concatenate the most recently processed audio_block
	# to the previous blocks stored already in audio_data
	# audio_data = np.concatenate((audio_data, audio_block))

if __name__ == "__main__":

	# read the raw IQ data from the recorded file
	# IQ data is assumed to be in 8-bits unsigned (and interleaved)
	in_fname = "../data/iq_samples.raw"
	raw_data = np.fromfile(in_fname, dtype='uint8')
	print("Read raw RF data from \"" + in_fname + "\" in unsigned 8-bit format")
	# IQ data is normalized between -1 and +1 in 32-bit float format
	# iq_data = (np.float32(raw_data) - 128.0) / 128.0
	# print("Reformatted raw RF data to 32-bit float format (" + str(iq_data.size * iq_data.itemsize) + " bytes)")

	# IQ data is normalized between -1 and +1 in 64-bit double format
	iq_data = (np.float64(raw_data) - 128.0) / 128.0
	print("Reformatted raw RF data to 64-bit double format (" + str(iq_data.size * iq_data.itemsize) + " bytes)")

	# coefficients for the front-end low-pass filter
	rf_coeff = signal.firwin(rf_taps, rf_Fc / (rf_Fs / 2), window=('hann'))

	# coefficients for the filter to extract mono audio
	audio_coeff = np.array([]) # to be changed by you

	# set up the subfigures for plotting
	subfig_height = np.array([0.8, 2, 1.6]) # relative heights of the subfigures
	plt.rc('figure', figsize=(7.5, 7.5))	# the size of the entire figure
	fig, (ax0, ax1, ax2) = plt.subplots(nrows=3, gridspec_kw={'height_ratios': subfig_height})
	fig.subplots_adjust(hspace = .6)

	# select a block_size that is in KB and
	# a multiple of decimation factors
	block_size = 1024 * rf_decim * audio_decim * 2
	block_count = 0

	# states needed for continuity in block processing
	state_i_lpf_100k = np.zeros(rf_taps - 1)
	state_q_lpf_100k = np.zeros(rf_taps - 1)
	state_I_last_sample = 0
	state_Q_last_sample = 0
	# add state as needed for the mono channel filter

	# audio buffer that stores all the audio blocks
	audio_data = np.array([])

	try:
		# calls the animation function (animate_update) repeatedly
		# check matplotlib documentation for further details
		ani = animation.FuncAnimation(fig, animate_update, \
						interval=150, init_func=animate_init, \
						fargs=(iq_data, block_size, rf_coeff, audio_coeff, ))
		plt.show()

	except KeyboardInterrupt:
		pass
