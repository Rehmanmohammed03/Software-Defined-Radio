import collections
import sys
import matplotlib.pyplot as plt
from scipy import signal
import numpy as np
import math

from fmSupportLib import fmDemodArctan, fmPlotPSD
from fmPll import fmPll
from fmRRC import impulseResponseRootRaisedCosine

rf_Fs    = 2.4e6
rf_Fc    = 100e3
rf_taps  = 101
rf_decim = 10

If_Fs = rf_Fs / rf_decim

rds_taps     = 101
rds_U        = 133
rds_D        = 320
rds_SPS      = 42
rds_sym_rate = 2375.0
rds_out_Fs   = rds_U * If_Fs / rds_D

rrc_taps = rds_taps

H_matrix = [
    0b1000000000, 0b0100000000, 0b0010000000, 0b0001000000,
    0b0000100000, 0b0000010000, 0b0000001000, 0b0000000100,
    0b0000000010, 0b0000000001, 0b1011011100, 0b0101101110,
    0b0010110111, 0b1010000111, 0b1110011111, 0b1100010011,
    0b1101010101, 0b1101110110, 0b0110111011, 0b1000000001,
    0b1111011100, 0b0111101110, 0b0011110111, 0b1010100111,
    0b1110001111, 0b1100011011,
]

SYNDROME = {
    0b1111011000: 0,
    0b1111010100: 1,
    0b1001011100: 2,
    0b1111001100: 2,
    0b1001011000: 3,
}

SYNDROME_SEQ = [0b1111011000, 0b1111010100, 0b1001011100, 0b1001011000]



def compute_syndrome(bits26):
    syn = 0
    for i, b in enumerate(bits26):
        if b:
            syn ^= H_matrix[i]
    return syn


rds_synced = False
rds_block_idx = 0
rds_group_bits = [0, 0, 0, 0]
rds_diff_prev = 0
rds_manch_buf = []
rds_bit_buf = collections.deque()
rds_group_type = -1
rds_version = -1
ps_chars = ['?'] * 8
rt_chars = ['\x00'] * 64   
rds_blocks_this_call = 0


def process_rds_block(info16, block_idx):
    global rds_group_type, rds_version, rds_blocks_this_call
    rds_blocks_this_call += 1
    rds_group_bits[block_idx] = info16
    if block_idx == 0:
        print(f"  PI: 0x{info16:04X}")
    elif block_idx == 1:
        rds_group_type = (info16 >> 12) & 0x0F
        rds_version = (info16 >> 11) & 0x01
        pty = (info16 >> 5)  & 0x1F
        ver_str = 'A' if rds_version == 0 else 'B'
        print(f"  Group {rds_group_type}{ver_str}, PTY={pty}")
    elif block_idx == 2:
        if rds_group_type == 2 and rds_version == 0:
            seg  = rds_group_bits[1] & 0x0F
            base = 4 * seg
            if base + 1 < 64:
                c0, c1 = (info16 >> 8) & 0xFF, info16 & 0xFF
                rt_chars[base]     = chr(c0) if 32 <= c0 < 128 else '?'
                rt_chars[base + 1] = chr(c1) if 32 <= c1 < 128 else '?'
    elif block_idx == 3:
        if rds_group_type == 0 and rds_version == 0:
            seg  = rds_group_bits[1] & 0x03
            c0, c1 = (info16 >> 8) & 0xFF, info16 & 0xFF
            ps_chars[2 * seg]     = chr(c0) if 32 <= c0 < 128 else '?'
            ps_chars[2 * seg + 1] = chr(c1) if 32 <= c1 < 128 else '?'
            print(f"  PS seg {seg}: '{chr(c0) if 32<=c0<128 else '?'}{chr(c1) if 32<=c1<128 else '?'}'  -> '{''.join(ps_chars)}'")
        elif rds_group_type == 2 and rds_version == 0:
            seg  = rds_group_bits[1] & 0x0F
            base = 4 * seg + 2
            if base + 1 < 64:
                c0, c1 = (info16 >> 8) & 0xFF, info16 & 0xFF
                rt_chars[base]     = chr(c0) if 32 <= c0 < 128 else '?'
                rt_chars[base + 1] = chr(c1) if 32 <= c1 < 128 else '?'
            rt_so_far = ''.join(c if c != '\x00' else '?' for c in rt_chars).rstrip('? ').strip()
            print(f"  RT seg {seg}: '{rt_so_far}'")


def _try_1bit_correct(bits26, expected_syn):
    error_syn = compute_syndrome(bits26) ^ expected_syn
    if error_syn == 0:
        return None
    for i, h in enumerate(H_matrix):
        if h == error_syn:
            corrected = list(bits26)
            corrected[i] ^= 1
            return corrected
    return None


def rds_decode(symbols):
    global rds_synced, rds_block_idx, rds_diff_prev, rds_manch_buf, rds_bit_buf

    all_sym = rds_manch_buf + list(symbols)
    bits_diff = []
    i = 0
    while i + 1 < len(all_sym):
        s1, s2 = all_sym[i], all_sym[i + 1]
        if s1 > 0 and s2 < 0:
            bits_diff.append(1)
            i += 2
        elif s1 < 0 and s2 > 0:
            bits_diff.append(0)
            i += 2
        else:
            i += 1

    rds_manch_buf.clear()
    if i < len(all_sym):
        rds_manch_buf.append(all_sym[-1])

    bits = []
    for b in bits_diff:
        bits.append(b ^ rds_diff_prev)
        rds_diff_prev = b

    rds_bit_buf.extend(bits)

    while True:
        if not rds_synced:
            if len(rds_bit_buf) < 52:
                break
            bits_a = [rds_bit_buf[k] for k in range(26)]
            syn_a = compute_syndrome(bits_a)
            if syn_a == SYNDROME_SEQ[0]:                        
                bits_b = [rds_bit_buf[26 + k] for k in range(26)]
                if compute_syndrome(bits_b) == SYNDROME_SEQ[1]:  
                    info16 = 0
                    for bit in bits_a[:16]:
                        info16 = (info16 << 1) | bit
                    process_rds_block(info16, 0)
                    rds_synced    = True
                    rds_block_idx = 1
                    for _ in range(26):
                        rds_bit_buf.popleft()
                    continue   
            rds_bit_buf.popleft()
        else:
            if len(rds_bit_buf) < 26:
                break
            bits26 = [rds_bit_buf[k] for k in range(26)]
            syn = compute_syndrome(bits26)
            expected = SYNDROME_SEQ[rds_block_idx]
            valid = (syn == expected) or (
                rds_block_idx == 2 and syn == 0b1111001100)

            if valid:
                use_bits = bits26
            else:
                if rds_block_idx == 2:
                    use_bits = (_try_1bit_correct(bits26, SYNDROME_SEQ[2]) or
                                _try_1bit_correct(bits26, 0b1111001100))
                else:
                    use_bits = _try_1bit_correct(bits26, expected)

            if use_bits is not None:
                info16 = 0
                for bit in use_bits[:16]:
                    info16 = (info16 << 1) | bit
                process_rds_block(info16, rds_block_idx)
                rds_block_idx = (rds_block_idx + 1) % 4
                for _ in range(26):
                    rds_bit_buf.popleft()
            else:
                rds_synced = False
                rds_bit_buf.popleft()


if __name__ == "__main__":

    sys.stdout.reconfigure(line_buffering=True)

    in_fname = "../data/samples3.raw"
    raw_data = np.fromfile(in_fname, dtype='uint8')
    print(f"Read {len(raw_data)} bytes from \"{in_fname}\"")
    iq_data = (np.float64(raw_data) - 128.0) / 128.0

    rf_coeff = signal.firwin(rf_taps, rf_Fc / (rf_Fs / 2), window='hann')

    rds_bpf_lo, rds_bpf_hi = 54e3, 60e3
    rds_bpf_coeff = signal.firwin(rds_taps,[rds_bpf_lo / (If_Fs / 2), rds_bpf_hi / (If_Fs / 2)], pass_zero=False, window='hann')

    print(f"RDS BPF: {rds_bpf_lo/1e3:.1f}-{rds_bpf_hi/1e3:.1f} kHz  "f"(bandwidth={rds_bpf_hi-rds_bpf_lo:.0f} Hz, "f"signal occupies ~{rds_sym_rate*(1+0.90)/1e3:.2f} kHz per sideband)")

    rds_114_lo, rds_114_hi = 113.5e3, 114.5e3
    rds_114_coeff = signal.firwin(rds_taps, [rds_114_lo / (If_Fs / 2), rds_114_hi / (If_Fs / 2)], pass_zero=False, window='hann')

    print(f"114 kHz BPF: {rds_114_lo/1e3:.1f}-{rds_114_hi/1e3:.1f} kHz  "
          f"(bandwidth={rds_114_hi-rds_114_lo:.0f} Hz)")

    rds_M = rds_taps * rds_U
    rds_resamp_coeff = (signal.firwin(rds_M, 3e3 / (If_Fs * rds_U / 2), window='hann') * rds_U)
    rrc_coeff = impulseResponseRootRaisedCosine(rds_out_Fs, rrc_taps)

    block_size  = int(rf_Fs * 0.04) * 2
    block_count = 0

    state_i_lpf   = np.zeros(rf_taps - 1)
    state_q_lpf   = np.zeros(rf_taps - 1)
    state_I_demod = 0.0
    state_Q_demod = 0.0

    state_rds_bpf = np.zeros(rds_taps - 1)
    state_rds_114 = np.zeros(rds_taps - 1)
    pll_state_rds = None

    allpass_len   = (rds_taps - 1) // 2
    allpass_state = np.zeros(allpass_len)

    all_mixed_I = []
    all_mixed_Q = []

    fm_demod_last    = np.zeros(1024)
    rds_bpf_out_last = np.zeros(1024)

    print("Starting block processing...")

    while (block_count + 1) * block_size < len(iq_data):

        iq_block = iq_data[block_count * block_size:(block_count + 1) * block_size]

        i_filt, state_i_lpf = signal.lfilter(rf_coeff, 1.0, iq_block[::2],  zi=state_i_lpf)
        q_filt, state_q_lpf = signal.lfilter(rf_coeff, 1.0, iq_block[1::2], zi=state_q_lpf)
        i_ds = i_filt[::rf_decim]
        q_ds = q_filt[::rf_decim]

        fm_demod, state_I_demod, state_Q_demod = fmDemodArctan(i_ds, q_ds, state_I_demod, state_Q_demod)
        rds_bpf_out, state_rds_bpf = signal.lfilter(rds_bpf_coeff, 1.0, fm_demod, zi=state_rds_bpf)
        rds_squared = rds_bpf_out ** 2
        rds_114_out, state_rds_114 = signal.lfilter(rds_114_coeff, 1.0, rds_squared, zi=state_rds_114)

        nco_full_I, nco_full_Q, pll_state_rds = fmPll(rds_114_out, freq=114e3, Fs=If_Fs,ncoScale=0.5, phaseAdjust=0.0, normBandwidth=0.002, state=pll_state_rds, return_quadrature=True)
        nco_I = nco_full_I[1:]
        nco_Q = nco_full_Q[1:]

        if allpass_len > 0:
            bpf_delayed = np.concatenate([allpass_state, rds_bpf_out[:-allpass_len]])
            allpass_state = rds_bpf_out[-allpass_len:].copy()
        else:
            bpf_delayed = rds_bpf_out

        mixed_I = bpf_delayed * nco_I
        mixed_Q = bpf_delayed * nco_Q

        all_mixed_I.extend(mixed_I)
        all_mixed_Q.extend(mixed_Q)

        fm_demod_last    = fm_demod.copy()
        rds_bpf_out_last = rds_bpf_out.copy()
        print(f"  Block {block_count:3d} processed")

        block_count += 1

    print(f"Block processing complete ({block_count} blocks)")

    rds_mixed_full_I = np.array(all_mixed_I)
    rds_mixed_full_Q = np.array(all_mixed_Q)

    print(f"Resampling {len(rds_mixed_full_I)} samples  (U={rds_U}, D={rds_D})...")
    rds_res_I = signal.upfirdn(rds_resamp_coeff, rds_mixed_full_I, up=rds_U, down=rds_D)
    rds_res_Q = signal.upfirdn(rds_resamp_coeff, rds_mixed_full_Q, up=rds_U, down=rds_D)

    trim = (rds_M - 1) // (2 * rds_D)
    rds_res_I = rds_res_I[trim:]
    rds_res_Q = rds_res_Q[trim:]

    print(f"Applying RRC filter ({rrc_taps} taps, beta=0.90, Fs={rds_out_Fs:.0f} Hz)...")
    rrc_out_I = np.convolve(rds_res_I, rrc_coeff, mode='same')
    rrc_out_Q = np.convolve(rds_res_Q, rrc_coeff, mode='same')

    rrc_rms = math.sqrt(float(np.mean(rrc_out_I ** 2 + rrc_out_Q ** 2)))
    if rrc_rms > 0:
        rrc_out_I /= rrc_rms
        rrc_out_Q /= rrc_rms
    print(f"RRC output normalized (RMS={rrc_rms:.4f})")

    best_score  = -1.0
    best_offset = rds_SPS // 2
    for trial in range(rds_SPS):
        trial_idx = np.arange(trial, len(rrc_out_I), rds_SPS)
        score = max(float(np.mean(np.abs(rrc_out_I[trial_idx]))),
                    float(np.mean(np.abs(rrc_out_Q[trial_idx]))))
        if score > best_score:
            best_score  = score
            best_offset = trial
    cdr_offset  = best_offset
    cdr_indices = np.arange(cdr_offset, len(rrc_out_I), rds_SPS)
    sym_I = rrc_out_I[cdr_indices]
    sym_Q = rrc_out_Q[cdr_indices]

    std_I = float(np.std(sym_I))
    std_Q = float(np.std(sym_Q))
    if std_I >= std_Q:
        symbols        = np.sign(sym_I)
        decision_axis  = 'I'
        plot_horiz     = sym_I
        plot_vert      = sym_Q
    else:
        symbols        = np.sign(sym_Q)
        decision_axis  = 'Q'
        plot_horiz     = sym_Q
        plot_vert      = sym_I

    print(f"CDR offset: {cdr_offset}/{rds_SPS}  "
          f"(score={best_score:.4f}, std_I={std_I:.3f}, std_Q={std_Q:.3f})")
    print(f"Bit decisions from {decision_axis}-axis  "
          f"({len(symbols)} symbols extracted)")

    print("\n=== RDS Decoded Data ===")
    rds_blocks_this_call = 0
    rds_decode(list(symbols))
    print(f"\nFinal PS name    : '{''.join(ps_chars)}'")
    rt_text = ''.join(c if c != '\x00' else '?' for c in rt_chars).rstrip('? ').strip()
    print(f"Final Radio Text : '{rt_text}'")

    fig, axes = plt.subplots(2, 2, figsize=(13, 9))
    fig.suptitle("RDS Demodulation - Mode 0 (Group 70, SPS=42)")

    fmPlotPSD(axes[0, 0], fm_demod_last, If_Fs / 1e3, 1.0, "FM Demod spectrum")

    fmPlotPSD(axes[0, 1], rds_bpf_out_last, If_Fs / 1e3, 1.0, "RDS BPF output (54-60 kHz)")

    axes[1, 0].scatter(plot_horiz[:800], plot_vert[:800], s=2, alpha=0.4)
    axes[1, 0].axhline(0, color='gray', linewidth=0.5)
    axes[1, 0].axvline(0, color='gray', linewidth=0.5)
    axes[1, 0].set_xlabel(f"Signal ({decision_axis}-axis)")
    axes[1, 0].set_ylabel(f"Orthogonal ({'Q' if decision_axis == 'I' else 'I'}-axis)")
    axes[1, 0].set_title(f"RDS Constellation - CDR offset={cdr_offset}, axis={decision_axis}")
    axes[1, 0].set_xlim([-2.5, 2.5])
    axes[1, 0].set_ylim([-2.5, 2.5])
    axes[1, 0].grid(True, alpha=0.4)

    signal_component = rrc_out_I if decision_axis == 'I' else rrc_out_Q
    n_show = min(30 * rds_SPS, len(signal_component))
    t_ms   = np.arange(n_show) / rds_out_Fs * 1e3
    axes[1, 1].plot(t_ms, signal_component[:n_show], linewidth=0.8)
    visible_idx = cdr_indices[cdr_indices < n_show]
    axes[1, 1].scatter(visible_idx / rds_out_Fs * 1e3, signal_component[visible_idx], color='red', s=20, zorder=5, label=f'CDR samples (offset={cdr_offset})')
    axes[1, 1].set_xlabel("Time (ms)")
    axes[1, 1].set_ylabel("Amplitude (normalized)")
    axes[1, 1].set_title(f"RRC {decision_axis}-axis - first 30 symbols (SPS={rds_SPS})")
    axes[1, 1].legend(fontsize=8)
    axes[1, 1].grid(True, alpha=0.4)

    plt.tight_layout()

    plt.savefig("../data/fmRdsBlock.png")
    print("\nPlot saved to ../data/fmRdsBlock.png")

    plt.show()  
