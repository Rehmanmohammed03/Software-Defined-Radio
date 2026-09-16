import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy import signal
import numpy as np

from fmSupportLib import fmDemodArctan, fmPlotPSD
from fmPll import fmPll

rf_Fs    = 2.4e6
rf_Fc    = 100e3
rf_taps  = 101
rf_decim = 10

If_Fs = rf_Fs / rf_decim

audio_Fs    = 48e3
audio_Fc    = 16e3
audio_taps  = 101
audio_decim = 5

stereo_taps = 101

if __name__ == "__main__":

	in_fname = "../data/iq_samples.raw"
	raw_data = np.fromfile(in_fname, dtype='uint8')
	print("Read raw RF data from \"" + in_fname + "\" in unsigned 8-bit format")

	iq_data = (np.float64(raw_data) - 128.0) / 128.0
	print("Reformatted raw RF data to 64-bit double format (" + str(iq_data.size * iq_data.itemsize) + " bytes)")

	rf_coeff = signal.firwin(rf_taps, rf_Fc / (rf_Fs / 2), window='hann')
	audio_coeff = signal.firwin(audio_taps, audio_Fc / (If_Fs / 2), window='hann')
	
	pilot_coeff = signal.firwin(stereo_taps,[18.5e3 / (If_Fs / 2), 19.5e3 / (If_Fs / 2)],pass_zero=False,window='hann')

	stereo_coeff = signal.firwin(stereo_taps,[22e3 / (If_Fs / 2), 54e3 / (If_Fs / 2)],pass_zero=False,window='hann')

	stereo_lpf_coeff = signal.firwin(audio_taps, audio_Fc / (If_Fs / 2), window='hann') * 2.0

	subfig_height = np.array([0.8, 2, 1.6])
	plt.rc('figure', figsize=(7.5, 7.5))
	fig, (ax0, ax1, ax2) = plt.subplots(nrows=3, gridspec_kw={'height_ratios': subfig_height})
	fig.subplots_adjust(hspace=.6)

	block_size  = 1024 * rf_decim * audio_decim * 2
	block_count = 0

	state_i_lpf    = np.zeros(rf_taps - 1)
	state_q_lpf    = np.zeros(rf_taps - 1)
	state_I_demod  = 0.0
	state_Q_demod  = 0.0
	state_mono_lpf = np.zeros(audio_taps - 1)

	state_pilot_bpf  = np.zeros(stereo_taps - 1)
	state_stereo_bpf = np.zeros(stereo_taps - 1)
	state_stereo_lpf = np.zeros(audio_taps - 1)

	pll_state = None

	audio_data = np.array([])

	while (block_count + 1) * block_size < len(iq_data):

		print('Processing block ' + str(block_count))

		i_filt, state_i_lpf = signal.lfilter(rf_coeff, 1.0,
			iq_data[block_count * block_size:(block_count + 1) * block_size:2],
			zi=state_i_lpf)

		q_filt, state_q_lpf = signal.lfilter(rf_coeff, 1.0,
			iq_data[block_count * block_size + 1:(block_count + 1) * block_size:2],
			zi=state_q_lpf)

		i_ds = i_filt[::rf_decim]
		q_ds = q_filt[::rf_decim]

		fm_demod, state_I_demod, state_Q_demod = fmDemodArctan(i_ds, q_ds, state_I_demod, state_Q_demod)

		mono_filt, state_mono_lpf = signal.lfilter(audio_coeff, 1.0, fm_demod, zi=state_mono_lpf)
		mono_audio = mono_filt[::audio_decim]

		pilot_filt, state_pilot_bpf = signal.lfilter(pilot_coeff, 1.0, fm_demod, zi=state_pilot_bpf)

		stereo_filt, state_stereo_bpf = signal.lfilter(stereo_coeff, 1.0, fm_demod, zi=state_stereo_bpf)

		nco_full, pll_state = fmPll(
			pilot_filt,
			freq=19e3,
			Fs=If_Fs,
			ncoScale=2.0,
			phaseAdjust=0.0,
			normBandwidth=0.01,
			state=pll_state
		)
		nco_out = nco_full[1:]

		mixed = stereo_filt * nco_out

		stereo_filt_lpf, state_stereo_lpf = signal.lfilter(stereo_lpf_coeff, 1.0, mixed, zi=state_stereo_lpf)
		stereo_audio = stereo_filt_lpf[::audio_decim]

		left  = 0.5 * (mono_audio + stereo_audio)
		right = 0.5 * (mono_audio - stereo_audio)

		stereo_block = np.empty(2 * len(left))
		stereo_block[0::2] = left
		stereo_block[1::2] = right

		audio_data = np.concatenate([audio_data, stereo_block])

		if block_count >= 10 and block_count < 12:

			ax0.clear()
			fmPlotPSD(ax0, fm_demod, If_Fs / 1e3, subfig_height[0],
				'Demodulated FM (block ' + str(block_count) + ')')

			ax1.clear()
			fmPlotPSD(ax1, pilot_filt, If_Fs / 1e3, subfig_height[1],
				'Pilot BPF 19 kHz (block ' + str(block_count) + ')')

			ax2.clear()
			fmPlotPSD(ax2, stereo_filt, If_Fs / 1e3, subfig_height[2],
				'Stereo BPF 22-54 kHz (block ' + str(block_count) + ')')

			fig.savefig("../data/fmStereoBlock" + str(block_count) + ".png")

		block_count += 1

	print('Finished processing all the blocks from the recorded I/Q samples')

	out_fname = "../data/fmStereoBlock.wav"
	audio_int16 = np.int16(np.clip(audio_data, -2.0, 2.0) * 16384)
	wavfile.write(out_fname, int(audio_Fs), audio_int16.reshape(-1, 2))
	print("Written stereo audio to \"" + out_fname + "\" in signed 16-bit format")

	plt.show()
