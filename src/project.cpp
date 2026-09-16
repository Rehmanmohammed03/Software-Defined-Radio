/*
Comp Eng 3DY4 (Computer Systems Integration Project)

Department of Electrical and Computer Engineering
McMaster University
Ontario, Canada
*/

#include "dy4.h"
#include "fourier.h"
#include "genfunc.h"
#include "iofunc.h"
#include "logfunc.h"
#include "filter.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <algorithm>
#include <numeric>
#include <cstring>


#ifdef _GLIBCXX_HAS_GTHREADS
#  include <thread>
#  include <mutex>
#  include <condition_variable>
#  define HAS_STD_THREAD 1
#else
#  define HAS_STD_THREAD 0
#endif

#if HAS_STD_THREAD

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t cap) : cap_(cap), done_(false) {}

    void push(T item) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_space_.wait(lk, [this]{ return buf_.size() < cap_ || done_; });
        if (!done_) { buf_.push_back(std::move(item)); cv_data_.notify_one(); }
    }

    bool pop(T &item) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_data_.wait(lk, [this]{ return !buf_.empty() || done_; });
        if (buf_.empty()) return false;
        item = std::move(buf_.front()); buf_.pop_front();
        cv_space_.notify_one();
        return true;
    }

    void finish() {
        { std::lock_guard<std::mutex> lk(mtx_); done_ = true; }
        cv_space_.notify_all();
        cv_data_.notify_all();
    }

private:
    std::deque<T> buf_;
    std::mutex    mtx_;
    std::condition_variable cv_space_, cv_data_;
    size_t cap_;
    bool   done_;
};

#else

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t) : done_(false) {}
    void push(T item) { if (!done_) buf_.push_back(std::move(item)); }
    bool pop(T &item) {
        if (buf_.empty()) return false;
        item = std::move(buf_.front()); buf_.pop_front(); return true;
    }
    void finish() { done_ = true; }
private:
    std::deque<T> buf_;
    bool done_;
};
#endif


struct ModeParams {
    real rf_Fs, If_Fs, audio_Fs;
    real block_ms;
    int  rf_decim;
    int  audio_decim, audio_up;
    int  filter_taps;
    char path;                 // 'm', 's', 'r'

    int  rfblocksize;
    int  block_IQ_size;
    int  If_block_size;
    int  audio_block_size;
    int  audio_M;

    // RDS (modes 0 and 2 only)
    int  rds_U, rds_D, rds_SPS, rds_M;
    real rds_out_Fs;
    int  rds_block_size;
};


static const uint32_t RDS_H[26] = {
    0b1000000000u, 0b0100000000u, 0b0010000000u, 0b0001000000u,
    0b0000100000u, 0b0000010000u, 0b0000001000u, 0b0000000100u,
    0b0000000010u, 0b0000000001u, 0b1011011100u, 0b0101101110u,
    0b0010110111u, 0b1010000111u, 0b1110011111u, 0b1100010011u,
    0b1101010101u, 0b1101110110u, 0b0110111011u, 0b1000000001u,
    0b1111011100u, 0b0111101110u, 0b0011110111u, 0b1010100111u,
    0b1110001111u, 0b1100011011u
};
static const uint32_t RDS_SYN[4] = {
    0b1111011000u, 0b1111010100u, 0b1001011100u, 0b1001011000u
};
static const uint32_t RDS_SYN_CP = 0b1111001100u;  // offset C'


struct RDSDecState {
    bool     synced       = false;
    int      blockIdx     = 0;
    int      sync_bad_cnt = 0;  
    int      gBits[4] = {0, 0, 0, 0};
    int      diffPrev = 0;
    std::vector<real>    manchBuf;
    std::deque<uint8_t>  bitBuf;
    int      groupType = -1;
    int      gVer      = -1;
    uint16_t pi  = 0;
    int      pty = -1;
    char     ps[8];
    char     rt[64];
    RDSDecState() {
        std::fill(ps, ps + 8,  '?');
        std::fill(rt, rt + 64, '\0');
    }
};


static uint32_t rdsSyn(const std::deque<uint8_t> &buf, int offset = 0) {
    uint32_t s = 0;
    for (int i = 0; i < 26; i++) if (buf[offset + i]) s ^= RDS_H[i];
    return s;
}

static uint32_t rdsSynV(const std::vector<int> &v) {
    uint32_t s = 0;
    for (int i = 0; i < 26; i++) if (v[i]) s ^= RDS_H[i];
    return s;
}


static bool rds1Fix(std::vector<int> &b, uint32_t expSyn) {
    uint32_t err = rdsSynV(b) ^ expSyn;
    if (!err) return false;
    for (int i = 0; i < 26; i++) { if (RDS_H[i] == err) { b[i] ^= 1; return true; } }
    return false;
}


static std::string rdsRTStr(const char rt[64]) {
    std::string s;
    for (int i = 0; i < 64; i++) s += (rt[i] ? rt[i] : '?');
    size_t e = s.size();
    while (e > 0 && (s[e-1] == '?' || s[e-1] == ' ')) e--;
    return s.substr(0, e);
}


static const char* PTY_NAMES[32] = {
    "No program type or undefined",  // 0
    "News",                          // 1
    "Information",                   // 2
    "Sports",                        // 3
    "Talk",                          // 4
    "Rock",                          // 5
    "Classic Rock",                  // 6
    "Adult Hits",                    // 7
    "Soft Rock",                     // 8
    "Top 40",                        // 9
    "Country",                       // 10
    "Oldies",                        // 11
    "Soft",                          // 12
    "Nostalgia",                     // 13
    "Jazz",                          // 14
    "Classical",                     // 15
    "Rhythm and Blues",              // 16
    "Soft Rhythm and Blues",         // 17
    "Foreign Language",              // 18
    "Religious Music",               // 19
    "Religious Talk",                // 20
    "Personality",                   // 21
    "Public",                        // 22
    "College",                       // 23
    "Unassigned",                    // 24
    "Unassigned",                    // 25
    "Unassigned",                    // 26
    "Unassigned",                    // 27
    "Unassigned",                    // 28
    "Weather",                       // 29
    "Emergency Test",                // 30
    "Emergency"                      // 31
};

static void rdsBlock(uint16_t info, int bidx, RDSDecState &d) {
    d.gBits[bidx] = info;
    if (bidx == 0) {
        d.pi = info;
        std::cerr << "  PI: 0x" << std::hex << info << std::dec << "\n";
    } else if (bidx == 1) {
        d.groupType = (info >> 12) & 0xF;
        d.gVer      = (info >> 11) & 0x1;
        d.pty       = (info >>  5) & 0x1F;
        std::cerr << "  Group " << d.groupType << (d.gVer == 0 ? "A" : "B")
                  << ", PTY=" << d.pty << " (" << PTY_NAMES[d.pty] << ")\n";
    } else if (bidx == 2) {
        if (d.groupType == 2 && d.gVer == 0) {
            int seg = d.gBits[1] & 0xF, base = 4 * seg;
            if (base + 1 < 64) {
                int c0 = (info >> 8) & 0xFF, c1 = info & 0xFF;
                d.rt[base]   = (c0 >= 32 && c0 < 128) ? (char)c0 : '?';
                d.rt[base+1] = (c1 >= 32 && c1 < 128) ? (char)c1 : '?';
            }
        }
    } else {  // bidx == 3
        if (d.groupType == 0 && d.gVer == 0) {
            int seg = d.gBits[1] & 0x3;
            int c0  = (info >> 8) & 0xFF, c1 = info & 0xFF;
            d.ps[2*seg]   = (c0 >= 32 && c0 < 128) ? (char)c0 : '?';
            d.ps[2*seg+1] = (c1 >= 32 && c1 < 128) ? (char)c1 : '?';
            std::cerr << "  PS seg " << seg << ": '"
                      << (char)((c0>=32&&c0<128)?c0:'?')
                      << (char)((c1>=32&&c1<128)?c1:'?')
                      << "'  -> '";
            for (int i = 0; i < 8; i++) std::cerr << d.ps[i];
            std::cerr << "'\n";
        } else if (d.groupType == 2 && d.gVer == 0) {
            int seg = d.gBits[1] & 0xF, base = 4 * seg + 2;
            if (base + 1 < 64) {
                int c0 = (info >> 8) & 0xFF, c1 = info & 0xFF;
                d.rt[base]   = (c0 >= 32 && c0 < 128) ? (char)c0 : '?';
                d.rt[base+1] = (c1 >= 32 && c1 < 128) ? (char)c1 : '?';
            }
            std::cerr << "  RT seg " << seg << ": '" << rdsRTStr(d.rt) << "'\n";
        }
    }
}


static void rdsDecode(const std::vector<real> &syms, RDSDecState &d) {
   
    std::vector<real> all;
    all.insert(all.end(), d.manchBuf.begin(), d.manchBuf.end());
    all.insert(all.end(), syms.begin(), syms.end());

    std::vector<int> diff;
    int i = 0;
    while (i + 1 < (int)all.size()) {
        if      (all[i] > 0 && all[i+1] < 0) { diff.push_back(1); i += 2; }
        else if (all[i] < 0 && all[i+1] > 0) { diff.push_back(0); i += 2; }
        else i++;
    }
    d.manchBuf.clear();
    if (i < (int)all.size()) d.manchBuf.push_back(all.back());

   
    for (int b : diff) {
        d.bitBuf.push_back((uint8_t)(b ^ d.diffPrev));
        d.diffPrev = b;
    }

    
    while (true) {
        if (!d.synced) {
            if ((int)d.bitBuf.size() < 52) break;

            
            bool acquired = false;
            for (int bidx = 0; bidx < 4 && !acquired; bidx++) {
                uint32_t syn0 = rdsSyn(d.bitBuf, 0);
                bool match0 = (syn0 == RDS_SYN[bidx]) ||
                              (bidx == 2 && syn0 == RDS_SYN_CP);
                if (match0) {
                    int next_bidx = (bidx + 1) % 4;
                    uint32_t syn1 = rdsSyn(d.bitBuf, 26);
                    bool match1 = (syn1 == RDS_SYN[next_bidx]) ||
                                  (next_bidx == 2 && syn1 == RDS_SYN_CP);
                    if (match1) {
                        uint16_t info = 0;
                        for (int k = 0; k < 16; k++) info = (info << 1) | d.bitBuf[k];
                        rdsBlock(info, bidx, d);
                        d.synced   = true;
                        d.blockIdx = next_bidx;
                        for (int k = 0; k < 26; k++) d.bitBuf.pop_front();
                        acquired = true;
                    }
                }
            }
            if (acquired) continue;
            d.bitBuf.pop_front();

        } else {
            if ((int)d.bitBuf.size() < 26) break;

            std::vector<int> b26(d.bitBuf.begin(), d.bitBuf.begin() + 26);
            uint32_t syn      = rdsSynV(b26);
            uint32_t expected = RDS_SYN[d.blockIdx];
            bool ok = (syn == expected) || (d.blockIdx == 2 && syn == RDS_SYN_CP);

            if (!ok) {
          
                std::vector<int> try1 = b26;
                if (d.blockIdx == 2) {
                    if (rds1Fix(try1, RDS_SYN[2])) {
                        b26 = try1; ok = true;
                    } else {
                        try1 = b26;
                        if (rds1Fix(try1, RDS_SYN_CP)) { b26 = try1; ok = true; }
                    }
                } else {
                    if (rds1Fix(try1, expected)) { b26 = try1; ok = true; }
                }
            }

            if (ok) {
                d.sync_bad_cnt = 0;
                uint16_t info = 0;
                for (int k = 0; k < 16; k++) info = (info << 1) | b26[k];
                rdsBlock(info, d.blockIdx, d);
                d.blockIdx = (d.blockIdx + 1) % 4;
                for (int k = 0; k < 26; k++) d.bitBuf.pop_front();
            } else {
                d.sync_bad_cnt++;
                if (d.sync_bad_cnt >= 8) {
                    
                    d.synced       = false;
                    d.sync_bad_cnt = 0;
                    d.bitBuf.pop_front();
                } else {
                    
                    for (int k = 0; k < 26; k++) d.bitBuf.pop_front();
                    d.blockIdx = (d.blockIdx + 1) % 4;
                }
            }
        }
    }
}


static std::vector<real> cdrSample(const std::vector<real> &sig, int SPS, int &phase) {
    std::vector<real> out;
    for (int k = phase; k < (int)sig.size(); k += SPS)
        out.push_back((sig[k] > 0.0f) ? 1.0f : -1.0f);
    if (!out.empty()) {
        int lastk = phase + ((int)out.size() - 1) * SPS;
        phase = lastk + SPS - (int)sig.size();
    }
    return out;
}


static void rfThread(const ModeParams &p,
                     BoundedQueue<std::vector<real>> &audioQ,
                     BoundedQueue<std::vector<real>> *rdsQ)
{
    std::vector<real> rf_coeff;
    impulseResponseLPF(p.rf_Fs, 100e3, p.filter_taps, rf_coeff, 1.0);

    std::vector<real> I_state(p.filter_taps - 1, 0.0);
    std::vector<real> Q_state(p.filter_taps - 1, 0.0);
    std::vector<real> combRF(p.filter_taps - 1 + p.rfblocksize, 0.0);
    real I_prev = 0.0, Q_prev = 0.0;

    std::vector<real> block_data(p.block_IQ_size, 0.0);
    std::vector<real> blk_I(p.rfblocksize), blk_Q(p.rfblocksize);
    std::vector<real> If_I(p.If_block_size), If_Q(p.If_block_size);
    std::vector<real> fm(p.If_block_size);
    std::vector<char> raw(p.block_IQ_size);
    int bid = 0;

    while (true) {
        readStdinBlockData(p.block_IQ_size, block_data, raw);
        if (std::cin.rdstate() != 0) break;

        if (bid % 50 == 0)
            std::cerr << "RF block " << bid << "\n";

        UnInterleave_IQ(block_data, blk_I, blk_Q);
        blockConvolve_DecimateFast(If_I, blk_I, rf_coeff, I_state, combRF, p.rf_decim);
        blockConvolve_DecimateFast(If_Q, blk_Q, rf_coeff, Q_state, combRF, p.rf_decim);
        fmDemodNoArctan(If_I, If_Q, I_prev, Q_prev, fm);

        audioQ.push(fm);
        if (rdsQ) rdsQ->push(fm);
        bid++;
    }

    std::cerr << "End of input stream reached\n";
    audioQ.finish();
    if (rdsQ) rdsQ->finish();
}


static void audioThread(const ModeParams &p,
                        BoundedQueue<std::vector<real>> &audioQ)
{
    bool do_stereo = (p.path == 's' || p.path == 'r');
    int  audio_M   = p.audio_M;

    std::vector<real> audio_coeff, stereo_lpf_coeff;
    impulseResponseLPF(p.If_Fs * p.audio_up, 16e3, audio_M,
                       audio_coeff, (real)p.audio_up);

    std::vector<real> pilot_coeff, stereo_coeff;
    if (do_stereo) {
        impulseResponseBPF(p.If_Fs, 18.5e3, 19.5e3, p.filter_taps, pilot_coeff);
        impulseResponseBPF(p.If_Fs, 22e3,   54e3,   p.filter_taps, stereo_coeff);
        impulseResponseLPF(p.If_Fs * p.audio_up, 16e3, audio_M,
                           stereo_lpf_coeff, (real)p.audio_up * 2.0);
    }

    int mono_delay_len = (p.filter_taps - 1) / 2;

    std::vector<real> audio_state(audio_M - 1, 0.0);
    std::vector<real> pilot_state(p.filter_taps - 1, 0.0);
    std::vector<real> stereo_bpf_state(p.filter_taps - 1, 0.0);
    std::vector<real> stereo_lpf_state(audio_M - 1, 0.0);
    std::vector<real> mono_delay_state(mono_delay_len, 0.0);

    std::vector<real> combAudio(audio_M - 1 + p.If_block_size, 0.0);
    std::vector<real> combPilot(p.filter_taps - 1 + p.If_block_size, 0.0);
    std::vector<real> combStereo(p.filter_taps - 1 + p.If_block_size, 0.0);
    std::vector<real> combStereoLPF(audio_M - 1 + p.If_block_size, 0.0);

    std::vector<real> pilot_filt(p.If_block_size), stereo_filt(p.If_block_size);
    std::vector<real> nco_out(p.If_block_size), mixed(p.If_block_size);
    std::vector<real> audio_data(p.audio_block_size);
    std::vector<real> stereo_audio(p.audio_block_size);
    std::vector<real> delayed_fm(p.If_block_size);
    std::vector<short int> final_mono(p.audio_block_size);
    std::vector<short int> final_stereo(2 * p.audio_block_size);
    PllState pll_state;

    std::vector<real> fm;
    while (audioQ.pop(fm)) {
        if (do_stereo) {
            // Mono path with allpass delay for stereo alignment
            for (int k = 0; k < mono_delay_len; k++)
                delayed_fm[k] = mono_delay_state[k];
            for (int k = 0; k < p.If_block_size - mono_delay_len; k++)
                delayed_fm[k + mono_delay_len] = fm[k];
            for (int k = 0; k < mono_delay_len; k++)
                mono_delay_state[k] = fm[p.If_block_size - mono_delay_len + k];

            if (p.audio_up > 1)
                blockConvolve_ResampleFast(audio_data, delayed_fm, audio_coeff,
                                           audio_state, combAudio, p.audio_decim, p.audio_up);
            else
                blockConvolve_DecimateFast(audio_data, delayed_fm, audio_coeff,
                                           audio_state, combAudio, p.audio_decim);

            // Stereo path
            blockConvolve_DecimateFast(pilot_filt, fm, pilot_coeff,
                                       pilot_state, combPilot, 1);
            pllBlock(pilot_filt, 19e3, p.If_Fs, 2.0, 0.0, 0.02,
                     pll_state, nco_out);
            blockConvolve_DecimateFast(stereo_filt, fm, stereo_coeff,
                                       stereo_bpf_state, combStereo, 1);
            for (int k = 0; k < p.If_block_size; k++)
                mixed[k] = stereo_filt[k] * nco_out[k];

            if (p.audio_up > 1)
                blockConvolve_ResampleFast(stereo_audio, mixed, stereo_lpf_coeff,
                                           stereo_lpf_state, combStereoLPF,
                                           p.audio_decim, p.audio_up);
            else
                blockConvolve_DecimateFast(stereo_audio, mixed, stereo_lpf_coeff,
                                           stereo_lpf_state, combStereoLPF, p.audio_decim);

            for (int k = 0; k < p.audio_block_size; k++) {
                real L = 0.5f * (audio_data[k] + stereo_audio[k]);
                real R = 0.5f * (audio_data[k] - stereo_audio[k]);
                final_stereo[2*k]   = std::isnan(L) ? 0 : static_cast<short int>(L * 16384);
                final_stereo[2*k+1] = std::isnan(R) ? 0 : static_cast<short int>(R * 16384);
            }
            fwrite(&final_stereo[0], sizeof(short int), final_stereo.size(), stdout);

        } else {
            if (p.audio_up > 1)
                blockConvolve_ResampleFast(audio_data, fm, audio_coeff,
                                           audio_state, combAudio, p.audio_decim, p.audio_up);
            else
                blockConvolve_DecimateFast(audio_data, fm, audio_coeff,
                                           audio_state, combAudio, p.audio_decim);

            for (int k = 0; k < p.audio_block_size; k++)
                final_mono[k] = std::isnan(audio_data[k])
                                 ? 0 : static_cast<short int>(audio_data[k] * 16384);
            fwrite(&final_mono[0], sizeof(short int), final_mono.size(), stdout);
        }
    }
}



static void rdsThread(const ModeParams &p,
                      BoundedQueue<std::vector<real>> &rdsQ)
{

    std::vector<real> bpf_coeff, bpf114_coeff, resamp_coeff, rrc_coeff;
    impulseResponseBPF(p.If_Fs, 54e3,    60e3,   p.filter_taps, bpf_coeff);
    impulseResponseBPF(p.If_Fs, 113.5e3, 114.5e3, p.filter_taps, bpf114_coeff);
    impulseResponseLPF(p.If_Fs * p.rds_U, 3e3, p.rds_M, resamp_coeff, (real)p.rds_U);
    impulseResponseRRC(p.rds_out_Fs, p.filter_taps, rrc_coeff);

    std::vector<real> bpf_state(p.filter_taps - 1, 0.0);
    std::vector<real> bpf114_state(p.filter_taps - 1, 0.0);
    std::vector<real> resI_state(p.rds_M - 1, 0.0);
    std::vector<real> resQ_state(p.rds_M - 1, 0.0);
    std::vector<real> rrcI_state(p.filter_taps - 1, 0.0);
    std::vector<real> rrcQ_state(p.filter_taps - 1, 0.0);

    std::vector<real> combBPF(p.filter_taps - 1 + p.If_block_size, 0.0);
    std::vector<real> comb114(p.filter_taps - 1 + p.If_block_size, 0.0);
    std::vector<real> combResI(p.rds_M - 1 + p.If_block_size, 0.0);
    std::vector<real> combResQ(p.rds_M - 1 + p.If_block_size, 0.0);
    std::vector<real> combRrcI(p.filter_taps - 1 + p.rds_block_size, 0.0);
    std::vector<real> combRrcQ(p.filter_taps - 1 + p.rds_block_size, 0.0);

  
    std::vector<real> bpf_out(p.If_block_size);
    std::vector<real> sq(p.If_block_size);
    std::vector<real> bpf114_out(p.If_block_size);
    std::vector<real> nco_I(p.If_block_size), nco_Q(p.If_block_size);
    std::vector<real> bpf_delayed(p.If_block_size);
    std::vector<real> mixed_I(p.If_block_size), mixed_Q(p.If_block_size);
    std::vector<real> res_I(p.rds_block_size), res_Q(p.rds_block_size);
    std::vector<real> rrc_I(p.rds_block_size), rrc_Q(p.rds_block_size);


    int allpass_len = (p.filter_taps - 1) / 2;
    std::vector<real> allpass_state(allpass_len, 0.0);

    PllState pll_state;

    const int WARMUP_BLOCKS = 50;
    std::vector<real> wu_I, wu_Q;
    int  cdr_offset  = p.rds_SPS / 2;
    char dec_axis    = 'I';
    real rrc_rms     = 1.0f;
    int  cdr_phase   = 0;
    bool warmup_done = false;

    RDSDecState dec;
    int block_cnt = 0;

    std::vector<real> fm;
    while (rdsQ.pop(fm)) {

        
        blockConvolve_DecimateFast(bpf_out, fm, bpf_coeff, bpf_state, combBPF, 1);

     
        for (int k = 0; k < p.If_block_size; k++) sq[k] = bpf_out[k] * bpf_out[k];

     
        blockConvolve_DecimateFast(bpf114_out, sq, bpf114_coeff, bpf114_state, comb114, 1);

       
        pllBlockIQ(bpf114_out, 114e3, p.If_Fs, 0.5, 0.0, 0.002,
                   pll_state, nco_I, nco_Q);

       
        for (int k = 0; k < allpass_len; k++)
            bpf_delayed[k] = allpass_state[k];
        for (int k = 0; k < p.If_block_size - allpass_len; k++)
            bpf_delayed[k + allpass_len] = bpf_out[k];
        for (int k = 0; k < allpass_len; k++)
            allpass_state[k] = bpf_out[p.If_block_size - allpass_len + k];

        
        for (int k = 0; k < p.If_block_size; k++) {
            mixed_I[k] = bpf_delayed[k] * nco_I[k];
            mixed_Q[k] = bpf_delayed[k] * nco_Q[k];
        }

        
        blockConvolve_ResampleFast(res_I, mixed_I, resamp_coeff,
                                   resI_state, combResI, p.rds_D, p.rds_U);
        blockConvolve_ResampleFast(res_Q, mixed_Q, resamp_coeff,
                                   resQ_state, combResQ, p.rds_D, p.rds_U);

        
        blockConvolve_DecimateFast(rrc_I, res_I, rrc_coeff, rrcI_state, combRrcI, 1);
        blockConvolve_DecimateFast(rrc_Q, res_Q, rrc_coeff, rrcQ_state, combRrcQ, 1);

        if (!warmup_done) {
           
            wu_I.insert(wu_I.end(), rrc_I.begin(), rrc_I.end());
            wu_Q.insert(wu_Q.end(), rrc_Q.begin(), rrc_Q.end());

            if (block_cnt == WARMUP_BLOCKS - 1) {
              
                real mI = 0, mQ = 0;
                for (auto v : wu_I) mI += v;
                mI /= (real)wu_I.size();
                for (auto v : wu_Q) mQ += v;
                mQ /= (real)wu_Q.size();
                for (auto &v : wu_I) v -= mI;
                for (auto &v : wu_Q) v -= mQ;

                
                real rms2 = 0;
                for (int k = 0; k < (int)wu_I.size(); k++)
                    rms2 += wu_I[k]*wu_I[k] + wu_Q[k]*wu_Q[k];
                rms2 /= (real)wu_I.size();
                rrc_rms = (rms2 > 0) ? std::sqrt(rms2) : 1.0f;
                for (auto &v : wu_I) v /= rrc_rms;
                for (auto &v : wu_Q) v /= rrc_rms;

                
                real best_score = -1.0f;
                for (int trial = 0; trial < p.rds_SPS; trial++) {
                    real sI = 0, sQ = 0; int n = 0;
                    for (int k = trial; k < (int)wu_I.size(); k += p.rds_SPS, n++) {
                        sI += std::abs(wu_I[k]);
                        sQ += std::abs(wu_Q[k]);
                    }
                    if (n > 0) { sI /= n; sQ /= n; }
                    real sc = std::max(sI, sQ);
                    if (sc > best_score) { best_score = sc; cdr_offset = trial; }
                }

       
                {
                    real sumI=0, sumQ=0, sumI2=0, sumQ2=0; int n=0;
                    for (int k=cdr_offset; k<(int)wu_I.size(); k+=p.rds_SPS, n++) {
                        sumI  += wu_I[k]; sumI2 += wu_I[k]*wu_I[k];
                        sumQ  += wu_Q[k]; sumQ2 += wu_Q[k]*wu_Q[k];
                    }
                    real stdI = 0, stdQ = 0;
                    if (n > 1) {
                        real meanI = sumI/n, meanQ = sumQ/n;
                        stdI = std::sqrt(std::max(0.0f, sumI2/n - meanI*meanI));
                        stdQ = std::sqrt(std::max(0.0f, sumQ2/n - meanQ*meanQ));
                    }
                    dec_axis = (stdI >= stdQ) ? 'I' : 'Q';
                    std::cerr << "[RDS warm-up] CDR=" << cdr_offset << "/" << p.rds_SPS
                              << " axis=" << dec_axis
                              << " stdI=" << stdI << " stdQ=" << stdQ
                              << " score=" << best_score << "\n";
                }

                
                const std::vector<real> &wuSig = (dec_axis == 'I') ? wu_I : wu_Q;
                cdr_phase = cdr_offset;
                std::vector<real> wu_syms = cdrSample(wuSig, p.rds_SPS, cdr_phase);
                rdsDecode(wu_syms, dec);
                warmup_done = true;
            }

        } else {
          
            for (auto &v : rrc_I) v /= rrc_rms;
            for (auto &v : rrc_Q) v /= rrc_rms;

            real mI = 0, mQ = 0;
            for (auto v : rrc_I) mI += v;
            mI /= (real)rrc_I.size();
            for (auto v : rrc_Q) mQ += v;
            mQ /= (real)rrc_Q.size();
            for (auto &v : rrc_I) v -= mI;
            for (auto &v : rrc_Q) v -= mQ;

       
            const std::vector<real> &sig = (dec_axis == 'I') ? rrc_I : rrc_Q;
            std::vector<real> syms = cdrSample(sig, p.rds_SPS, cdr_phase);
            rdsDecode(syms, dec);

            if (block_cnt % 100 == 0)
                std::cerr << "[RDS block " << block_cnt << "] synced="
                          << dec.synced << " syms=" << syms.size() << "\n";
        }

        block_cnt++;
    }

    std::cerr << "\n=== RDS Final Results ===\n";
    if (dec.pi)
        std::cerr << "PI  : 0x" << std::hex << dec.pi << std::dec << "\n";
    if (dec.pty >= 0)
        std::cerr << "PTY : " << dec.pty << " (" << PTY_NAMES[dec.pty] << ")\n";
    std::cerr << "PS  : '";
    for (int i = 0; i < 8; i++) std::cerr << dec.ps[i];
    std::cerr << "'\n";
    std::string rt = rdsRTStr(dec.rt);
    if (!rt.empty())
        std::cerr << "RT  : '" << rt << "'\n";
}


int main(int argc, char* argv[])
{
    ModeParams p;
    p.path        = 'm';
    p.filter_taps = 101;
    int mode      = 0;

    if (argc < 2) {
        std::cerr << "Default: mode 0, mono, 101 taps\n";
    } else if (argc >= 3) {
        mode   = atoi(argv[1]);
        p.path = argv[2][0];
        if (argc == 4) p.filter_taps = atoi(argv[3]);
        if (mode > 3) { std::cerr << "Invalid mode " << mode << "\n"; exit(1); }
    } else {
        std::cerr << "Usage: " << argv[0] << " <mode> <m|s|r> [taps]\n";
        exit(1);
    }

    std::cerr << "Mode " << mode << "  path=" << p.path
              << "  taps=" << p.filter_taps << "\n";
    std::cerr << "Working with reals on " << sizeof(real) << " bytes\n";


    if (mode == 0) {
        p.rf_Fs=2400e3; p.If_Fs=240e3; p.audio_Fs=48e3;
        p.block_ms=40;  p.rf_decim=10; p.audio_decim=5; p.audio_up=1;
    } else if (mode == 1) {
        p.rf_Fs=2304e3; p.If_Fs=288e3; p.audio_Fs=36e3;
        p.block_ms=56;  p.rf_decim=8;  p.audio_decim=8; p.audio_up=1;
    } else if (mode == 2) {
        p.rf_Fs=2400e3; p.If_Fs=240e3; p.audio_Fs=44.1e3;
        p.block_ms=60;  p.rf_decim=10; p.audio_decim=800; p.audio_up=147;
    } else {
        p.rf_Fs=1800e3; p.If_Fs=360e3; p.audio_Fs=44.1e3;
        p.block_ms=30;  p.rf_decim=5;  p.audio_decim=400; p.audio_up=49;
    }

    p.rfblocksize      = (int)(p.rf_Fs * p.block_ms / 1000.0);
    p.block_IQ_size    = p.rfblocksize * 2;
    p.If_block_size    = p.rfblocksize / p.rf_decim;
    p.audio_M          = p.filter_taps * p.audio_up;
    p.audio_block_size = (p.If_block_size * p.audio_up) / p.audio_decim;


    if (mode == 0 || mode == 2) {
        p.rds_SPS    = (mode == 0) ? 42 : 21;
        p.rds_U      = 133;
        p.rds_D      = (mode == 0) ? 320 : 640;
        p.rds_out_Fs = (real)(p.rds_U * p.If_Fs / p.rds_D);
        p.rds_M      = p.filter_taps * p.rds_U;
        p.rds_block_size = (p.If_block_size * p.rds_U) / p.rds_D;
    } else {
        if (p.path == 'r') {
            std::cerr << "RDS not supported in mode " << mode << "\n";
            exit(0);
        }
        p.rds_SPS=42; p.rds_U=133; p.rds_D=320;
        p.rds_out_Fs=(real)(p.rds_U*p.If_Fs/p.rds_D);
        p.rds_M=p.filter_taps*p.rds_U;
        p.rds_block_size=(p.If_block_size*p.rds_U)/p.rds_D;
    }

    std::cerr << "RF block=" << p.rfblocksize
              << "  IF block=" << p.If_block_size
              << "  audio block=" << p.audio_block_size << "\n";
    if (p.path == 'r')
        std::cerr << "RDS: SPS=" << p.rds_SPS
                  << "  U=" << p.rds_U << "  D=" << p.rds_D
                  << "  out_Fs=" << p.rds_out_Fs
                  << "  rds_block=" << p.rds_block_size << "\n";

    BoundedQueue<std::vector<real>> audioQ(4);
    BoundedQueue<std::vector<real>> rdsQ(4);
    bool do_rds = (p.path == 'r');


#if HAS_STD_THREAD
    std::thread rf_t([&](){ rfThread(p, audioQ, do_rds ? &rdsQ : nullptr); });
    std::thread audio_t([&](){ audioThread(p, audioQ); });
    std::thread rds_t;
    if (do_rds)
        rds_t = std::thread([&](){ rdsThread(p, rdsQ); });

    rf_t.join();
    audio_t.join();
    if (do_rds && rds_t.joinable()) rds_t.join();
#else
    std::cerr << "[INFO] std::thread unavailable; running sequentially\n";
    rfThread(p, audioQ, do_rds ? &rdsQ : nullptr);
    audioThread(p, audioQ);
    if (do_rds) rdsThread(p, rdsQ);
#endif

    return 0;
}
