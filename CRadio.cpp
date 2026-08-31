// QRO20 - HF SDR transceiver control software
// Copyright (C) 2026  Justin Crooks
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CRadio.h"
#include <thread>
#include <cmath>
#include <cctype>
#include <wchar.h>
#include <stddef.h>
#include <ipp.h>
#include <chrono>
#include <iostream>
#include <cstdio>
//#include "frame1.h"

#define ENABLE_CESSB    // Comment out to bypass CESSB (unprocessed SSB, no envelope clipping)

bool gUseDebugWaveform = false; // try this

void ProcessDataThread(int id, void* p) {
    CRadio* pRadio = (CRadio*)p;
    pRadio->DataThread();
    // Code to be executed in the new thread
}

void BuildTXPacket(char* wd, Ipp32fc* pIQ); // defined below; used by both TXDataLoop and CWTXDataLoop
void ResetTXPacketState(); // defined below, next to BuildTXPacket


//void ProcessPlotThread(int id, void* p) {
//    CRadio* pRadio = (CRadio*)p;
//    pRadio->PlotThread();
//    // Code to be executed in the new thread
//}


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
    if (crp->myStatus->mode == RX_MODE) // USB RX
    {
        int IQSamples = crp->IQWriteAddr - crp->IQReadAddr;
        if (crp->IQWriteAddr < crp->IQReadAddr) IQSamples += 16000;
        if ((!crp->audioOutStarted) && (crp->audioOutWrPtr > 4095)) crp->audioOutStarted = true;
    }
    else if (crp->myStatus->mode == CW_TX_MODE) // CW sidetone - see CWTXDataLoop
    {
        if ((!crp->audioOutStarted) && (crp->audioOutWrPtr > 4095)) crp->audioOutStarted = true;
    }
    else // Not in RX mode or CW TX - no audio output source running, so don't let stale data play
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
    for (int i = 0; i < 64; i++) s->TXAvgPowerPlot[i] = 0.0;
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
    fprintf(f, "  \"myCallsign\": \"%s\",\n", myCallsign);

    fprintf(f, "  \"contestID\": \"%s\",\n", contestID);
    fprintf(f, "  \"exchangeTemplate\": %d,\n", exchangeTemplate);
    fprintf(f, "  \"myExchange\": \"%s\",\n", myExchange);
    fprintf(f, "  \"nextSerialSent\": %d,\n", nextSerialSent);
    fprintf(f, "  \"categoryMode\": \"%s\",\n", categoryMode);
    fprintf(f, "  \"categoryPower\": \"%s\",\n", categoryPower);
    fprintf(f, "  \"operatorName\": \"%s\",\n", operatorName);
    fprintf(f, "  \"addressLine\": \"%s\",\n", addressLine);
    fprintf(f, "  \"addressCity\": \"%s\",\n", addressCity);
    fprintf(f, "  \"addressState\": \"%s\",\n", addressState);
    fprintf(f, "  \"addressPostal\": \"%s\",\n", addressPostal);
    fprintf(f, "  \"addressCountry\": \"%s\",\n", addressCountry);
    fprintf(f, "  \"claimedScore\": \"%s\",\n", claimedScore);

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
        int ival; float fval, re, im; double dval = 0.0; char cval[16]; char cbuf[64];
        if      (sscanf_s(line, " \"comPort\": %d",       &ival) == 1) comPort       = ival;
        else if (sscanf_s(line, " \"LOfreq\": %lf",       &dval) == 1) LOfreq        = dval;
        else if (sscanf_s(line, " \"RelaySettings\": %d", &ival) == 1) RelaySettings = ival;
        else if (sscanf_s(line, " \"myCallsign\": \"%[^\"]\"", cval, (unsigned)sizeof(cval)) == 1)
            strncpy_s(myCallsign, sizeof(myCallsign), cval, _TRUNCATE);
        else if (sscanf_s(line, " \"contestID\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(contestID, sizeof(contestID), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"exchangeTemplate\": %d", &ival) == 1) exchangeTemplate = ival;
        else if (sscanf_s(line, " \"myExchange\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(myExchange, sizeof(myExchange), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"nextSerialSent\": %d", &ival) == 1) nextSerialSent = ival;
        else if (sscanf_s(line, " \"categoryMode\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(categoryMode, sizeof(categoryMode), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"categoryPower\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(categoryPower, sizeof(categoryPower), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"operatorName\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(operatorName, sizeof(operatorName), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"addressLine\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(addressLine, sizeof(addressLine), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"addressCity\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(addressCity, sizeof(addressCity), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"addressState\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(addressState, sizeof(addressState), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"addressPostal\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(addressPostal, sizeof(addressPostal), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"addressCountry\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(addressCountry, sizeof(addressCountry), cbuf, _TRUNCATE);
        else if (sscanf_s(line, " \"claimedScore\": \"%[^\"]\"", cbuf, (unsigned)sizeof(cbuf)) == 1)
            strncpy_s(claimedScore, sizeof(claimedScore), cbuf, _TRUNCATE);
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
    IQCorrAlpha = 1.0f;
    IQCorrBeta  = 0.0f;
    IQBalanceEnabled = true;
    memset(myCallsign, 0, sizeof(myCallsign));

    memset(contestID, 0, sizeof(contestID));
    exchangeTemplate = EXCH_NONE;
    memset(myExchange, 0, sizeof(myExchange));
    nextSerialSent = 1;
    strncpy_s(categoryMode, sizeof(categoryMode), "SSB", _TRUNCATE);
    strncpy_s(categoryPower, sizeof(categoryPower), "HIGH", _TRUNCATE);
    memset(operatorName, 0, sizeof(operatorName));
    memset(addressLine, 0, sizeof(addressLine));
    memset(addressCity, 0, sizeof(addressCity));
    memset(addressState, 0, sizeof(addressState));
    memset(addressPostal, 0, sizeof(addressPostal));
    memset(addressCountry, 0, sizeof(addressCountry));
    memset(claimedScore, 0, sizeof(claimedScore));

    CWModeEnabled = false;
    // cwSquelch gates CWSlicer::signalDetect (see AssignCWSlicers): the sum of the phase-coherent
    // vector-sum magnitudes of dftOnHistory's newest 4 and oldest 4 entries ("signal"), times
    // cwSquelch, must exceed the last 8 hops' worth of off-pitch dftLowCorr/dftHighCorr magnitude
    // ("noise floor near the signal") - a real tone's on-pitch correlator outputs stay phase-aligned
    // hop to hop so each 4-hop half adds up in its own vector sum, while the off-pitch reference tones
    // see no such tone and just track ambient noise level.
    cwSquelch = 0.5f;
    cwHysteresis = 0.1f;
    ResetCWDecoder();

    cwTxDotMs = 60.0f; // TX dot length, ms - hard-coded for now
    cwSidetoneHz = 700.0f; // matches the "CW Mode (700 Hz)" RX convention - hard-coded for now
    memset(cwTxMessage, 0, sizeof(cwTxMessage));

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
    isRXOnly = true;
    hasTransmitted = false;

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

    // CW Peaking (see StartCWPeakCapture/CRadio.h) - just runs FindCWPeak, so nothing to allocate
    // here beyond its own confirm-state.
    CWPeakCapturing = false;
    CWPeakReady = false;
    CWPeakPendingBin = -1;
    CWPeakFreq = 0.0f;
    CWPeakHopAccum = 0;

    // CW multi-tone decode: 8192-point (order 13) complex FFT on a sliding window of wideband
    // (pre-CW-filter) audio, same IPP FFT setup pattern as CW Peaking above. The window slides
    // every DoRXDSP hop (~5.33 ms), but the FFT itself (peak search) only runs every kCWPeakHopSize
    // samples (~85.3 ms) - see FindCWPeak/DoCWFFTDecode.
    cwFFTSampleWindow = ippsMalloc_32f(kCWFFTSize);
    cwHannWindow      = ippsMalloc_32f(kCWFFTSize);
    ippsZero_32f(cwFFTSampleWindow, kCWFFTSize);
    for (int i = 0; i < kCWFFTSize; i++)
        cwHannWindow[i] = 0.5f - 0.5f * cosf(IPP_2PI * i / kCWFFTSize);
    cwFFTData = ippsMalloc_32fc(kCWFFTSize);

    // 3-tone 512-point correlator (see BuildCWDFTTone), the per-hop mark/space correlator.
    // cwDFTGenScratch is sized to generate one continuous-phase tone long enough to slice into
    // kCWDFTSegments overlapping kCWDFTSize-sample windows (the last segment needs oscillator samples
    // up to (kCWDFTSegments-1)*kCWHopSize + kCWDFTSize - 1 = kCWFFTSize + kCWHopSize - 1).
    cw512HannWindow = ippsMalloc_32f(kCWDFTSize);
    for (int i = 0; i < kCWDFTSize; i++)
        cw512HannWindow[i] = 0.5f - 0.5f * cosf(IPP_2PI * i / kCWDFTSize);
    cwDFTGenScratch = ippsMalloc_32fc(kCWFFTSize + kCWHopSize);

    {
        Ipp8u* pCWFFTInitBuf;
        int sizeCWFFTSpec, sizeCWFFTInitBuf, sizeCWFFTWorkBuf;
        ippsFFTGetSize_C_32fc(kCWFFTOrder, IPP_FFT_DIV_FWD_BY_N,
            ippAlgHintAccurate, &sizeCWFFTSpec, &sizeCWFFTInitBuf, &sizeCWFFTWorkBuf);

        pCWFFTSpecBuf = ippsMalloc_8u(sizeCWFFTSpec);
        pCWFFTInitBuf = ippsMalloc_8u(sizeCWFFTInitBuf);
        pCWFFTWorkBuf = ippsMalloc_8u(sizeCWFFTWorkBuf);

        ippsFFTInit_C_32fc(&pCWFFTSpec, kCWFFTOrder, IPP_FFT_DIV_FWD_BY_N,
            ippAlgHintAccurate, pCWFFTSpecBuf, pCWFFTInitBuf);
        if (pCWFFTInitBuf) ippFree(pCWFFTInitBuf);
    }

    // Each slicer's 3-tone 512-point correlator buffers are pre-allocated once here (see
    // BuildCWDFTTone/AssignCWSlicers) so tracking a tone never allocates memory on the fly - kCWDFTBufLen
    // samples each, generated once per assignment and read back a block at a time (dftSegIndex),
    // never regenerated hop-to-hop.
    for (int s = 0; s < kCWMaxSlicers; s++)
    {
        cwSlicers[s].dftTonesOn   = ippsMalloc_32fc(kCWDFTBufLen);
        cwSlicers[s].dftTonesLow  = ippsMalloc_32fc(kCWDFTBufLen);
        cwSlicers[s].dftTonesHigh = ippsMalloc_32fc(kCWDFTBufLen);
    }

    // Just needs to keep the correlator's dot-product output in a sane float range (see cwToneMagn
    // declaration in CRadio.h) - signalDetect is a signal/noise ratio (see AssignCWSlicers), so it's
    // invariant to cwToneMagn's exact value as long as it doesn't underflow/overflow.
    cwToneMagn = sqrtf(0.375f / ((float)kCWFFTSize * kCWHopSize));

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
        if (myStatus->mode != IDLE_MODE)
        {
            myStatus->mode = IDLE_MODE;
            Sleep(250);
        }
        connected = false;
        //myStatus->mode = IDLE_MODE;
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

        // Wideband adaptive I/Q image-balance correction, applied to the raw (pre-tune) block.
        // Shear model: Icorr = I, Qcorr = alpha*Q - beta*I.
        //
        // The error terms are normalized (a correlation coefficient for phase, a power ratio
        // for gain) so the step reflects actual imbalance *magnitude*, not the block's absolute
        // signal level - raw unnormalized sums would saturate the clamp on almost every block
        // (real captured RF is rarely silent), turning this into a fixed-rate walk regardless of
        // whether there's really a strong signal to learn from. "Strong signal converges fast,
        // weak signal barely moves it" instead comes from an explicit confidence weight below,
        // scaled by how far the block's energy is above a reference (quiet-band) level.
        if (!IQBalanceEnabled)
        {
            IQCorrAlpha = 1.0f;
            IQCorrBeta  = 0.0f;
        }
        else
        {
            float sumIQ = 0.0f, sumII = 0.0f, sumQQ = 0.0f;
            for (int n = 0; n < 250; n++)
            {
                float rawI  = WindowedData[n].im;
                float rawQ  = WindowedData[n].re;
                float corrI = rawI;
                float corrQ = IQCorrAlpha * rawQ - IQCorrBeta * rawI;
                WindowedData[n].im = corrI;
                WindowedData[n].re = corrQ;
                sumIQ += corrI * corrQ;
                sumII += corrI * corrI;
                sumQQ += corrQ * corrQ; 
            }

            const float eps = 1.0e-12f;
            float totalPower = sumII + sumQQ;
            float rho    = sumIQ / sqrtf(sumII * sumQQ + eps);   // phase/skew error, in [-1,1]
            float powErr = (sumQQ - sumII) / (totalPower + eps); // gain error, in [-1,1]

            const float refPower = 0.02f; // block power ~= "a strong signal" - needs on-air tuning
            float weight = totalPower / refPower;
            if (weight > 1.0f) weight = 1.0f;

            const float muPhase = 1.0e-2f; // starting points only - needs on-air tuning
            const float muGain  = 1.0e-2f;

            IQCorrBeta  += muPhase * rho    * weight;
            IQCorrAlpha -= muGain  * powErr * weight;
            if      (IQCorrAlpha < 0.8f)  IQCorrAlpha = 0.8f;
            else if (IQCorrAlpha > 1.25f) IQCorrAlpha = 1.25f;
            if      (IQCorrBeta < -0.2f) IQCorrBeta = -0.2f;
            else if (IQCorrBeta >  0.2f) IQCorrBeta =  0.2f;
        }

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

    // CW Peaking capture: repeatedly runs FindCWPeak, the same 8192-point/600-800 Hz/two-hit-
    // confirmed search the live CW decoder uses (see FindCWPeak), every kCWPeakHopSize samples until
    // it reports a confirmed peak - using its own independent CWPeakPendingBin/CWPeakHopAccum state
    // so it doesn't interfere with the live decoder's own confirm-bin stream if CW Mode happens to
    // be enabled at the same time. Only slides the shared cwFFTSampleWindow itself when CW Mode
    // isn't already doing so this hop (DoCWFFTDecode, below, calls CaptureCWSpectrum for its own
    // purposes) - sliding it twice in the same hop would corrupt it.
    if (CWPeakCapturing)
    {
        if (!CWModeEnabled) CaptureCWSpectrum(RawAudio, 256);

        CWPeakHopAccum += 256;
        if (CWPeakHopAccum >= kCWPeakHopSize)
        {
            CWPeakHopAccum -= kCWPeakHopSize;

            float mag[kCWNumBins];
            bool  masked[kCWNumBins];
            ComputeCWFFTMag(mag, masked);

            int peakBin;
            float peakMag;
            if (FindCWPeak(mag, masked, kCWPeakBinLo[0], kCWPeakBinHi[0], &peakBin, &peakMag, CWPeakPendingBin))
            {
                CWPeakFreq = peakBin * (48000.0f / kCWFFTSize);
                CWPeakCapturing = false;
                CWPeakReady = true;
            }
        }
    }

    // CW mode: the FFT multi-tone decoder reads RawAudio while still wideband (same tap point as
    // CW Peaking above) so it sees the full 200-2800 Hz band regardless of the narrow listening
    // filter applied afterward. ApplyCWBandpass then narrows what's actually played back, exactly
    // as before.
    if (CWModeEnabled)
    {
        DoCWFFTDecode(RawAudio, 256);
        ApplyCWBandpass(RawAudio, 256);
    }

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

// One RBJ constant-peak-gain bandpass biquad, f0=700 Hz, Q=7 (~100 Hz wide) at the 48 kHz audio
// rate RawAudio/audioOutBuf run at. Applied twice in series (see ApplyCWBandpass) for steeper
// skirts, the same way the TX path already cascades separate HPF/LPF biquads.
static inline float CWBiquadStep(float in, float& x1, float& x2, float& y1, float& y2)
{
    // w0 = 2*pi*700/48000, Q = 7 (RBJ constant-peak-gain bandpass, normalized by a0)
    const float b0 =  0.0064933902f;
    const float b1 =  0.0f;
    const float b2 = -0.0064933902f;
    const float a1 = -1.9786775552f;
    const float a2 =  0.9870132196f;

    float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = in;
    y2 = y1; y1 = out;
    return out;
}

void CRadio::ApplyCWBandpass(Ipp32f* buf, int n)
{
    for (int i = 0; i < n; i++)
    {
        float s = CWBiquadStep(buf[i], cwBPF1_x1, cwBPF1_x2, cwBPF1_y1, cwBPF1_y2);
        s       = CWBiquadStep(s,      cwBPF2_x1, cwBPF2_x2, cwBPF2_y1, cwBPF2_y2);
        buf[i]  = s;
    }
}

struct MorseTableEntry { const char* code; const char* text; };
static const MorseTableEntry kMorseTable[] = {
    {".-","A"},{"-...","B"},{"-.-.","C"},{"-..","D"},{".","E"},
    {"..-.","F"},{"--.","G"},{"....","H"},{"..","I"},{".---","J"},
    {"-.-","K"},{".-..","L"},{"--","M"},{"-.","N"},{"---","O"},
    {".--.","P"},{"--.-","Q"},{".-.","R"},{"...","S"},{"-","T"},
    {"..-","U"},{"...-","V"},{".--","W"},{"-..-","X"},{"-.--","Y"},{"--..","Z"},
    {"-----","0"},{".----","1"},{"..---","2"},{"...--","3"},{"....-","4"},
    {".....","5"},{"-....","6"},{"--...","7"},{"---..","8"},{"----.","9"},
    {".-.-.-","."},{"--..--",","},{"..--..","?"},{"-..-.","/"},
    // Prosigns are keyed as one continuous element run (no inter-character gap), so they land
    // here as ordinary "characters" - angle-bracketed per user request so they read distinctly from text.
    {".-.-.","<AR>"},{"...-.-","<SK>"},{"-...-","<BT>"},{"-.--.","<KN>"},{".-...","<AS>"},
};

// Re-initializes one slicer to defaults and marks it unused. Called by ResetCWDecoder and by
// AssignCWSlicers when a slot is (re)assigned to a new tone (caller sets .used/.binIndex after).
static void ResetCWSlicer(CWSlicer& slicer)
{
    slicer.used = false;
    slicer.binIndex = -1;
    slicer.markState = false;
    slicer.stateHopCount = 0;
    slicer.resolvedThisSpace = false;
    slicer.hopsSinceMark = 0;
    slicer.dotUnitMs = 60.0f; // ~20 WPM starting guess
    slicer.patternLen = 0;
    slicer.textLen = 0;
    slicer.text[0] = '\0';
    memset(slicer.markLengths, 0, sizeof(slicer.markLengths));
    memset(slicer.spaceLengths, 0, sizeof(slicer.spaceLengths));
    memset(slicer.dftOnHistory, 0, sizeof(slicer.dftOnHistory));
    memset(slicer.dftNoiseHistory, 0, sizeof(slicer.dftNoiseHistory));
    
    slicer.signalDetect = false;
    slicer.freqNorm = 0.0f;
    slicer.peakMag = 0.0f;
    slicer.lastDiff = 0.0f;
    slicer.diffRising = false;
    slicer.markPeakAmp = 0.0f;
    slicer.dftSegIndex = 0;
    slicer.dftOnCorr.re = slicer.dftOnCorr.im = 0.0f;
    slicer.dftLowCorr.re = slicer.dftLowCorr.im = 0.0f;
    slicer.dftHighCorr.re = slicer.dftHighCorr.im = 0.0f;
}

// Shifts a 16-entry run-length history left and appends the newest value at the end (oldest first).
static void PushCWHistory(int (&history)[16], int value)
{
    memmove(history, history + 1, 15 * sizeof(int));
    history[15] = value;
}

void CRadio::AppendCWSlicerText(CWSlicer& slicer, const char* s)
{
    int addLen = (int)strlen(s);
    int cap = (int)sizeof(slicer.text) - 1;
    if (addLen > cap) { s += (addLen - cap); addLen = cap; }
    if (slicer.textLen + addLen > cap)
    {
        int drop = slicer.textLen + addLen - cap;
        memmove(slicer.text, slicer.text + drop, slicer.textLen - drop);
        slicer.textLen -= drop;
    }
    memcpy(slicer.text + slicer.textLen, s, addLen);
    slicer.textLen += addLen;
    slicer.text[slicer.textLen] = '\0';
}

void CRadio::ResolveCWSlicerCharacter(CWSlicer& slicer)
{
    if (slicer.patternLen == 0) return;
    slicer.pattern[slicer.patternLen] = '\0';

    const char* text = "~";
    for (const MorseTableEntry& e : kMorseTable)
        if (strcmp(e.code, slicer.pattern) == 0) { text = e.text; break; }

    AppendCWSlicerText(slicer, text);
    slicer.patternLen = 0;
}

void CRadio::ResetCWDecoder()
{
    cwBPF1_x1 = cwBPF1_x2 = cwBPF1_y1 = cwBPF1_y2 = 0.0f;
    cwBPF2_x1 = cwBPF2_x2 = cwBPF2_y1 = cwBPF2_y2 = 0.0f;

    cwPeakHopAccum = 0;
    for (int s = 0; s < kCWMaxSlicers; s++) 
        cwPendingPeakBin[s] = -1;
    // cwSquelch/cwHysteresis are deliberately not touched here - they're user-tunable settings
    // (Radio > CW Squelch.../CW Hysteresis...) and ResetCWDecoder() now runs on every frequency
    // retune, which shouldn't wipe out a manually-chosen value. Only ever initialized in the constructor.

    for (int s = 0; s < kCWMaxSlicers; s++)
        ResetCWSlicer(cwSlicers[s]);
}

///////////////////////////////////////////////////////////////////////////
// CW transmit - see CRadio::CWTXDataLoop for the send side.

// Reverse of kMorseTable: single-character text -> dot/dash code. Prosign entries ("<AR>" etc) are
// multi-character and intentionally not matched here - typed TX text has no way to enter them yet.
static const char* CWTxLookup(char c)
{
    for (const MorseTableEntry& e : kMorseTable)
        if (e.text[0] == c && e.text[1] == '\0') return e.code;
    return nullptr;
}

// Writes (or, if env is null, just measures) samples starting at env[pos]; returns the sample count
// either way, so the same call sequence sizes the buffer on a first dry-run pass and fills it on a
// second real pass (see EmitCWTXSequence / CRadio::BuildCWTXEnvelope).
static int EmitCWGap(Ipp32f* env, int pos, int samples)
{
    if (env) ippsZero_32f(env + pos, samples);
    return samples;
}

// A mark (key-down) segment: peakAmp for the whole run, except a taper-sample raised-cosine ramp
// in at the start and out at the end (0.5-0.5*cos, i.e. half of the same shape CRadio's other Hann
// windows use). samples is always >> 2*taper here (dot/dash lengths in ms vs a handful of ms taper).
static int EmitCWMark(Ipp32f* env, int pos, int samples, int taper, float peakAmp)
{
    if (env)
    {
        for (int i = 0; i < samples; i++)
        {
            float amp = peakAmp;
            if (i < taper)
                amp = peakAmp * (0.5f - 0.5f * cosf((float)IPP_2PI * i / (2.0f * taper)));
            else if (i >= samples - taper)
                amp = peakAmp * (0.5f - 0.5f * cosf((float)IPP_2PI * (samples - i) / (2.0f * taper)));
            env[pos + i] = amp;
        }
    }
    return samples;
}

// Leader flush + every character's tapered marks (1-unit gaps between elements) + inter-character
// (3-unit) or, after a space, inter-word (7-unit) gaps + trailer flush. Characters with no Morse
// mapping (CWTxLookup returns null) are silently dropped instead of breaking the sequence.
static int EmitCWTXSequence(Ipp32f* env, const char* text, int dotSamples, int taperSamples, int flushSamples, float peakAmp)
{
    int pos = 0;
    pos += EmitCWGap(env, pos, flushSamples); // leader

    bool wordGapPending = false;
    bool firstChar = true;
    for (const char* p = text; *p; p++)
    {
        char c = (char)toupper((unsigned char)*p);
        if (c == ' ') { wordGapPending = true; continue; }

        const char* code = CWTxLookup(c);
        if (!code) continue; // no Morse mapping for this character - skip it

        if (!firstChar)
            pos += EmitCWGap(env, pos, wordGapPending ? dotSamples * 7 : dotSamples * 3);
        firstChar = false;
        wordGapPending = false;

        for (int i = 0; code[i]; i++)
        {
            if (i > 0) pos += EmitCWGap(env, pos, dotSamples); // 1-unit intra-character gap
            int markSamples = (code[i] == '-') ? dotSamples * 3 : dotSamples;
            pos += EmitCWMark(env, pos, markSamples, taperSamples, peakAmp);
        }
    }

    pos += EmitCWGap(env, pos, flushSamples); // trailer
    return pos;
}

// Builds the full raised-cosine-tapered on/off keyed envelope for text, at 48 kHz, as a real-only
// magnitude buffer - CWTXDataLoop is what turns this into an actual I/Q signal (modulating it onto
// a cwSidetoneHz phasor). Caller owns the returned buffer (ippsFree it).
Ipp32f* CRadio::BuildCWTXEnvelope(const char* text, int* outSampleCount)
{
    int dotSamples = (int)lroundf(cwTxDotMs * 48.0f); // cwTxDotMs is in ms; audio rate is 48 kHz

    int rawTotal = EmitCWTXSequence(nullptr, text, dotSamples, kCWTxTaperSamples, kCWTxFlushSamples, kCWTxPeakAmplitude);

    // Round up to a whole number of 1024-sample resampler blocks (padding with extra trailing
    // silence) so CWTXDataLoop can always feed ippsResamplePolyphase_32f fixed 1024-sample chunks,
    // the same block size TXDataLoop already relies on elsewhere.
    int total = ((rawTotal + 1023) / 1024) * 1024;
    Ipp32f* env = ippsMalloc_32f(total);
    ippsZero_32f(env, total);
    EmitCWTXSequence(env, text, dotSamples, kCWTxTaperSamples, kCWTxFlushSamples, kCWTxPeakAmplitude);

    *outSampleCount = total;
    return env;
}

// Called from the UI thread (CWSendDialog's SEND button). Stashes the text and switches to
// CW_TX_MODE; CRadio::DataThread() picks that up on the background data thread and runs
// CWTXDataLoop(), which does the actual building and sending.
void CRadio::StartCWTransmit(const char* text)
{
    strncpy_s(cwTxMessage, sizeof(cwTxMessage), text, _TRUNCATE);
    myStatus->mode = CW_TX_MODE;
}

// Builds the keyed envelope for cwTxMessage and streams it to the hardware, then returns to RX_MODE.
// Deliberately skips TXDataLoop's HPF/LPF/AGC/CESSB chain entirely - the keying envelope (env[],
// from BuildCWTXEnvelope) is already clean, so none of that voice-band processing is needed. It's
// not plain carrier on/off at LOfreq, though: the transmitted tone has to land cwSidetoneHz *above*
// the tuned frequency (matching the RX side's own 700 Hz CW convention), so the envelope alone is
// run through the same LO-locked polyphase resampler TXDataLoop uses (factor tied to LOfreq) - the
// exact same real-only resample call this used before the 700 Hz offset existed - and only *after*
// that is it turned into a rotating I/Q phasor at cwSidetoneHz (I = env*cos, Q = env*sin, a
// positive-frequency analytic signal - same sign convention ApplyAnalyticBandpass uses for CESSB's
// Hilbert transform, which is what makes CESSB's own upper-sideband upconversion land above the
// dial frequency instead of on it). Modulating after resampling instead of before means the
// resampler itself only ever has to handle the smooth envelope, never an oscillating signal.
void CRadio::CWTXDataLoop()
{
    hasTransmitted = true;

    char writeData[132]; // 1 header + 32 IQ pairs x 4 bytes = 129 bytes
    char readData[64];
    DWORD bytesWritten = 0;

    int envCount = 0;
    Ipp32f* env = BuildCWTXEnvelope(cwTxMessage, &envCount);

    // This is a fresh, independent transmission, not a continuation of whatever was last sent
    // (SSB or an earlier CW message) - reset the state BuildTXPacket and the resampler otherwise
    // carry across calls by design, so it can't start out attenuated/distorted by leftover history.
    ResetTXPacketState();
    ippsResamplePolyphaseInit_32f(128, 32, 0.4, 4.0, resample_state,   ippAlgHintAccurate);
    ippsResamplePolyphaseInit_32f(128, 32, 0.4, 4.0, resample_state_q, ippAlgHintAccurate);

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

    SetRXBits(RelaySettings);
    Sleep(16);

    writeData[0] = 't'; // Transmit mode
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL);
    Sleep(32); // Wait for command / relays

    Ipp64f time = 80.0; // matches TXDataLoop's resampler start offset
    Ipp64f factor = LOfreq * 1000.0 / (48.0 * 256); // same LO-locked rate TXDataLoop resamples to
    int ResampleOutCount = 0;

    float sidetonePhase = 0.0f;
    float sidetonePhaseInc = (float)(IPP_2PI * cwSidetoneHz / 48000.0);

    // The RF tone is modulated on *after* resampling, at the resampler's actual output rate
    // (factor * 48000 Hz - "resample to the LO frequency / 256" per TXDataLoop's own comment on
    // factor), not before. Only the plain envelope ever goes through ippsResamplePolyphase_32f -
    // the exact same real-only, proven call this used before the 700 Hz offset was added - so the
    // resampler is never asked to handle an oscillating input, only the smooth env[] it always has.
    double outputRate = 48000.0 * factor;
    double rfPhase = 0.0;
    double rfPhaseInc = IPP_2PI * cwSidetoneHz / outputRate;

    // TXDataLoop is naturally real-time-paced because it only processes a block once that much new
    // mic audio has actually arrived from the live 48 kHz callback. This loop has no live input to
    // wait on - env[] is all sitting in memory already - so without an explicit pace it would blast
    // every block through as fast as the CPU allows, hundreds of ms of audio at a time. That's fine
    // for the serial/hardware side (its own FIFO drains it at the real rate regardless), but it
    // massively overruns audioOutBuf's ~341 ms (16384-sample) ring well ahead of PortAudio's
    // real-time playback, so most of the sidetone gets overwritten before it's ever heard. Pace each
    // 1024-sample (~21.3 ms) block to wall-clock time, computed from the start rather than
    // incrementally, so any single sleep's imprecision doesn't accumulate into drift over a message.
    auto txStartTime = std::chrono::steady_clock::now();

    int pos = 0;
    while (pos < envCount && CW_TX_MODE == myStatus->mode) // abort early if the user hits RECEIVE
    {
        // Sidetone: same envelope shape as the RF path (env[] is still at 48 kHz here, pre-resample),
        // at an audible pitch instead of DC, written straight into the audioOutBuf ring PortAudio's
        // output callback plays from - so it comes out the speaker roughly in step with the RF.
        for (int k = 0; k < 1024; k++)
        {
            float norm = env[pos + k] / kCWTxPeakAmplitude; // 0..1, independent of RF drive level
            audioOutBuf[audioOutWrPtr] = norm * kCWSidetoneAmplitude * cosf(sidetonePhase);
            sidetonePhase += sidetonePhaseInc;
            if (sidetonePhase >= (float)IPP_2PI) sidetonePhase -= (float)IPP_2PI;
            audioOutWrPtr++;
            if (audioOutWrPtr >= 16384) audioOutWrPtr = 0;
        }

        int newSampleCount = 0;
        ippsResamplePolyphase_32f(&env[pos], 1024, &resampledAudioOut[ResampleOutCount],
            factor, 1.0, &time, &newSampleCount, resample_state);
        time -= 1024;
        pos += 1024;

        // Modulate the just-resampled real envelope onto the RF tone: resampledAudioOut currently
        // holds resampled |env|, at the resampler's own output rate - turn each sample into I, and
        // derive Q from it, at that same rate (rfPhaseInc), rather than resampling an already-
        // modulated signal (see the comment above rfPhase for why).
        for (int n = 0; n < newSampleCount; n++)
        {
            float e = resampledAudioOut[ResampleOutCount + n];
            resampledAudioOut[ResampleOutCount + n]  = (float)(e * cos(rfPhase));
            resampledAudioOutQ[ResampleOutCount + n] = (float)(e * sin(rfPhase));
            rfPhase += rfPhaseInc;
            if (rfPhase >= IPP_2PI) rfPhase -= IPP_2PI;
        }
        ResampleOutCount += newSampleCount;

        int sendSampleCount = (ResampleOutCount / 32) * 32;
        Ipp32fc iq[32];
        for (int s = 0; s < sendSampleCount; s += 32)
        {
            for (int k = 0; k < 32; k++)
                iq[k] = { resampledAudioOut[s + k], resampledAudioOutQ[s + k] };
            BuildTXPacket(writeData, iq);
            WriteFile(hSerial, writeData, 129, &bytesWritten, NULL);
        }

        // Carry the <32-sample remainder (below one packet's worth) to the front for next time.
        ippsCopy_32f(&resampledAudioOut[sendSampleCount],  resampledAudioOut,  ResampleOutCount - sendSampleCount);
        ippsCopy_32f(&resampledAudioOutQ[sendSampleCount], resampledAudioOutQ, ResampleOutCount - sendSampleCount);
        ResampleOutCount -= sendSampleCount;

        std::this_thread::sleep_until(txStartTime + std::chrono::microseconds((long long)pos * 1000000LL / 48000LL));
    }
    // Any final leftover (<32 samples) is dropped - it only ever comes from the tail of the zero
    // trailer, so there's no real content in it to lose.

    ippsFree(env);

    writeData[0] = 'u';
    writeData[1] = 'r';
    WriteFile(hSerial, writeData, 2, &bytesWritten, NULL);

    writeData[0] = 'a'; // ADC payload
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // query adc values once
    ReadFile(hSerial, readData, 4, &bytesWritten, NULL); // Read volt / current
    UpdateADCs(readData);

    // Same voltage-based ratchet as TXDataLoop - see there for why it's one-way and volts>0 guarded.
    if (myStatus->volts > 0.0f)
    {
        int voltLimitedMax = (int)floor(myStatus->volts / 31.0f * 260.0f + 0.5f);
        if (voltLimitedMax < g_max_amp)
        {
            g_max_amp = voltLimitedMax;
            g_abs_max_amp = (int)round(1.15 * g_max_amp);
        }
    }

    SetRXBits(0);
    Sleep(16);

    myStatus->mode = RX_MODE;
}

// Slides the kCWFFTSize-sample window by n (always kCWHopSize) samples of wideband pre-filter
// audio. Called every hop so the window is always current for both FindCWPeak's FFT (every
// kCWPeakHopSize samples) and every active slicer's tone-correlator (every hop).
void CRadio::CaptureCWSpectrum(Ipp32f* wideband, int n)
{
    memmove(cwFFTSampleWindow, cwFFTSampleWindow + n, (kCWFFTSize - n) * sizeof(Ipp32f));
    memcpy(cwFFTSampleWindow + (kCWFFTSize - n), wideband, n * sizeof(Ipp32f));
}

// Runs the kCWFFTSize-point peak-search FFT on the current sliding window (Hann-windowed), then:
//  - masks +-kCWPeakMaskRadius bins around every already-tracked slicer's bin (so an existing tone
//    can't inflate the noise floor or be re-detected as new);
//  - restricts the search (and the noise-floor average below) to the kCWPeakBinLo..kCWPeakBinHi
//    sub-band (600-800 Hz) - new peaks are never looked for outside that range, even though the
//    wider 200-2800 Hz capture band is still what's masked/tracked per slicer;
//  - averages the remaining (unmasked, in-band) bins' magnitude into avgMag, the noise-floor
//    reference for the new-peak test below (mark/space slicing is handled independently per slicer
//    via edge detection - see CWSlicer::diffRising / AssignCWSlicers);
//  - finds the loudest unmasked in-band bin exceeding avgMag by kCWPeakThresholdDb (loud enough and
//    more than kCWPeakMaskRadius bins from every tracked slicer), then requires it to land within
//    kCWPeakConfirmBins of the *previous* peak-search hop's candidate before reporting it as a
//    confirmed new peak - a single noisy hop can no longer launch a slicer on its own, since a
//    real CW tone persists across consecutive ~kCWPeakHopSize-sample hops while a noise spike
//    generally doesn't land on the same bin twice in a row. pendingBin is the caller's own
//    confirm-state (the live decoder passes cwPendingPeakBin, CW Peaking passes its own independent
//    CWPeakPendingBin - see DoRXDSP) so two callers running at different cadences don't interfere
//    with each other's confirmation stream.
// Runs the kCWFFTSize-point peak-search FFT on the current sliding window (Hann-windowed) and fills
// mag[kCWNumBins] with each in-band bin's magnitude and masked[kCWNumBins] with whether that bin
// falls within kCWPeakMaskRadius of any currently-tracked slicer's bin (so an existing tone can't
// inflate the noise floor or be re-detected as new). Computed once per peak-search hop and shared
// across every slicer's own band-scoped FindCWPeak call below, instead of re-running the FFT per band.
void CRadio::ComputeCWFFTMag(float* mag, bool* masked)
{
    for (int i = 0; i < kCWFFTSize; i++)
    {
        cwFFTData[i].re = cwFFTSampleWindow[i] * cwHannWindow[i];
        cwFFTData[i].im = 0.0f;
    }
    ippsFFTFwd_CToC_32fc_I(cwFFTData, pCWFFTSpec, pCWFFTWorkBuf);

    for (int b = 0; b < kCWNumBins; b++)
    {
        float re = cwFFTData[kCWBinLo + b].re;
        float im = cwFFTData[kCWBinLo + b].im;
        mag[b] = sqrtf(re * re + im * im);
    }

    memset(masked, 0, kCWNumBins * sizeof(bool));
    for (int s = 0; s < kCWMaxSlicers; s++)
    {
        if (!cwSlicers[s].used) continue;
        int center = cwSlicers[s].binIndex - kCWBinLo;
        int lo = center - kCWPeakMaskRadius; if (lo < 0) lo = 0;
        int hi = center + kCWPeakMaskRadius; if (hi > kCWNumBins - 1) hi = kCWNumBins - 1;
        for (int b = lo; b <= hi; b++) masked[b] = true;
    }
}

// Searches [searchLoAbs, searchHiAbs] (absolute FFT bins - one slicer's own fixed sub-band, see
// kCWPeakBinLo/Hi) of the shared mag[]/masked[] (see ComputeCWFFTMag, run once per peak-search hop)
// for a new peak: averages the unmasked in-band bins into avgMag, the local noise-floor reference so
// the threshold test compares against the actual local noise in that band instead of the full
// 200-2800 Hz capture band, then finds the loudest unmasked in-band bin exceeding avgMag by
// kCWPeakThresholdDb (loud enough and more than kCWPeakMaskRadius bins from every tracked slicer),
// then requires it to land within kCWPeakConfirmBins of the *previous* peak-search hop's candidate
// before reporting it as a confirmed new peak - a single noisy hop can no longer launch a slicer on
// its own, since a real CW tone persists across consecutive ~kCWPeakHopSize-sample hops while a noise
// spike generally doesn't land on the same bin twice in a row. pendingBin is the caller's own
// confirm-state for this band (the live decoder passes its own cwPendingPeakBin[s] per slicer, CW
// Peaking passes its own independent CWPeakPendingBin - see DoRXDSP) so different bands/callers don't
// interfere with each other's confirmation stream.
bool CRadio::FindCWPeak(const float* mag, const bool* masked, int searchLoAbs, int searchHiAbs, int* peakBin, float* peakMag, int& pendingBin)
{
    int searchLo = searchLoAbs - kCWBinLo;
    int searchHi = searchHiAbs - kCWBinLo;

    double sum = 0.0;
    int count = 0;
    for (int b = searchLo; b <= searchHi; b++)
    {
        if (masked[b]) continue;
        sum += mag[b];
        count++;
    }
    float avgMag = (count > 0) ? (float)(sum / count) : 0.0f;

    const float kPeakRatio = powf(10.0f, kCWPeakThresholdDb / 20.0f); // magnitude-domain dB
    float peakThreshold = avgMag * kPeakRatio;

    int bestB = -1;
    float bestMag = peakThreshold;
    for (int b = searchLo; b <= searchHi; b++)
    {
        if (masked[b]) continue;
        if (mag[b] > bestMag) { bestMag = mag[b]; bestB = b; }
    }
    if (bestB < 0)
    {
        pendingBin = -1; // no candidate this hop breaks any run in progress
        return false;
    }

    int candidateBin = bestB + kCWBinLo;

    int binDiff = candidateBin - pendingBin;
    if (binDiff < 0) binDiff = -binDiff;
    bool confirmed = (pendingBin >= 0) && (binDiff <= kCWPeakConfirmBins);

    pendingBin = candidateBin;
    if (!confirmed) return false;

    pendingBin = -1; // consumed - next new peak needs its own fresh two-hit confirmation
    *peakBin = candidateBin;
    *peakMag = bestMag;
    return true;
}

// Fills dest (kCWDFTBufLen samples) with kCWDFTSegments independently Hann(kCWDFTSize)-windowed
// blocks of a pure tone at freqNorm, one per hop position. Block seg covers continuous-oscillator
// phase [seg*kCWHopSize, seg*kCWHopSize+kCWDFTSize) - i.e. consecutive blocks overlap 50% in
// oscillator phase, matching the 50%-overlap sliding audio window each hop is dot-producted
// against (see AssignCWSlicers). That overlap is why this can't reuse the single-tone correlator's
// trick of one periodic buffer read via a strided slice: a given oscillator sample falls inside two
// different blocks, each multiplying it by a different Hann coefficient, so each block needs its
// own separately-windowed copy. Generates one continuous-phase tone into cwDFTGenScratch first
// (long enough to cover every block without wraparound) and slices/windows from that.
void CRadio::BuildCWDFTTone(Ipp32fc* dest, float freqNorm)
{
    float tonePhase = 0.0f; // arbitrary fixed start - a constant phase offset cancels out of the vector-sum magnitudes below
    ippsTone_32fc(cwDFTGenScratch, kCWFFTSize + kCWHopSize, cwToneMagn, freqNorm, &tonePhase, ippAlgHintFast);

    for (int seg = 0; seg < kCWDFTSegments; seg++)
    {
        ippsCopy_32fc(cwDFTGenScratch + seg * kCWHopSize, dest + seg * kCWDFTSize, kCWDFTSize);
        ippsMul_32f32fc_I(cw512HannWindow, dest + seg * kCWDFTSize, kCWDFTSize);
    }
}

// Each of the kCWMaxSlicers slots is permanently bound to its own fixed search band (see
// kCWPeakBinLo/Hi/DoCWFFTDecode) - slot s only ever claims a peak found in band s, and holds it
// indefinitely once claimed. If a new peak was found this hop for slot s (already confirmed by
// FindCWPeak to be above the average unmasked bin magnitude by kCWPeakThresholdDb, and clear of
// every tracked slicer's bin), it only claims slot s if that slot is unused, or if this peak's
// magnitude beats the magnitude that slot was last tuned with - i.e. a new candidate only steals a
// slicer away from whatever it's currently tracking if it's the stronger signal. A weaker candidate
// is simply ignored and the current tuning is left alone. FindCWPeak's own confirm-bin logic (see
// cwPendingPeakBin) still resets/re-confirms independently whenever the candidate frequency moves,
// before any of this amplitude gating ever runs. The claimed slot's 3-tone 512-point correlator (see
// BuildCWDFTTone) is built once here - on-pitch plus +-kCWDFTBinOffset bins (a Hann(kCWDFTSize) null,
// so an on-pitch signal doesn't leak into the offset correlators) - and never regenerated again until
// the slot is reclaimed for a different peak.
//
// Then every used slicer (including one just claimed above) dot-products all 3 tones against the
// last kCWDFTSize samples of cwFFTSampleWindow - CaptureCWSpectrum, called before this every hop, has
// already slid the newest kCWHopSize samples into its tail, so this is exactly the previous hop's
// kCWHopSize samples plus this hop's, with no separate buffer needed. dftSegIndex selects which of
// the kCWDFTSegments precomputed blocks lines up with this hop, advancing by 1 (mod kCWDFTSegments)
// every hop. dftOnCorr is pushed newest-first into the slicer's 8-entry dftOnHistory, and
// dftLowCorr/dftHighCorr's combined magnitude is pushed the same way into dftNoiseHistory (see
// CRadio.h). Two things are derived from those 8-hop windows every hop:
//  - signalDetect: (|vectorSum(newest 4)| + |vectorSum(oldest 4)|) * cwSquelch > sum of the last 8
//    hops' dftNoiseHistory ("signal" vs "noise floor near the signal"). dftNoiseHistory[h] is
//    sqrt(|dftLowCorr|^2 + |dftHighCorr|^2) from that hop's off-pitch +-kCWDFTBinOffset correlators
//    (see below), which see no on-pitch tone and so track ambient noise; a real tone's on-pitch
//    correlator outputs stay phase-aligned hop to hop, so each 4-hop half adds up in its own vector
//    sum (splitting into two 4-hop sums rather than one 8-hop sum keeps this robust to slow phase
//    drift over the full 8-hop window). Reuses the same two vector sums computed for edge detection
//    below rather than a separate signal-only accumulation.
//  - edge detection: diff = |vectorSum(newest 4)| - |vectorSum(oldest 4)|. diffRising tracks whether
//    diff was rising or falling as of the previous hop; a rising-to-falling reversal (diff just
//    peaked positive, meaning the tone's energy just shifted from the old half of the window into
//    the new half) is the space->mark edge, gated by signalDetect so noise can't key up a mark, and
//    the peak's magnitude is captured into markPeakDiff. While marking, a falling-to-rising reversal
//    (diff just peaked negative) only ends the mark if that negative peak's magnitude is at least
//    half markPeakDiff - a small wobble on top of a strong mark isn't enough to release it;
//    signalDetect dropping out ends the mark unconditionally, regardless of any trough. Between
//    edges isMark just holds the slicer's current markState. RunCWSlicerTiming turns that per-hop
//    isMark verdict into dots/dashes. This runs every hop (kCWHopSize samples) regardless of how
//    often FindCWPeak itself runs, so timing resolution is unaffected by the slower peak-search
//    cadence. The active slicer is never freed/timed-out on idle - it only gets retuned when a
//    stronger peak comes along.
void CRadio::AssignCWSlicers(const bool* foundPeak, const int* peakBin, const float* peakMag, float hopMs)
{
    for (int idx = 0; idx < kCWMaxSlicers; idx++)
    {
        if (!foundPeak[idx]) continue;

        if (!cwSlicers[idx].used || peakMag[idx] > cwSlicers[idx].peakMag)
        {
            ResetCWSlicer(cwSlicers[idx]);
            cwSlicers[idx].used = true;
            cwSlicers[idx].binIndex = peakBin[idx];
            cwSlicers[idx].freqNorm = peakBin[idx] / (float)kCWFFTSize;
            cwSlicers[idx].peakMag = peakMag[idx];

            // 3-tone 512-point correlator (see BuildCWDFTTone) - on-pitch plus +-kCWDFTBinOffset bins.
            BuildCWDFTTone(cwSlicers[idx].dftTonesOn,   cwSlicers[idx].freqNorm);
            BuildCWDFTTone(cwSlicers[idx].dftTonesLow,  (peakBin[idx] - kCWDFTBinOffset) / (float)kCWFFTSize);
            BuildCWDFTTone(cwSlicers[idx].dftTonesHigh, (peakBin[idx] + kCWDFTBinOffset) / (float)kCWFFTSize);
        }
    }

    for (int s = 0; s < kCWMaxSlicers; s++)
    {
        if (!cwSlicers[s].used) continue;

        // 3-tone 512-point correlator (see BuildCWDFTTone). dftInput is the last kCWDFTSize samples
        // of cwFFTSampleWindow - CaptureCWSpectrum already slid this hop's kCWHopSize samples into
        // its tail before AssignCWSlicers runs, so this is exactly the previous hop's kCWHopSize
        // samples followed by this hop's, with no separate buffer needed.
        Ipp32f* dftInput = cwFFTSampleWindow + (kCWFFTSize - kCWDFTSize);
        int dftSeg = cwSlicers[s].dftSegIndex * kCWDFTSize;
        ippsDotProd_32f32fc(dftInput, cwSlicers[s].dftTonesOn   + dftSeg, kCWDFTSize, &cwSlicers[s].dftOnCorr);
        ippsDotProd_32f32fc(dftInput, cwSlicers[s].dftTonesLow  + dftSeg, kCWDFTSize, &cwSlicers[s].dftLowCorr);
        ippsDotProd_32f32fc(dftInput, cwSlicers[s].dftTonesHigh + dftSeg, kCWDFTSize, &cwSlicers[s].dftHighCorr);
        cwSlicers[s].dftSegIndex = (cwSlicers[s].dftSegIndex + 1) % kCWDFTSegments;

        for (int h = 7; h > 0; h--) {
            cwSlicers[s].dftOnHistory[h] = cwSlicers[s].dftOnHistory[h - 1];
            cwSlicers[s].dftNoiseHistory[h] = cwSlicers[s].dftNoiseHistory[h - 1];
        }
        cwSlicers[s].dftOnHistory[0] = cwSlicers[s].dftOnCorr;
        // Combined magnitude of this hop's two off-pitch correlators - an on-pitch tone reads ~zero
        // on both (see kCWDFTBinOffset), so this tracks the ambient noise floor near the tracked bin.
        cwSlicers[s].dftNoiseHistory[0] = sqrtf(cwSlicers[s].dftLowCorr.re * cwSlicers[s].dftLowCorr.re +
            cwSlicers[s].dftLowCorr.im * cwSlicers[s].dftLowCorr.im +
            cwSlicers[s].dftHighCorr.re * cwSlicers[s].dftHighCorr.re +
            cwSlicers[s].dftHighCorr.im * cwSlicers[s].dftHighCorr.im);

        // Sum (not vector-sum) over the last 8 hops - the noiseMag side of the signalDetect ratio
        // below; unlike dftOnHistory this is a plain scalar accumulation since there's no coherent
        // phase to preserve across noise samples.
        float noiseMag = 0.0f;
        for (int h = 0; h < 8; h++)
            noiseMag += cwSlicers[s].dftNoiseHistory[h];

        Ipp32fc olderSum = { 0.0f, 0.0f };
        Ipp32fc newerSum = { 0.0f, 0.0f };
        for (int h = 0; h < 4; h++)
        {
            newerSum.re += cwSlicers[s].dftOnHistory[h].re;
            newerSum.im += cwSlicers[s].dftOnHistory[h].im;
        }
        for (int h = 4; h < 8; h++)
        {
            olderSum.re += cwSlicers[s].dftOnHistory[h].re;
            olderSum.im += cwSlicers[s].dftOnHistory[h].im;
        }
        float signalMag = sqrtf(newerSum.re * newerSum.re + newerSum.im * newerSum.im);
        float olderMag = sqrtf(olderSum.re * olderSum.re + olderSum.im * olderSum.im);
        // signalMag + olderMag covers the full 8-hop window (as two 4-hop coherent sums) to match
        // noiseMag's own 8-hop span - using signalMag alone here under-counted signal energy relative
        // to noise and made signalDetect too easy to drop on a legitimate tone.
        float adjSquelch = (cwSlicers[s].signalDetect) ? cwSquelch : 0.5 * cwSquelch; // A little hysteresis
        cwSlicers[s].signalDetect = ((signalMag + olderMag) * adjSquelch) > noiseMag;


        // Edge detect: diff = |vectorSum(newest 4)| - |vectorSum(oldest 4)| (genuinely bipolar,
        // unlike "signal" above). A rising-to-falling reversal (diff just peaked positive) is the
        // space->mark edge; diffRising records which way diff was moving as of the previous hop, so
        // a reversal is recognized on the single hop it happens, and the peak/trough value itself is
        // just cwSlicers[s].lastDiff at that point (not yet overwritten below).
        float diff = (cwSlicers[s].signalDetect)? signalMag - olderMag : 0.0f;
        bool risingNow = diff > cwSlicers[s].lastDiff;

        bool wasMark = cwSlicers[s].markState;
        bool isMark = wasMark;

        if (!wasMark)
        {
            // space->mark: a positive diff peak, gated by signalDetect so noise can't key up a mark,
            // captures the peak's magnitude for the mark->space trough test below.
            if (cwSlicers[s].diffRising && !risingNow && cwSlicers[s].signalDetect)
            {
                isMark = true;
                cwSlicers[s].markPeakAmp = signalMag; //Capture signal amplitude after rising edge detect
            }
        }
        else
        {
            // mark->space: either the signal itself drops out, or a negative diff peak (trough) at
            // least half the magnitude of the captured positive peak is detected.
            bool trough = !cwSlicers[s].diffRising && risingNow
                && (signalMag < 0.5f * cwSlicers[s].markPeakAmp);
            if (!cwSlicers[s].signalDetect || trough) isMark = false;
        }

        cwSlicers[s].diffRising = risingNow;
        cwSlicers[s].lastDiff = diff;

        if (isMark) cwSlicers[s].hopsSinceMark = 0;
        else        cwSlicers[s].hopsSinceMark++;

        RunCWSlicerTiming(cwSlicers[s], isMark, hopMs);
    }
}

// Ported from the old envelope decoder's per-hop run-length state machine, driven by a per-hop
// "did this slicer see a mark this hop" boolean instead of a continuous envelope signal.
void CRadio::RunCWSlicerTiming(CWSlicer& slicer, bool markThisHop, float hopMs)
{
    if (markThisHop == slicer.markState)
    {
        slicer.stateHopCount++;

        // Mid-space and already long enough to end the character - resolve now instead of waiting
        // for the next mark (which, for the last character of a transmission, may never come).
        if (!slicer.markState && !slicer.resolvedThisSpace)
        {
            float durationMsSoFar = slicer.stateHopCount * hopMs;
            if (durationMsSoFar >= slicer.dotUnitMs * 2.25f)
            {
                ResolveCWSlicerCharacter(slicer);
                slicer.resolvedThisSpace = true;
            }
        }
        return;
    }

    // The 0.5/1.0/0.5-weighted 3-hop magnitude history in AssignCWSlicers smears each transition
    // by ~2 hops: it lengthens marks and shortens spaces, so back that out (in opposite directions)
    // before classifying the run. Anything left at zero or below is too short to be real and is
    // dropped as noise instead of being misread as a spurious dot or gap.
    int adjustedHopCount = slicer.markState 
        ? (slicer.stateHopCount - 1)
        : (slicer.stateHopCount + 1);
 //   int adjustedHopCount = slicer.stateHopCount;

    PushCWHistory(slicer.markState ? slicer.markLengths : slicer.spaceLengths, adjustedHopCount);

    if (adjustedHopCount > 0)
    {
        float durationMs = adjustedHopCount * hopMs;

        if (slicer.markState) // a mark run just ended
        {
            if (durationMs < hopMs * 3.5)
            {
                //Disregard as noise
            }
            else if (durationMs < slicer.dotUnitMs * 2.0f)
            {
                if (slicer.patternLen < (int)sizeof(slicer.pattern) - 1) slicer.pattern[slicer.patternLen++] = '.';
                slicer.dotUnitMs = slicer.dotUnitMs * 0.7f + durationMs * 0.3f;
            }
            else
            {
                if (slicer.patternLen < (int)sizeof(slicer.pattern) - 1) slicer.pattern[slicer.patternLen++] = '-';
                slicer.dotUnitMs = slicer.dotUnitMs * 0.9f + (durationMs / 3.0f) * 0.1f;
            }
            if (slicer.dotUnitMs < 20.0f)  slicer.dotUnitMs = 20.0f;
            if (slicer.dotUnitMs > 200.0f) slicer.dotUnitMs = 200.0f;
        }
        else // a space run just ended
        {
            // The character itself, if any, was most likely already resolved mid-space above, as
            // soon as the run crossed dotUnitMs*2.25 - this just catches the case where the run was
            // too short for that to have fired, and still appends the word space if warranted.
            if (durationMs >= slicer.dotUnitMs * 6.0f)
            {
                if (!slicer.resolvedThisSpace) ResolveCWSlicerCharacter(slicer);
                AppendCWSlicerText(slicer, " ");
            }
            else if (durationMs >= slicer.dotUnitMs * 2.25f)
            {
                if (!slicer.resolvedThisSpace) ResolveCWSlicerCharacter(slicer);
            }
            // else: intra-character gap
        }
    }

    slicer.markState = markThisHop;
    slicer.stateHopCount = 1;
    slicer.resolvedThisSpace = false;
}

// Called from the UI thread (CW Peaking button). Just arms the capture; DoRXDSP repeatedly runs
// FindCWPeak (see there) until it reports a confirmed peak.
void CRadio::StartCWPeakCapture()
{
    CWPeakReady = false;
    CWPeakCapturing = true;
    CWPeakPendingBin = -1;
    CWPeakHopAccum = 0;
}

// Called once per DoRXDSP hop (256 samples @ 48 kHz => ~5.33 ms) on wideband, pre-CW-filter audio.
// The sliding window and every active slicer's tone-correlator + mark/space timing update every
// hop; the much heavier kCWFFTSize-point peak search only runs once every kCWPeakHopSize samples
// (~85.3 ms) via the accumulator below, so CW timing resolution doesn't depend on peak-search cost.
void CRadio::DoCWFFTDecode(Ipp32f* wideband, int n)
{
    CaptureCWSpectrum(wideband, n);

    bool  foundPeak[kCWMaxSlicers] = {};
    int   peakBin[kCWMaxSlicers]   = {};
    float peakMag[kCWMaxSlicers]   = {};

    cwPeakHopAccum += n;
    if (cwPeakHopAccum >= kCWPeakHopSize)
    {
        cwPeakHopAccum -= kCWPeakHopSize;

        // One shared FFT (see ComputeCWFFTMag) per peak-search hop; each slicer then searches only
        // its own fixed sub-band (kCWPeakBinLo/Hi) of that same mag[]/masked[] for a new peak.
        float mag[kCWNumBins];
        bool  masked[kCWNumBins];
        ComputeCWFFTMag(mag, masked);

        for (int s = 0; s < kCWMaxSlicers; s++)
            foundPeak[s] = FindCWPeak(mag, masked, kCWPeakBinLo[s], kCWPeakBinHi[s], &peakBin[s], &peakMag[s], cwPendingPeakBin[s]);
    }

    float hopMs = n * 1000.0f / 48000.0f;
    AssignCWSlicers(foundPeak, peakBin, peakMag, hopMs);
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
    myStatus->volts =  (52.9 / 4096.0)* val;
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
            ResetCWDecoder(); // tuning moves every tone's bin index - stale slicers/filter state no longer apply
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

bool DEBUG_BUF_GENERATED = false;
Ipp32f* pTwoToneAudio;
void CRadio::Get1280AudioSamples(float gain)
{
    if (!DEBUG_BUF_GENERATED)
    {
        pTwoToneAudio = ippsMalloc_32f(256);
        for (int i = 0; i < 256; i++) pTwoToneAudio[i] = 0.4999 * cos(IPP_2PI * 3 * i / 256) + 0.4999 * cos(IPP_2PI * 8 * i / 256);
        //for (int i = 0; i < 256; i++) pTwoToneAudio[i] = 0.4999 * cos(IPP_2PI * 5 * i / 256) + 0.4999 * cos(IPP_2PI * 6 * i / 256);
        //for (int i = 0; i < 256; i++) pTwoToneAudio[i] = 0.9999 * sin(IPP_2PI * 6 * i / 256) ;
        DEBUG_BUF_GENERATED = true;
    }

    if (gUseDebugWaveform)
    {
        // Two-tone debug signal: 0.017 + 0.027, each at 0.5 amplitude.
        // Each tone is split into 1024 + 256 calls so the persistent phases
        // advance by 1024 samples (matching the audioInRdPtr stride), not 1280.
        //Ipp32f toneBuf[1280];

        //// Tone 1 (0.017) into resampledAudioIn
        //ippsTone_32f(resampledAudioIn,       1024, 0.5f, 0.017f, &debugTonePhase,  ippAlgHintFast);
        //Ipp32f tmp1 = debugTonePhase;
        //ippsTone_32f(resampledAudioIn + 1024, 256, 0.5f, 0.017f, &tmp1,            ippAlgHintFast);

        //// Tone 2 (0.027) into temp buffer, then add to resampledAudioIn
        //ippsTone_32f(toneBuf,       1024, 0.5f, 0.027f, &debugTonePhase2, ippAlgHintFast);
        //Ipp32f tmp2 = debugTonePhase2;
        //ippsTone_32f(toneBuf + 1024, 256, 0.5f, 0.027f, &tmp2,           ippAlgHintFast);
        
        for (int i = 0; i < 1280; i += 256)
            ippsCopy_32f(pTwoToneAudio, &resampledAudioIn[i], 256);

        //ippsAdd_32f_I(toneBuf, resampledAudioIn, 1280);
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
int g_min_amp = 32;// 32;
int g_max_amp = 177;// 50% power (250 * sqrt(0.5))
int g_abs_max_amp = 204;// 1.15 * g_max_amp

static int gTXPacketLastPhase = 0;
static float gTXPacketRemainder = 0.0f;

// TXDataLoop/CWTXDataLoop are independent transmissions, not a continuous stream, but
// BuildTXPacket's phase-continuity and amplitude-dither state persists across calls by default
// (shared between both). CWTXDataLoop calls this before sending, so a CW message never starts by
// inheriting phase/dither state left over from whatever was sent last.
void ResetTXPacketState()
{
    gTXPacketLastPhase = 0;
    gTXPacketRemainder = 0.0f;
}

void BuildEmptyPacket(char* wd)
{
    for (int i = 0; i < 32; i++)
    {
        *(wd++) = 0x20;
        *(wd++) = 0x20;
        *(wd++) = 0x20;
        *(wd++) = 0x20 + g_min_amp;
    }
}

void BuildTXPacket(char* wd, Ipp32fc* pIQ)
{
    int& lastPhase = gTXPacketLastPhase;
    float& remainder = gTXPacketRemainder;
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
    debugTonePhase  = 0.0f;
    debugTonePhase2 = 0.0f;
}

// Real audio -> bandpass-filtered analytic signal via Hilbert transform (overlap-save, 2048-pt FFT).
// This is CESSB's own passband (300-2695 Hz) and is also used, standalone, as the TX audio
// prefilter in TXDataLoop (replacing the old biquad HPF/LPF) so AGC's peak measurement and CESSB's
// clip/filter iterations both work from the same already-analytic, band-limited signal.
// pIn  : 1280 real audio samples at 48 kHz (normalized, peak <= 1.0 recommended)
// pOut : 1280 Ipp32fc analytic baseband signal; Re(pOut) is the bandpass-filtered audio
void CRadio::ApplyCESSBBandpass(Ipp32f* pIn, Ipp32fc* pOut)
{
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
}

// Iteratively clips the envelope of an already-analytic signal (see ApplyCESSBBandpass) and
// re-applies the bandpass to remove clipping artifacts, 3 times over, in place.
// pInOut : 1280 Ipp32fc analytic baseband signal; Re(pInOut) is the CESSB SSB audio
void CRadio::ProcessCESSB(Ipp32fc* pInOut)
{
    // --- Iteration 1: clip envelope, re-apply bandpass to remove clipping artifacts ---
    ClipEnvelope(pInOut, CESSB_BLOCK);
    ippsCopy_32fc(cessbCplxOverlap1, cessbWorkBuf, CESSB_OVERLAP);
    ippsCopy_32fc(pInOut, cessbWorkBuf + CESSB_OVERLAP, CESSB_BLOCK);
    ippsCopy_32fc(pInOut + CESSB_BLOCK - CESSB_OVERLAP, cessbCplxOverlap1, CESSB_OVERLAP);
    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyComplexBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pInOut, CESSB_BLOCK);

    // --- Iteration 2: second clip + bandpass for further PAPR reduction ---
    ClipEnvelope(pInOut, CESSB_BLOCK);
    ippsCopy_32fc(cessbCplxOverlap2, cessbWorkBuf, CESSB_OVERLAP);
    ippsCopy_32fc(pInOut, cessbWorkBuf + CESSB_OVERLAP, CESSB_BLOCK);
    ippsCopy_32fc(pInOut + CESSB_BLOCK - CESSB_OVERLAP, cessbCplxOverlap2, CESSB_OVERLAP);
    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyComplexBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pInOut, CESSB_BLOCK);

    // --- Iteration 3: third clip + bandpass to catch residual overshoot ---
    ClipEnvelope(pInOut, CESSB_BLOCK);
    ippsCopy_32fc(cessbCplxOverlap3, cessbWorkBuf, CESSB_OVERLAP);
    ippsCopy_32fc(pInOut, cessbWorkBuf + CESSB_OVERLAP, CESSB_BLOCK);
    ippsCopy_32fc(pInOut + CESSB_BLOCK - CESSB_OVERLAP, cessbCplxOverlap3, CESSB_OVERLAP);
    ippsFFTFwd_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ApplyComplexBandpass(cessbWorkBuf);
    ippsFFTInv_CToC_32fc_I(cessbWorkBuf, cessbFFTSpec, cessbFFTWorkBuf);
    ippsCopy_32fc(cessbWorkBuf + CESSB_OVERLAP, pInOut, CESSB_BLOCK);
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
    hasTransmitted = true;

    char writeData[132]; // 1 header + 32 IQ pairs × 4 bytes = 129 bytes
    char readData[64];
    DWORD bytesWritten = 0;
    float agcGain           = 4.0f;
    const float agcMinGain  = 1.0f;
    const float agcAttack   = 0.41f;    // per-block attack  (~50 ms at 1280/48k blocks)
    const float agcRelease  = 0.065f;   // per-block release (~400 ms)
    int txPacketCount = 0;

//    FILE* f;
//    fopen_s(&f, "avgpower.csv", "w");

    Ipp32fc * TXIFFTAccum = ippsMalloc_32fc(3072);
    Ipp32f * pTXPowerScratch = ippsMalloc_32f(2048); // scratch for average-power limiter below

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

    SetRXBits(RelaySettings);
    Sleep(16);

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

            // Bandpass filter: analytic (Hilbert) bandpass, same overlap-save FFT stage ProcessCESSB's
            // clip/filter iterations reuse below - applied here up front so AGC's peak measurement
            // and CESSB both work from the same already band-limited (300-2695 Hz) analytic signal.
            ApplyCESSBBandpass(resampledAudioIn, cessbOut);

            // AGC: measure peak envelope magnitude of the (already band-limited) analytic signal,
            // update agcGain for next block
            Ipp32f cessbMag[CESSB_BLOCK];
            ippsMagnitude_32fc(cessbOut, cessbMag, CESSB_BLOCK);
            Ipp32f peakLevel;
            ippsMax_32f(cessbMag, CESSB_BLOCK, &peakLevel);
            if (peakLevel > 0.001f)
            {
                float desired = agcGain * agcTarget / peakLevel;
                desired = fmaxf(agcMinGain, fminf(agcMaxGain, desired));
                float coeff = (desired < agcGain) ? agcAttack : agcRelease;
                agcGain += coeff * (desired - agcGain);
            }

            // CESSB: clip + reapply bandpass in place, then split into I/Q for resampling
#ifdef ENABLE_CESSB
            ProcessCESSB(cessbOut);
#endif
            for (int i = 0; i < CESSB_BLOCK; i++)
                { cessbI[i] = cessbOut[i].re; cessbQ[i] = cessbOut[i].im; }

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

                // Average-power limiter: samples are normalized to peak <= 1.0 (max power = 1.0),
                // so if this block's average power exceeds 35% of that, scale I/Q down instead of
                // copying unmodified - a coarser, block-level backstop on top of CESSB's per-sample
                // envelope clipping.
                Ipp32f sumI2, sumQ2;
                ippsSqr_32f(pI, pTXPowerScratch, 2048);
                ippsSum_32f(pTXPowerScratch, 2048, &sumI2, ippAlgHintFast);
                ippsSqr_32f(pQ, pTXPowerScratch, 2048);
                ippsSum_32f(pTXPowerScratch, 2048, &sumQ2, ippAlgHintFast);
                float avgPower = (sumI2 + sumQ2) / 2048.0f;
                float peakPower = peakLevel * peakLevel;

 //               fprintf(f, "%.3f,%.3f,%.3f\n", avgPower, peakPower, agcGain);

                // Scroll avgPower (pre-cap) into the TX plot history, oldest at [0], newest at [63]
                memmove(myStatus->TXAvgPowerPlot, myStatus->TXAvgPowerPlot + 1, 63 * sizeof(float));
                myStatus->TXAvgPowerPlot[63] = avgPower;

                for (int s = 0; s < 2048; s++) TXFFTData[s] = { pI[s], pQ[s] };
 /*               if (avgPower > 0.6f)
                {
                    float scale = sqrtf(0.6f / avgPower);
                    for (int s = 0; s < 2048; s++) TXFFTData[s] = { pI[s] * scale, pQ[s] * scale };
                }
                else
                {
                }*/


                //window
                ippsMul_32f32fc_I(TXHannWindow, TXFFTData, 2048);
                //FFT
                ippsFFTFwd_CToC_32fc_I(TXFFTData, pTFFTSpec, pTFFTWorkBuf);
                //Bandpass only — signal is already one-sided analytic from CESSB
                BandpassOneSided(TXFFTData);
                //Back to time domain
                ippsFFTInv_CToC_32fc_I(TXFFTData, pTFFTSpec, pTFFTWorkBuf);

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

    // Finish with 128, 0's after taper
    for (int s = 0; s < 4; s++)
    {
        BuildEmptyPacket(writeData);
        WriteFile(hSerial, writeData, 129, &bytesWritten, NULL);
    }
    //Dump residual ADC data if any
    //Sleep(16);
    //if (ADCWaitingCounter)
    //{
    //    ReadFile(hSerial, readData, 4 * ADCWaitingCounter, &bytesWritten, NULL);
    //}

    //Put back in receive mode

 //   fclose(f);

    ippsFree(TXIFFTAccum);
    ippsFree(pTXPowerScratch);
    ippsFree(pTFFTSpecBuf);
    ippsFree(pTFFTWorkBuf);
 //   ippsFree(pTFFTSpec);

//    writeData[0] = 'i';
    writeData[0] = 'u'; // New command to flush before idle
    writeData[1] = 'r';
    WriteFile(hSerial, writeData, 2, &bytesWritten, NULL); 

    Sleep(100); // Wait for data to finish
    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

    writeData[0] = 'a'; // ADC payload
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // query adc values once
    ReadFile(hSerial, readData, 4, &bytesWritten, NULL); // Read volt / current
    UpdateADCs(readData);

    // Ratchet g_max_amp down (never up - that only happens via the Max Amp dialog) so the next
    // transmission can't drive the modulator harder than this battery voltage supports, keeping
    // ~1V of headroom below the ~30V/260 hardware ceiling. Skip on an implausible (<=0) reading -
    // a disconnected/garbage ADC sample would otherwise zero out TX power until the dialog is reopened.
    if (myStatus->volts > 0.0f)
    {
        int voltLimitedMax = (int)floor(myStatus->volts / 31.0f * 260.0f + 0.5f);
        if (voltLimitedMax < g_max_amp)
        {
            g_max_amp = voltLimitedMax;
            g_abs_max_amp = (int)round(1.15 * g_max_amp);
        }
    }

    SetRXBits(0);
    Sleep(16);


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

    SetRXBits(0);

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
    if (myStatus->calMode == 6)
    {
        QuickSync();
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
    float minRMAG = 1.0;

    // Untuned sweep across the full 14.00-14.35 MHz range (36 points, matching the OSL cal table)
    for (int jj = 0; jj < 36; jj++)
    {
        int storeIdx = jj;
        SetFreq(14.0 + jj * 0.01);

        writeData[0] = 'w';
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL);
        Sleep(16);
        ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
        if (readData[0] != 'R')
            int g = 0;

        Ipp32fc S11 = GetS11(H2Window);
        S11 = GetCorrectedS11(storeIdx, S11);
        float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);

        myStatus->SmithChartUntuned[storeIdx * 2]     = S11.re;
        myStatus->SmithChartUntuned[storeIdx * 2 + 1] = S11.im;
        myStatus->SWRUntuned[storeIdx]                 = (1.0f + Rmag) / (1.0f - Rmag);
        myVNACal->SweptIQMu[storeIdx]                  = m_lastIQMu;
        myStatus->UpdateVSWR = true;
    }

    // Calibration/store index for lastLO within the 36-point table (14.0-14.35 MHz)
    int tuneCalIdx = (int)round((lastLO - 14.0) / 0.01);
    if (tuneCalIdx < 0)  tuneCalIdx = 0;
    if (tuneCalIdx > 35) tuneCalIdx = 35;
    int tuneStoreIdx = tuneCalIdx;

    if (myStatus->calMode == 3)
    {
        // Phase 2: relay search at current operating frequency across all 32 relay settings
        SetFreq(lastLO);
        for (int jj = 0; jj < 32; jj++)
        {
            writeData[0] = 'c';
            writeData[1] = 0x20 + jj;
            writeData[2] = 0x20 + 0x3B;
            WriteFile(hSerial, writeData, 3, &bytesWritten, NULL);
            Sleep(16);

            writeData[0] = 'w';
            WriteFile(hSerial, writeData, 1, &bytesWritten, NULL);
            Sleep(16);
            ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
            if (readData[0] != 'R')
                int g = 0;

            Ipp32fc S11 = GetS11(H2Window);
            S11 = GetCorrectedS11(tuneCalIdx, S11);
            float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);
            if (Rmag < minRMAG) { minRMAG = Rmag; bestRelaySetting = jj; }
            myStatus->UpdateVSWR = true;
        }

        // Phase 3: tuned sweep across the full 14.00-14.35 MHz range at the best relay setting
        RelaySettings = bestRelaySetting;
        writeData[0] = 'c';
        writeData[1] = 0x20 + RelaySettings;
        writeData[2] = 0x20 + 0x3B;
        WriteFile(hSerial, writeData, 3, &bytesWritten, NULL);
        Sleep(16);

        for (int jj = 0; jj < 36; jj++)
        {
            int storeIdx = jj;
            SetFreq(14.0 + jj * 0.01);

            writeData[0] = 'w';
            WriteFile(hSerial, writeData, 1, &bytesWritten, NULL);
            Sleep(16);
            ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
            if (readData[0] != 'R')
                int g = 0;

            Ipp32fc S11 = GetS11(H2Window);
            S11 = GetCorrectedS11(25, S11);
            float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);

            myStatus->SmithChartTuned[storeIdx * 2]     = S11.re;
            myStatus->SmithChartTuned[storeIdx * 2 + 1] = S11.im;
            myStatus->SWRTuned[storeIdx]                 = (1.0f + Rmag) / (1.0f - Rmag);
            myStatus->UpdateVSWR = true;
        }

        sprintf_s(dbgText, "TUNE %d %.2f", bestRelaySetting, myStatus->SWRTuned[tuneStoreIdx]);
    }

    SetFreq(lastLO);
    SetRXBits(0);
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

	SetRXBits(0);

	Sleep(16);

	//writeData[0] = 'i';
	//WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in RX mode
	writeData[0] = 'r';
	WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Put device in RX mode
	myStatus->mode = 1;

}

void CRadio::QuickSync()
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

    {
        writeData[0] = 'w'; // idle vna mode
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 8 * 250 IQ values
        Sleep(16);
        ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
        if (readData[0] != 'R')
            int g = 0;

        writeData[1] = 0x20 + RelaySettings;
        writeData[2] = 0x20 + 0x3B; // FWD power
        WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
        Sleep(16);

        Ipp32fc S11 = GetS11(H2Window);
        S11 = GetCorrectedS11(25, S11);

        float Rmag = sqrt(S11.re * S11.re + S11.im * S11.im);

     }

    SetRXBits(0);

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
        else if (myStatus->mode == CW_TX_MODE) // CW send
        {
            CWTXDataLoop();
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
    }

	return 0;
}
int CRadio::SetFreq(double freqMHz)
{
    char writeData[8];
    DWORD bytesWritten = 0;
    double loLimit = gUseDebugWaveform ? 13.000 : 14.000;
    double hiLimit = gUseDebugWaveform ? 15.000 : 14.350;
    if (freqMHz < loLimit) freqMHz = loLimit;
    if (freqMHz > hiLimit) freqMHz = hiLimit;
    LOfreq = freqMHz;
    m_iFreq = (int)round(LOfreq * 1000000.0);
    writeData[0] = 'f';
    writeData[1] = 0x20 + ((m_iFreq >> 18) & 0x3F);
    writeData[2] = 0x20 + ((m_iFreq >> 12) & 0x3F);
    writeData[3] = 0x20 + ((m_iFreq >> 6) & 0x3F);
    writeData[4] = 0x20 + ((m_iFreq) & 0x3F);
    WriteFile(hSerial, writeData, 5, &bytesWritten, NULL); // Set frequency

    //Compute PWM settings
//    int pwmval = int(2400.0 / freqMHz) - 0x60;
    //int pwmval = 169 - 0x60;
    //writeData[0] = 'p';
    //writeData[1] = pwmval;
    //WriteFile(hSerial, writeData, 2, &bytesWritten, NULL); // Set frequency
    return bytesWritten;
}

int CRadio::SetRXBits(uint8_t rly)
{
    char writeData[8];
    DWORD bytesWritten = 0;

    writeData[0] = 'c';
    writeData[1] = 0x20 + rly;
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
//    port = 3; //DEBUG
    fscanf_s(f, "%d", &AudioInputChannels);
 //   fscanf_s(f, "%f", &LOfreq);
    fscanf_s(f, "%d", &RelaySettings);
    fclose(f);
    comPort = port;

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
        MessageBox(NULL, L"Device not found. Check COM port settings", L"COM port error", MB_OK);

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

    writeData[0] = 'a'; // ADC payload
    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // query adc values once
    ReadFile(hSerial, readData, 4, &bytesWritten, NULL); // Read volt / current
    UpdateADCs(readData);

    is8S = (myStatus->volts > 27.0);
    writeData[0] = 'z';
    writeData[1] = (is8S)? 0x21 : 0x20;// Battery range
    WriteFile(hSerial, writeData, 2, &bytesWritten, NULL); // 
    Sleep(16);


    isRXOnly = (myStatus->volts < 12.5);
    if (isRXOnly)
    {
        writeData[0] = 'h'; // Disable 24 V
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // Set frequency
        Sleep(16);
    }
	writeData[0] = 'c';
	writeData[1] = 0x20;// +RelaySettings;
	writeData[2] = 0x20 + 0x3E; // RX
	WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // 
    Sleep(16);

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);


    myStatus->mode = 1;
    myDThread = std::thread(ProcessDataThread, 1, this); // Creates a thread executing myFunction with argument 1
    

SkipComPortStuff:
    err = Pa_StartStream(stream);
    //Pa_Sleep(1000);
    //err = Pa_StopStream(stream);
    if (err != paNoError) {
        MessageBox(NULL, L"Audio Driver Fail. Check that mic is plugged in", L"Audio Error", MB_OK);
        sprintf_s(dbgText, "AUDIO ERR");
        return 0;
    }

    sprintf_s(dbgText, "Connect done");

//    DataThread();//debug
//    sprintf_s(dbgText, "stop data");

 	return 1;
}
