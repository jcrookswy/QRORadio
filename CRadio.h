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
	int SetFreq(float freqMHz);
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

	float LOfreq;
	float stepSize;
	float micGain;
	float agcMaxGain;
	float agcTarget;
	bool NewLOFreq;

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
	bool audioOutStarted;


};

