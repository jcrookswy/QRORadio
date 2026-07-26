#pragma once
#include "portaudio.h"
#include <complex>
#include <thread>
#include <windows.h>
#include <ipp.h>

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

    // Reference-tone correlator (see AssignCWSlicers): pre-allocated kCWFFTSize-sample complex tone
    // buffer, generated once via ippsTone_32fc when the slot is (re)assigned to a new peak - never
    // regenerated hop-to-hop. Because the tracked frequency is always an exact FFT bin (freqNorm =
    // binIndex/kCWFFTSize), the tone is exactly periodic over kCWFFTSize samples, so a plain
    // kCWHopSize-wide slice starting at toneIndex is phase-correct for every hop; toneIndex just
    // advances by kCWHopSize (wrapping modulo kCWFFTSize, which divides evenly) each time, no
    // per-hop trig evaluation needed. magHistory[0]/[1] are the previous two hops' raw complex
    // correlator outputs (oldest, then middle) - combined 0.5/1.0/0.5 with the newest complex value
    // *before* taking the magnitude, so the boxcar is a complex (phase-coherent) sum, not an average
    // of magnitudes.
    Ipp32fc* tone;
    float freqNorm;
    int toneIndex;
    Ipp32fc magHistory[2];

    // Per-slicer adaptive mark/space threshold (see AssignCWSlicers): rolling history of the last
    // 64 hops' weighted correlator magnitudes, oldest first. Each hop, threshold-high/low are
    // recomputed from this history's own min/max (60/40 and 40/60 split) rather than from the
    // shared FFT noise-floor estimate, so each slicer self-calibrates to its own tone's actual
    // signal range instead of a global cwSquelch/cwHysteresis multiplier.
    float weightedHistory[64];

    // IIR-smoothed histMin (see AssignCWSlicers): histMinAvg = histMin on the first hop after
    // (re)tuning, then 0.9*histMinAvg + 0.1*histMin every hop after - damps hop-to-hop noise in
    // the raw 64-entry-history min before it's used for the mark/space threshold.
    float histMinAvg;
    bool  histMinAvgInit;

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
	int SetRXBits();
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
	static const int kCWPeakMaskRadius    = 10;              // bins excluded around each tracked slicer ("more than 3 bins" away is a valid new peak)
	static constexpr float kCWPeakThresholdDb = 10.0f;      // new peak must exceed the average unmasked-bin magnitude by this many dB
	static const int kCWPeakConfirmBins  = 2;               // a candidate peak must land within this many bins of the previous peak-search hop's candidate before a slicer is launched

	Ipp32f*  cwFFTSampleWindow;   // kCWFFTSize sliding window of wideband pre-filter audio, slid every hop
	Ipp32f*  cwHannWindow;
	Ipp32fc* cwFFTData;
	IppsFFTSpec_C_32fc* pCWFFTSpec;
	Ipp8u*   pCWFFTSpecBuf;
	Ipp8u*   pCWFFTWorkBuf;
	int      cwPeakHopAccum;      // samples accumulated since the last big-FFT peak search; fires FindCWPeak at kCWPeakHopSize
	int      cwPendingPeakBin;    // unconfirmed candidate bin from the previous peak-search hop, or -1; see FindCWPeak

	// Per-slicer reference-tone amplitude (see AssignCWSlicers): just needs to keep the correlator's
	// output in a sane float range (not underflowing/overflowing) - since each slicer's mark/space
	// threshold is now self-derived from its own weightedHistory min/max (see CWSlicer), the
	// correlator's absolute scale no longer has to be calibrated against anything else.
	float cwToneMagn;

	// NOTE: no longer read by the CW mark/space decision (see CWSlicer::weightedHistory /
	// AssignCWSlicers) - each slicer now self-calibrates its own threshold instead of using a
	// shared multiplier against the FFT noise floor. Still user-settable via Radio > CW
	// Squelch/Hysteresis, but currently has no effect; left in place pending a decision on
	// whether to remove that menu or repurpose these.
	float cwSquelch;
	float cwHysteresis;

	CWSlicer cwSlicers[kCWMaxSlicers];

	void CaptureCWSpectrum(Ipp32f* wideband, int n);
	bool FindCWPeak(int* peakBin, float* peakMag);
	void AssignCWSlicers(bool foundPeak, int peakBin, float peakMag, float hopMs, Ipp32f* wideband, int n);
	void RunCWSlicerTiming(CWSlicer& slicer, bool markThisHop, float hopMs);
	void AppendCWSlicerText(CWSlicer& slicer, const char* s);
	void ResolveCWSlicerCharacter(CWSlicer& slicer);
	void DoCWFFTDecode(Ipp32f* wideband, int n);
	void ResetCWDecoder();

	// CW transmit (see CWTXDataLoop in CRadio.cpp): StartCWTransmit stashes the text and switches
	// to CW_TX_MODE; CWTXDataLoop (background data thread) builds a raised-cosine-tapered on/off
	// keyed envelope from it and streams it out. Deliberately bypasses the SSB HPF/AGC/CESSB chain -
	// Q is always 0 (plain carrier on/off keying at LOfreq, no audio-frequency offset needed) - but
	// still runs through the same LO-locked polyphase resampler TXDataLoop uses before BuildTXPacket,
	// so dot timing comes out right in real time regardless of LOfreq.
	float cwTxDotMs;             // TX dot length, ms - hard-coded for now (see CRadio constructor)
	static const int kCWTxTaperSamples = 256; // raised-cosine rise/fall on each mark, at 48 kHz
	static const int kCWTxFlushSamples = 768; // zero-amplitude leader/trailer, at 48 kHz, to flush the pipe
	static constexpr float kCWTxPeakAmplitude = 0.5f; // peak I magnitude (25% power) - revisit later

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

	// CW Peaking: one-shot capture of 16384 (full-bandwidth, pre-CW-filter) RX audio samples,
	// Hann-windowed and FFT'd to find the loudest tone between 300-1100 Hz. Runs across many
	// DoRXDSP hops (256 samples each) before the analysis fires.
	bool  CWPeakCapturing;   // true while samples are still being collected
	bool  CWPeakReady;       // true once CWPeakFreq holds a fresh result for the UI to report
	int   CWPeakSampleCount;
	Ipp32f*  CWPeakBuffer;       // 16384 raw audio samples
	Ipp32f*  CWPeakHannWindow;   // 16384-point Hann window
	Ipp32fc* CWPeakFFTData;      // 16384 complex FFT working buffer
	IppsFFTSpec_C_32fc* pCWPeakFFTSpec;
	Ipp8u* pCWPeakFFTSpecBuf;
	Ipp8u* pCWPeakFFTWorkBuf;
	float CWPeakFreq; // Hz; last CW Peaking result - also intended for a future auto-tune feature

	void StartCWPeakCapture();
	void RunCWPeakAnalysis();

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
	void ProcessCESSB(Ipp32f* pIn, Ipp32fc* pOut);

	IppsFFTSpec_C_32fc* pFFTSpec;
	Ipp8u* pFFTSpecBuf, * pFFTWorkBuf;

	Ipp32fc* IFFTData;// = new Ipp32fc[16000];
	Ipp32fc* IFFTAccum;// = new Ipp32fc[16000];
	Ipp32f* RawAudio;

	// TX audio bandpass filter state (300 Hz HPF + 3 kHz LPF, reset each PTT)
	float txHPF_x1, txHPF_x2, txHPF_y1, txHPF_y2;
	float txLPF_x1, txLPF_x2, txLPF_y1, txLPF_y2;

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


};

