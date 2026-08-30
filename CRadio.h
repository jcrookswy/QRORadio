#pragma once
#include "portaudio.h"
#include <complex>
#include <thread>
#include <windows.h>
#include <ipp.h>  

extern bool gUseDebugWaveform;
extern int g_max_amp;
extern int g_abs_max_amp;

//modes
//0 = IDLE
//1 = RX USB
//2 = RX LSB
//3 = RX CW
//4 = TX USB
//5 = TX LSB
//6 = TX CW
#define IDLE_MODE 0
#define RX_MODE 1
#define TX_MODE 2
#define VNA_MODE 3
#define CW_TX_MODE 4

class MyFrame;

struct AntTuneCal {
	Ipp32fc SweptOpen[36];
	Ipp32fc SweptShort[36];
	Ipp32fc SweptLoad[36];
	Ipp32fc SweptIQMu[36];
};

struct RadioStatus {
	float RXFreq;
	float TunerFreq;
	float SWRTuned[36];
	float SWRUntuned[36];
	float SmithChartTuned[72]; // x,y coords
	float SmithChartUntuned[72]; // x,y coords
	int Sunit;
	int mode;
	int calMode;
	float volts;
	float amps;
	char GMTTime[16];
	float AudioFreqPlot[16];
	float AudioTimePlot[128];
	float TXAvgPowerPlot[64];  // scrolling history of TX average power per 2048-sample chunk, pre-limiter (0.0-1.0)
	float RFFreqPlot[256];
	bool UpdateText;
	bool UpdateAudio;
	bool UpdateVSWR;
	bool UpdateRFPlot;
};

// One FFT-tone tracker for the CW multi-tone decoder. Up to CRadio::kCWMaxSlicers run in parallel.
struct CWSlicer {
    bool  used;          // false = slot never assigned (UI shows "--- Hz")
    int   binIndex;       // absolute FFT bin (kCWBinLo..kCWBinHi) of the tracked tone
    bool  markState;      // true if marking as of the last hop
    int   stateHopCount;  // consecutive hops in the current mark/space state
    bool  resolvedThisSpace; // see RunCWSlicerTiming: true once the in-progress space run has already
                             // triggered ResolveCWSlicerCharacter, so the end-of-run check doesn't resolve twice
    int   hopsSinceMark;  // hops since last matched a peak; eviction picks the largest
    float dotUnitMs;      // adaptive Morse "dot" duration estimate, ms, clamped [20,200]
    char  pattern[16];    // in-progress dot/dash pattern
    int   patternLen;
    char  text[128];      // this slicer's own scrolling decoded text
    int   textLen;

    // Smear-corrected run-length history, in hops, oldest first - for tuning cwSquelch and the
    // mark/space correction without recompiling (see RunCWSlicerTiming in CRadio.cpp).
    int   markLengths[16];
    int   spaceLengths[16];

    // Tracked frequency, normalized to kCWFFTSize (binIndex/kCWFFTSize) - used to build the on-pitch
    // 512-point DFT tone below (see BuildCWDFTTone).
    float freqNorm;

    // dftOnHistory holds the last 8 hops' raw dftOnCorr values (see AssignCWSlicers), newest first
    // (index 0 = most recent hop, shifted right each hop as new values come in). Two vector sums are
    // derived from it every hop - |vectorSum(newest 4)| and |vectorSum(oldest 4)| - which feed both:
    //  - signalDetect: (|vectorSum(newest 4)| + |vectorSum(oldest 4)|) * cwSquelch > sum of the last
    //    8 hops' dftNoiseHistory (see below) ("signal" vs "noise floor near the signal"). A real
    //    tone's on-pitch correlator outputs stay roughly phase-aligned hop to hop, so each 4-hop half
    //    adds up in its own vector sum; dftNoiseHistory's off-pitch correlators see no such tone, so
    //    the ratio between the two separates signal from noise regardless of absolute level.
    //  - diff = |vectorSum(newest 4)| - |vectorSum(oldest 4)| - the edge-detect input below (see
    //    lastDiff), genuinely bipolar (unlike a plain envelope magnitude): near zero in a steady
    //    mark or space, and forms a clean, well-localized peak right at each transition.
	Ipp32fc dftOnHistory[8];

	// Last 8 hops' noise-floor estimate, newest first (shifted alongside dftOnHistory above): each
	// entry is sqrt(|dftLowCorr|^2 + |dftHighCorr|^2) from that hop's two off-pitch correlators (see
	// dftTonesLow/dftTonesHigh below), which see no on-pitch tone and so track ambient noise near the
	// tracked bin. Summed (scalar, not vector - there's no coherent phase across noise samples to
	// preserve) into the noiseMag side of the signalDetect ratio above.
	Ipp32f	dftNoiseHistory[8];
	bool    signalDetect;

    // Edge-detect mark/space slicing (see AssignCWSlicers): diff (see dftOnHistory above), tracked
    // hop to hop. A rising-to-falling reversal (a positive peak) marks the space->mark transition
    // (gated by signalDetect). diffRising records which direction diff was last moving, so a
    // reversal can be recognized on the single hop it happens.
    float lastDiff;
    bool  diffRising;

    // Captured at the space->mark transition (the positive diff peak that triggered it) - see
    // AssignCWSlicers. The mark->space transition then requires a negative diff peak (trough) of at
    // least half this magnitude, or signalDetect dropping out, before releasing the mark.
    float markPeakAmp;

    // 3-tone 512-point overlapped-window correlator (see BuildCWDFTTone/AssignCWSlicers): dftTonesOn
    // is on-pitch (binIndex), dftTonesLow/dftTonesHigh are +-kCWDFTBinOffset bins away - exactly the
    // first null of a Hann(kCWDFTSize) window's frequency response, so an on-pitch signal reads ~zero
    // on them. Each is kCWDFTBufLen samples: kCWDFTSegments independently Hann-windowed kCWDFTSize-
    // sample blocks, one per hop position. Unlike a simple periodic buffer, these 512-sample windows
    // overlap 50% in the underlying oscillator's phase from one hop to the next, so the same
    // oscillator sample lands in two different windows under two different window coefficients and
    // can't be read from one shared buffer via a strided slice - each block is precomputed and stored
    // separately instead. dftSegIndex cycles 0..kCWDFTSegments-1, selecting which precomputed block
    // lines up with the current hop's audio. dftOnCorr/dftLowCorr/dftHighCorr are the latest hop's
    // raw dot-product results; dftOnCorr feeds dftOnHistory (see above) and drives the mark/space
    // decision, dftLowCorr/dftHighCorr feed dftNoiseHistory (see above) each hop and are not otherwise
    // retained.
    Ipp32fc* dftTonesOn;
    Ipp32fc* dftTonesLow;
    Ipp32fc* dftTonesHigh;
    int      dftSegIndex;
    Ipp32fc  dftOnCorr;
    Ipp32fc  dftLowCorr;
    Ipp32fc  dftHighCorr;

    // FindCWPeak-reported magnitude of the candidate peak this slicer was last (re)tuned to (see
    // AssignCWSlicers) - a later candidate only steals this slot if its own magnitude beats this.
    float peakMag;
};

class CRadio
{
public:
	CRadio();
	~CRadio() ;
	int Connect();
	bool connected;
	int UpdatePlot();
	int DataThread();
	void RXDataLoop();
	void TXDataLoop();
	void AntTuneDataLoop();

	Ipp32fc GetS11(Ipp32f* H2Window);
	void AntTuneSweepOSL(int osl);
	Ipp32fc GetCorrectedS11(int index, Ipp32fc S11Raw);
	void SweepAntTune();


	int RelaySettings;
	int comPort;
	bool SaveSettings(const char* path);
	bool LoadSettings(const char* path);
	int SetRXBits(uint8_t rly);
//	int DoAudioFFT();

	void ProcessIQ(char* data); // Change 4, 6-bit values into a float
	void DoRXDSP(bool bypassALC); // Change 4, 6-bit values into a float
	int SetFreq(double freqMHz);
	void UpdateADCs(char* readData);

	std::thread myAThread;
	std::thread myDThread;

	int AudioInputChannels;
	int AudioOutputChannels;

	MyFrame* theFrame;
	RadioStatus* myStatus;
	AntTuneCal* myVNACal;
	HANDLE hSerial;

	Ipp32f* audioInBuf;
	Ipp32f* audioOutBuf;
	Ipp32f* resampledAudioOut;   // I channel resampled (also used for real-only path)
	Ipp32f* resampledAudioOutQ; // Q channel resampled (CESSB complex path)
	Ipp32f* resampledAudioIn;
	void Get1280AudioSamples(float gain);
	IppsResamplingPolyphase_32f* resample_state;    // I channel polyphase resampler
	IppsResamplingPolyphase_32f* resample_state_q;  // Q channel polyphase resampler

	Ipp32fc* cessbOut;  // CESSB complex output, 1280 samples
	Ipp32f*  cessbI;    // I channel split for resampler, 1280 samples
	Ipp32f*  cessbQ;    // Q channel split for resampler, 1280 samples

	double LOfreq;
	double stepSize;
	float micGain;
	float agcMaxGain;
	float agcTarget;
	bool NewLOFreq;
	bool is8S;
	bool isRXOnly;

	float RXChannelPower;  // 5-second peak channel power, center to +3 kHz

	float rxPowerPeaks[32];   // peak of each 16-reading block (~3 s of history)
	int   rxPowerPeakIdx;     // next slot to write in rxPowerPeaks
	int   rxPowerBlockPos;    // readings accumulated in current block (0-15)
	float rxPowerBlockMax;    // running max within current block

	int   plotSfloor;   // S-unit at bottom of spectrum plot (0-9)
	int   plotSunits;   // number of S-unit divisions shown (8-24)
	float plotSoffset;  // calibration trim in dB (-10 to +10)

	float IQCorrAlpha;    // adaptive RX I/Q gain-balance correction (wideband, pre-tune)
	float IQCorrBeta;     // adaptive RX I/Q phase/skew-balance correction (wideband, pre-tune)
	bool  IQBalanceEnabled; // user toggle (Radio > Image Reject menu)

	bool CWModeEnabled; // Radio > CW Mode menu: narrows RX audio to ~100 Hz around 700 Hz and runs the FFT multi-tone CW decoder

	// Narrowband audio bandpass - unchanged, still narrows what's actually heard. The FFT decoder
	// taps RawAudio *before* this runs (see DoRXDSP) so it sees the full 200-2800 Hz band.
	float cwBPF1_x1, cwBPF1_x2, cwBPF1_y1, cwBPF1_y2;
	float cwBPF2_x1, cwBPF2_x2, cwBPF2_y1, cwBPF2_y2;
	void ApplyCWBandpass(Ipp32f* buf, int n);

	// --- CW FFT multi-tone decode engine ---
	static const int kCWFFTOrder = 13;
	static const int kCWFFTSize  = 8192;                    // 1 << kCWFFTOrder
	static const int kCWHopSize  = 256;                     // samples/hop: per-slicer tone-correlator + mark/space timing cadence (~5.33 ms, unchanged)
	static const int kCWPeakHopSize = 4096;                 // samples between big-FFT peak-search updates (50% overlap with kCWFFTSize, ~85.3 ms)
	static const int kCWBinLo    = 35;                      // ceil(200 / (48000/8192))
	static const int kCWBinHi    = 477;                     // floor(2800 / (48000/8192))
	static const int kCWNumBins  = kCWBinHi - kCWBinLo + 1; // 443
	static const int kCWMaxSlicers        = 4;
	// Per-slicer new-peak search sub-band, in absolute FFT bins (see FindCWPeak) - slicer s only ever
	// hunts for new peaks within [kCWPeakBinLo[s], kCWPeakBinHi[s]], and only retunes within that same
	// band (see AssignCWSlicers); contiguous 600-800/800-1400/1400-2100/2100-2800 Hz quarters of the
	// wider 200-2800 Hz capture band (kCWBinLo/kCWBinHi above).
	static constexpr int kCWPeakBinLo[kCWMaxSlicers] = { 103, 137, 239, 359 };
	static constexpr int kCWPeakBinHi[kCWMaxSlicers] = { 136, 238, 358, 477 };
	static const int kCWPeakMaskRadius    = 10;              // bins excluded around each tracked slicer ("more than 3 bins" away is a valid new peak)
	static constexpr float kCWPeakThresholdDb = 10.0f;      // new peak must exceed the average unmasked-bin magnitude by this many dB
	static const int kCWPeakConfirmBins  = 2;               // a candidate peak must land within this many bins of the previous peak-search hop's candidate before a slicer is launched

	// 3-tone 512-point overlapped-window correlator (see CWSlicer::dftTonesOn/Low/High and
	// AssignCWSlicers/BuildCWDFTTone) - separate from, and additional to, the kCWHopSize single-tone
	// correlator above.
	static const int kCWDFTSize      = 512;                        // dot-product window - 2x kCWHopSize, so consecutive hops overlap it 50%
	static const int kCWDFTSegments  = kCWFFTSize / kCWHopSize;    // 32 - one precomputed Hann(kCWDFTSize)-windowed block per hop position
	static const int kCWDFTBufLen    = kCWDFTSegments * kCWDFTSize; // 16384 - total precomputed samples per tone
	static const int kCWDFTBinOffset = 2 * (kCWFFTSize / kCWDFTSize); // 32 - +-2 bins of the kCWDFTSize-point DFT, in kCWFFTSize-grid units;
	                                                                    // exactly the first null of a Hann(kCWDFTSize) window's frequency response,
	                                                                    // so an on-pitch tone reads ~zero on the low/high correlators

	Ipp32f*  cwFFTSampleWindow;   // kCWFFTSize sliding window of wideband pre-filter audio, slid every hop
	Ipp32f*  cwHannWindow;
	Ipp32f*  cw512HannWindow;    // kCWDFTSize-point Hann window for the 3-tone correlator (see BuildCWDFTTone)
	Ipp32fc* cwDFTGenScratch;    // scratch buffer, kCWFFTSize+kCWHopSize samples - see BuildCWDFTTone
	Ipp32fc* cwFFTData;
	IppsFFTSpec_C_32fc* pCWFFTSpec;
	Ipp8u*   pCWFFTSpecBuf;
	Ipp8u*   pCWFFTWorkBuf;
	int      cwPeakHopAccum;      // samples accumulated since the last big-FFT peak search; fires FindCWPeak at kCWPeakHopSize
	int      cwPendingPeakBin[kCWMaxSlicers]; // per-slicer unconfirmed candidate bin from the previous peak-search hop, or -1; see FindCWPeak

	// Per-slicer reference-tone amplitude (see BuildCWDFTTone): just needs to keep the correlator's
	// output in a sane float range (not underflowing/overflowing) - the edge-detect mark/space
	// slicing (see CWSlicer::diffRising) only cares about the shape of the per-hop signal magnitude,
	// not its absolute scale, so this doesn't need careful calibration.
	float cwToneMagn;

	// cwSquelch gates CWSlicer::signalDetect - see its assignment and use in AssignCWSlicers.
	// cwHysteresis is still unused; user-settable via Radio > CW Squelch/Hysteresis but currently has
	// no effect; left in place pending a decision on whether to remove that field or repurpose it.
	float cwSquelch;
	float cwHysteresis;

	CWSlicer cwSlicers[kCWMaxSlicers];

	void CaptureCWSpectrum(Ipp32f* wideband, int n);
	void ComputeCWFFTMag(float* mag, bool* masked);
	bool FindCWPeak(const float* mag, const bool* masked, int searchLoAbs, int searchHiAbs, int* peakBin, float* peakMag, int& pendingBin);
	void AssignCWSlicers(const bool* foundPeak, const int* peakBin, const float* peakMag, float hopMs);
	void BuildCWDFTTone(Ipp32fc* dest, float freqNorm);
	void RunCWSlicerTiming(CWSlicer& slicer, bool markThisHop, float hopMs);
	void AppendCWSlicerText(CWSlicer& slicer, const char* s);
	void ResolveCWSlicerCharacter(CWSlicer& slicer);
	void DoCWFFTDecode(Ipp32f* wideband, int n);
	void ResetCWDecoder();

	// CW transmit (see CWTXDataLoop in CRadio.cpp): StartCWTransmit stashes the text and switches
	// to CW_TX_MODE; CWTXDataLoop (background data thread) builds a raised-cosine-tapered on/off
	// keyed envelope from it, modulates it onto a cwSidetoneHz rotating I/Q phasor (so the carrier
	// lands cwSidetoneHz above the tuned frequency, matching the RX side's CW convention, instead of
	// exactly on it), and streams that out. Deliberately bypasses the SSB HPF/AGC/CESSB chain - but
	// still runs through the same LO-locked polyphase resampler TXDataLoop uses before BuildTXPacket,
	// so dot timing comes out right in real time regardless of LOfreq.
	float cwTxDotMs;             // TX dot length, ms - hard-coded for now (see CRadio constructor)
	static const int kCWTxTaperSamples = 256; // raised-cosine rise/fall on each mark, at 48 kHz
	static const int kCWTxFlushSamples = 768; // zero-amplitude leader/trailer, at 48 kHz, to flush the pipe
	static constexpr float kCWTxPeakAmplitude = 0.5f; // peak envelope magnitude (25% power) - revisit later

	// Sidetone: the same envelope, at an audible tone instead of DC, written to audioOutBuf so
	// PortAudio plays the dots/dashes out while CWTXDataLoop is sending (see patestCallback, which
	// keeps audio flowing during CW_TX_MODE the same way it does for RX_MODE). Independent of
	// kCWTxPeakAmplitude so sidetone loudness and RF drive level can be tuned separately later.
	float cwSidetoneHz;                                // sidetone pitch, Hz - hard-coded for now
	static constexpr float kCWSidetoneAmplitude = 0.3f; // peak sidetone level - revisit later
	char cwTxMessage[64];        // pending text for CWTXDataLoop to build/send; set by StartCWTransmit
	void StartCWTransmit(const char* text);
	void CWTXDataLoop();
	Ipp32f* BuildCWTXEnvelope(const char* text, int* outSampleCount);

	// CW Peaking: repeatedly runs FindCWPeak (the same 8192-point, 600-800 Hz, two-hit-confirmed
	// search the live CW decoder uses) every kCWPeakHopSize samples until it reports a confirmed
	// peak, using its own independent CWPeakPendingBin/CWPeakHopAccum state so it doesn't interfere
	// with the live decoder's own confirm-bin stream if CW Mode happens to be running at the same
	// time (see DoRXDSP). Works whether or not CW Mode is enabled - if it isn't, this drives
	// CaptureCWSpectrum itself to keep the shared window current.
	bool  CWPeakCapturing;    // true while waiting for a confirmed peak
	bool  CWPeakReady;        // true once CWPeakFreq holds a fresh result for the UI to report
	int   CWPeakPendingBin;   // FindCWPeak's confirm-state, independent of the live decoder's cwPendingPeakBin
	int   CWPeakHopAccum;     // samples accumulated since the last FindCWPeak call; fires at kCWPeakHopSize
	float CWPeakFreq; // Hz; last CW Peaking result - also intended for a future auto-tune feature

	void StartCWPeakCapture();

	Ipp32f* MagData ;
	Ipp32f* MagMinAccumData;
	Ipp32f* MagAccumData;
	bool ClearMagAccum;
	Ipp32f* LogMagData;
	int m_iFreq;

	Ipp32fc* RawIQData;// = new Ipp32fc[16000];
	Ipp32fc* TunerData;// = new Ipp32fc[16000];
	Ipp32f TunerPhase;
	Ipp32f TunerMag;
	float TunerFreq;
	Ipp32f* HannWindow;// = new Ipp32f[250];
	Ipp32f* TXHannWindow;// = new Ipp32f[2048];
	Ipp32fc* WindowedData;// = new Ipp32fc[250];
	Ipp32fc* DFTData;// = new Ipp32fc[250];
	IppsDFTSpec_C_32fc* pDFTSpec;
	Ipp8u* pDFTWorkBuf;
	Ipp8u* pTXFFTWorkBuf;

	Ipp32fc* TXFFTData;
	Ipp32f* RaisedCosUpDown;// = new Ipp32f[2048];
	void BuildGainRamp(float* ramp, float GainPA, float GainAB, float GainBC);

	void QuickSync();

	void InitCESSB();
	void ApplyCESSBBandpass(Ipp32f* pIn, Ipp32fc* pOut);
	void ProcessCESSB(Ipp32fc* pInOut);

	IppsFFTSpec_C_32fc* pFFTSpec;
	Ipp8u* pFFTSpecBuf, * pFFTWorkBuf;

	Ipp32fc* IFFTData;// = new Ipp32fc[16000];
	Ipp32fc* IFFTAccum;// = new Ipp32fc[16000];
	Ipp32f* RawAudio;

	Ipp32f debugTonePhase;  // persistent phase for debug tone 1
	Ipp32f debugTonePhase2; // persistent phase for debug tone 2

	// CESSB state (overlap-save, 2048-pt FFT, 768-sample overlap)
	Ipp32f*             cessbRealOverlap;   // 768 real samples: audio history for Hilbert stage
	Ipp32fc*            cessbCplxOverlap1;  // 768 complex: history for iteration-1 post-clip filter
	Ipp32fc*            cessbCplxOverlap2;  // 768 complex: history for iteration-2 post-clip filter
	Ipp32fc*            cessbCplxOverlap3;  // 768 complex: history for iteration-3 post-clip filter
	Ipp32fc*            cessbWorkBuf;       // 2048 complex: FFT working buffer
	IppsFFTSpec_C_32fc* cessbFFTSpec;
	Ipp8u*              cessbFFTSpecBuf;
	Ipp8u*              cessbFFTWorkBuf;


	Ipp32fc m_lastIQMu;

	int IQWriteAddr;
	int IQReadAddr;

	int audioInWrPtr;
	int audioInRdPtr;
	int audioOutWrPtr;
	int audioOutRdPtr;
	char dbgText[16];
	char myCallsign[16];
	bool audioOutStarted;

	// Contest / Cabrillo export settings
	enum ContestExchangeType { EXCH_NONE = 0, EXCH_SERIAL, EXCH_FIELD_DAY, EXCH_STATE_SECTION };
	char contestID[24];
	int  exchangeTemplate;      // ContestExchangeType
	char myExchange[32];        // constant "my" exchange: FD class+section, or QSO-party county/state
	int  nextSerialSent;
	char categoryMode[16];      // SSB / CW / MIXED
	char categoryPower[16];     // QRP / LOW / HIGH
	char operatorName[32];
	char addressLine[48], addressCity[32], addressState[16], addressPostal[16], addressCountry[32];
	char claimedScore[16];      // freeform, filled in by hand before export

};

