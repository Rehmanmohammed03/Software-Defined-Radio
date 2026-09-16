#
# Comp Eng 3DY4 (Computer Systems Integration Project)
#
# Department of Electrical and Computer Engineering
# McMaster University
# Ontario, Canada
#

import sys
import numpy as np
from scipy import signal
from pathlib import Path

# sample rates in samples/sec
sample_rate_table = [
	2400000,	# 0
	2880000,	# 1
	2560000,	# 2
	2457600,	# 3
	2304000,	# 4
	2048000,	# 5
	1920000,	# 6
	1843200,	# 7
	1800000,	# 8
	1638400,	# 9
	1600000,	# 10
	1536000,	# 11
	1440000,	# 12
	1280000,	# 13
	1228800,	# 14
	1200000,	# 15
	1152000,	# 16
	1024000,	# 17
	960000,		# 18
	921600		# 19
]

def print_usage_and_exit(prog_name: str) -> None:

	print('\nRun it as follows:\n')
	print(f'python {prog_name} <inputFile> <outFsID> [inFsID]')
	print('\t<inputFile> holds raw I/Q samples (interleaved/8-bit unsigned)')
	print('\t<outFsID>  ID for the output (target) sample rate (required)')
	print('\t[inFsID]   ID for the input (source) sample rate (defaults to 0)\n')
	print('Sample rate IDs are:')
	for id, fs in enumerate(sample_rate_table):
		fs_mhz = fs / 1e6
		fs_str = f"{fs_mhz:.5f}".rstrip('0').rstrip('.')
		print(f'\t {id:2d} - {fs_str.ljust(7)} Msamples/sec')
	print('')
	exit(1)

def validate_fs_id(fs_id: int, label: str, prog_name: str) -> None:

	if fs_id < 0 or fs_id >= len(sample_rate_table):
		print(f'\nError: {label} {fs_id} is out of range.\n')
		print_usage_and_exit(prog_name)

def read_iq_u8_to_float(in_fname: str) -> np.ndarray:

	raw_data = np.fromfile(in_fname, dtype='uint8')
	if (len(raw_data) % 2) != 0:
		raise ValueError(f'Input file "{in_fname}" has odd length ({len(raw_data)} bytes). Expected interleaved I/Q.')

	iq_data = (raw_data - 128.0) / 128.0
	print("Read raw RF data from \"" + in_fname + "\" in unsigned 8-bit format")

	return iq_data

def resample_iq(iq_data: np.ndarray, Fs_in: int, Fs_out: int) -> tuple[np.ndarray, np.ndarray]:

	g = np.gcd(Fs_in, Fs_out)
	expand = Fs_out // g
	decim = Fs_in // g

	resampled_i = signal.resample_poly(iq_data[0::2], expand, decim)
	resampled_q = signal.resample_poly(iq_data[1::2], expand, decim)

	return resampled_i, resampled_q

def write_iq_float_to_u8(out_fname: str, i_data: np.ndarray, q_data: np.ndarray) -> None:

	peak = max(np.max(np.abs(i_data)), np.max(np.abs(q_data)))
	if peak > 0.0:
		scale = 1.0 / peak
		i_data = i_data * scale
		q_data = q_data * scale

	out_data = np.empty(2 * len(i_data), dtype=np.uint8)
	out_data[0::2] = (128 + np.trunc(i_data * 127)).astype(np.uint8)
	out_data[1::2] = (128 + np.trunc(q_data * 127)).astype(np.uint8)

	out_data.tofile(out_fname)
	print("Written resampled RF data to \"" + out_fname + "\" in unsigned 8-bit format")

if __name__ == "__main__":

	if len(sys.argv[1:]) < 2:
		print_usage_and_exit(sys.argv[0])
	in_fname = sys.argv[1]
	outFsID = int(sys.argv[2])
	inFsID = 0
	if len(sys.argv[3:]) >= 1:
		inFsID = int(sys.argv[3])
	validate_fs_id(outFsID, 'outFsID', sys.argv[0])
	validate_fs_id(inFsID, 'inFsID', sys.argv[0])

	Fs_in = int(sample_rate_table[inFsID])
	Fs_out = int(sample_rate_table[outFsID])
	iq_data = read_iq_u8_to_float(in_fname)
	resampled_i, resampled_q = resample_iq(iq_data, Fs_in, Fs_out)

	out_fname = str(Path(in_fname).with_name(f"{Path(in_fname).stem}_{Fs_out}{Path(in_fname).suffix}"))
	write_iq_float_to_u8(out_fname, resampled_i, resampled_q)
