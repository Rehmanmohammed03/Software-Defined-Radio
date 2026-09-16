#
# Comp Eng 3DY4 (Computer Systems Integration Project)
#
# Department of Electrical and Computer Engineering
# McMaster University
# Ontario, Canada
#

"""
Ten prerecorded I/Q sample files can be downloaded from the link sent via email
and posted on Avenue.

There is NO need to copy all the sample files at once into your data sub-folder.
Instead, it is recommended to download one file at a time and overwrite the
iq_samples.raw file in the data sub-folder, which is the default path used by the script.

For those of you with the RF dongle and Raspberry Pi kit at home, the command-line
instructions for recording RF data are provided below. After installing the necessary
drivers to work with the RF dongle, the 8-bit unsigned values for the I/Q pairs can be
recorded using the following command:

rtl_sdr -f 99.9M -s 2.4M - > iq_samples.raw

The above command assumes that we are tuned to the FM station at 99.9 MHz, using an
RF sample rate of 2.4 Msamples/sec, and saving the data to a file named iq_samples.raw
(you can change the filename as needed).

For the above command, data acquisition runs indefinitely and must be stopped manually
by pressing Ctrl+C. If you want to stop recording after a predefined number of samples,
e.g., 12 million I/Q pairs (equivalent to 5 seconds at 2.4 Msamples/sec), you can
include an additional argument:

rtl_sdr -f 99.9M -s 2.4M -n 12000000 - > iq_samples.raw

To verify that the raw I/Q data has been recorded properly, place the file in your
project repository's data sub-folder and run the Python script from the model sub-folder.
The script should generate both .png image files (showing PSD estimates) and a .wav file.

In the source code below (at the beginning of the main function), you can observe where
the raw_data is read and where the normalization of the 8-bit unsigned I/Q samples to
32-bit float samples (in the range -1 to +1) is performed. While using 32-bit floats and
normalizing to the range -1 to +1 are optional choices (commonly adopted by many
third-party SDR software implementations), it is up to each project group to decide how
to handle the 8-bit unsigned I/Q samples in their Python model and C++ implementation.
For example, one can choose how to reformat the data in the range -1 to +1 in 64-bit double
format (as shown in the lines commented below the normalization to 32-bit floats). Note,
however, it is recommended to use the same data format in both the Python model and C++.

A final but critical note: the .gitignore file should NOT be modified to allow pushing
.raw files to GitHub. Including .raw files in the repository will result in very large
repositories that take excessive time to clone, pull, or push. As outlined in the
reference .gitignore file, only source files should be kept in your repositories.
"""

import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile
from scipy import signal

from fmSupportLib import fmDemodUnwrap, fmDemodArctan, fmPlotPSD
# for take-home add your functions

# the radio-frequency (RF) sampling rate
rf_Fs = 2.4e6

# the cutoff frequency to extract the FM channel from raw IQ data
rf_Fc = 100e3

# the number of taps for the low-pass filter
rf_taps = 101

# decimation rate for reducing sampling rate at the intermediate frequency (IF)
rf_decim = 10

# audio sampling rate (48 KSamples/sec)
audio_Fs = 48e3

# placeholders for audio channel settings
# audio_Fc = ...
# audio_decim = ...
# audio_taps = ...

# flag for in-lab (0) vs take-home (1) mode
il_vs_th = 0

if __name__ == "__main__":

	# read the raw IQ data
	in_fname = "../data/iq_samples.raw"
	raw_data = np.fromfile(in_fname, dtype='uint8')
	print("Read raw RF data from \"" + in_fname + "\" in unsigned 8-bit format")

	# normalize raw IQ data to 32-bit float format (-1 to +1)
	# iq_data = (np.float32(raw_data) - 128.0) / 128.0
	# print("Reformatted raw RF data to 32-bit float format (" + str(iq_data.size * iq_data.itemsize) + " bytes)")

	# IQ data is normalized between -1 and +1 in 64-bit double format
	iq_data = (np.float64(raw_data) - 128.0) / 128.0
	print("Reformatted raw RF data to 64-bit double format (" + str(iq_data.size * iq_data.itemsize) + " bytes)")

	# coefficients for front-end low-pass filter
	rf_coeff = signal.firwin(rf_taps, rf_Fc / (rf_Fs / 2), window=('hann'))

	# filter to extract the FM channel
	i_filt = signal.lfilter(rf_coeff, 1.0, iq_data[0::2])
	q_filt = signal.lfilter(rf_coeff, 1.0, iq_data[1::2])

	# downsample the FM channel
	i_ds = i_filt[::rf_decim]
	q_ds = q_filt[::rf_decim]

	# set up subfigures for plotting
	subfig_height = np.array([0.8, 2, 1.6])
	plt.rc('figure', figsize=(7.5, 7.5))
	fig, (ax0, ax1, ax2) = plt.subplots(nrows=3, gridspec_kw={'height_ratios': subfig_height})
	fig.subplots_adjust(hspace = .6)

	# FM demodulator
	fm_demod, dummy_phase = fmDemodUnwrap(i_ds, q_ds)
	#
	# after you run the code above with phase unwrapping for FM demodulation, you can comment it
	# then you should uncomment the code below where the phase differential is computed without
	# the explicit phase computation (hence no phase unwrapping will be needed)
	#
	# fm_demod, dummy_I, dummy_Q = fmDemodArctan(i_ds, q_ds)
	#

	# PSD after FM demodulation
	# (for easier visualization purposes we divide Fs by 1e3 to imply the kHz units on the x-axis)
	# (this scales the y axis of the PSD, but not the relative strength of different frequencies)
	fmPlotPSD(ax0, fm_demod, (rf_Fs / rf_decim) / 1e3, subfig_height[0], \
		'Demodulated FM (full recording)')

	# placeholder for audio filter coefficients
	audio_coeff = np.array([])

	# extract mono audio data (placeholder)
	audio_filt = np.array([])

	# PSD for mono audio (placeholder)
	# fmPlotPSD(ax1, audio_filt, (rf_Fs / rf_decim) / 1e3, subfig_height[1], 'Extracted Mono')

	# downsample mono audio (placeholder)
	audio_data = np.array([])

	# PSD for downsampled audio (placeholder)
	# fmPlotPSD(ax2, audio_data, audio_Fs / 1e3, subfig_height[2], 'Downsampled Mono Audio')

	# save plots
	fig.savefig("../data/fmMonoBasic.png")
	plt.show()

	# write audio data to file
	out_fname = "../data/fmMonoBasic.wav"
	wavfile.write(out_fname, int(audio_Fs), np.int16((audio_data / 2) * 32767))
	print("Written audio samples to \"" + out_fname + "\" in signed 16-bit format")
