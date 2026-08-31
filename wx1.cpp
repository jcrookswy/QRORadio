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

// Start of wxWidgets "Hello World" Program
#include <wx/wx.h>
#include <wx/numdlg.h>
#include <wx/listctrl.h>
#include <cmath>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include <map>
//#include "MyProjectBase.h"
#include "frame1.h"
#include "CRadio.h"

class MyApp : public wxApp
{
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(MyApp);

/*
class MyFrame : public wxFrame
{
public:
    MyFrame();

private:
    void OnHello(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
};

enum
{
    ID_Hello = 1
};
*/
bool MyApp::OnInit()
{
    MyFrame* frame = new MyFrame(NULL);
    frame->Show(true);
    return true;
}

//BasicDrawPane::BasicDrawPane(wxFrame* parent) :
//    wxPanel(parent)
//{
//}
void BasicDrawPane::paintEvent(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
    render(dc);
}
void BasicDrawPane::paintNow()
{
    wxClientDC dc(this);
    render(dc);
}

void BasicDrawPane::OnSize(wxSizeEvent& event)
{
    isPartial = false;
    Refresh(false);
    event.Skip();
}

void SmithPlot(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, wchar_t* text)
{
    // draw a rectangle
    dc.SetBrush(wxBrush(wxColor(48, 48, 48))); //
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3)); // 5-pixels-thick border
    dc.DrawRectangle(xofs, yofs, xsize, ysize); // Draw outer perimeter
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3)); // 3-pixels-thick border
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);

    xofs += 3;
    yofs += 3;
    xsize -= 6;
    ysize -= 6;

    {
        wxSize tsz = dc.GetTextExtent(text);
        int titleH = tsz.GetHeight() + 6;
        dc.DrawText(text, xofs + 10, yofs + 3);
        yofs += titleH;
        ysize -= titleH;
    }

    dc.SetPen(wxPen(wxColor(255, 255, 255), 1)); // 1-pixels-thick pink outline
    int radius = xsize >> 1;
    int halfRadius = xsize >> 2;
    int xmid = xofs + radius;
    int ymid = yofs + radius;
    //Draw Smith Chart
    dc.DrawCircle(xmid, ymid, radius);
    dc.DrawCircle(xmid + halfRadius, ymid, halfRadius);
    dc.DrawLine(xmid-radius, ymid, xmid + radius, ymid); // draw line across the rectangle

//    dc.DrawArc(xmid, ymid - radius, xmid + radius, ymid, xmid + radius, ymid - radius);
//    dc.DrawArc(xmid, ymid + radius, xmid + radius, ymid, xmid + radius, ymid + radius);

    int lastx = 0, lasty = 0, x1 = 0, y1 = 0, y2 = 0;
    int lasty2 = 0;

    for (int i = 0; i <= 32; i++)
    {
        float angle = 0.5 * 3.14159265 * i / 32.0; // 1/4 arc
        x1 = xmid + radius - radius * sin(angle);
        y1 = ymid + radius - radius * cos(angle);
        y2 = ymid - radius + radius * cos(angle);

        if (i > 0) {
            dc.DrawLine(lastx, lasty, x1, y1); // draw line across the rectangle
            dc.DrawLine(lastx, lasty2, x1, y2); // draw line across the rectangle
        }
        lastx = x1;
        lasty = y1;
        lasty2 = y2;
    }

    for (int i = 0; i <= 32; i++)
    {
        float angle = 0.25 * 3.14159265 * i / 32.0; // 1/4 arc
        x1 = xmid + radius - 2.4142 * radius * sin(angle);
        y1 = ymid + 2.4142 * radius - 2.4142 * radius * cos(angle);
        y2 = ymid - 2.4142 * radius + 2.4142 * radius * cos(angle);

        if (i > 0) {
            dc.DrawLine(lastx, lasty, x1, y1); // draw line across the rectangle
            dc.DrawLine(lastx, lasty2, x1, y2); // draw line across the rectangle
        }
        lastx = x1;
        lasty = y1;
        lasty2 = y2;
    }

    dc.SetPen(wxPen(wxColor(255, 255, 128), 2)); // yellow pen
    for (int i = 0; i < numElements; i++)
    {
        x1 = xmid + radius * data[i * 2];
        y1 = ymid - radius * data[i * 2 + 1];
        if (i > 0) {
            dc.DrawLine(lastx, lasty, x1, y1); // draw line across the rectangle
        }
        lastx = x1;
        lasty = y1;
    }

}

void SmithPlot2(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float* data2, wchar_t* text)
{
    SmithPlot(dc, xofs, yofs, xsize, ysize, numElements, data, text);
    xofs += 3; // Border and scale to match "plot"
    {
        wxSize tsz = dc.GetTextExtent(text);
        int titleH = tsz.GetHeight() + 6;
        yofs += 3 + titleH;
        ysize -= 6 + titleH;
    }
    xsize -= 6;
    int radius = xsize >> 1;
    int xmid = xofs + radius;
    int ymid = yofs + radius;
    int lastx = 0, lasty = 0, x1 = 0, y1 = 0;

    dc.SetPen(wxPen(wxColor(160, 255, 160), 2)); // green pen
    for (int i = 0; i < numElements; i++)
    {
        x1 = xmid + radius * data2[i * 2];
        y1 = ymid - radius * data2[i * 2 + 1];
        if (i > 0) {
            dc.DrawLine(lastx, lasty, x1, y1); // draw line 
        }
        lastx = x1;
        lasty = y1;
    }

}

static void SpecLevelToRGB(float level, unsigned char& r, unsigned char& g, unsigned char& b)
{
    if (level <= 0.0f) { r = g = b = 0; return; }
    if (level >= 1.0f) { r = g = b = 255; return; }
    float t;
    if (level < 0.25f) {
        t = level * 4.0f;
        r = 0; g = 0; b = (unsigned char)(t * 255.0f);
    } else if (level < 0.5f) {
        t = (level - 0.25f) * 4.0f;
        r = 0; g = (unsigned char)(t * 255.0f); b = (unsigned char)((1.0f - t) * 255.0f);
    } else if (level < 0.75f) {
        t = (level - 0.5f) * 4.0f;
        r = (unsigned char)(t * 255.0f); g = 255; b = 0;
    } else {
        t = (level - 0.75f) * 4.0f;
        r = 255; g = 255; b = (unsigned char)(t * 255.0f);
    }
}

//Plot widget (float)
//inputs: array size, array pointer, x offset, y offset, x size, y size, y min, y max, horz grat count, vert grat count
void Plot(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float ymin, float ymax, int hgrat, int ygrat, wchar_t* text, int wfallPix = 0, int shadeL = -1, int shadeR = -1, unsigned char* wfPixels = nullptr, int wfBins = 0, int wfRows = 0)
{
    // draw a rectangle
    dc.SetBrush(wxBrush(wxColor(48, 48, 48))); //
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3)); // 5-pixels-thick border
    dc.DrawRectangle(xofs, yofs, xsize, ysize); // Draw outer perimeter
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3)); // 3-pixels-thick border
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);

    xofs += 3;
    yofs += 3;
    xsize -= 6;
    ysize -= 6;

    {
        wxSize tsz = dc.GetTextExtent(text);
        int titleH = tsz.GetHeight() + 6;
        dc.DrawText(text, xofs + 10, yofs + 3);
        yofs += titleH;
        ysize -= titleH;
    }

    if (wfallPix > 0)
    {
        int wfY = yofs + ysize - wfallPix + 2;
        int wfH = wfallPix - 4;
        if (wfPixels && wfBins > 0 && wfRows > 0 && wfH > 0)
        {
            wxImage img(wfBins, wfRows, wfPixels, true);
            wxBitmap bmp(img);
            wxMemoryDC mdc(bmp);
            dc.StretchBlit(xofs, wfY, xsize, wfH, &mdc, 0, 0, wfBins, wfRows);
        }
        else
        {
            dc.SetPen(wxPen(wxColor(32, 32, 32), 0));
            dc.SetBrush(wxBrush(wxColor(0, 32, 192)));
            dc.DrawRectangle(xofs, wfY, xsize, wfH);
        }
        ysize -= wfallPix;
    }


    // Optional passband shading (e.g. USB region) — drawn before graticules so lines show on top
    if (shadeL >= 0 && shadeR > shadeL && numElements > 1)
    {
        int sx0 = xofs + shadeL * xsize / (numElements - 1);
        int sx1 = xofs + shadeR * xsize / (numElements - 1);
        dc.SetBrush(wxBrush(wxColor(58, 58, 58)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(sx0, yofs, sx1 - sx0, ysize);
    }

    //dc.SetBrush(*wxBLACK_BRUSH); // black filling
    dc.SetPen(wxPen(wxColor(255, 255, 255), 1)); // 1-pixels-thick pink outline
    //dc.DrawRectangle(xofs, yofs, xsize , ysize ); // Draw outer perimeter
    for (int i = 0; i <= hgrat; i++)
    {
        int ypos = yofs + i * ysize / hgrat;
        dc.DrawLine(xofs, ypos, xsize + xofs, ypos); // draw line across the rectangle
    }
    for (int i = 0; i <= ygrat; i++)
    {
        int xpos = xofs + i * xsize / ygrat;
        dc.DrawLine(xpos, yofs, xpos, ysize + yofs); // draw line across the rectangle
    }

    dc.SetClippingRegion(xofs, yofs, xsize, ysize);
    dc.SetPen(wxPen(wxColor(255, 255, 128), 2)); // yellow pen
    int xposLast = 0;
    int yposLast = 0;
    for (int i = 0; i < numElements; i++)
    {
        int xpos = (int)(xofs + 1.0 * i * xsize / (numElements - 1));
        int ypos = (int)(yofs + (ymax - data[i]) * ysize / (ymax - ymin));
        if (ypos < yofs) ypos = yofs;
        if (ypos > (yofs + ysize)) ypos = yofs + ysize;

        if (i > 0)
            dc.DrawLine(xposLast, yposLast, xpos, ypos);
        xposLast = xpos;
        yposLast = ypos;
    }
    dc.DestroyClippingRegion();
}

// Same panel chrome as the old DrawCWText, but shows CRadio::kCWMaxSlicers stacked single-line
// rows, one per CW FFT slicer: a frequency label ("701 Hz" / "--- Hz" if never used) followed by
// that slicer's trailing decoded text (ticker-style - only the tail that fits is shown).
void DrawCWSlicerRows(wxDC& dc, int xofs, int yofs, int xsize, int ysize, CRadio* pRadio, wchar_t* label)
{
    dc.SetBrush(wxBrush(wxColor(64, 64, 64)));
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3));
    dc.DrawRectangle(xofs, yofs, xsize, ysize);
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3));
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);

    dc.SetTextForeground(wxColor(255, 255, 0));
    dc.DrawText(label, xofs + 6, yofs + 4);

    wxCoord charW, charH;
    dc.GetTextExtent(_T("M"), &charW, &charH);
    if (charW < 1) charW = 1;
    int lineH = charH + 2;

    int textTop = yofs + 4 + lineH;
    const int kRows = CRadio::kCWMaxSlicers;
    const int kHistoryRows = 4; // 2 rows of 8 mark lengths + 2 rows of 8 space lengths
    int rowH = (yofs + ysize - textTop) / (kRows + kHistoryRows);
    if (rowH < lineH) rowH = lineH;

    const float kCWBinHz = 48000.0f / CRadio::kCWFFTSize;

    for (int i = 0; i < kRows; i++)
    {
        CWSlicer& slicer = pRadio->cwSlicers[i];
        int y = textTop + i * rowH;

        wchar_t freqLabel[16];
        if (slicer.used)
            swprintf_s(freqLabel, _T("%4.0f Hz"), slicer.binIndex * kCWBinHz);
        else
            swprintf_s(freqLabel, _T(" --- Hz"));

        dc.SetTextForeground(wxColor(255, 255, 0));
        dc.DrawText(freqLabel, xofs + 6, y);

        wxCoord freqW, freqH;
        dc.GetTextExtent(freqLabel, &freqW, &freqH);
        int textX = xofs + 6 + freqW + charW;

        int charsAvail = (xofs + xsize - 6 - textX) / charW;
        if (charsAvail < 1) charsAvail = 1;

        wxString full(slicer.text, wxConvUTF8);
        wxString tail = (full.length() > (size_t)charsAvail)
            ? full.Mid(full.length() - charsAvail)
            : full;

        dc.SetTextForeground(wxColor(0, 255, 0));
        dc.DrawText(tail, textX, y);
    }

    // Diagnostic rows: smear-corrected mark/space run-length history (in hops) for the most
    // recently active slicer, so cwSquelch and the mark/space correction can be tuned without
    // recompiling. "Most recently active" = smallest hopsSinceMark among used slicers.
    int mostRecent = -1;
    for (int i = 0; i < kRows; i++)
    {
        if (!pRadio->cwSlicers[i].used) continue;
        if (mostRecent < 0 || pRadio->cwSlicers[i].hopsSinceMark < pRadio->cwSlicers[mostRecent].hopsSinceMark)
            mostRecent = i;
    }

    if (mostRecent >= 0)
    {
        CWSlicer& slicer = pRadio->cwSlicers[mostRecent];
        int y = textTop + kRows * rowH;
        wchar_t line[96];

        const float kCWHopMs = 1000.0f * CRadio::kCWHopSize / 48000.0f;

        dc.SetTextForeground(wxColor(0, 255, 0));
        for (int row = 0; row < 2; row++)
        {
            int* v = &slicer.markLengths[row * 8];
            if (row == 1)
            {
                // 9th value: current adaptive dot-length estimate, in hops, for comparison
                // against the raw mark lengths above it.
                float dotUnitHops = slicer.dotUnitMs / kCWHopMs;
                swprintf_s(line, _T("Marks : %2d %2d %2d %2d %2d %2d %2d %2d %.1f"),
                    v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], dotUnitHops);
            }
            else
            {
                swprintf_s(line, _T("Marks : %2d %2d %2d %2d %2d %2d %2d %2d"),
                    v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
            }
            dc.DrawText(line, xofs + 6, y);
            y += rowH;
        }
        for (int row = 0; row < 2; row++)
        {
            int* v = &slicer.spaceLengths[row * 8];
            swprintf_s(line, _T("Spaces: %2d %2d %2d %2d %2d %2d %2d %2d"),
                v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
            dc.DrawText(line, xofs + 6, y);
            y += rowH;
        }
    }
}

void Plot2(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float ymin, float ymax, int hgrat, int ygrat, wchar_t* text, float* data2)
{
    Plot(dc, xofs, yofs, xsize, ysize, numElements, data, ymin, ymax, hgrat, ygrat, text);

    xofs += 3; // Border and scale to match "plot"
    {
        wxSize tsz = dc.GetTextExtent(text);
        int titleH = tsz.GetHeight() + 6;
        yofs += 3 + titleH;
        ysize -= 6 + titleH;
    }
    xsize -= 6;

    dc.SetPen(wxPen(wxColor(160, 255, 160), 2)); // green pen

    int xposLast = 0;
    int yposLast = 0;
    for (int i = 0; i < numElements; i++)
    {
        int xpos = (int)(xofs + 1.0 * i * xsize / (numElements - 1));
        int ypos = (int)(yofs + (ymax - data2[i]) * ysize / (ymax - ymin));
        if (ypos < yofs) ypos = yofs;
        if (ypos > (yofs + ysize)) ypos = yofs + ysize;

        if (i > 0)
            dc.DrawLine(xposLast, yposLast, xpos, ypos); // draw line across the rectangle
        xposLast = xpos;
        yposLast = ypos;
    }

}

//Adds boxes of text in a row; scale=1 gives the original 24pt / 60px-tall layout.
int PrettyText(wxDC& dc, int xofs, int yofs, int numChars, wchar_t* text, float scale = 1.0f)
{
    int xsize = (int)(scale * (numChars * 20 + 32));
    int ysize = (int)(60 * scale);
    dc.SetBrush(wxBrush(wxColor(64, 64, 64)));
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3));
    dc.DrawRectangle(xofs, yofs, xsize, ysize);
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3));
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);
    dc.DrawText(text, xofs + (int)(20 * scale), yofs + (int)(15 * scale));
    return xsize;
}

void BasicDrawPane::render(wxDC& dc)
{
    int panelW, panelH;
    GetClientSize(&panelW, &panelH);

    // Dynamic layout — all positions derived from actual panel size
    const int margin = 10;
    const int gap = 8;

    // Text row scaling: natural box total at 1x (24pt) = 272+152+232+72+272 = 1000px;
    // overhead = initial xofs(12) + 5 inter-box gaps(8) = 52px.
    float textScale = (float)(panelW - 52) / 1000.0f;
    if (textScale < 0.5f) textScale = 0.5f;
    if (textScale > 2.5f) textScale = 2.5f;
    int topH = (int)(60.0f * textScale) + 10; // scaled box height + small margin
    int textFontPt = (int)(24.0f * textScale);
    if (textFontPt < 8) textFontPt = 8;

    // Vertical split: middle plots row (~47%), bottom plot (rest)
    int midY = topH + margin;
    int availH = panelH - midY - 2 * margin;
    int midH = (availH * 47) / 100;
    int botY = midY + midH + gap;
    int botH = panelH - botY - margin;

    // Horizontal split: 3 equal plots + Smith chart (slightly taller than wide).
    // SmithPlot subtracts 6px border + 24px title before drawing circle, so for a
    // round circle the box must satisfy height = width + 24 → smithW = midH - 24.
    int smithW = midH - 24;
    int availW = panelW - 2 * margin - 3 * gap;
    int plotW = (availW - smithW) / 3;

    int x1 = margin;
    int x2 = x1 + plotW + gap;
    int x3 = x2 + plotW + gap;
    int x4 = x3 + plotW + gap;
    int botW = panelW - 2 * margin;

    wxFont f = dc.GetFont();
    f.SetPointSize(textFontPt);
    f.MakeBold();
    f.SetFamily(wxFONTFAMILY_TELETYPE);
    dc.SetFont(f);
    dc.SetTextForeground(wxColor(255, 255, 0));

    if (!isPartial) { // Everything modified if it's not a partial redraw
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        textModified = true;
        VSWRModified = true;
        audioModified = true;
        RFModified = true;
    }
    else
    {
        textModified = pRadio->myStatus->UpdateText;
        VSWRModified = pRadio->myStatus->UpdateVSWR;
        //debug audioModified = pRadio->myStatus->UpdateAudio;
        //RFModified = pRadio->myStatus->UpdateRFPlot;
    }

    pRadio->myStatus->UpdateText = false;
    pRadio->myStatus->UpdateVSWR = false;
    pRadio->myStatus->UpdateAudio = false;
    pRadio->myStatus->UpdateRFPlot = false;

    int xofs = 12;
    if (textModified)
    {
        textModified = false;
        wchar_t freqText[16];
        swprintf_s(freqText, _T("%.2f kHz"), pRadio->myStatus->RXFreq * 1000.0);
        int extraGap = 8;
        xofs += extraGap + PrettyText(dc, xofs, 5, 12, freqText, textScale);

        wchar_t powerText[16];
        if (pRadio->myStatus->Sunit > 9)
            swprintf_s(powerText, _T("USB 9+"));
        else
            swprintf_s(powerText, _T("USB S%d"), pRadio->myStatus->Sunit);
        xofs += extraGap + PrettyText(dc, xofs, 5, 6, powerText, textScale);

        wchar_t txText[16];
        char voltText[16];
        float displayAmps = pRadio->hasTransmitted ? pRadio->myStatus->amps : 0.0f;
        float watts = pRadio->myStatus->volts * displayAmps;
        if (watts < 10.0)
            sprintf_s(voltText, "%.1fV %.1fW", pRadio->myStatus->volts, watts);
        else
            sprintf_s(voltText, "%.1fV %.0fW", pRadio->myStatus->volts, watts);
        mbstowcs(txText, voltText, 16);
        float lowBattThresh = pRadio->is8S ? 28.0f : 21.0f; // 3.5V/cell, 8S or 6S
        if (pRadio->myStatus->volts < lowBattThresh)
            dc.SetTextForeground(wxColor(255, 64, 64));
        xofs += extraGap + PrettyText(dc, xofs, 5, 10, txText, textScale);

        dc.SetTextForeground(wxColor(128, 255, 128));
        wchar_t moreText[16] = _T("RX");
        xofs += extraGap + PrettyText(dc, xofs, 5, 2, moreText, textScale);

        dc.SetTextForeground(wxColor(192, 192, 192));
        wchar_t timeText[16];
        mbstowcs(timeText, pRadio->myStatus->GMTTime, 16);
        xofs += extraGap + PrettyText(dc, xofs, 5, 12, timeText, textScale);
    }

    float data[10] = { 0.1, 0.4, 0.2, 0.3, 0.8, 0.8, 0.3, 0.5, 0.2, 0.1 };
    int numElements = 10;

    int plotFontPt = textFontPt / 2;
    if (plotFontPt < 8) plotFontPt = 8;
    f.SetPointSize(plotFontPt);
    dc.SetFont(f);
    dc.SetTextForeground(wxColor(255, 255, 0));

    if (VSWRModified)
    {
        VSWRModified = false;
        wchar_t plot1Label[36] = _T("VSWR 14.00-14.35 MHz 0.5/");
        Plot2(dc, x3, midY, plotW, midH, 36, &pRadio->myStatus->SWRUntuned[0], 1.0, 3.0, 4, 7, plot1Label, &pRadio->myStatus->SWRTuned[0]);

        wchar_t plot8Label[24] = _T("Antenna Impedance");
        SmithPlot2(dc, x4, midY, smithW, midH, 36, &pRadio->myStatus->SmithChartUntuned[0], &pRadio->myStatus->SmithChartTuned[0], plot8Label);
    }

    if (audioModified)
    {
        audioModified = false;
        if (pRadio->CWModeEnabled )
        {
            wchar_t cwLabel[24] = _T("CW Decode (200-2800 Hz)");
            DrawCWSlicerRows(dc, x1, midY, plotW + gap + plotW, midH, pRadio, cwLabel);
        }
        else
        {
            wchar_t plot3Label[20] = _T("Audio (freq) 1kHz/");
            Plot(dc, x1, midY, plotW, midH, 16, pRadio->myStatus->AudioFreqPlot, 0.0, 1.0, 4, 3, plot3Label);

            if (pRadio->myStatus->mode == TX_MODE)
            {
                wchar_t plot4Label[24] = _T("TX Avg Power 0.2/");
                Plot(dc, x2, midY, plotW, midH, 64, pRadio->myStatus->TXAvgPowerPlot, 0.0, 1.0, 5, 5, plot4Label);
            }
            else
            {
                wchar_t plot4Label[20] = _T("Audio (time) 1ms/");
                Plot(dc, x2, midY, plotW, midH, 128, pRadio->myStatus->AudioTimePlot, -1.0, 1.0, 4, 5, plot4Label);
            }
        }
    }

    if (RFModified)
    {
        RFModified = false;
        char rfText[64];
        sprintf_s(rfText, "RF Power vs Freq, %.3f - %.3f MHz, 5 kHz/, 6 dB/", pRadio->myStatus->TunerFreq - 0.02, pRadio->myStatus->TunerFreq + 0.02);
        wchar_t plot7Label[64];
        mbstowcs(plot7Label, rfText, 64);
        // Map S-unit floor/range to log10(MagData) scale: dBm = 20*log10(MagData) - 46.94
        // With calibration offset: raw_dBm = calibrated_dBm - plotSoffset
        float dBmFloor = -73.0f + (pRadio->plotSfloor - 9) * 6.0f;
        float dBmTop   = dBmFloor + pRadio->plotSunits * 6.0f;
        float ymin = (dBmFloor - pRadio->plotSoffset + 46.94f) / 20.0f;
        float ymax = (dBmTop   - pRadio->plotSoffset + 46.94f) / 20.0f;

        // Waterfall: find per-row min, shift history down, write new top row
        float* spec = pRadio->myStatus->RFFreqPlot;
        float wfMin = spec[0];
        for (int i = 1; i < WF_BINS; i++)
            if (spec[i] < wfMin) wfMin = spec[i];
        const float wfRange = 1.8f; // 36 dB in log10(magnitude) units
        memmove(wfPixels + WF_BINS * 3, wfPixels, (size_t)WF_BINS * 3 * (WF_ROWS - 1));
        unsigned char* row0 = wfPixels;
        const float wfHalf = (WF_BINS - 1) * 0.5f;
        for (int i = 0; i < WF_BINS; i++)
        {
            float t = 1.0f - fabsf(i - wfHalf) / wfHalf; // 0 at edges, 1 at center
            float offset = 0.25f + t * 0.25f;             // 5 dB at edges, 10 dB at center
            float level = (spec[i] - wfMin - offset) / wfRange;
            if (level < 0.0f) level = 0.0f;
            SpecLevelToRGB(level, row0[i * 3], row0[i * 3 + 1], row0[i * 3 + 2]);
        }

        // shadeL=125 (DC center), shadeR=141 (center+3 kHz = 16 bins × 187.5 Hz)
        Plot(dc, margin, botY, botW, botH, 250, spec, ymin, ymax, pRadio->plotSunits, 8, plot7Label, WF_ROWS, 125, 141, wfPixels, WF_BINS, WF_ROWS);
    }
    isPartial = false;
};


    // draw a line
   // dc.SetPen(wxPen(wxColor(0, 0, 0), 3)); // black line, 3 pixels thick
   // dc.DrawLine(300, 100, 700, 300); // draw line across the rectangle

    // Look at the wxDC docs to learn how to draw other stuff


///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////

class CabrilloSettingsDialog : public wxDialog
{
public:
    CabrilloSettingsDialog(MyFrame* parent)
        : wxDialog((wxWindow*)parent, wxID_ANY, _("Contest Settings"),
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_parent(parent)
    {
        CRadio* r = parent->myRadio;

        wxFlexGridSizer* grid = new wxFlexGridSizer(0, 2, 5, 8);
        grid->AddGrowableCol(1);

        auto AddRow = [&](const wxString& lbl, wxTextCtrl*& ctrl,
                          const wxString& val, int maxLen) {
            grid->Add(new wxStaticText(this, wxID_ANY, lbl),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
            ctrl = new wxTextCtrl(this, wxID_ANY, val,
                                  wxDefaultPosition, wxSize(220, -1));
            ctrl->SetMaxLength(maxLen);
            grid->Add(ctrl, 1, wxEXPAND);
        };

        AddRow(_("Contest ID:"), m_contestID, wxString(r->contestID, wxConvUTF8), 24);

        grid->Add(new wxStaticText(this, wxID_ANY, _("Exchange Template:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        wxString choices[] = { _("None"), _("Serial Number"), _("Field Day (Class + Section)"), _("State/Section QSO Party") };
        m_exchangeTemplate = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 4, choices);
        m_exchangeTemplate->SetSelection(r->exchangeTemplate);
        grid->Add(m_exchangeTemplate, 1, wxEXPAND);

        AddRow(_("My Exchange (fixed):"), m_myExchange, wxString(r->myExchange, wxConvUTF8), 32);
        AddRow(_("Next Serial #:"),       m_nextSerial, wxString::Format("%d", r->nextSerialSent), 8);
        AddRow(_("Category Mode:"),       m_categoryMode, wxString(r->categoryMode, wxConvUTF8), 16);
        AddRow(_("Category Power:"),      m_categoryPower, wxString(r->categoryPower, wxConvUTF8), 16);
        AddRow(_("Operator Name:"),       m_operatorName, wxString(r->operatorName, wxConvUTF8), 32);
        AddRow(_("Address:"),             m_addressLine, wxString(r->addressLine, wxConvUTF8), 48);
        AddRow(_("City:"),                m_addressCity, wxString(r->addressCity, wxConvUTF8), 32);
        AddRow(_("State:"),                m_addressState, wxString(r->addressState, wxConvUTF8), 16);
        AddRow(_("Postal Code:"),         m_addressPostal, wxString(r->addressPostal, wxConvUTF8), 16);
        AddRow(_("Country:"),             m_addressCountry, wxString(r->addressCountry, wxConvUTF8), 32);
        AddRow(_("Claimed Score:"),       m_claimedScore, wxString(r->claimedScore, wxConvUTF8), 16);

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(grid, 1, wxEXPAND | wxALL, 12);
        wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
        wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
        btnSizer->Add(ok, 0, wxALL, 4);
        btnSizer->Add(cancel, 0, wxALL, 4);
        sizer->Add(btnSizer, 0, wxALIGN_CENTER | wxBOTTOM, 8);
        SetSizer(sizer);
        Fit();

        ok->Bind(wxEVT_BUTTON, &CabrilloSettingsDialog::OnOK, this);
    }

private:
    MyFrame*    m_parent;
    wxTextCtrl* m_contestID;
    wxChoice*   m_exchangeTemplate;
    wxTextCtrl* m_myExchange;
    wxTextCtrl* m_nextSerial;
    wxTextCtrl* m_categoryMode;
    wxTextCtrl* m_categoryPower;
    wxTextCtrl* m_operatorName;
    wxTextCtrl* m_addressLine;
    wxTextCtrl* m_addressCity;
    wxTextCtrl* m_addressState;
    wxTextCtrl* m_addressPostal;
    wxTextCtrl* m_addressCountry;
    wxTextCtrl* m_claimedScore;

    void OnOK(wxCommandEvent&)
    {
        CRadio* r = m_parent->myRadio;
        strncpy_s(r->contestID,      sizeof(r->contestID),      m_contestID->GetValue().Upper().ToStdString().c_str(), _TRUNCATE);
        r->exchangeTemplate = m_exchangeTemplate->GetSelection();
        strncpy_s(r->myExchange,     sizeof(r->myExchange),     m_myExchange->GetValue().Upper().ToStdString().c_str(), _TRUNCATE);
        r->nextSerialSent = wxAtoi(m_nextSerial->GetValue());
        if (r->nextSerialSent < 1) r->nextSerialSent = 1;
        strncpy_s(r->categoryMode,   sizeof(r->categoryMode),   m_categoryMode->GetValue().Upper().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->categoryPower,  sizeof(r->categoryPower),  m_categoryPower->GetValue().Upper().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->operatorName,   sizeof(r->operatorName),   m_operatorName->GetValue().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->addressLine,    sizeof(r->addressLine),    m_addressLine->GetValue().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->addressCity,    sizeof(r->addressCity),    m_addressCity->GetValue().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->addressState,   sizeof(r->addressState),   m_addressState->GetValue().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->addressPostal,  sizeof(r->addressPostal),  m_addressPostal->GetValue().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->addressCountry, sizeof(r->addressCountry), m_addressCountry->GetValue().ToStdString().c_str(), _TRUNCATE);
        strncpy_s(r->claimedScore,   sizeof(r->claimedScore),   m_claimedScore->GetValue().ToStdString().c_str(), _TRUNCATE);
        r->SaveSettings("settings.json");
        EndModal(wxID_OK);
    }
};

///////////////////////////////////////////////////////////////////////////

class LogDialog : public wxDialog
{
public:
    LogDialog(MyFrame* parent, const wxString& lastCall)
        : wxDialog((wxWindow*)parent, wxID_ANY, _("Log Contact"),
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_parent(parent)
    {
        char freqBuf[16], rstRcvdBuf[8];
        sprintf_s(freqBuf, "%.4f", parent->myRadio->LOfreq);

        int sunit = parent->myRadio->myStatus->Sunit;
        if (sunit > 9)       sprintf_s(rstRcvdBuf, "59+");
        else if (sunit >= 1) sprintf_s(rstRcvdBuf, "5%d", sunit);
        else                 sprintf_s(rstRcvdBuf, "59");

        wxFlexGridSizer* grid = new wxFlexGridSizer(0, 2, 5, 8);
        grid->AddGrowableCol(1);

        auto AddRow = [&](const wxString& lbl, wxTextCtrl*& ctrl,
                          const wxString& val, int maxLen, long style = 0) {
            grid->Add(new wxStaticText(this, wxID_ANY, lbl),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
            ctrl = new wxTextCtrl(this, wxID_ANY, val,
                                  wxDefaultPosition, wxSize(220, -1), style);
            ctrl->SetMaxLength(maxLen);
            grid->Add(ctrl, 1, wxEXPAND);
        };

        AddRow(_("Their Call:"),     m_call,    lastCall,                    16, wxTE_PROCESS_ENTER);
        AddRow(_("Their Name:"),     m_name,    wxEmptyString,               32);
        AddRow(_("RST Sent:"),       m_rstSent, _("59"),                      8);
        AddRow(_("RST Rcvd:"),       m_rstRcvd, wxString(rstRcvdBuf),         8);
        AddRow(_("Comment:"),        m_comment, wxEmptyString,              128);
        AddRow(_("Freq (MHz):"),     m_freq,    wxString(freqBuf),           12);
        AddRow(_("Band:"),           m_band,    _("20m"),                     8);
        AddRow(_("Mode:"),           m_mode,    _("USB"),                     8);

        // Contest exchange row(s), shown only when a contest exchange template is active
        m_exchTemplate = parent->myRadio->exchangeTemplate;
        m_serialSent = m_serialRcvd = m_fdClass = m_fdSection = m_stateSection = nullptr;
        switch (m_exchTemplate)
        {
        case CRadio::EXCH_SERIAL:
            AddRow(_("Serial Sent:"), m_serialSent, wxString::Format("%03d", parent->myRadio->nextSerialSent), 8);
            AddRow(_("Serial Rcvd:"), m_serialRcvd, wxEmptyString, 8);
            break;
        case CRadio::EXCH_FIELD_DAY:
            AddRow(_("Their Class:"),   m_fdClass,   wxEmptyString, 8);
            AddRow(_("Their Section:"), m_fdSection, wxEmptyString, 8);
            break;
        case CRadio::EXCH_STATE_SECTION:
            AddRow(_("Their County/State:"), m_stateSection, wxEmptyString, 16);
            break;
        default:
            break;
        }

        // POTA row: checkbox + park field on the last grid row
        m_potaCheck = new wxCheckBox(this, wxID_ANY, _("POTA"));
        m_potaCheck->SetValue(parent->m_potaChecked);
        grid->Add(m_potaCheck, 0, wxALIGN_CENTER_VERTICAL);
        m_potaPark = new wxTextCtrl(this, wxID_ANY, parent->m_potaPark,
                                    wxDefaultPosition, wxSize(220, -1));
        m_potaPark->SetMaxLength(16);
        m_potaPark->SetHint(_("Park ref (e.g. K-0001)"));
        m_potaPark->Enable(parent->m_potaChecked);
        grid->Add(m_potaPark, 1, wxEXPAND);
        m_potaCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            m_potaPark->Enable(m_potaCheck->IsChecked());
            if (m_potaCheck->IsChecked()) m_potaPark->SetFocus();
        });

        // P2P row: checkbox + their park field, below the POTA row
        m_p2pCheck = new wxCheckBox(this, wxID_ANY, _("P2P"));
        m_p2pCheck->SetValue(parent->m_p2pChecked);
        grid->Add(m_p2pCheck, 0, wxALIGN_CENTER_VERTICAL);
        m_p2pPark = new wxTextCtrl(this, wxID_ANY, parent->m_p2pPark,
                                    wxDefaultPosition, wxSize(220, -1));
        m_p2pPark->SetMaxLength(16);
        m_p2pPark->SetHint(_("Their park ref (e.g. K-0001)"));
        m_p2pPark->Enable(parent->m_p2pChecked);
        grid->Add(m_p2pPark, 1, wxEXPAND);
        m_p2pCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            m_p2pPark->Enable(m_p2pCheck->IsChecked());
            if (m_p2pCheck->IsChecked()) m_p2pPark->SetFocus();
        });

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(grid, 1, wxEXPAND | wxALL, 12);
        wxButton* ok = new wxButton(this, wxID_ANY, _("LOG"));
        sizer->Add(ok, 0, wxALL | wxALIGN_CENTER, 8);
        SetSizer(sizer);
        Fit();

        m_call->SetFocus();
        m_call->SelectAll();
        m_call->Bind(wxEVT_TEXT_ENTER, &LogDialog::OnOK, this);
        ok->Bind(wxEVT_BUTTON, &LogDialog::OnOK, this);
        Bind(wxEVT_CLOSE_WINDOW, &LogDialog::OnClose, this);
    }

private:
    MyFrame*    m_parent;
    wxTextCtrl* m_call;
    wxTextCtrl* m_name;
    wxTextCtrl* m_rstSent;
    wxTextCtrl* m_rstRcvd;
    wxTextCtrl* m_comment;
    wxCheckBox* m_potaCheck;
    wxTextCtrl* m_potaPark;
    wxCheckBox* m_p2pCheck;
    wxTextCtrl* m_p2pPark;
    wxTextCtrl* m_freq;
    wxTextCtrl* m_band;
    wxTextCtrl* m_mode;
    int         m_exchTemplate;
    wxTextCtrl* m_serialSent;
    wxTextCtrl* m_serialRcvd;
    wxTextCtrl* m_fdClass;
    wxTextCtrl* m_fdSection;
    wxTextCtrl* m_stateSection;

    void SavePotaState()
    {
        m_parent->m_potaChecked = m_potaCheck->GetValue();
        m_parent->m_potaPark    = m_potaPark->GetValue();
        m_parent->m_p2pChecked  = m_p2pCheck->GetValue();
        m_parent->m_p2pPark     = m_p2pPark->GetValue();
    }

    static std::string AdifField(const char* name, const std::string& val)
    {
        if (val.empty()) return "";
        char buf[512];
        sprintf_s(buf, "<%s:%d>%s", name, (int)val.size(), val.c_str());
        return buf;
    }

    void DoLog()
    {
        wxString callsign = m_call->GetValue().Left(16).Upper();
        m_parent->RemoteCallsign = callsign;

        bool needsHeader = false;
        {
            std::ifstream testf("log.adi", std::ios::binary | std::ios::ate);
            needsHeader = !testf.is_open() || testf.tellg() == 0;
        }

        std::ofstream f("log.adi", std::ios::app);
        if (!f.is_open()) { Destroy(); return; }

        if (needsHeader)
        {
            f << "QRO20 Radio ADIF Log\n";
            f << "<adif_ver:5>3.1.4\n";
            f << "<programid:11>QRO20 Radio\n";
            f << "<EOH>\n\n";
        }

        time_t logNow = time(nullptr);
        struct tm logGmt;
        gmtime_s(&logGmt, &logNow);
        char timeBuf[8], dateBuf[16];
        sprintf_s(timeBuf, "%02d%02d%02d", logGmt.tm_hour, logGmt.tm_min, logGmt.tm_sec);
        sprintf_s(dateBuf, "%04d%02d%02d", logGmt.tm_year + 1900, logGmt.tm_mon + 1, logGmt.tm_mday);

        std::string rec;
        rec += AdifField("CALL",             callsign.ToStdString());
        rec += AdifField("QSO_DATE",         dateBuf);
        rec += AdifField("TIME_ON",          timeBuf);
        rec += AdifField("BAND",             m_band->GetValue().ToStdString());
        rec += AdifField("FREQ",             m_freq->GetValue().ToStdString());
        rec += AdifField("MODE",             m_mode->GetValue().ToStdString());
        rec += AdifField("RST_SENT",         m_rstSent->GetValue().ToStdString());
        rec += AdifField("RST_RCVD",         m_rstRcvd->GetValue().ToStdString());
        rec += AdifField("STATION_CALLSIGN", m_parent->m_myCallsign.ToStdString());
        rec += AdifField("NAME",             m_name->GetValue().ToStdString());
        rec += AdifField("COMMENT",          m_comment->GetValue().ToStdString());
        if (m_potaCheck->IsChecked())
        {
            rec += AdifField("MY_SIG",      "POTA");
            rec += AdifField("MY_SIG_INFO", m_potaPark->GetValue().ToStdString());
        }
        if (m_p2pCheck->IsChecked())
        {
            rec += AdifField("SIG",      "POTA");
            rec += AdifField("SIG_INFO", m_p2pPark->GetValue().ToStdString());
        }

        if (m_exchTemplate != CRadio::EXCH_NONE)
        {
            rec += AdifField("CONTEST_ID", m_parent->myRadio->contestID);
            switch (m_exchTemplate)
            {
            case CRadio::EXCH_SERIAL:
                rec += AdifField("STX", m_serialSent->GetValue().ToStdString());
                rec += AdifField("SRX", m_serialRcvd->GetValue().ToStdString());
                break;
            case CRadio::EXCH_FIELD_DAY:
                rec += AdifField("CLASS",      m_fdClass->GetValue().Upper().ToStdString());
                rec += AdifField("ARRL_SECT",  m_fdSection->GetValue().Upper().ToStdString());
                break;
            case CRadio::EXCH_STATE_SECTION:
                rec += AdifField("SRX_STRING", m_stateSection->GetValue().Upper().ToStdString());
                break;
            }
        }

        rec += "<EOR>\n";
        f << rec;

        if (m_exchTemplate == CRadio::EXCH_SERIAL)
        {
            m_parent->myRadio->nextSerialSent++;
            m_parent->myRadio->SaveSettings("settings.json");
        }

        SavePotaState();
        Destroy();
    }

    void OnOK(wxCommandEvent&) { DoLog(); }
    void OnClose(wxCloseEvent&) { SavePotaState(); Destroy(); }
};

///////////////////////////////////////////////////////////////////////////

static std::vector<std::map<std::string, std::string>> ParseAdif(const char* path)
{
    std::vector<std::map<std::string, std::string>> records;
    std::ifstream f(path);
    if (!f.is_open()) return records;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Skip header up to <EOH>
    size_t pos = 0;
    {
        std::string up = content;
        for (auto& c : up) c = (char)toupper((unsigned char)c);
        size_t eoh = up.find("<EOH>");
        if (eoh != std::string::npos) pos = eoh + 5;
    }

    std::map<std::string, std::string> rec;
    while (pos < content.size())
    {
        if (content[pos] != '<') { ++pos; continue; }
        ++pos;
        size_t close = content.find('>', pos);
        if (close == std::string::npos) break;
        std::string tag = content.substr(pos, close - pos);
        pos = close + 1;

        std::string tagUp = tag;
        for (auto& c : tagUp) c = (char)toupper((unsigned char)c);
        if (tagUp == "EOR") { if (!rec.empty()) { records.push_back(rec); rec.clear(); } continue; }
        if (tagUp == "EOH") continue;

        size_t colon = tag.find(':');
        if (colon == std::string::npos) continue;
        std::string name = tag.substr(0, colon);
        for (auto& c : name) c = (char)toupper((unsigned char)c);
        size_t colon2 = tag.find(':', colon + 1);
        std::string lenStr = tag.substr(colon + 1, colon2 != std::string::npos ? colon2 - colon - 1 : std::string::npos);
        int len = 0;
        try { len = std::stoi(lenStr); } catch (...) { continue; }
        if (len > 0 && pos + len <= content.size())
        {
            rec[name] = content.substr(pos, len);
            pos += len;
        }
    }
    return records;
}

// One-way ADIF -> Cabrillo v3 converter. Reads log.adi (via ParseAdif) and keeps only
// records tagged with CONTEST_ID matching the currently-configured contest, so casual/POTA
// QSOs logged in the same file never leak into a contest submission.
static void WriteCabrilloLog(MyFrame* frame, const wxString& outPath)
{
    CRadio* r = frame->myRadio;
    auto records = ParseAdif("log.adi");

    std::ofstream f(outPath.ToStdString());
    if (!f.is_open())
    {
        wxMessageBox(_("Could not open output file for writing."), _("Export Cabrillo"), wxOK | wxICON_ERROR, frame);
        return;
    }

    f << "START-OF-LOG: 3.0\n";
    f << "CALLSIGN: "            << r->myCallsign     << "\n";
    f << "CONTEST: "             << r->contestID      << "\n";
    f << "CATEGORY-BAND: 20M\n";
    f << "CATEGORY-MODE: "       << r->categoryMode   << "\n";
    f << "CATEGORY-POWER: "      << r->categoryPower  << "\n";
    f << "CATEGORY-OPERATOR: SINGLE-OP\n";
    f << "CLAIMED-SCORE: "       << r->claimedScore   << "\n";
    f << "NAME: "                << r->operatorName   << "\n";
    f << "ADDRESS: "             << r->addressLine    << "\n";
    f << "ADDRESS-CITY: "        << r->addressCity    << "\n";
    f << "ADDRESS-STATE: "       << r->addressState   << "\n";
    f << "ADDRESS-POSTALCODE: "  << r->addressPostal  << "\n";
    f << "ADDRESS-COUNTRY: "     << r->addressCountry << "\n";
    f << "CREATED-BY: QRO20 Radio ADIF-Cabrillo Converter\n";

    int exported = 0, skippedOther = 0, skippedFields = 0;
    for (auto& rec : records)
    {
        auto get = [&](const char* k) -> std::string {
            auto it = rec.find(k); return it != rec.end() ? it->second : "";
        };

        std::string contestId = get("CONTEST_ID");
        if (contestId.empty() || contestId != r->contestID) { skippedOther++; continue; }

        std::string call = get("CALL");
        std::string date = get("QSO_DATE");
        std::string time = get("TIME_ON");
        if (call.empty() || date.size() != 8 || time.size() < 4) { skippedFields++; continue; }

        std::string mycall = get("STATION_CALLSIGN");
        if (mycall.empty()) mycall = r->myCallsign;

        double mhz = 0.0;
        try { mhz = std::stod(get("FREQ")); } catch (...) {}
        long khz = std::lround(mhz * 1000.0);

        std::string adifMode = get("MODE");
        for (auto& c : adifMode) c = (char)toupper((unsigned char)c);
        std::string cabMode = (adifMode == "CW")    ? "CW" :
                               (adifMode == "FM")    ? "FM" :
                               (adifMode == "RTTY")  ? "RY" : "PH";

        std::string cabDate = date.substr(0, 4) + "-" + date.substr(4, 2) + "-" + date.substr(6, 2);
        std::string cabTime = time.substr(0, 4);

        std::string rstSent = get("RST_SENT");
        std::string rstRcvd = get("RST_RCVD");

        std::string sentExch, rcvdExch;
        switch (r->exchangeTemplate)
        {
        case CRadio::EXCH_SERIAL:
            sentExch = get("STX");
            rcvdExch = get("SRX");
            break;
        case CRadio::EXCH_FIELD_DAY:
        {
            sentExch = r->myExchange; // constant "CLASS SECTION" for the whole operation
            std::string cls = get("CLASS"), sect = get("ARRL_SECT");
            rcvdExch = cls + ((cls.empty() || sect.empty()) ? "" : " ") + sect;
            break;
        }
        case CRadio::EXCH_STATE_SECTION:
            sentExch = r->myExchange;
            rcvdExch = get("SRX_STRING");
            break;
        default:
            break;
        }

        f << "QSO: " << khz << " " << cabMode << " " << cabDate << " " << cabTime << " "
          << mycall << " " << rstSent;
        if (!sentExch.empty()) f << " " << sentExch;
        f << " " << call << " " << rstRcvd;
        if (!rcvdExch.empty()) f << " " << rcvdExch;
        f << "\n";
        exported++;
    }

    f << "END-OF-LOG:\n";
    f.close();

    wxMessageBox(wxString::Format(
        _("Exported %d QSOs to %s.\n%d skipped (different/missing contest), %d skipped (missing call/date/time)."),
        exported, outPath, skippedOther, skippedFields),
        _("Export Cabrillo"), wxOK | wxICON_INFORMATION, frame);
}

class LogViewDialog : public wxDialog
{
public:
    LogViewDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, _("Log"), wxDefaultPosition, wxSize(900, 480),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_HRULES | wxLC_VRULES | wxLC_SINGLE_SEL);
        m_list->AppendColumn(_("Date"),    wxLIST_FORMAT_LEFT,  90);
        m_list->AppendColumn(_("Time"),    wxLIST_FORMAT_LEFT,  55);
        m_list->AppendColumn(_("Call"),    wxLIST_FORMAT_LEFT,  90);
        m_list->AppendColumn(_("Name"),    wxLIST_FORMAT_LEFT,  90);
        m_list->AppendColumn(_("RST R"),   wxLIST_FORMAT_LEFT,  52);
        m_list->AppendColumn(_("Freq"),    wxLIST_FORMAT_LEFT,  80);
        m_list->AppendColumn(_("Mode"),    wxLIST_FORMAT_LEFT,  50);
        m_list->AppendColumn(_("Comment"), wxLIST_FORMAT_LEFT, 250);

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_list, 1, wxEXPAND | wxALL, 4);
        SetSizer(sizer);
        Reload();
    }

    void Reload()
    {
        m_list->DeleteAllItems();
        auto records = ParseAdif("log.adi");
        for (auto& rec : records)
        {
            auto get = [&](const char* k) -> std::string {
                auto it = rec.find(k); return it != rec.end() ? it->second : "";
            };

            std::string date = get("QSO_DATE");
            if (date.size() == 8)
                date = date.substr(0,4) + "-" + date.substr(4,2) + "-" + date.substr(6,2);

            std::string time = get("TIME_ON");
            if (time.size() >= 4)
                time = time.substr(0,2) + ":" + time.substr(2,2);

            long idx = m_list->InsertItem(m_list->GetItemCount(), date);
            m_list->SetItem(idx, 1, time);
            m_list->SetItem(idx, 2, get("CALL"));
            m_list->SetItem(idx, 3, get("NAME"));
            m_list->SetItem(idx, 4, get("RST_RCVD"));
            m_list->SetItem(idx, 5, get("FREQ"));
            m_list->SetItem(idx, 6, get("MODE"));
            m_list->SetItem(idx, 7, get("COMMENT"));
        }
    }

private:
    wxListCtrl* m_list;
};

///////////////////////////////////////////////////////////////////////////

// Modeless CW send dialog: the top field/SEND button is the message actually keyed; the 4
// preset rows are just canned-text memories whose COPY button loads them into the top field
// (nothing is transmitted until SEND is pressed). Presets live in MyFrame::m_cwMemory for the
// life of the app - loaded into the fields on open, written back on close.
class CWSendDialog : public wxDialog
{
public:
    CWSendDialog(MyFrame* parent)
        : wxDialog((wxWindow*)parent, wxID_ANY, _("CW Send"),
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_parent(parent)
    {
        wxFlexGridSizer* grid = new wxFlexGridSizer(0, 3, 5, 8);
        grid->AddGrowableCol(1);

        wxButton* sendBtn = nullptr;
        {
            grid->Add(new wxStaticText(this, wxID_ANY, _("Send:")),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
            m_sendText = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxSize(260, -1), wxTE_PROCESS_ENTER);
            m_sendText->SetMaxLength(40);
            grid->Add(m_sendText, 1, wxEXPAND);
            sendBtn = new wxButton(this, wxID_ANY, _("SEND"));
            grid->Add(sendBtn, 0);
        }

        for (int i = 0; i < 4; i++)
        {
            grid->Add(new wxStaticText(this, wxID_ANY, wxString::Format(_("%d:"), i + 1)),
                      0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
            m_preset[i] = new wxTextCtrl(this, wxID_ANY, parent->m_cwMemory[i],
                                         wxDefaultPosition, wxSize(260, -1));
            m_preset[i]->SetMaxLength(40);
            grid->Add(m_preset[i], 1, wxEXPAND);

            wxButton* copyBtn = new wxButton(this, wxID_ANY, _("COPY"));
            copyBtn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) {
                m_sendText->SetValue(m_preset[i]->GetValue());
                m_sendText->SetFocus();
                m_sendText->SetInsertionPointEnd();
            });
            grid->Add(copyBtn, 0);
        }

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(grid, 1, wxEXPAND | wxALL, 12);
        SetSizer(sizer);
        Fit();

        m_sendText->SetFocus();
        m_sendText->Bind(wxEVT_TEXT_ENTER, &CWSendDialog::OnSend, this);
        sendBtn->Bind(wxEVT_BUTTON, &CWSendDialog::OnSend, this);
        Bind(wxEVT_CLOSE_WINDOW, &CWSendDialog::OnClose, this);
    }

private:
    MyFrame*    m_parent;
    wxTextCtrl* m_sendText;
    wxTextCtrl* m_preset[4];

    void SavePresets()
    {
        for (int i = 0; i < 4; i++)
            m_parent->m_cwMemory[i] = m_preset[i]->GetValue();
    }

    void OnSend(wxCommandEvent&)
    {
        wxString text = m_sendText->GetValue();
        if (text.IsEmpty()) return;
        m_parent->myRadio->StartCWTransmit(text.ToStdString().c_str());
    }

    void OnClose(wxCloseEvent&) { SavePresets(); Destroy(); }
};

///////////////////////////////////////////////////////////////////////////

MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{

    this->SetSizeHints(900, 520);
    this->SetTitle(_("QRO Radio 20m"));

    wxMenuBar* menuBar = new wxMenuBar();
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(wxID_OPEN, _("&Load...\tCtrl+O"));
    fileMenu->Append(wxID_SAVE, _("&Save...\tCtrl+S"));
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_CONTEST_SETTINGS, _("&Contest Settings..."));
    fileMenu->Append(ID_EXPORT_CABRILLO,  _("&Export Cabrillo Log..."));
    menuBar->Append(fileMenu, _("&File"));
    wxMenu* radioMenu = new wxMenu();
    radioMenu->Append(ID_RADIO_SET_COM, _("Set COM &port..."));
    radioMenu->Append(ID_MY_CALLSIGN,   _("My &Callsign..."));
    radioMenu->AppendSeparator();
    radioMenu->Append(ID_SWEEP_OPEN,  _("Sweep &Open"));
    radioMenu->Append(ID_SWEEP_SHORT, _("Sweep &Short"));
    radioMenu->Append(ID_SWEEP_LOAD,  _("Sweep &Load"));
    radioMenu->AppendSeparator();
    radioMenu->Append(ID_SWEEP_TUNER,   _("Sweep &Tuner"));
    radioMenu->Append(ID_SWEEP_ANTENNA, _("Sweep &Antenna"));
    radioMenu->AppendSeparator();
    radioMenu->AppendCheckItem(ID_IMAGE_REJECT, _("Image &Reject"));
    radioMenu->Check(ID_IMAGE_REJECT, true);
    radioMenu->AppendCheckItem(ID_CW_MODE, _("&CW Mode (700 Hz)"));
    radioMenu->Append(ID_CW_SQUELCH, _("CW &Squelch..."));
    radioMenu->Append(ID_CW_HYSTERESIS, _("CW &Hysteresis..."));
    radioMenu->AppendSeparator();
    radioMenu->Append(ID_MAX_AMP, _("Max &Amp..."));
    radioMenu->AppendSeparator();
    radioMenu->Append(ID_PLOT_SETTINGS, _("&Plot Settings..."));
    menuBar->Append(radioMenu, _("&Radio"));
    wxMenu* audioMenu = new wxMenu();
    audioMenu->Append(ID_AUDIO_MAX_MIC_GAIN,    _("Max Mic Gain (dB)"));
    audioMenu->Append(ID_AUDIO_CESSB_SETPOINT,  _("CESSB Setpoint (dB)"));
    menuBar->Append(audioMenu, _("&Audio"));
    this->SetMenuBar(menuBar);
    Bind(wxEVT_MENU, &MyFrame::OnFileLoad,   this, wxID_OPEN);
    Bind(wxEVT_MENU, &MyFrame::OnFileSave,   this, wxID_SAVE);
    Bind(wxEVT_MENU, &MyFrame::OnSetComPort,  this, ID_RADIO_SET_COM);
    Bind(wxEVT_MENU, &MyFrame::OnMyCallsign,  this, ID_MY_CALLSIGN);
    Bind(wxEVT_MENU, &MyFrame::OnContestSettings, this, ID_CONTEST_SETTINGS);
    Bind(wxEVT_MENU, &MyFrame::OnExportCabrillo,  this, ID_EXPORT_CABRILLO);
    Bind(wxEVT_MENU, &MyFrame::OnImageReject,  this, ID_IMAGE_REJECT);
    Bind(wxEVT_MENU, &MyFrame::OnCWMode,       this, ID_CW_MODE);
    Bind(wxEVT_MENU, &MyFrame::OnCWSquelch,    this, ID_CW_SQUELCH);
    Bind(wxEVT_MENU, &MyFrame::OnCWHysteresis, this, ID_CW_HYSTERESIS);
    Bind(wxEVT_MENU, &MyFrame::OnMaxAmp,       this, ID_MAX_AMP);
    Bind(wxEVT_MENU, &MyFrame::OnSweepOpen,  this, ID_SWEEP_OPEN);
    Bind(wxEVT_MENU, &MyFrame::OnSweepShort, this, ID_SWEEP_SHORT);
    Bind(wxEVT_MENU, &MyFrame::OnSweepLoad,  this, ID_SWEEP_LOAD);
    Bind(wxEVT_MENU, &MyFrame::OnSweepTuner,   this, ID_SWEEP_TUNER);
    Bind(wxEVT_MENU, &MyFrame::OnSweepAntenna, this, ID_SWEEP_ANTENNA);
    Bind(wxEVT_MENU, &MyFrame::OnAudioMaxMicGain,   this, ID_AUDIO_MAX_MIC_GAIN);
    Bind(wxEVT_MENU, &MyFrame::OnAudioCESSBSetpoint, this, ID_AUDIO_CESSB_SETPOINT);
    Bind(wxEVT_MENU, &MyFrame::OnPlotSettings,       this, ID_PLOT_SETTINGS);

    myRadio = new CRadio();//Do this after frame exists

    // --- Color palette (centralized; was previously inline per-widget literals) ---
    const wxColour kBgDark(45, 45, 48);      // frame + group-box background
    const wxColour kBtnBg(32, 32, 32);       // general button bg
    const wxColour kBtnFg(255, 255, 128);    // general button fg
    const wxColour kLogBtnBg(32, 64, 32);    // LOG/VIEW LOG bg
    const wxColour kLogBtnFg(128, 255, 128); // LOG/VIEW LOG fg
    const wxColour kLabelFg(220, 220, 220);  // static text / group-box titles
    const wxColour kReadoutBg(20, 20, 20);   // text entries (freq, step, debug)
    const wxColour kReadoutFg(255, 255, 128);

    this->SetBackgroundColour(kBgDark);

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* bSizer2;
    bSizer2 = new wxBoxSizer(wxVERTICAL);

    m_panel1 = new BasicDrawPane(this, myRadio, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel1->SetBackgroundColour(wxColour(64, 64, 64)); // intentionally lighter than frame - "screen" vs bezel

    bSizer2->Add(m_panel1, 1, wxEXPAND | wxALL, 5);


    bSizer1->Add(bSizer2, 1, wxEXPAND, 5);

    wxBoxSizer* controlsSizer = new wxBoxSizer(wxVERTICAL);

    // --- Group: Control ---
    wxStaticBoxSizer* controlBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Control"));
    controlBox->GetStaticBox()->SetBackgroundColour(kBgDark);
    controlBox->GetStaticBox()->SetForegroundColour(kLabelFg);
    wxWindow* controlParent = controlBox->GetStaticBox();

    m_button1 = new wxButton(controlParent, wxID_ANY, _("  CONNECT  "), wxDefaultPosition, wxSize(210, 40), 0);
    wxFont bf = m_button1->GetFont();
    bf.SetPointSize(16);
    bf.MakeBold();
    m_button1->SetFont(bf);
    m_button1->SetBackgroundColour(kBtnBg);
    m_button1->SetForegroundColour(kBtnFg);
    controlBox->Add(m_button1, 0, wxALL, 2);

    m_button2 = new wxButton(controlParent, wxID_ANY, _("DISABLE 24V "), wxDefaultPosition, wxSize(210, 40), 0);
    m_button2->SetFont(bf);
    m_button2->SetBackgroundColour(kBtnBg);
    m_button2->SetForegroundColour(kBtnFg);
    controlBox->Add(m_button2, 0, wxALL, 2);

    m_button3 = new wxButton(controlParent, wxID_ANY, _(" ANT TUNE  "), wxDefaultPosition, wxSize(210, 40), 0);
    m_button3->SetFont(bf);
    m_button3->SetBackgroundColour(kBtnBg);
    m_button3->SetForegroundColour(kBtnFg);
    controlBox->Add(m_button3, 0, wxALL, 2);

    m_button4 = new wxButton(controlParent, wxID_ANY, _(" TRANSMIT  "), wxDefaultPosition, wxSize(210, 40), 0);
    m_button4->SetFont(bf);
    m_button4->SetBackgroundColour(kBtnBg);
    m_button4->SetForegroundColour(kBtnFg);
    controlBox->Add(m_button4, 0, wxALL, 2);

    m_button5 = new wxButton(controlParent, wxID_ANY, _(" RECEIVE  "), wxDefaultPosition, wxSize(210, 40), 0);
    m_button5->SetFont(bf);
    m_button5->SetBackgroundColour(kBtnBg);
    m_button5->SetForegroundColour(kBtnFg);
    controlBox->Add(m_button5, 0, wxALL, 2);

    m_buttonSync = new wxButton(controlParent, wxID_ANY, _("   SYNC    "), wxDefaultPosition, wxSize(210, 40), 0);
    m_buttonSync->SetFont(bf);
    m_buttonSync->SetBackgroundColour(kBtnBg);
    m_buttonSync->SetForegroundColour(kBtnFg);
    controlBox->Add(m_buttonSync, 0, wxALL, 2);

    controlsSizer->Add(controlBox, 0, wxEXPAND | wxALL, 2);

    // --- Group: Frequency ---
    wxStaticBoxSizer* freqBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Frequency"));
    freqBox->GetStaticBox()->SetBackgroundColour(kBgDark);
    freqBox->GetStaticBox()->SetForegroundColour(kLabelFg);
    wxWindow* freqParent = freqBox->GetStaticBox();

    // 2x3 grid so the frequency row and step row share column widths and line up
    // left/right edge to edge, instead of each row sizing itself independently.
    wxFlexGridSizer* freqGrid = new wxFlexGridSizer(2, 3, 2, 0);

    wxFont stepperFont = bf;
    stepperFont.SetPointSize(9);

    wxBoxSizer* stepperStack = new wxBoxSizer(wxVERTICAL);

    m_button8 = new wxButton(freqParent, wxID_ANY, _("+"), wxDefaultPosition, wxSize(34, 18), 0);
    m_button8->SetFont(stepperFont);
    m_button8->SetBackgroundColour(kBtnBg);
    m_button8->SetForegroundColour(kBtnFg);
    stepperStack->Add(m_button8, 0, wxBOTTOM, 1);

    m_button7 = new wxButton(freqParent, wxID_ANY, _("-"), wxDefaultPosition, wxSize(34, 18), 0);
    m_button7->SetFont(stepperFont);
    m_button7->SetBackgroundColour(kBtnBg);
    m_button7->SetForegroundColour(kBtnFg);
    stepperStack->Add(m_button7, 0, 0, 0);

    freqGrid->Add(stepperStack, 0, wxALIGN_CENTER_VERTICAL);

    m_textCtrl1 = new wxTextCtrl(freqParent, wxID_ANY, _("14.25000"), wxDefaultPosition, wxSize(105,40), 0);
    m_textCtrl1->SetFont(bf);
    m_textCtrl1->SetBackgroundColour(kReadoutBg);
    m_textCtrl1->SetForegroundColour(kReadoutFg);
    freqGrid->Add(m_textCtrl1, 0, wxRIGHT, 5);

    m_button6 = new wxButton(freqParent, wxID_ANY, _("MHz"), wxDefaultPosition, wxSize(60, 40), 0);
    m_button6->SetFont(bf);
    m_button6->SetBackgroundColour(kBtnBg);
    m_button6->SetForegroundColour(kBtnFg);
    freqGrid->Add(m_button6, 0, 0);

    wxStaticText* staticText2 = new wxStaticText(freqParent, wxID_ANY, _("STEP"), wxDefaultPosition, wxDefaultSize, 0);
    staticText2->SetFont(stepperFont);
    staticText2->SetForegroundColour(kLabelFg);
    freqGrid->Add(staticText2, 0, wxALIGN_CENTER_VERTICAL);

    m_textCtrl2 = new wxTextCtrl(freqParent, wxID_ANY, _("1000"), wxDefaultPosition, wxSize(105, 40), 0);
    m_textCtrl2->SetFont(bf);
    m_textCtrl2->SetBackgroundColour(kReadoutBg);
    m_textCtrl2->SetForegroundColour(kReadoutFg);
    freqGrid->Add(m_textCtrl2, 0, wxRIGHT, 5);

    m_button9 = new wxButton(freqParent, wxID_ANY, _("Hz"), wxDefaultPosition, wxSize(60, 40), 0);
    m_button9->SetFont(bf);
    m_button9->SetBackgroundColour(kBtnBg);
    m_button9->SetForegroundColour(kBtnFg);
    freqGrid->Add(m_button9, 0, 0);

    freqBox->Add(freqGrid, 0, wxALL, 5);

    m_buttonCWPeak = new wxButton(freqParent, wxID_ANY, _("CW Peaking"), wxDefaultPosition, wxSize(180, 32), 0);
    m_buttonCWPeak->SetFont(bf);
    m_buttonCWPeak->SetBackgroundColour(kBtnBg);
    m_buttonCWPeak->SetForegroundColour(kBtnFg);
    freqBox->Add(m_buttonCWPeak, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

    controlsSizer->Add(freqBox, 0, wxEXPAND | wxALL, 2);

    // --- Group: Options ---
    wxStaticBoxSizer* optionsBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Options"));
    optionsBox->GetStaticBox()->SetBackgroundColour(kBgDark);
    optionsBox->GetStaticBox()->SetForegroundColour(kLabelFg);
    wxWindow* optionsParent = optionsBox->GetStaticBox();

    m_button10 = new wxButton(optionsParent, wxID_ANY, _("Hotkeys = Y"), wxDefaultPosition, wxSize(210, 40), 0);
    m_button10->SetFont(bf);
    m_button10->SetBackgroundColour(kBtnBg);
    m_button10->SetForegroundColour(kBtnFg);
    optionsBox->Add(m_button10, 0, wxALL, 2);

    m_textDebug = new wxTextCtrl(optionsParent, wxID_ANY, _("DEBUG"), wxDefaultPosition, wxSize(210, 40), 0);
    m_textDebug->SetFont(bf);
    m_textDebug->SetBackgroundColour(kReadoutBg);
    m_textDebug->SetForegroundColour(kReadoutFg);
    optionsBox->Add(m_textDebug, 0, wxALL, 2);

    controlsSizer->Add(optionsBox, 0, wxEXPAND | wxALL, 2);

    // --- Group: Log ---
    wxStaticBoxSizer* logBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Log"));
    logBox->GetStaticBox()->SetBackgroundColour(kBgDark);
    logBox->GetStaticBox()->SetForegroundColour(kLabelFg);
    wxWindow* logParent = logBox->GetStaticBox();

    m_button11 = new wxButton(logParent, wxID_ANY, _("    LOG    "), wxDefaultPosition, wxSize(210, 40), 0);
    m_button11->SetFont(bf);
    m_button11->SetBackgroundColour(kLogBtnBg);
    m_button11->SetForegroundColour(kLogBtnFg);
    logBox->Add(m_button11, 0, wxALL, 2);

    m_buttonViewLog = new wxButton(logParent, wxID_ANY, _("  VIEW LOG  "), wxDefaultPosition, wxSize(210, 40), 0);
    m_buttonViewLog->SetFont(bf);
    m_buttonViewLog->SetBackgroundColour(kLogBtnBg);
    m_buttonViewLog->SetForegroundColour(kLogBtnFg);
    logBox->Add(m_buttonViewLog, 0, wxALL, 2);

    controlsSizer->Add(logBox, 0, wxEXPAND | wxALL, 2);

    RemoteCallsign = wxEmptyString;

    bSizer1->Add(controlsSizer, 0, wxALIGN_TOP, 5);


    this->SetSizer(bSizer1);
    this->Layout();

    //this->Centre(wxBOTH);
    this->SetPosition(wxPoint(0, 0));

    // Connect Events


    m_button1->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B1Click), NULL, this);
    m_button2->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B2Click), NULL, this);
    m_button3->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B3Click), NULL, this);
    m_button4->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B4Click), NULL, this);
    m_button5->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B5Click), NULL, this);
    m_button6->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B6Click), NULL, this);
    m_button7->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B7Click), NULL, this);
    m_button8->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B8Click), NULL, this);
    m_button9->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B9Click), NULL, this);
    m_button10->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B10Click), NULL, this);
    m_button11->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::BLogClick), NULL, this);
    m_buttonSync->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::BSyncClick), NULL, this);
    m_buttonViewLog->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::BViewLogClick), NULL, this);
    m_buttonCWPeak->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::BCWPeakClick), NULL, this);

    SPtoTX = true;
    antTuneRun = false;
    m_button3->Enable(false);
    m_button4->Enable(false);
    Bind(wxEVT_CHAR_HOOK, &MyFrame::OnCharHook, this);
//    m_button1->Enable();
//    m_panel1->Connect(wxEVT_PAINT, wxPaintEventHandler(BasicDrawPane::paintEvent), NULL, this);
    m_timer.SetOwner(this, TIMER_ID);
    m_timer.Connect(wxEVT_TIMER, wxTimerEventHandler(MyFrame::OnTimer));
}

MyFrame::~MyFrame()
{
    m_timer.Stop();

    delete myRadio;
}
void MyFrame::UpdateDebugText(char* text)
{
    wchar_t label[16];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    mbstowcs(label, text, 16);
    m_textDebug->SetLabelText(label);
}

void MyFrame::OnFileSave(wxCommandEvent& event)
{
    wxFileDialog dlg(this, _("Save Settings"), wxEmptyString, _("settings.json"),
        _("JSON files (*.json)|*.json"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;
    myRadio->SaveSettings(dlg.GetPath().mb_str());
}

void MyFrame::OnFileLoad(wxCommandEvent& event)
{
    wxFileDialog dlg(this, _("Load Settings"), wxEmptyString, wxEmptyString,
        _("JSON files (*.json)|*.json"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;
    if (myRadio->LoadSettings(dlg.GetPath().mb_str()))
    {
        m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
        m_myCallsign = wxString(myRadio->myCallsign, wxConvUTF8);
    }
}

void MyFrame::OnSetComPort(wxCommandEvent& event)
{
    long port = wxGetNumberFromUser(
        _("Enter the COM port number for the radio:"),
        _("COM port:"), _("Set COM Port"),
        myRadio->comPort, 1, 255, this);
    if (port == -1) return;
    myRadio->comPort = (int)port;
}

void MyFrame::OnMyCallsign(wxCommandEvent& event)
{
    wxString call = wxGetTextFromUser(
        _("Enter your station callsign:"),
        _("My Callsign"),
        m_myCallsign, this);
    if (call.IsEmpty()) return;
    m_myCallsign = call.Upper();
    strncpy_s(myRadio->myCallsign, sizeof(myRadio->myCallsign),
              m_myCallsign.ToStdString().c_str(), _TRUNCATE);
    myRadio->SaveSettings("settings.json");
}

void MyFrame::OnContestSettings(wxCommandEvent& event)
{
    CabrilloSettingsDialog dlg(this);
    dlg.ShowModal();
}

void MyFrame::OnExportCabrillo(wxCommandEvent& event)
{
    if (myRadio->exchangeTemplate == CRadio::EXCH_NONE || myRadio->contestID[0] == '\0')
    {
        wxMessageBox(_("Set a Contest ID and Exchange Template in Contest Settings first."),
                     _("Export Cabrillo"), wxOK | wxICON_WARNING, this);
        return;
    }

    wxString defaultName = wxString(myRadio->contestID, wxConvUTF8) + _(".log");
    wxFileDialog dlg(this, _("Export Cabrillo Log"), wxEmptyString, defaultName,
        _("Cabrillo files (*.log)|*.log"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    WriteCabrilloLog(this, dlg.GetPath());
}

void MyFrame::OnImageReject(wxCommandEvent& event)
{
    myRadio->IQBalanceEnabled = event.IsChecked();
}

void MyFrame::OnCWMode(wxCommandEvent& event)
{
    myRadio->CWModeEnabled = event.IsChecked();
    myRadio->ResetCWDecoder(); // clear stale filter/timing state on either transition
}

void MyFrame::OnSweepOpen(wxCommandEvent& event)  {
    myRadio->myStatus->calMode = 0;
    myRadio->myStatus->mode = 3;
}
void MyFrame::OnSweepShort(wxCommandEvent& event) {
    myRadio->myStatus->calMode = 1;
    myRadio->myStatus->mode = 3;
}
void MyFrame::OnSweepLoad(wxCommandEvent& event)  {
    myRadio->myStatus->calMode = 2;
    myRadio->myStatus->mode = 3;
}
void MyFrame::OnSweepTuner(wxCommandEvent& event) {
    myRadio->myStatus->calMode = 4; // Special mode for sweeping tuner w load
    myRadio->myStatus->mode = 3;
}
void MyFrame::OnSweepAntenna(wxCommandEvent& event) {
    myRadio->myStatus->calMode = 5;
    myRadio->myStatus->mode = 3;
}

void MyFrame::OnAudioMaxMicGain(wxCommandEvent& event)
{
    double currentdB = 20.0 * log10((double)myRadio->agcMaxGain);
    wxString input = wxGetTextFromUser(
        _("Enter maximum mic gain in dB (20 dB = 10x linear):"),
        _("Max Mic Gain (dB)"),
        wxString::Format("%.1f", currentdB), this);
    if (input.IsEmpty()) return;
    double dB;
    if (input.ToDouble(&dB))
        myRadio->agcMaxGain = (float)pow(10.0, dB / 20.0);
}

void MyFrame::OnAudioCESSBSetpoint(wxCommandEvent& event)
{
    double currentdB = 20.0 * log10((double)myRadio->agcTarget);
    wxString input = wxGetTextFromUser(
        _("Enter CESSB AGC setpoint in dB (overdrive level into clipper):"),
        _("CESSB Setpoint (dB)"),
        wxString::Format("%.1f", currentdB), this);
    if (input.IsEmpty()) return;
    double dB;
    if (input.ToDouble(&dB))
        myRadio->agcTarget = (float)pow(10.0, dB / 20.0);
}

void MyFrame::OnCWSquelch(wxCommandEvent& event)
{
    wxString input = wxGetTextFromUser(
        _("CW FFT decoder squelch multiplier (threshold = median power x this x 9; default 20.0):"),
        _("CW Squelch"),
        wxString::Format("%.2f", myRadio->cwSquelch), this);
    if (input.IsEmpty()) return;
    double squelch;
    if (input.ToDouble(&squelch))
    {
        if (squelch < 0.1) squelch = 0.1;
        myRadio->cwSquelch = (float)squelch;
    }
}

void MyFrame::OnCWHysteresis(wxCommandEvent& event)
{
    wxString input = wxGetTextFromUser(
        _("CW FFT decoder squelch hysteresis (enter mark above squelch+this, exit mark below squelch-this; default 4.0):"),
        _("CW Hysteresis"),
        wxString::Format("%.2f", myRadio->cwHysteresis), this);
    if (input.IsEmpty()) return;
    double hysteresis;
    if (input.ToDouble(&hysteresis))
    {
        if (hysteresis < 0.0) hysteresis = 0.0;
        myRadio->cwHysteresis = (float)hysteresis;
    }
}

void MyFrame::OnMaxAmp(wxCommandEvent& event)
{
    // Power scales with amplitude^2, so the amp code (125-250) is 250 * sqrt(percent),
    // with 25% power = 125 (min) and 100% power = 250 (max, hardware full scale).
    long currentPercent = (long)round(100.0 * pow(g_max_amp / 250.0, 2.0));
    long val = wxGetNumberFromUser(
        _("Max TX power (25-100%):"),
        _("Max Power %:"),
        _("Max Amp"),
        currentPercent, 25, 100, this);
    if (val < 0) return;
    g_max_amp = (int)round(250.0 * sqrt(val / 100.0));
    g_abs_max_amp = (int)round(1.15 * g_max_amp);
}

void MyFrame::OnPlotSettings(wxCommandEvent& event)
{
    long val;

    val = wxGetNumberFromUser(
        _("Bottom S-unit of spectrum display (0 = S0, 9 = S9):"),
        _("S-unit floor (0-9):"),
        _("Plot Settings - S-unit Floor"),
        myRadio->plotSfloor, 0, 9, this);
    if (val < 0) return;
    myRadio->plotSfloor = (int)val;

    val = wxGetNumberFromUser(
        _("Number of S-unit divisions to show (8-24):"),
        _("S-units shown (8-24):"),
        _("Plot Settings - S-units Shown"),
        myRadio->plotSunits, 8, 24, this);
    if (val < 0) return;
    myRadio->plotSunits = (int)val;

    wxString input = wxGetTextFromUser(
        _("Calibration offset in dB (positive = reads higher, -10.0 to +10.0):"),
        _("Plot Settings - S-unit Offset (dB)"),
        wxString::Format("%.1f", myRadio->plotSoffset), this);
    if (input.IsEmpty()) return;
    double offset;
    if (input.ToDouble(&offset))
    {
        if (offset < -10.0) offset = -10.0;
        if (offset >  10.0) offset =  10.0;
        myRadio->plotSoffset = (float)offset;
    }
}

void MyFrame::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);

    dc.DrawText(wxT("Testing"), 40, 60);

    // draw a circle
    dc.SetBrush(*wxGREEN_BRUSH); // green filling
    dc.SetPen(wxPen(wxColor(255, 0, 0), 5)); // 5-pixels-thick red outline
    dc.DrawCircle(wxPoint(200, 100), 25 /* radius */);

    // draw a rectangle
    dc.SetBrush(*wxBLUE_BRUSH); // blue filling
    dc.SetPen(wxPen(wxColor(255, 175, 175), 10)); // 10-pixels-thick pink outline
    dc.DrawRectangle(300, 100, 400, 200);

    // draw a line
    dc.SetPen(wxPen(wxColor(0, 0, 0), 3)); // black line, 3 pixels thick
    dc.DrawLine(300, 100, 700, 300); // draw line across the rectangle

 }

void MyFrame::OnTimer(wxTimerEvent& event)
{
 //   return;
    time_t now = time(nullptr);
    struct tm gmt;
    gmtime_s(&gmt, &now);
    sprintf_s(myRadio->myStatus->GMTTime, "%02d:%02d:%02d UTC",
              gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
    static int lastSec = -1;
    if (gmt.tm_sec != lastSec)
    {
        lastSec = gmt.tm_sec;
        myRadio->myStatus->UpdateText = true;
    }

    wchar_t label[16];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    mbstowcs(label, myRadio->dbgText, 16);
    m_textDebug->SetLabelText(label);

    if (!antTuneRun)
    {
        static int prevMode = -1;
        int curMode = myRadio->myStatus->mode;
        if (prevMode == VNA_MODE && curMode == RX_MODE)
        {
            antTuneRun = true;
            UpdateTransmitEnable();
        }
        prevMode = curMode;
    }

    myRadio->UpdatePlot();
    m_panel1->isPartial = true;
    m_panel1->audioModified = true;
    m_panel1->RFModified = true;
    m_panel1->Refresh(false);

    if (myRadio->CWPeakReady)
    {
        myRadio->CWPeakReady = false;
        wxMessageBox(wxString::Format("Peak frequency: %.0f Hz", myRadio->CWPeakFreq),
                     "CW Peaking", wxOK | wxICON_INFORMATION, this);
    }
}

void MyFrame::UpdateTransmitEnable()
{
    if (!antTuneRun) return;

    double pos = (myRadio->LOfreq - 14.0) / 0.01;
    int idx0 = (int)floor(pos);
    if (idx0 < 0)  idx0 = 0;
    if (idx0 > 34) idx0 = 34;
    int idx1 = idx0 + 1;
    double frac = pos - idx0;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;

    float vswr0 = myRadio->myStatus->SWRTuned[idx0];
    float vswr1 = myRadio->myStatus->SWRTuned[idx1];
    float vswrInterp = vswr0 + (float)((vswr1 - vswr0) * frac);

    m_button4->Enable(vswrInterp < 1.5f);
}

void MyFrame::B1Click(wxCommandEvent& event) // CONNECT
{
    if (myRadio->LoadSettings("settings.json"))
    {
        m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
        m_myCallsign = wxString(myRadio->myCallsign, wxConvUTF8);
    }

    int retval = myRadio->Connect();

    if (retval)
    {
        m_button1->SetLabelText(_(" CONNECTED "));
        m_button3->Enable(!myRadio->isRXOnly);
    }

    m_panel1->Refresh(false);
    m_timer.Start(125);
}

void MyFrame::B2Click(wxCommandEvent& event) // Dis 24V
{
   // myRadio->myStatus->mode = 3; // VNA mode. Completes and returns to RX mode.
   myRadio->myStatus->mode = 0;

}

void MyFrame::B3Click(wxCommandEvent& event) // ANT TUNE
{
    myRadio->myStatus->calMode = 3;
    myRadio->myStatus->mode = 3; // VNA mode. Completes and returns to RX mode.
}

void MyFrame::B4Click(wxCommandEvent& event) // TRANSMIT
{
    if (!antTuneRun) return;
    if (myRadio->CWModeEnabled)
    {
        CWSendDialog* dlg = new CWSendDialog(this);
        dlg->Show(true);
        return;
    }

    myRadio->myStatus->mode = 2;
}

void MyFrame::B5Click(wxCommandEvent& event) // RECEIVE
{
    myRadio->myStatus->mode = 1;
}

void MyFrame::BSyncClick(wxCommandEvent& event) // SYNC
{
    myRadio->myStatus->calMode = 6;
    myRadio->myStatus->mode = VNA_MODE;
}

void MyFrame::BCWPeakClick(wxCommandEvent& event) // CW Peaking
{
    myRadio->StartCWPeakCapture();
}

void MyFrame::B6Click(wxCommandEvent& event) // MHz
{
    wxString value = m_textCtrl1->GetValue();
    double freq;
    value.ToDouble(&freq);
    double loLimit = gUseDebugWaveform ? 13.000 : 14.000;
    double hiLimit = gUseDebugWaveform ? 15.000 : 14.350;
    if (freq < loLimit) freq = loLimit;
    if (freq > hiLimit) freq = hiLimit;
    myRadio->LOfreq = freq;
    m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
    myRadio->myStatus->UpdateText = true;
    UpdateTransmitEnable();
}

void MyFrame::B7Click(wxCommandEvent& event) // Step down
{
    myRadio->LOfreq -= myRadio->stepSize / 1.0e6;
    myRadio->LOfreq = round(myRadio->LOfreq * 10000.0) / 10000.0;
    if (myRadio->LOfreq < (gUseDebugWaveform ? 13.000 : 14.000)) myRadio->LOfreq = gUseDebugWaveform ? 13.000 : 14.000;
    m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
    myRadio->myStatus->UpdateText = true;
    UpdateTransmitEnable();
}

void MyFrame::B8Click(wxCommandEvent& event) // Step up
{
    myRadio->LOfreq += myRadio->stepSize / 1.0e6;
    myRadio->LOfreq = round(myRadio->LOfreq * 10000.0) / 10000.0;
    if (myRadio->LOfreq > (gUseDebugWaveform ? 15.000 : 14.350)) myRadio->LOfreq = gUseDebugWaveform ? 15.000 : 14.350;
    m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
    myRadio->myStatus->UpdateText = true;
    UpdateTransmitEnable();
}

void MyFrame::B9Click(wxCommandEvent& event) // Hz (update step size)
{
    wxString value = m_textCtrl2->GetValue();
    double step;
    if (value.ToDouble(&step))
        myRadio->stepSize = step;
}

void MyFrame::B10Click(wxCommandEvent& event)
{
    SPtoTX = !SPtoTX;
    m_button10->SetLabelText(SPtoTX ? _("Hotkeys = Y") : _("Hotkeys = N"));
}

void MyFrame::BLogClick(wxCommandEvent& event)
{
    LogDialog* dlg = new LogDialog(this, wxEmptyString);
    dlg->Show(true);
}

void MyFrame::BViewLogClick(wxCommandEvent& event)
{
    LogViewDialog* dlg = new LogViewDialog(this);
    dlg->Show(true);
}

void MyFrame::OnCharHook(wxKeyEvent& event)
{
    if (SPtoTX)
    {
        int key = event.GetKeyCode();
        if (key == 'T' && antTuneRun) {
            if (myRadio->CWModeEnabled)
            {
                CWSendDialog* dlg = new CWSendDialog(this);
                dlg->Show(true);
                return;
            }

            myRadio->myStatus->mode = 2;
            return;
        }
        if (key == 'R') { myRadio->myStatus->mode = RX_MODE; return; }
        if (key == WXK_RIGHT) { wxCommandEvent dummy; B8Click(dummy); return; }
        if (key == WXK_LEFT)  { wxCommandEvent dummy; B7Click(dummy); return; }
        if (key == WXK_UP || key == WXK_DOWN)
        {
            double fine = myRadio->stepSize / 10.0 / 1.0e6;
            myRadio->LOfreq += (key == WXK_UP) ? fine : -fine;
            myRadio->LOfreq = round(myRadio->LOfreq * 100000.0) / 100000.0;
            if (myRadio->LOfreq < (gUseDebugWaveform ? 13.000 : 14.000)) myRadio->LOfreq = gUseDebugWaveform ? 13.000 : 14.000;
            if (myRadio->LOfreq > (gUseDebugWaveform ? 15.000 : 14.350)) myRadio->LOfreq = gUseDebugWaveform ? 15.000 : 14.350;
            m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
            myRadio->NewLOFreq = true;
            myRadio->myStatus->UpdateText = true;
            UpdateTransmitEnable();
            return;
        }
    }
    int key = event.GetKeyCode();
    if (key == 'L') { wxCommandEvent dummy; BLogClick(dummy); return; }
    if (key == 'S') { wxCommandEvent dummy; BSyncClick(dummy); return; }
    event.Skip();
}

