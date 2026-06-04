#include "CRadio.h"
#include <thread>
#include <cmath>
#include <stddef.h>
#include <ipp.h>
#include <chrono>
#include <iostream>
//#include "frame1.h"

#define ENABLE_CESSB    // Comment out to bypass CESSB (unprocessed SSB, no envelope clipping)

bool gUseDebugWaveform = false;


//void ProcessPlotThread(int id, void* p) {
//    CRadio* pRadio = (CRadio*)p;
//    pRadio->PlotThread();
//    // Code to be executed in the new thread
//}

void ProcessDataThread(int id, void* p) {
    CRadio* pRadio = (CRadio*)p;
    pRadio->DataThread();
    // Code to be executed in the new thread
}

typedef int PaStreamCallback(const void* input,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData);

//paTestData;
/* This routine will be called by the PortAudio engine when audio is needed.
 * It may called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
*/
float DummyBuffer[64];
static int patestCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    if (statusFlags)
        int j = statusFlags;
    // Cast data passed through stream to our structure. 
    CRadio* crp = (CRadio*)userData;
    float* out = (float*)outputBuffer;
    float* in = (float*)inputBuffer;
    unsigned int i;
 //   (void)inputBuffer; /* Prevent unused variable warning. */


    //for (int i = 0; i < 8; i++) memcpy(&out[i * 64], DummyBuffer, framesPerBuffer * 8);
    //return 0;

    float* obuf = &crp->audioOutBuf[crp->audioOutRdPtr];

    if  (crp->audioOutStarted)
    {
        for (int i = 0; i < framesPerBuffer; i++)
        {
            for (int q = 0; q < crp->AudioOutputChannels; q++)  *(out++) = *obuf;
            obuf++;
        }

        //memcpy(outputBuffer, &crp->audioOutBuf[crp->audioOutRdPtr], framesPerBuffer * 8);
        //crp->audioOutRdPtr += framesPerBuffer * 2;
        //if (crp->audioOutRdPtr >= 32768) crp->audioOutRdPtr = 0;
        crp->audioOutRdPtr += framesPerBuffer;
        if (crp->audioOutRdPtr >= 16384) crp->audioOutRdPtr = 0;
    }
    else
    {
        memset(outputBuffer, 0, framesPerBuffer * crp->AudioOutputChannels * 4);

        //for (int i = 0; i < 8; i++) memcpy(&out[i * 64], DummyBuffer, framesPerBuffer * 8);
        //memset(outputBuffer, 0, framesPerBuffer * 8);
    }
    if (crp->myStatus->mode == 1) // USB RX
    {
        int IQSamples = crp->IQWriteAddr - crp->IQReadAddr;
        if (crp->IQWriteAddr < crp->IQReadAddr) IQSamples += 16000;
        if ((!crp->audioOutStarted) && (crp->audioOutWrPtr > 4095)) crp->audioOutStarted = true;
    }
    else // Not in RX mode
    {
        crp->audioOutStarted = false;
        crp->audioOutWrPtr = 0;
        crp->audioOutRdPtr = 0;
    }
    if(crp->AudioInputChannels > 0)
        memcpy(&crp->audioInBuf[crp->audioInWrPtr], inputBuffer, framesPerBuffer * 4);
    crp->audioInWrPtr += framesPerBuffer;
    if (crp->audioInWrPtr >= 16384) crp->audioInWrPtr = 0;
    return 0;
}

#define SAMPLE_RATE (48000)
//#define SAMPLE_RATE (16000)
//static paTestData data;

// CESSB: overlap-save block processing at 48 kHz
// FFT size 2048, block advance 1280, overlap history 768
// Bandpass 300-2695 Hz (bins 13-115 of 2048-pt FFT at 48 kHz)
#define CESSB_BLOCK     1280
#define CESSB_FFT_ORDER 11
#define CESSB_FFT_N     (1 << CESSB_FFT_ORDER)  // 2048
#define CESSB_OVERLAP   (CESSB_FFT_N - CESSB_BLOCK)  // 768
#define CESSB_BIN_LOW   13   // 304.7 Hz
#define CESSB_BIN_HIGH  115  // 2695.3 Hz

PaStream* stream;
PaError err;
void InitStatus(RadioStatus* s)
{
    s->RXFreq = 14.2500;
    s->TunerFreq = 14.2500;
    for (int i = 0; i < 36; i++)
    {
        s->SWRTuned[i] = 1.1;
        s->SWRUntuned[i] = 2.1;
        s->SmithChartTuned[i * 2] = 0.05 + i * 0.001;
        s->SmithChartTuned[i * 2 + 1] = 0.05 + i * 0.001;
        s->SmithChartUntuned[i * 2] = 0.15 - i * 0.002;
        s->SmithChartUntuned[i * 2 + 1] = -0.15 + i * 0.002;
    }
    s->Sunit = 7;
    s->mode = 0;
    s->volts = 23.9;
    s->amps = 0.2;
    sprintf_s(s->GMTTime, "15:35:56 GMT");
    s->UpdateText = true;
    s->UpdateAudio = true;
    s->UpdateVSWR = true;
    s->UpdateRFPlot = true;

    for (int i = 0; i < 16; i++) s->AudioFreqPlot[i] = 0.0;
    for (int i = 0; i < 128; i++) s->AudioTimePlot[i] = 0.0;
    for (int i = 0; i < 256; i++) s->RFFreqPlot[i] = -20.0;

    s->calMode = 0;
 
}

bool CRadio::SaveSettings(const char* path)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"comPort\": %d,\n", comPort);
    fprintf(f, "  \"LOfreq\": %lf,\n", LOfreq);
    fprintf(f, "  \"RelaySettings\": %d,\n", RelaySettings);

    const char*  names[]  = { "SweptOpen", "SweptShort", "SweptLoad", "SweptIQMu" };
    Ipp32fc*     arrays[] = { myVNACal->SweptOpen, myVNACal->SweptShort, myVNACal->SweptLoad, myVNACal->SweptIQMu };
    for (int a = 0; a < 4; a++)
    {
        fprintf(f, "  \"%s\": [\n", names[a]);
        for (int i = 0; i < 36; i++)
            fprintf(f, "    [%f, %f]%s\n", arrays[a][i].re, arrays[a][i].im, i < 35 ? "," : "");
        fprintf(f, "  ]%s\n", a < 3 ? "," : "");
    }

    fprintf(f, "}\n");
    fclose(f);
    return true;
}

bool CRadio::LoadSettings(const char* path)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f) return false;

    Ipp32fc* calArrays[] = { myVNACal->SweptOpen, myVNACal->SweptShort, myVNACal->SweptLoad, myVNACal->SweptIQMu };
    int calSection = -1; // 0=Open 1=Short 2=Load 3=IQMu
    int calIdx = 0;

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        int ival; float fval, re, im; double dval = 0.0;
        if      (sscanf_s(line, " \"comPort\": %d",       &ival) == 1) comPort       = ival;
        else if (sscanf_s(line, " \"LOfreq\": %lf",       &dval) == 1) LOfreq        = dval;
        else if (sscanf_s(line, " \"RelaySettings\": %d", &ival) == 1) RelaySettings = ival;
        else if (strstr(line, "\"SweptOpen\""))        { calSection = 0; calIdx = 0; }
        else if (strstr(line, "\"SweptShort\""))       { calSection = 1; calIdx = 0; }
        else if (strstr(line, "\"SweptLoad\""))        { calSection = 2; calIdx = 0; }
        else if (strstr(line, "\"SweptIQMu\""))        { calSection = 3; calIdx = 0; }
        else if (calSection >= 0 && calIdx < 36 && sscanf_s(line, " [%f , %f]", &re, &im) == 2)
        {
            calArrays[calSection][calIdx].re = re;
            calArrays[calSection][calIdx].im = im;
            calIdx++;
        }
    }
    fclose(f);
    return true;
}

CRadio::CRadio()
{
    ippInit();                      // Initialize Intel� IPP library 
    micGain = 2.5;
    stepSize = 1000.0;
    agcMaxGain = 10.0f;
    agcTarget  = 4.0f;
    RXChannelPower  = 0.0f;
    rxPowerPeakIdx  = 0;
    rxPowerBlockPos = 0;
    rxPowerBlockMax = 0.0f;
    memset(rxPowerPeaks, 0, sizeof(rxPowerPeaks));
    plotSfloor  = 2;
    plotSunits  = 11;
    plotSoffset = 0.0f;

    audioInBuf = new Ipp32f[16384];
    audioOutBuf = new Ipp32f[16384];
    resampledAudioOut = new Ipp32f[8192];
    resampledAudioIn = new Ipp32f[1280];

    audioInWrPtr = 0;
    audioInRdPtr = 0;
    audioOutWrPtr = 0;
    audioOutRdPtr = 0;
    audioOutStarted = false;
 
    memset(audioInBuf, 0, 65536);
    memset(audioOutBuf, 0, 65536);
    connected = false;

    NewLOFreq = false;
    RelaySettings = 0;// 31;
    comPort = 3;

    //Step 1: I/Q data comes in at 46.875 kHz
    RawIQData = ippsMalloc_32fc(16000);
    HannWindow = ippsMalloc_32f(250);
    WindowedData = ippsMalloc_32fc(250);
    DFTData = ippsMalloc_32fc(250);
    TunerData = ippsMalloc_32fc(250);
    IFFTData = ippsMalloc_32fc(256);
    TXFFTData = ippsMalloc_32fc(2048);
    IFFTAccum = ippsMalloc_32fc(384);
    RawAudio = ippsMalloc_32f(256);
    ippsZero_32fc(IFFTAccum, 384);

    MagData = ippsMalloc_32f(250);
    MagAccumData = ippsMalloc_32f(250);
    MagMinAccumData = ippsMalloc_32f(250);
    ippsZero_32f(MagAccumData, 250);
    LogMagData = ippsMalloc_32f(250);
    ClearMagAccum = false;

    TunerPhase = 0.0;
    TunerFreq = 0.0;
    TunerMag = 1.0;

    Ipp8u* pDFTInitBuf;

    // Query to get buffer sizes
    int sizeDFTSpec, sizeDFTInitBuf, sizeDFTWorkBuf;
    ippsDFTGetSize_C_32fc(250, IPP_FFT_NODIV_BY_ANY,
        ippAlgHintAccurate, &sizeDFTSpec, &sizeDFTInitBuf, &sizeDFTWorkBuf);

    // Alloc DFT buffers
    pDFTSpec = (IppsDFTSpec_C_32fc*)ippsMalloc_8u(sizeDFTSpec);
    pDFTInitBuf = ippsMalloc_8u(sizeDFTInitBuf);
    pDFTWorkBuf = ippsMalloc_8u(sizeDFTWorkBuf);

    // Initialize DFT - has gain of 250
    ippsDFTInit_C_32fc(250, IPP_FFT_NODIV_BY_ANY,
        ippAlgHintAccurate, pDFTSpec, pDFTInitBuf);
    if (pDFTInitBuf) ippFree(pDFTInitBuf);
    // *** DFT is good to go ***

    Ipp8u* pFFTInitBuf;

    // Query to get buffer sizes
    int sizeFFTSpec, sizeFFTInitBuf, sizeFFTWorkBuf;
    ippsFFTGetSize_C_32fc(8, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, &sizeFFTSpec, &sizeFFTInitBuf, &sizeFFTWorkBuf);

    // Alloc FFT buffers
    pFFTSpecBuf = ippsMalloc_8u(sizeFFTSpec);
    pFFTInitBuf = ippsMalloc_8u(sizeFFTInitBuf);
    pFFTWorkBuf = ippsMalloc_8u(sizeFFTWorkBuf);

    // Initialize FFT
    ippsFFTInit_C_32fc(&pFFTSpec, 8, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, pFFTSpecBuf, pFFTInitBuf);
    if (pFFTInitBuf) ippFree(pFFTInitBuf);
    // *** FFT is good to go ***

	//Build window
	for (int i = 0; i < 250; i++)
		HannWindow[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 250);

    TXHannWindow = ippsMalloc_32f(2048);
    for (int i = 0; i < 2048; i++)
        TXHannWindow[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 2048);

    RaisedCosUpDown = ippsMalloc_32f(1024);
    for (int i = 0; i < 1024; i++)
        RaisedCosUpDown[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 1024);

    //Initialize polyphase resamplers (I and Q channels, identical parameters)
    int polySize = 0;
    ippsResamplePolyphaseGetSize_32f(128, 32, &polySize, ippAlgHintAccurate);
    resample_state   = (IppsResamplingPolyphase_32f*)ippsMalloc_8u(polySize);
    resample_state_q = (IppsResamplingPolyphase_32f*)ippsMalloc_8u(polySize);
    ippsResamplePolyphaseInit_32f(128, 32, 0.4, 4.0, resample_state,   ippAlgHintAccurate);
    ippsResamplePolyphaseInit_32f(128, 32, 0.4, 4.0, resample_state_q, ippAlgHintAccurate);

    cessbOut           = ippsMalloc_32fc(CESSB_BLOCK);
    cessbI             = ippsMalloc_32f(CESSB_BLOCK);
    cessbQ             = ippsMalloc_32f(CESSB_BLOCK);
    resampledAudioOutQ = ippsMalloc_32f(8192);


	IQWriteAddr = 0; // We want I/Q data in chunks of 250
    IQReadAddr = 0;
    myStatus = new RadioStatus;
    myVNACal = new AntTuneCal;
    for (int k = 0; k < 36; k++)
    {
        myVNACal->SweptOpen[k] = { 1.0, 0.0 };
        myVNACal->SweptShort[k] = { -1.0, 0.0 };
        myVNACal->SweptLoad[k] = { -1.0, 0.0 };
        myVNACal->SweptIQMu[k] = { 0.0, 0.0 };
    }
    m_lastIQMu = { 0.0, 0.0 };
    InitStatus(myStatus);

    // CESSB overlap-save buffers
    cessbRealOverlap  = ippsMalloc_32f(CESSB_OVERLAP);
    cessbCplxOverlap1 = ippsMalloc_32fc(CESSB_OVERLAP);
    cessbCplxOverlap2 = ippsMalloc_32fc(CESSB_OVERLAP);
    cessbCplxOverlap3 = ippsMalloc_32fc(CESSB_OVERLAP);
    cessbWorkBuf      = ippsMalloc_32fc(CESSB_FFT_N);

    // 2048-pt FFT for CESSB (order 11, forward divides by N)
    int sizeCFFTSpec, sizeCFFTInitBuf, sizeCFFTWorkBuf;
    ippsFFTGetSize_C_32fc(CESSB_FFT_ORDER, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, &sizeCFFTSpec, &sizeCFFTInitBuf, &sizeCFFTWorkBuf);
    cessbFFTSpecBuf = ippsMalloc_8u(sizeCFFTSpec);
    Ipp8u* cessbInitBuf = ippsMalloc_8u(sizeCFFTInitBuf);
    cessbFFTWorkBuf = ippsMalloc_8u(sizeCFFTWorkBuf);
    ippsFFTInit_C_32fc(&cessbFFTSpec, CESSB_FFT_ORDER, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, cessbFFTSpecBuf, cessbInitBuf);
    if (cessbInitBuf) ippFree(cessbInitBuf);

    InitCESSB();
}

CRadio::~CRadio()
{
    //To do: delete everything we allocated
    if (connected)
    {
        connected = false;
        myStatus->mode = IDLE_MODE;
        Pa_StopStream(stream);
        connected = false;
        myAThread.join();
        myDThread.join();
    }
    delete[] audioInBuf;
    delete[] audioOutBuf;
    delete myStatus;
}

void CRadio::ProcessIQ(char * data) // Change 40 bytes into 5 complex float, little endian input
{
    int valI, valQ;
    Ipp32fc valC;
    for (int i = 0; i < 40; i++) data[i] -= 0x20;
    for (int i = 0; i < 5; i++)
    {
        valI = *(data++);
        valI |= (*(data++)) << 6;
        valI |= (*(data++)) << 12;
        valI |= (*(data++)) << 18;
        if (valI & 0x00800000) valI += 0xFF000000; // sign bit
        valQ = *(data++);
        valQ |= (*(data++)) << 6;
        valQ |= (*(data++)) << 12;
        valQ |= (*(data++)) << 18;
        if (valQ & 0x00800000) valQ += 0xFF000000; // sign bit
        valC.re = valQ * 0.0000000596046447754; // 24 bit number, 1/2^24
        valC.im = valI * 0.0000000596046447754; // !!! swap
        RawIQData[IQWriteAddr++] = valC;
    }
    if(IQWriteAddr > 15999) IQWriteAddr = 0;//wrap

}

//int TimesThroughDebugGlobal = 0;

void CRadio::DoRXDSP(bool bypassALC) // 2000 bytes = 250 I/Q = 256 audio
{
    static int MinMaxCounter = 0;
//    TimesThroughDebugGlobal++;
    // *** Do next 2 DFT - Inv FFTs
    // Grab data
    for (int i = 0; i < 2; i++)
    {
        ippsCopy_32fc(&RawIQData[IQReadAddr], WindowedData, 125);

        IQReadAddr += 125; //50% overlap. More than necessary but simplifies math with Hann window
        if (IQReadAddr > 15999) IQReadAddr = 0;

        ippsCopy_32fc(&RawIQData[IQReadAddr], &WindowedData[125], 125); //This will be reused next time

        // Tune
        ippsTone_32fc(TunerData, 250, 1.0, TunerFreq, &TunerPhase, ippAlgHintFast);
        ippsMul_32fc_I(TunerData, WindowedData, 250); // Tune in place

        // Window data
        ippsMul_32f32fc_I(HannWindow, WindowedData, 250);

        // DFT
        ippsDFTFwd_CToC_32fc(WindowedData, DFTData, pDFTSpec, pDFTWorkBuf);

        //Mask - Copy only bins in freq range
        ippsZero_32fc(IFFTData, 256);
        for (int i = 1; i < 16; i++) IFFTData[i] = DFTData[i];

        //Inv FFT
        ippsFFTInv_CToC_32fc_I(IFFTData, pFFTSpec, pFFTWorkBuf);
        ippsAdd_32fc_I(IFFTData, &IFFTAccum[i * 128], 256);
        //ippsAdd_32fc_I(&IFFTData[128], &IFFTAccum[i * 128], 128);
        //ippsAdd_32fc_I(IFFTData, &IFFTAccum[i * 128 + 128], 128);
    }   //Repeat once

    //For plotting //
    ippsMagnitude_32fc(DFTData, MagData, 250);

    // Channel power: sum bins 0-16 (0 to +3 kHz at 187.5 Hz/bin), divide by Hann noise BW
    {
        float chanPow = 0.0f;
        for (int k = 0; k <= 16; k++)
        {
            float m = MagData[k] / 125.0f; // normalize for N=250 DFT and Hann coherent gain 0.5
            chanPow += m * m;
        }
        chanPow /= 1.5f;
        if (chanPow > rxPowerBlockMax) rxPowerBlockMax = chanPow;
        if (++rxPowerBlockPos >= 16)
        {
            rxPowerPeaks[rxPowerPeakIdx] = rxPowerBlockMax;
            if (++rxPowerPeakIdx >= 32) rxPowerPeakIdx = 0;
            rxPowerBlockPos = 0;
            rxPowerBlockMax = 0.0f;
            float peak = 0.0f;
            for (int n = 0; n < 32; n++)
                if (rxPowerPeaks[n] > peak) peak = rxPowerPeaks[n];
            RXChannelPower = peak;
        }
    }

    if (ClearMagAccum)
        MinMaxCounter = 0;
    ClearMagAccum = false;

	if (MinMaxCounter & 0x03)
		ippsMinEvery_32f_I(MagData, MagMinAccumData, 250);
	else
	{
		if (MinMaxCounter == 4)
			ippsCopy_32f(MagMinAccumData, MagAccumData, 250); // just copy to max accum
		else
			ippsMaxEvery_32f_I(MagMinAccumData, MagAccumData, 250); // accum max of mins

		ippsCopy_32f(MagData, MagMinAccumData, 250);
	}

    MinMaxCounter++;

	//Generate 256 samples audio
    for (int i = 0; i < 256; i++) RawAudio[i] = IFFTAccum[i].re; // Audio in real portion?

    //ALC
    Ipp32f MaxAudio = 0;
    Ipp32f FabsRawAudio = 0.0;

    //scale audio
    for (int i = 0; i < 256; i++) RawAudio[i] *= TunerMag;

    //find abs peak
    for (int i = 0; i < 256; i++)
    {
        FabsRawAudio = fabsf(RawAudio[i]);
        if (MaxAudio < FabsRawAudio) MaxAudio = FabsRawAudio;
    }
    if ((MaxAudio > 1.0f) || (bypassALC && MaxAudio > 0.001f)) {
        TunerMag /= MaxAudio;
        Ipp32f recip = 1.0f / MaxAudio;
        for (int i = 0; i < 256; i++) RawAudio[i] *= recip;
    }
    else if (MaxAudio < 0.5f) TunerMag *= 1.02f;
    else if (MaxAudio > 0.8f) TunerMag *= 0.99f;
    if (TunerMag > 100.0f) TunerMag = 100.0f;

    //Copy audio to output buf
//    float * audioOutBufPtr = &audioOutBuf[audioOutWrPtr];
//    float * RawAudioPointer = RawAudio;
    memcpy(&audioOutBuf[audioOutWrPtr], RawAudio, 256 * 4);
    audioOutWrPtr += 256;
//    for (int i = 0; i < 256; i++)
//    {
//        audioOutBuf[audioOutWrPtr++] = RawAudio[i];
////        audioOutBuf[audioOutWrPtr++] = RawAudio[i];
////        *(audioOutBufPtr++) = *RawAudio;
////        *(audioOutBufPtr++) = *(RawAudio++);
//    }
//    audioOutWrPtr += 512;
    if (audioOutWrPtr > 16383) audioOutWrPtr = 0;


    //Let's peek at accumulator
//    float peekAt[768];
//    memcpy(peekAt, IFFTAccum, 768 * 4);



    //Adjust accumulator
    ippsCopy_32fc(&IFFTAccum[256], IFFTAccum, 128); // Last 128 IFFT bins move to start for next accum
    ippsZero_32fc(&IFFTAccum[128], 256); // Clear the rest

}
void CRadio::UpdateADCs(char * readData)
{
    //int dbg_val = 0;
    //for (int i = 0; i < 2; i++)
    //{
    //    dbg_val = dbg_val << 6;
    //    dbg_val += (readData[i] - 0x20);
    //}
    //myStatus->volts = dbg_val * 0.1;

 //   if ((dbg_val < 40000) && (dbg_val > 8))
 //       int g = 0;

	int val = ((readData[0] - 0x20) << 6) | (readData[1] - 0x20);
    myStatus->volts =  (36.3 / 4096.0)* val;
	val = ((readData[2] - 0x20) << 6) | (readData[3] - 0x20);
 //   myStatus->amps = (66.0 / 4096.0) * val;
    myStatus->amps = (66.0 / 8192.0)* val;
    myStatus->UpdateText = true;

}

void CRadio::RXDataLoop()
{
    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;
    bool bypassALC = true;
    int ALCCounter = 0;

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR); 

    writeData[0] = 'a'; // ADC payload
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // query adc values once
    ReadFile(hSerial, readData, 4, &bytesWritten, NULL); // Read volt / current
    UpdateADCs(readData); 

    for (int i = 0; i < 8; i++)
        writeData[i] = 'b'; // Let's make 'b' send 250 I/Q, our processing size
   // writeData[8] = 'a';//Get voltage / current
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    bool OKtoProcess = false;
    while (RX_MODE == myStatus->mode) // Continue until mode changes
    {
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); //queue up 8 * 250 IQ values
        //int val = ((readData[0] - 0x20) << 6) | (readData[1] - 0x20);
        //myStatus->volts = (36.3 / 4096.0) * val;
        //val = ((readData[2] - 0x20) << 6) | (readData[3] - 0x20);
        //myStatus->amps = (66.0 / 4096.0) * val;
        //myStatus->UpdateText = true;
        for (int k = 0; k < 4; k++)
        {
            for (int i = 0; i < 50; i++) // 2000 bytes = 250 I/Q
            {
                ReadFile(hSerial, readData, 40, &bytesWritten, NULL);
                if (bytesWritten < 40)
                    int h = 0;
                ProcessIQ(readData);
            }
            if (OKtoProcess) DoRXDSP(bypassALC); // every 250 I/Q
            OKtoProcess = true; // But overlap needs a bit of runway
        }
        if (NewLOFreq)
        {
            SetFreq(LOfreq);
            myStatus->RXFreq = LOfreq;
            NewLOFreq = false;
        }
        if (ALCCounter < 80) ALCCounter++;
        else bypassALC = false;
    }
    //Final xfer
//    ReadFile(hSerial, readData, 4, &bytesWritten, NULL); // Read volt / current
    for (int i = 0; i < 200; i++) // 2000 bytes = 250 I/Q
    {
        ReadFile(hSerial, readData, 40, &bytesWritten, NULL);
        //ProcessIQ(readData);
    }
//    ReadFile(hSerial, readData, 4, &bytesWritten, NULL); // Read volt / current

    writeData[0] = 'i';
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in idle mode

}

void fabsxoveroneplusx(float* inPlaceData)
{
    for (int i = 0; i < 256; i++) inPlaceData[i] = inPlaceData[i] / (1.0 + fabs(inPlaceData[i]));
}

//bool DEBUG_BUF_GENERATED = false;
//Ipp32f* pTwoToneAudio;
void CRadio::Get1280AudioSamples(float gain)
{
    //if (!DEBUG_BUF_GENERATED)
    //{
    //    pTwoToneAudio = ippsMalloc_32f(256);
    //    for (int i = 0; i < 256; i++) pTwoToneAudio[i] = 0.4999 * cos(IPP_2PI * 3 * i / 256) + 0.4999 * cos(IPP_2PI * 8 * i / 256);
    //    //for (int i = 0; i < 256; i++) pTwoToneAudio[i] = 0.4999 * cos(IPP_2PI * 5 * i / 256) + 0.4999 * cos(IPP_2PI * 6 * i / 256);
    //    //for (int i = 0; i < 256; i++) pTwoToneAudio[i] = 0.9999 * sin(IPP_2PI * 8 * i / 256) ;
    //    DEBUG_BUF_GENERATED = true;
    //}

    if (gUseDebugWaveform)
    {
        // Two-tone debug signal: 0.017 + 0.027, each at 0.5 amplitude.
        // Each tone is split into 1024 + 256 calls so the persistent phases
        // advance by 1024 samples (matching the audioInRdPtr stride), not 1280.
        Ipp32f toneBuf[1280];

        // Tone 1 (0.017) into resampledAudioIn
        ippsTone_32f(resampledAudioIn,       1024, 0.5f, 0.017f, &debugTonePhase,  ippAlgHintFast);
        Ipp32f tmp1 = debugTonePhase;
        ippsTone_32f(resampledAudioIn + 1024, 256, 0.5f, 0.017f, &tmp1,            ippAlgHintFast);

        // Tone 2 (0.027) into temp buffer, then add to resampledAudioIn
        ippsTone_32f(toneBuf,       1024, 0.5f, 0.027f, &debugTonePhase2, ippAlgHintFast);
        Ipp32f tmp2 = debugTonePhase2;
        ippsTone_32f(toneBuf + 1024, 256, 0.5f, 0.027f, &tmp2,           ippAlgHintFast);

        ippsAdd_32f_I(toneBuf, resampledAudioIn, 1280);
    }
    else
    {
        int tempReadPointer = audioInRdPtr;
        for (int i = 0; i < 1280; i += 256)
        {
            //ippsCopy_32f(&audioInBuf[tempReadPointer], &resampledAudioIn[i], 256);
            ippsMulC_32f(&audioInBuf[tempReadPointer], gain, &resampledAudioIn[i], 256); // Apply gain pre compression
            tempReadPointer = (tempReadPointer + 256) & 16383; // wrap
        }
    }
}
void ConjAndFilter(Ipp32fc* pData) // 2048 sample FFT result
{
    //Zero out LF samples, raised cosine taper in and out
    for (int i = 0; i < 8; i++) pData[i] = { 0.0, 0.0 };
    pData[8].re *= 0.2929;
    pData[8].im *= 0.2929;
    pData[10].re *= 1.7071;
    pData[10].im *= 1.7071;

    pData[111].re *= 1.7071;
    pData[111].im *= 1.7071;
    pData[113].re *= 0.2929;
    pData[113].im *= 0.2929;

    //Filter
    for (int i = 11; i < 111; i++) // Double because we throw away sideband, except 1st bin -6 dB
    {
        pData[i].re *= 2.0;
        pData[i].im *= 2.0;
    }
    for (int i = 114; i < 2048; i++) pData[i] = { 0.0, 0.0 };
 
}

// Bandpass for an already-analytic (CESSB) complex signal — same bin structure as
// ConjAndFilter but without the x2 (signal is already one-sided; edge tapers halved).
static void BandpassOneSided(Ipp32fc* pData)
{
    for (int i = 0; i < 8; i++) pData[i] = { 0.0f, 0.0f };
    pData[8].re  *= 0.1464f; pData[8].im  *= 0.1464f;
    // bin 9: pass through (mirrors ConjAndFilter's untouched bin)
    pData[10].re *= 0.8536f; pData[10].im *= 0.8536f;
    // bins 11-110: pass through (x1.0 — ConjAndFilter doubled these)
    pData[111].re *= 0.8536f; pData[111].im *= 0.8536f;
    // bin 112: pass through
    pData[113].re *= 0.1464f; pData[113].im *= 0.1464f;
    for (int i = 114; i < 2048; i++) pData[i] = { 0.0f, 0.0f };
}

int debug_xyz = 0;
int g_min_amp = 32;
int g_max_amp = 240;
int g_abs_max_amp = 276;

#define DBG_MAX_AMP 250 // 70 seems to be 10% output 250 = 166W. 280 = 207W
//#define DBG_MAX_AMP 210 // 70 seems to be 10% output
#define DBG_MIN_AMP 40 // off below this setting
//#define DBG_MAX_AMP 30 // 30 is barely on
//#define DBG_MIN_AMP 25 // 28 is barely off
void BuildTXPacket(char* wd, Ipp32fc* pIQ)
{
    static int lastPhase = 0;
    static float remainder = 0.0;
    Ipp32f ampl[32];
    Ipp32f phase[32];
    ippsMagnitude_32fc(pIQ, ampl, 32);
    ippsPhase_32fc(pIQ, phase, 32);

    *(wd++) = 'd';
    int iPhase[32];
    int delta;
    int amp;
    for (int i = 0; i < 32; i++)
    {
        iPhase[i] = floor(1024.0 * phase[i] / IPP_2PI + 0.5); //round
        if (i == 0) delta = iPhase[0] - lastPhase;
        else delta = iPhase[i] - iPhase[i - 1];
        if (delta < 0) delta += 1024;
        if (delta > 1023) delta -= 1024; //Possible

        amp = floor(ampl[i] * g_max_amp + remainder + 0.5);
        remainder += ampl[i] * g_max_amp - amp; // Carry remainder to approximate extra bits

        if (amp > g_abs_max_amp) amp = g_abs_max_amp; // max is 1.0, abs max is just below modulator clipping
        if (amp < 0) amp = 0;
        amp += g_min_amp; // this is "barely off"

        *(wd++) = 0x20 + (delta >> 6);
        *(wd++) = 0x20 + (delta & 0x3F);
        *(wd++) = 0x20 + (amp >> 6);
        *(wd++) = 0x20 + (amp & 0x3F);
    }
    lastPhase = iPhase[31];
}
void CRadio::BuildGainRamp(float * ramp, float GainPA, float GainAB, float GainBC)
{
    //First half ramps from PA to AB
    float gain1 = 1.0 / GainPA;
    float gain2 = 1.0 / GainAB;
    float gain3 = 1.0 / GainBC;
    if (GainPA < GainAB) // lower to higher Pre-A should have ramped atten up already. Hold the higher atten
    {
        for (int i = 0; i < 512; i++) 
            ramp[i] = gain2;
    }
    else // higher sig to lower. ramp down to lower atten
    {
        for (int i = 0; i < 512; i++)
            ramp[i] = gain1 - (gain1 - gain2) * RaisedCosUpDown[i];
    }

    if (GainAB > GainBC) // higher to lower. Hold the higher atten
    {
        for (int i = 512; i < 1024; i++) 
            ramp[i] = gain2;
    }
    else // lower sig to higher, 2nd half. ramp up to higher atten
    {
        for (int i = 512; i < 1024; i++)
            ramp[i] = gain3 - (gain3 - gain2) * RaisedCosUpDown[i];
    }


}

// Zero DC, sub-bass, and all negative-frequency bins; double the voice passband
// to form the one-sided analytic spectrum (Hilbert transform + bandpass in one step).
// Raised-cosine taper weights for 4-step ramp (k=1,2,3 of N=4): 0.1464, 0.5, 0.8536
// Applied to 3 bins just outside the pass edges; ×2 here because this is the Hilbert stage.
static void ApplyAnalyticBandpass(Ipp32fc* pFFT)
{
    ippsZero_32fc(pFFT, CESSB_BIN_LOW - 3);          // bins 0-9: zero
    pFFT[CESSB_BIN_LOW-3].re *= 0.2929f; pFFT[CESSB_BIN_LOW-3].im *= 0.2929f; // bin 10
    pFFT[CESSB_BIN_LOW-2].re *= 1.0000f; pFFT[CESSB_BIN_LOW-2].im *= 1.0000f; // bin 11
    pFFT[CESSB_BIN_LOW-1].re *= 1.7071f; pFFT[CESSB_BIN_LOW-1].im *= 1.7071f; // bin 12
    for (int i = CESSB_BIN_LOW; i <= CESSB_BIN_HIGH; i++)
        { pFFT[i].re *= 2.0f; pFFT[i].im *= 2.0f; }
    pFFT[CESSB_BIN_HIGH+1].re *= 1.7071f; pFFT[CESSB_BIN_HIGH+1].im *= 1.7071f; // bin 116
    pFFT[CESSB_BIN_HIGH+2].re *= 1.0000f; pFFT[CESSB_BIN_HIGH+2].im *= 1.0000f; // bin 117
    pFFT[CESSB_BIN_HIGH+3].re *= 0.2929f; pFFT[CESSB_BIN_HIGH+3].im *= 0.2929f; // bin 118
    ippsZero_32fc(pFFT + CESSB_BIN_HIGH + 4, CESSB_FFT_N - CESSB_BIN_HIGH - 4); // bins 119+
}

// Bandpass-only filter for an already-analytic (one-sided) complex signal.
// Zeros everything outside the voice passband without doubling.
static void ApplyComplexBandpass(Ipp32fc* pFFT)
{
    ippsZero_32fc(pFFT, CESSB_BIN_LOW - 3);          // bins 0-9: zero
    pFFT[CESSB_BIN_LOW-3].re *= 0.1464f; pFFT[CESSB_BIN_LOW-3].im *= 0.1464f; // bin 10
    pFFT[CESSB_BIN_LOW-2].re *= 0.5000f; pFFT[CESSB_BIN_LOW-2].im *= 0.5000f; // bin 11
    pFFT[CESSB_BIN_LOW-1].re *= 0.8536f; pFFT[CESSB_BIN_LOW-1].im *= 0.8536f; // bin 12
    // bins 13-115: pass through
    pFFT[CESSB_BIN_HIGH+1].re *= 0.8536f; pFFT[CESSB_BIN_HIGH+1].im *= 0.8536f; // bin 116
    pFFT[CESSB_BIN_HIGH+2].re *= 0.5000f; pFFT[CESSB_BIN_HIGH+2].im *= 0.5000f; // bin 117
    pFFT[CESSB_BIN_HIGH+3].re *= 0.1464f; pFFT[CESSB_BIN_HIGH+3].im *= 0.1464f; // bin 118
    ippsZero_32fc(pFFT + CESSB_BIN_HIGH + 4, CESSB_FFT_N - CESSB_BIN_HIGH - 4); // bins 119+
}

// Hard-limit the envelope of a complex array to 1.0, preserving phase.
static void ClipEnvelope(Ipp32fc* pData, int n)
{
    for (int i = 0; i < n; i++) {
        float r = pData[i].re, q = pData[i].im;
        float mag2 = r * r + q * q;
        if (mag2 > 1.0f) {
            float inv = 1.0f / sqrtf(mag2);
            pData[i].re = r * inv;
            pData[i].im = q * inv;
        }
    }
}

// Zero all CESSB overlap history; call once before transmitting a new audio stream.
void CRadio::InitCESSB()
{
    ippsZero_32f(cessbRealOverlap, CESSB_OVERLAP);
    ippsZero_32fc(cessbCplxOverlap1, CESSB_OVERLAP);
    ippsZero_32fc(cessbCplxOverlap2, CESSB_OVERLAP);
    ippsZero_32fc(cessbCplxOverlap3, CESSB_OVERLAP);
    txHPF_x1 = txHPF_x2 = txHPF_y1 = txHPF_y2 = 0.0f;
    txLPF_x1 = txLPF_x2 = txLPF_y1 = txLPF_y2 = 0.0f;
    debugTonePhase  = 0.0f;
    debugTonePhase2 = 0.0f;
}

// Process 1280 real audio samples into 1280 complex analytic SSB samples (CESSB).
// pIn  : 1280 real audio samples at 48 kHz (normalized, peak <= 1.0 recommended)
// pOut : 1280 Ipp32fc analytic SSB baseband signal; Re(pOut) is the SSB audio
void CRadio::ProcessCESSB(Ipp32f* pIn, Ipp32fc* pOut)
{
    // --- Stage 1: real audio -> bandpass-filtered analytic signal via Hilbert ---
    // Build 2048-sample complex block from [768 real history | 1280 new samples]
    for (int i = 0; i < CESSB_OVERLAP; i++)
        cessbWorkBuf[i] = { cessbRealOverlap[i], 0.0f };
    for (int i = 0; i < CESSB_BLOCK; i++)
        cessbWorkBuf[CESSB_OVERLAP + i] = { pIn[i], 0.0f };
    // Save last 768 input samples as history for the next call
    ippsCopy_32f(pIn + CESSB_BLOCK - CESSB_OVERLAP, cessbRealOverlap, CESSB_OVERLAP);

    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyAnalyticBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    // Discard the first 768 overlap samples; the last 1280 are the valid output
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pOut, CESSB_BLOCK);

    // --- Iteration 1: clip envelope, re-apply bandpass to remove clipping artifacts ---
    ClipEnvelope(pOut, CESSB_BLOCK);
    ippsCopy_32fc(cessbCplxOverlap1, cessbWorkBuf, CESSB_OVERLAP);
    ippsCopy_32fc(pOut, cessbWorkBuf + CESSB_OVERLAP, CESSB_BLOCK);
    ippsCopy_32fc(pOut + CESSB_BLOCK - CESSB_OVERLAP, cessbCplxOverlap1, CESSB_OVERLAP);
    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyComplexBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pOut, CESSB_BLOCK);

    // --- Iteration 2: second clip + bandpass for further PAPR reduction ---
    ClipEnvelope(pOut, CESSB_BLOCK);
    ippsCopy_32fc(cessbCplxOverlap2, cessbWorkBuf, CESSB_OVERLAP);
    ippsCopy_32fc(pOut, cessbWorkBuf + CESSB_OVERLAP, CESSB_BLOCK);
    ippsCopy_32fc(pOut + CESSB_BLOCK - CESSB_OVERLAP, cessbCplxOverlap2, CESSB_OVERLAP);
    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyComplexBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pOut, CESSB_BLOCK);

    // --- Iteration 3: third clip + bandpass to catch residual overshoot ---
    ClipEnvelope(pOut, CESSB_BLOCK);
    ippsCopy_32fc(cessbCplxOverlap3, cessbWorkBuf, CESSB_OVERLAP);
    ippsCopy_32fc(pOut, cessbWorkBuf + CESSB_OVERLAP, CESSB_BLOCK);
    ippsCopy_32fc(pOut + CESSB_BLOCK - CESSB_OVERLAP, cessbCplxOverlap3, CESSB_OVERLAP);
    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyComplexBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pOut, CESSB_BLOCK);
}

DWORD GetBytesAvailable(HANDLE hComm) {
    COMSTAT comStat;
    DWORD dwErrors;
    if (ClearCommError(hComm, &dwErrors, &comStat)) {
        return comStat.cbInQue; // Returns bytes in input buffer
    }
    return 0;
}


void CRadio::TXDataLoop()
{
    char writeData[132]; // 1 header + 32 IQ pairs × 4 bytes = 129 bytes
    char readData[64];
    DWORD bytesWritten = 0;
    float agcGain           = 1.0f;
    const float agcMinGain  = 1.0f;
    const float agcAttack   = 0.41f;    // per-block attack  (~50 ms at 1280/48k blocks)
    const float agcRelease  = 0.065f;   // per-block release (~400 ms)
    int txPacketCount = 0;

    Ipp32fc * TXIFFTAccum = ippsMalloc_32fc(3072); 

    // Query to get buffer sizes
    int sizeTFFTSpec, sizeTFFTInitBuf, sizeTFFTWorkBuf;
    ippsFFTGetSize_C_32fc(11, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, &sizeTFFTSpec, &sizeTFFTInitBuf, &sizeTFFTWorkBuf);

    // Alloc FFT buffers
    Ipp8u * pTFFTSpecBuf = ippsMalloc_8u(sizeTFFTSpec);
    Ipp8u* pTFFTInitBuf = ippsMalloc_8u(sizeTFFTInitBuf);
    Ipp8u* pTFFTWorkBuf = ippsMalloc_8u(sizeTFFTWorkBuf);
    IppsFFTSpec_C_32fc* pTFFTSpec;

    // Initialize FFT
    ippsFFTInit_C_32fc(&pTFFTSpec, 11, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, pTFFTSpecBuf, pTFFTInitBuf);
    if (pTFFTInitBuf) ippFree(pTFFTInitBuf);
    // *** FFT is good to go ***


    int ADCWaitingCounter = 0;

    Ipp64f time = 80.0; // Initially how many samples extra for resampler
    Ipp64f factor = LOfreq * 1000.0 / (48.0 * 256); // Resample to the LO frequency / 256
    int ResampleOutCount = 0;

    ippsZero_32fc(TXIFFTAccum, 3072); //Re-use IFFT accumulator. This will also be used to raised-cosine taper rising / falling edge of audio

    audioInRdPtr = audioInWrPtr; // Purge old audio
    audioInRdPtr = audioInRdPtr & 0xFF00; // round down
    if (audioInRdPtr >= 16384) audioInRdPtr = 0;

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    InitCESSB(); // Clear overlap history so each transmission starts clean

    writeData[0] = 't'; // Transmit mode
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    Sleep(32);//Wait for command / relays

    while (TX_MODE == myStatus->mode) // Continue until mode changes
    {
        //Is there enough audio data to upsample? If so, upsample.
        int audioSamplecount = audioInWrPtr - audioInRdPtr;
        if (audioSamplecount < 0) audioSamplecount += 16384;

        if (audioSamplecount >= 1280)
        {
            Get1280AudioSamples(agcGain);
            audioInRdPtr += 1024;
            if (audioInRdPtr >= 16384) audioInRdPtr -= 16384;

            // Bandpass filter: 2nd-order Butterworth HPF (300 Hz) cascaded with LPF (3 kHz).
            // Applied in-place so both AGC and CESSB see only in-band audio.
            // Coefficients computed via bilinear transform at 48 kHz.
            static const float hb0= 0.97256f, hb1=-1.94512f, hb2= 0.97256f; // HPF 300 Hz
            static const float ha1=-1.94436f, ha2= 0.94583f;
            static const float lb0= 0.02996f, lb1= 0.05991f, lb2= 0.02996f; // LPF 3 kHz
            static const float la1=-1.45427f, la2= 0.57413f;
            for (int i = 0; i < CESSB_BLOCK; i++) {
                float xh = resampledAudioIn[i];
                float yh = hb0*xh + hb1*txHPF_x1 + hb2*txHPF_x2 - ha1*txHPF_y1 - ha2*txHPF_y2;
                txHPF_x2 = txHPF_x1; txHPF_x1 = xh;
                txHPF_y2 = txHPF_y1; txHPF_y1 = yh;
                float yl = lb0*yh + lb1*txLPF_x1 + lb2*txLPF_x2 - la1*txLPF_y1 - la2*txLPF_y2;
                txLPF_x2 = txLPF_x1; txLPF_x1 = yh;
                txLPF_y2 = txLPF_y1; txLPF_y1 = yl;
                resampledAudioIn[i] = yl;
            }

            // AGC: measure peak of gained block, update agcGain for next block
            Ipp32f peakLevel;
            ippsMaxAbs_32f(resampledAudioIn, 1280, &peakLevel);
            if (peakLevel > 0.001f)
            {
                float desired = agcGain * agcTarget / peakLevel;
                desired = fmaxf(agcMinGain, fminf(agcMaxGain, desired));
                float coeff = (desired < agcGain) ? agcAttack : agcRelease;
                agcGain += coeff * (desired - agcGain);
            }

            // CESSB: real audio -> complex analytic SSB, then resample I and Q separately
#ifdef ENABLE_CESSB
            ProcessCESSB(resampledAudioIn, cessbOut);
            for (int i = 0; i < CESSB_BLOCK; i++)
                { cessbI[i] = cessbOut[i].re; cessbQ[i] = cessbOut[i].im; }
#else
            ippsCopy_32f(resampledAudioIn, cessbI, CESSB_BLOCK);
            ippsZero_32f(cessbQ, CESSB_BLOCK);
#endif

            int newSampleCount = 0, newSampleCountQ = 0;
            Ipp64f timeQ = time;
            ippsResamplePolyphase_32f(cessbI, 1024, &resampledAudioOut[ResampleOutCount],
                factor, 1.0, &time, &newSampleCount, resample_state);
            ippsResamplePolyphase_32f(cessbQ, 1024, &resampledAudioOutQ[ResampleOutCount],
                factor, 1.0, &timeQ, &newSampleCountQ, resample_state_q);
            ResampleOutCount += newSampleCount;
            time -= 1024;

            // window-overlap-FFT on complex resampled CESSB signal
            int sendSampleCount = 0;
            while ((sendSampleCount + 2047) < ResampleOutCount)
            {
                Ipp32f* pI = &resampledAudioOut[sendSampleCount];
                Ipp32f* pQ = &resampledAudioOutQ[sendSampleCount];
                for (int s = 0; s < 2048; s++) TXFFTData[s] = { pI[s], pQ[s] };
                //window
                ippsMul_32f32fc_I(TXHannWindow, TXFFTData, 2048);
                //FFT
                ippsFFTFwd_CToC_32fc_I(TXFFTData, pTFFTSpec, pTFFTWorkBuf);
                //Bandpass only — signal is already one-sided analytic from CESSB
                BandpassOneSided(TXFFTData);
                //Back to time domain
                ippsFFTInv_CToC_32fc_I(TXFFTData, pTFFTSpec, pTFFTWorkBuf);

                //Now we have 3 IFFTs ready to overlap-add
                // When we add them, we will likely exceed 1.0 meaning we'll have to scale both of them back.
                // Once we know how much to scale back, we have enough information to send the last one. 
                // IFFTAccumRe is 768. This stores the two previous IFFTs. 0-255 working buffer

                //We need to save full previous IFFT (B), and newest half of IFFT before that (A).
                //Essentially this is the 3rd IFFT (C), used to compute the gain of the 2nd. The 1st was scaled last time.
                //The 2nd will be scaled this time. Then the new 1/2 of 1st and old 1/2 of 2nd make IQ
                //memcpy(DebugStuff, TXIFFTAccum, 3072 * 8);
                //nope

                ippsAdd_32fc_I(&TXIFFTAccum[1024], TXIFFTAccum, 1024);  // A + B => output
                if (txPacketCount == 0)
                    ippsMul_32f32fc_I(TXHannWindow, TXIFFTAccum, 1024); // rising taper on first packet

                for (int s = 0; s < 1024; s += 32)
                {
                    BuildTXPacket(writeData, &TXIFFTAccum[s]);
                    WriteFile(hSerial, writeData, 129, &bytesWritten, NULL);
                }
                ippsCopy_32fc(&TXIFFTAccum[2048], TXIFFTAccum, 1024);
                ippsCopy_32fc(TXFFTData, &TXIFFTAccum[1024], 2048);
                txPacketCount++;

                sendSampleCount += 1024;

            } //((sendSampleCount + 255) < ResampleOutCount)

            //Shift remaining resampled audio samples to front of buffer
            ippsCopy_32f(&resampledAudioOut[sendSampleCount],  resampledAudioOut,  ResampleOutCount - sendSampleCount);
            ippsCopy_32f(&resampledAudioOutQ[sendSampleCount], resampledAudioOutQ, ResampleOutCount - sendSampleCount);
            ResampleOutCount -= sendSampleCount;

            myStatus->UpdateText = true;
            for (int s = 64; s < 192; s++) myStatus->AudioTimePlot[s - 64] = TXFFTData[s].re;
            //memcpy(myStatus->AudioTimePlot, audioInBuf, 128 * 4);
            myStatus->UpdateAudio = true;

            //Send ADC request, increment adc waiting counter
			//if (ADCReadInterval == 0)
			//{
			//	ADCReadInterval = 16;
			//	writeData[0] = 'a'; // Read back ADC
			//	WriteFile(hSerial, writeData, 1, &bytesWritten, NULL);
			//	ADCWaitingCounter++;
			//	//myStatus->volts = audioInRdPtr * 0.001;//DEBUG

			//}
			//else ADCReadInterval--;

			//if (ADCWaitingCounter > 16) // Read and process ADC values
			//{
			//	ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
			//	UpdateADCs(readData);
			//	ADCWaitingCounter--;
			//}

        } // (audioSamplecount >= 1280)
        else Sleep(0);

    } // while (TX_MODE == myStatus->mode)
 //   fclose(f);

    // Final packet: overlap-add remaining tail, apply falling taper, send
    ippsAdd_32fc_I(&TXIFFTAccum[1024], TXIFFTAccum, 1024);
    ippsMul_32f32fc_I(&TXHannWindow[1024], TXIFFTAccum, 1024);
    for (int s = 0; s < 1024; s += 32)
    {
        BuildTXPacket(writeData, &TXIFFTAccum[s]);
        WriteFile(hSerial, writeData, 129, &bytesWritten, NULL);
    }

    //Dump residual ADC data if any
    //Sleep(16);
    //if (ADCWaitingCounter)
    //{
    //    ReadFile(hSerial, readData, 4 * ADCWaitingCounter, &bytesWritten, NULL);
    //}

    //Put back in receive mode

    ippsFree(TXIFFTAccum);
    ippsFree(pTFFTSpecBuf);
    ippsFree(pTFFTWorkBuf);
 //   ippsFree(pTFFTSpec);

    writeData[0] = 'i';
    writeData[1] = 'r';
    WriteFile(hSerial, writeData, 2, &bytesWritten, NULL); 
    myStatus->mode = RX_MODE;
}

Ipp32fc CRadio::GetCorrectedS11(int index, Ipp32fc S11Raw)
{
    // 1-port OSL error model: S11_raw = e00 + (e10e01 * S11) / (1 - e11 * S11)
    // Ideal standards: Open = +1, Short = -1, Load = 0  =>  e00 = raw load measurement

    Ipp32fc e00 = myVNACal->SweptLoad[index];

    // a = RawOpen - e00,  b = RawShort - e00
    Ipp32fc a = { myVNACal->SweptOpen[index].re  - e00.re, myVNACal->SweptOpen[index].im  - e00.im };
    Ipp32fc b = { myVNACal->SweptShort[index].re - e00.re, myVNACal->SweptShort[index].im - e00.im };

    // e11 = (a + b) / (a - b)
    Ipp32fc apb = { a.re + b.re, a.im + b.im };
    Ipp32fc amb = { a.re - b.re, a.im - b.im };
    float ambMag2 = amb.re * amb.re + amb.im * amb.im;
    if (ambMag2 == 0.0f) return S11Raw;
    Ipp32fc e11 = { (apb.re * amb.re + apb.im * amb.im) / ambMag2,
                    (apb.im * amb.re - apb.re * amb.im) / ambMag2 };

    // e10e01 = -2*a*b / (a - b)
    Ipp32fc ab = { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re };
    Ipp32fc e10e01 = { (-2.0f * ab.re * amb.re + -2.0f * ab.im * amb.im) / ambMag2,
                       (-2.0f * ab.im * amb.re - -2.0f * ab.re * amb.im) / ambMag2 };

    // S11_corrected = M / (e10e01 + e11 * M),  where M = S11Raw - e00
    Ipp32fc M = { S11Raw.re - e00.re, S11Raw.im - e00.im };
    Ipp32fc e11M = { e11.re * M.re - e11.im * M.im, e11.re * M.im + e11.im * M.re };
    Ipp32fc den = { e10e01.re + e11M.re, e10e01.im + e11M.im };
    float denMag2 = den.re * den.re + den.im * den.im;
    if (denMag2 == 0.0f) return S11Raw;

    Ipp32fc CorrectedS11 = { (M.re * den.re + M.im * den.im) / denMag2,
                              (M.im * den.re - M.re * den.im) / denMag2 };
    return CorrectedS11;
}

Ipp32fc CRadio::GetS11(Ipp32f* H2Window)
{
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;

    writeData[0] = 'v'; // VNA mode. Auto switches to REV pwr
    writeData[1] = 'x'; // read FWD Each one is 125 I/Q pairs
    FlushFileBuffers(hSerial);
    Sleep(50);
    WriteFile(hSerial, writeData, 2, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    IQWriteAddr = 0;
    for (int i = 0; i < 100; i++) // 4000 bytes = 500 I/Q
    {
        ReadFile(hSerial, readData, 40, &bytesWritten, NULL);
        if (bytesWritten < 40)
            int h = 0;
        ProcessIQ(readData);
    }

    // Tune. Tone is offset by (LOfreq / 16384 Hz) / 46875.0 Hz
    Ipp32f tuneFreq = (Ipp32f)(1.0 - LOfreq * 1.0e6 / (16384.0 * 46875.0));

    // Copy pre-tuning FWD data for image bin computation (FWD occupies indices 64..223)
    Ipp32fc imageFwdBuf[224];
    ippsCopy_32fc(RawIQData, imageFwdBuf, 224);

    TunerPhase = 0.0;
    ippsTone_32fc(TunerData, 250, 1.0, tuneFreq, &TunerPhase, ippAlgHintFast);
    ippsMul_32fc_I(TunerData, RawIQData, 250); // Tune in place
    ippsTone_32fc(TunerData, 250, 1.0, tuneFreq, &TunerPhase, ippAlgHintFast);
    ippsMul_32fc_I(TunerData, &RawIQData[250], 250); // Tune in place

    Ipp32fc avgFwd, avgRev, S11;
    ippsMul_32f32fc_I(H2Window, &RawIQData[64], 160);
    ippsMul_32f32fc_I(H2Window, &RawIQData[314], 160); // Window because of DC feedthru
    ippsSum_32fc(&RawIQData[64], 160, &avgFwd, ippAlgHintAccurate);
    ippsSum_32fc(&RawIQData[314], 160, &avgRev, ippAlgHintAccurate);
    ippsDiv_32fc_A21(&avgRev, &avgFwd, &S11, 1); // S11 = Rev / FWD

    // Image bin: apply conjugate tone to pre-tuning FWD data, window, sum.
    // Phase is aligned to sample index 64 (start of FWD window in RawIQData).
    Ipp32f imagePhase = 0.0f;
    ippsTone_32fc(TunerData, 250, 1.0f, 1.0f-tuneFreq, &imagePhase, ippAlgHintFast);
    ippsMul_32fc_I(TunerData, &imageFwdBuf[64], 160);
    ippsMul_32f32fc_I(H2Window, &imageFwdBuf[64], 160);
    Ipp32fc imageSum;
    ippsSum_32fc(&imageFwdBuf[64], 160, &imageSum, ippAlgHintAccurate);
    ippsDiv_32fc_A21(&imageSum, &avgFwd, &m_lastIQMu, 1); // μ = image / signal

    return S11;

}

void CRadio::AntTuneSweepOSL(int osl)
{
    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;
    double lastLO = LOfreq;

    Ipp32f H2Window[160];
    for (int i = 0; i < 160; i++)
        H2Window[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 160);

    IQWriteAddr = 0;

    writeData[0] = 'w'; // prep vna
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    Sleep(16);

    writeData[0] = 'c';
    writeData[1] = 0x20; // Bypass
    writeData[2] = 0x20 + 0x3B; // FWD power
    WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    Sleep(16);
    double thisFreq;
    float minRMAG = 1.0;

    for (int ifrq = 0; ifrq < 36; ifrq++)//36 @ bypass 10k steps
    {
        thisFreq = 14.0 + ifrq * 0.01;
        SetFreq(thisFreq); //10 kHz steps

        writeData[0] = 'w'; // idle vna mode
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
        Sleep(16);
        ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
        if (readData[0] != 'R')
            int g = 0;

        Ipp32fc S11 = GetS11(H2Window);
        float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);

		myStatus->SmithChartUntuned[ifrq * 2] = S11.re;
		myStatus->SmithChartUntuned[ifrq * 2 + 1] = S11.im;
		myStatus->SWRUntuned[ifrq] = (1.0 + Rmag) / (1.0 - Rmag);
        myStatus->UpdateVSWR = true;
    }

    if (osl == 0) memcpy(&myVNACal->SweptOpen[0], &myStatus->SmithChartUntuned[0], 72 * 4);
    if (osl == 1) memcpy(&myVNACal->SweptShort[0], &myStatus->SmithChartUntuned[0], 72 * 4);
    if (osl == 2) memcpy(&myVNACal->SweptLoad[0], &myStatus->SmithChartUntuned[0], 72 * 4);

    SetFreq(lastLO);

    SetRXBits();

    Sleep(16);
    writeData[0] = 'r';
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in RX mode
    myStatus->mode = 1;

}

void CRadio::AntTuneDataLoop()
{
    if (myStatus->calMode < 3)
    {
        AntTuneSweepOSL(myStatus->calMode);
        return;
    }
    if (myStatus->calMode == 4)
    {
        SweepAntTune();
        return;
    }

    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;
//    int RelaySettings = 31; // 31 is bypass
    Ipp32f H2Window[160];
    for (int i = 0; i < 160; i++)
        H2Window[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 160);

    IQWriteAddr = 0;
    double lastLO = LOfreq;
    int bestRelaySetting = 0;

	writeData[0] = 'w'; // prep vna
	WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
	Sleep(16);
    SetFreq(14.25); //10 kHz steps

    writeData[0] = 'c';
    writeData[1] = 0x20 ; // Bypass
    writeData[2] = 0x20 + 0x3B; // FWD power
    WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    Sleep(16);
    double thisFreq;
    float minRMAG = 1.0;

    for (int ifrq = 0; ifrq < 104; ifrq++)//36 @ bypass, 32@14.25 GHz across settings, 36@best setting
    {
        int calIndex = ifrq;
        if (ifrq > 35) calIndex = 25;
        if (ifrq < 36) thisFreq = 14.0 + ifrq * 0.01;
        else if (ifrq < 68) thisFreq = 14.25;
        else thisFreq = 14.0 + (ifrq - 68) * 0.01;
        SetFreq(thisFreq); //10 kHz steps

        writeData[0] = 'w'; // idle vna mode
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
        Sleep(16);
        ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
        if (readData[0] != 'R')
            int g = 0;

        if ((ifrq > 36) && (ifrq < 69)) // All indices where we change relay states
        {
            RelaySettings = (ifrq == 68) ? bestRelaySetting : ifrq - 36;
			writeData[0] = 'c';
			writeData[1] = 0x20 + RelaySettings;
			writeData[2] = 0x20 + 0x3B; // FWD power
			WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
			Sleep(16);
        }

        Ipp32fc S11 = GetS11(H2Window);
        S11 = GetCorrectedS11(calIndex, S11);

        float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);

        int ifrqx2 = (ifrq % 36) * 2;
        if (ifrq < 36)
        {
            myStatus->SmithChartUntuned[ifrqx2] = S11.re;
            myStatus->SmithChartUntuned[ifrqx2 + 1] = S11.im;
            myStatus->SWRUntuned[ifrq] = (1.0 + Rmag) / (1.0 - Rmag);
            myVNACal->SweptIQMu[ifrq] = m_lastIQMu;
        }

        else if(ifrq >= 68)
        {
			myStatus->SmithChartTuned[ifrqx2] = S11.re;
			myStatus->SmithChartTuned[ifrqx2 + 1] = S11.im;
            myStatus->SWRTuned[ifrq-68] = (1.0 + Rmag) / (1.0 - Rmag);
        }
        else
        {
            if (Rmag < minRMAG)
            {
                minRMAG = Rmag;
                bestRelaySetting = ifrq - 36;
            }
             
        }
        myStatus->UpdateVSWR = true;
        if ((myStatus->calMode == 5) && (ifrq == 35)) break;
	}
	if (myStatus->calMode == 3)
	{
		RelaySettings = bestRelaySetting;
        sprintf_s(dbgText, "TUNE %d %.2f", bestRelaySetting, myStatus->SWRTuned[25]);
        //Cleanup
		myStatus->UpdateVSWR = true;
	}
	SetFreq(lastLO);

    SetRXBits();

	Sleep(16);

    writeData[0] = 'r';
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in RX mode
    myStatus->mode = 1;

}

void CRadio::SweepAntTune()
{
	DWORD WriteData4[16];
	char writeData[64];
	char readData[64];
	DWORD bytesWritten = 0;
	DWORD bytesToWrite = 4;
	//    int RelaySettings = 31; // 31 is bypass
	Ipp32f H2Window[160];
	for (int i = 0; i < 160; i++)
		H2Window[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 160);

	IQWriteAddr = 0;

	writeData[0] = 'w'; // prep vna
	WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
	Sleep(16);

	writeData[0] = 'c';
	writeData[1] = 0x20; // Bypass
	writeData[2] = 0x20 + 0x3B; // FWD power
	WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
	Sleep(16);
	double thisFreq;
	float minRMAG = 1.0;

	for (int r = 0; r < 36; r++)//36 @ bypass, 32@14.25 GHz across settings, 36@best setting
	{
		writeData[0] = 'w'; // idle vna mode
		WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
		Sleep(16);
		ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
		if (readData[0] != 'R')
			int g = 0;

		RelaySettings = r & 0x1F;
		writeData[0] = 'c';
		writeData[1] = 0x20 + RelaySettings;
		writeData[2] = 0x20 + 0x3B; // FWD power
		WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
		Sleep(16);

		Ipp32fc S11 = GetS11(H2Window);
		S11 = GetCorrectedS11(25, S11);

		float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);

		myStatus->SmithChartUntuned[r * 2] = S11.re;
		myStatus->SmithChartUntuned[r * 2 + 1] = S11.im;
		myStatus->SWRUntuned[r] = (1.0 + Rmag) / (1.0 - Rmag);

		myStatus->UpdateVSWR = true;
	}

	SetRXBits();

	Sleep(16);

	//writeData[0] = 'i';
	//WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in RX mode
	writeData[0] = 'r';
	WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in RX mode
	myStatus->mode = 1;

}


int CRadio::DataThread()
{
    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;
    FILE* f;
    int mode = 0;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    //fopen_s(&f, "debugData.csv", "w");
    while (connected)
    {
        if (myStatus->mode == IDLE_MODE)
        {
            char writeData[4];
            writeData[0] = 'h';
            DWORD bytesWritten = 0;
            WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Turn off 24V
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
        else if (myStatus->mode == RX_MODE) // Receive
        {
            RXDataLoop();
        }
        else if (myStatus->mode == TX_MODE) // Receive
        {
            TXDataLoop();
        }
        else if (myStatus->mode == VNA_MODE) // Tune
        {
            AntTuneDataLoop();
        }

	}

	return 0;
}

int CRadio::UpdatePlot()
{
    if (!connected) return 0;

   // return 0;

    float AudioFreqPlot[128];
//    float AudioTimePlot[128];
    float RFFreqPlot[256];
    bool UpdateText;
    bool UpdateAudio;
    bool UpdateVSWR;
    bool UpdateRFPlot;
    float thisval = 0.0;
    float maxval = 0.0;

    
    if (RX_MODE == myStatus->mode)
    {
        for (int i = 0; i < 16; i++)
        {
            thisval = sqrt(DFTData[i].re * DFTData[i].re + DFTData[i].im * DFTData[i].im) / (TunerMag * 256);
            if (maxval < thisval) maxval = thisval;
            myStatus->AudioFreqPlot[i] = thisval;
        }
        if (maxval > 0.0) maxval = 1.0 / maxval;
        for (int i = 0; i < 16; i++) myStatus->AudioFreqPlot[i] *= maxval;

        memcpy(myStatus->AudioTimePlot, RawAudio, 128 * 4); //Other modes take care of this
    }
//    Ipp32f* MagData = ippsMalloc_32f(250);
 //   Ipp32f* LogMagData = ippsMalloc_32f(250);
//    ippsMagnitude_32fc(DFTData, MagData, 250);
//    ippsMulC_32f_I(0.004, MagData, 250); //arb scale

    ippsLog10_32f_A11(MagAccumData, LogMagData, 250);
    ClearMagAccum = true;

    //Center the zero
    memcpy(&myStatus->RFFreqPlot[125], LogMagData, 125 * 4);
    memcpy(myStatus->RFFreqPlot, &LogMagData[125], 125 * 4);

    // S-unit: power=1.0 → -5 dBm, S9 = -73 dBm, 6 dB per S-unit
    if (RX_MODE == myStatus->mode && RXChannelPower > 0.0f)
    {
        float dBm = 10.0f * log10f(RXChannelPower) - 5.0f + plotSoffset;
        int sunit = 9 + (int)roundf((dBm + 73.0f) / 6.0f);
        if (sunit < 0) sunit = 0;
        myStatus->Sunit = sunit;
        myStatus->UpdateText = true;
    }

	return 0;
}
int CRadio::SetFreq(double freqMHz)
{
    char writeData[8];
    DWORD bytesWritten = 0;
    LOfreq = freqMHz;
    m_iFreq = (int)round(LOfreq * 1000000.0);
    writeData[0] = 'f';
    writeData[1] = 0x20 + ((m_iFreq >> 18) & 0x3F);
    writeData[2] = 0x20 + ((m_iFreq >> 12) & 0x3F);
    writeData[3] = 0x20 + ((m_iFreq >> 6) & 0x3F);
    writeData[4] = 0x20 + ((m_iFreq) & 0x3F);
    WriteFile(hSerial, writeData, 5, &bytesWritten, NULL); // Set frequency
    return bytesWritten;
}

int CRadio::SetRXBits()
{
    char writeData[8];
    DWORD bytesWritten = 0;

    writeData[0] = 'c';
    writeData[1] = 0x20 + RelaySettings;
    writeData[2] = 0x20 + 0x3E; // actual input

     WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // Set frequency
    return bytesWritten;
}

int CRadio::Connect()
{
    if (connected) return 0;
    for (int i = 0; i < 64; i++) DummyBuffer[i] = 0.0;// 0.5 * cos(IPP_2PI * i / 64.0);

    sprintf_s(dbgText, "Connecting");
    FILE* f;
    fopen_s(&f, "comport.txt", "r");
    int port = 5;
    AudioInputChannels = 1;
    fscanf_s(f, "%d", &port);
    port = 5; //DEBUG
    fscanf_s(f, "%d", &AudioInputChannels);
    fscanf_s(f, "%f", &LOfreq);
    fscanf_s(f, "%d", &RelaySettings);
    comPort = port;
    fclose(f);
    
    //debug
    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        sprintf_s(dbgText, "PA Init Fail");
        return 0;
    }
    const PaDeviceInfo* pai = Pa_GetDeviceInfo(0);
    double sr = pai->defaultSampleRate;
    AudioInputChannels = pai->maxInputChannels;
    AudioOutputChannels = pai->maxOutputChannels;
    char msg[64];
//    fopen_s(&f, "debug.txt", "w");
//    fprintf_s(f, "%f %d %d", sr, AudioInputChannels, AudioOutputChannels);
//    fclose(f);

//    MessageBoxA(NULL, msg, "Audio Capabilities", MB_OK);
    // Open an audio I/O stream. 

    connected = true;
 //   myAThread = std::thread(ProcessPlotThread, 1, this); // Creates a thread executing myFunction with argument 1
    AudioInputChannels = 1;
    err = Pa_OpenDefaultStream(&stream,
        AudioInputChannels,          // 1 input channels 
        AudioOutputChannels,          // stereo output 
        paFloat32,  // 32 bit floating point output 
        SAMPLE_RATE,
        256,        /* frames per buffer, i.e. the number
                           of sample frames that PortAudio will
                           request from the callback. Many apps
                           may want to use
                           paFramesPerBufferUnspecified, which
                           tells PortAudio to pick the best,
                           possibly changing, buffer size.*/
        patestCallback, /* this is your callback function */
        this); /*This is a pointer that will be passed to
                           your callback*/
    if (err != paNoError) {
        sprintf_s(dbgText, "PA Open Fail");
        return 0;
    }


//    HANDLE hSerial;
    DCB dcbSerialParams = { 0 };
    COMMTIMEOUTS timeouts = { 0 };
    char data[256];
    DWORD bytesRead;

    // Open the serial port
    char cport[16];
    sprintf_s(cport, "\\\\.\\COM%d", port);

    hSerial = CreateFileA(cport, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE) {
        //perror("Error opening serial port");
        sprintf_s(dbgText, "NO DEVICE!");
        goto SkipComPortStuff;
    }

    // Set serial communication parameters
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        sprintf_s(dbgText, "COMSTATE ERR");
        CloseHandle(hSerial);
        goto SkipComPortStuff;
    }
    dcbSerialParams.BaudRate = 115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fRtsControl = 0;
    dcbSerialParams.fDtrControl = 1;
    
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        sprintf_s(dbgText, "COMSTATE ERR");
        CloseHandle(hSerial);
        goto SkipComPortStuff;
    }

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    SetupComm(hSerial, 1048576, 1048576); // 1MB buffer

    // Set timeouts
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        sprintf_s(dbgText, "COMTMO ERR");
        CloseHandle(hSerial);
        goto SkipComPortStuff;
    }

    SetFreq(LOfreq); //DEBUG: SET FREQ FROM FILE
    myStatus->RXFreq = LOfreq;
    Sleep(16);
//    SetRXBits();
//    Sleep(16);

    writeData[0] = 'g'; // Enable 24 V
    writeData[1] = 'r'; //Change mode to RX
//    writeData[2] = 'b';// Read bogus data
//    writeData[3] = 'b';
    WriteFile(hSerial, writeData, 2, &bytesWritten, NULL); // Set frequency
    Sleep(100);

    writeData[0] = 'c';
    writeData[1] = 0x20 + RelaySettings; 
    writeData[2] = 0x20 + 0x3E; // RX
    WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    Sleep(16);

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);


    myStatus->mode = 1;
    myDThread = std::thread(ProcessDataThread, 1, this); // Creates a thread executing myFunction with argument 1
    

SkipComPortStuff:
    err = Pa_StartStream(stream);
    //Pa_Sleep(1000);
    //err = Pa_StopStream(stream);
    if (err != paNoError) {
        sprintf_s(dbgText, "AUDIO ERR");
        return 0;
    }

    sprintf_s(dbgText, "Connect done");

//    DataThread();//debug
//    sprintf_s(dbgText, "stop data");

 	return 1;
}
