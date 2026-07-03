// Start of wxWidgets "Hello World" Program
#include <wx/wx.h>
#include <wx/numdlg.h>
#include <cmath>
#include <ctime>
#include <fstream>
#include <string>
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
        float watts = pRadio->myStatus->volts * pRadio->myStatus->amps;
        if (watts < 10.0)
            sprintf_s(voltText, "%.1fV %.1fW", pRadio->myStatus->volts, watts);
        else
            sprintf_s(voltText, "%.1fV %.0fW", pRadio->myStatus->volts, watts);
        mbstowcs(txText, voltText, 16);
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
        wchar_t plot1Label[36] = _T("VSWR 14.15-14.35 MHz 0.5/");
        Plot2(dc, x3, midY, plotW, midH, 21, &pRadio->myStatus->SWRUntuned[15], 1.0, 3.0, 4, 7, plot1Label, &pRadio->myStatus->SWRTuned[15]);

        wchar_t plot8Label[24] = _T("Antenna Impedance");
        SmithPlot2(dc, x4, midY, smithW, midH, 21, &pRadio->myStatus->SmithChartUntuned[30], &pRadio->myStatus->SmithChartTuned[30], plot8Label);
    }

    if (audioModified)
    {
        audioModified = false;
        wchar_t plot3Label[20] = _T("Audio (freq) 1kHz/");
        Plot(dc, x1, midY, plotW, midH, 16, pRadio->myStatus->AudioFreqPlot, 0.0, 1.0, 4, 3, plot3Label);

        wchar_t plot4Label[20] = _T("Audio (time) 1ms/");
        Plot(dc, x2, midY, plotW, midH, 128, pRadio->myStatus->AudioTimePlot, -1.0, 1.0, 4, 5, plot4Label);
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

class LogDialog : public wxDialog
{
public:
    LogDialog(MyFrame* parent, const wxString& lastCall)
        : wxDialog((wxWindow*)parent, wxID_ANY, _("Log Contact"),
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_parent(parent)
    {
        time_t now = time(nullptr);
        struct tm gmt;
        gmtime_s(&gmt, &now);

        char dateBuf[16], timeBuf[16], freqBuf[16], rstRcvdBuf[8];
        sprintf_s(dateBuf, "%04d%02d%02d", gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday);
        sprintf_s(timeBuf, "%02d%02d%02d", gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
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
        AddRow(_("My Callsign:"),    m_myCall,  parent->m_myCallsign,        16);
        AddRow(_("Their Name:"),     m_name,    wxEmptyString,               32);
        AddRow(_("RST Sent:"),       m_rstSent, _("59"),                      8);
        AddRow(_("RST Rcvd:"),       m_rstRcvd, wxString(rstRcvdBuf),         8);
        AddRow(_("Comment:"),        m_comment, wxEmptyString,              128);

        // POTA row: checkbox + park field on one grid row
        m_potaCheck = new wxCheckBox(this, wxID_ANY, _("POTA"));
        grid->Add(m_potaCheck, 0, wxALIGN_CENTER_VERTICAL);
        m_potaPark = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(220, -1));
        m_potaPark->SetMaxLength(16);
        m_potaPark->SetHint(_("Park ref (e.g. K-0001)"));
        m_potaPark->Enable(false);
        grid->Add(m_potaPark, 1, wxEXPAND);
        m_potaCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            m_potaPark->Enable(m_potaCheck->IsChecked());
            if (m_potaCheck->IsChecked()) m_potaPark->SetFocus();
        });
        AddRow(_("Freq (MHz):"),     m_freq,    wxString(freqBuf),           12);
        AddRow(_("Band:"),           m_band,    _("20m"),                     8);
        AddRow(_("Mode:"),           m_mode,    _("USB"),                     8);
        AddRow(_("QSO Date (UTC):"), m_date,    wxString(dateBuf),            8);
        AddRow(_("Time On (UTC):"),  m_timeOn,  wxString(timeBuf),            6);

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
    wxTextCtrl* m_myCall;
    wxTextCtrl* m_name;
    wxTextCtrl* m_rstSent;
    wxTextCtrl* m_rstRcvd;
    wxTextCtrl* m_comment;
    wxCheckBox* m_potaCheck;
    wxTextCtrl* m_potaPark;
    wxTextCtrl* m_freq;
    wxTextCtrl* m_band;
    wxTextCtrl* m_mode;
    wxTextCtrl* m_date;
    wxTextCtrl* m_timeOn;

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
        m_parent->m_myCallsign   = m_myCall->GetValue().Left(16).Upper();
        strncpy_s(m_parent->myRadio->myCallsign, sizeof(m_parent->myRadio->myCallsign),
                  m_parent->m_myCallsign.ToStdString().c_str(), _TRUNCATE);

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

        std::string rec;
        rec += AdifField("CALL",             callsign.ToStdString());
        rec += AdifField("QSO_DATE",         m_date->GetValue().ToStdString());
        rec += AdifField("TIME_ON",          m_timeOn->GetValue().ToStdString());
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
        rec += "<EOR>\n";
        f << rec;

        Destroy();
    }

    void OnOK(wxCommandEvent&) { DoLog(); }
    void OnClose(wxCloseEvent&) { Destroy(); }
};

MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{

    this->SetSizeHints(900, 520);
    this->SetTitle(_("QRO Radio 20m"));

    wxMenuBar* menuBar = new wxMenuBar();
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(wxID_OPEN, _("&Load...\tCtrl+O"));
    fileMenu->Append(wxID_SAVE, _("&Save...\tCtrl+S"));
    menuBar->Append(fileMenu, _("&File"));
    wxMenu* radioMenu = new wxMenu();
    radioMenu->Append(ID_RADIO_SET_COM, _("Set COM &port..."));
    radioMenu->AppendSeparator();
    radioMenu->Append(ID_SWEEP_OPEN,  _("Sweep &Open"));
    radioMenu->Append(ID_SWEEP_SHORT, _("Sweep &Short"));
    radioMenu->Append(ID_SWEEP_LOAD,  _("Sweep &Load"));
    radioMenu->AppendSeparator();
    radioMenu->Append(ID_SWEEP_TUNER,   _("Sweep &Tuner"));
    radioMenu->Append(ID_SWEEP_ANTENNA, _("Sweep &Antenna"));
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
    Bind(wxEVT_MENU, &MyFrame::OnSetComPort, this, ID_RADIO_SET_COM);
    Bind(wxEVT_MENU, &MyFrame::OnSweepOpen,  this, ID_SWEEP_OPEN);
    Bind(wxEVT_MENU, &MyFrame::OnSweepShort, this, ID_SWEEP_SHORT);
    Bind(wxEVT_MENU, &MyFrame::OnSweepLoad,  this, ID_SWEEP_LOAD);
    Bind(wxEVT_MENU, &MyFrame::OnSweepTuner,   this, ID_SWEEP_TUNER);
    Bind(wxEVT_MENU, &MyFrame::OnSweepAntenna, this, ID_SWEEP_ANTENNA);
    Bind(wxEVT_MENU, &MyFrame::OnAudioMaxMicGain,   this, ID_AUDIO_MAX_MIC_GAIN);
    Bind(wxEVT_MENU, &MyFrame::OnAudioCESSBSetpoint, this, ID_AUDIO_CESSB_SETPOINT);
    Bind(wxEVT_MENU, &MyFrame::OnPlotSettings,       this, ID_PLOT_SETTINGS);

    myRadio = new CRadio();//Do this after frame exists

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* bSizer2;
    bSizer2 = new wxBoxSizer(wxVERTICAL);

    m_panel1 = new BasicDrawPane(this, myRadio, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel1->SetBackgroundColour(wxColour(64, 64, 64));

    bSizer2->Add(m_panel1, 1, wxEXPAND | wxALL, 5);


    bSizer1->Add(bSizer2, 1, wxEXPAND, 5);

    wxFlexGridSizer* gSizer1;
    gSizer1 = new wxFlexGridSizer(0, 1, 2, 2);
    gSizer1->SetFlexibleDirection(wxBOTH);
    gSizer1->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);


    //wxGridSizer* gSizer1;
    //gSizer1 = new wxGridSizer(8, 1, 5, 5);

    m_button1 = new wxButton(this, wxID_ANY, _("  CONNECT  "), wxDefaultPosition, wxSize(180, 40), 0);
    wxFont bf = m_button1->GetFont();
    bf.SetPointSize(16);
    bf.MakeBold();
    m_button1->SetFont(bf);
    m_button1->SetBackgroundColour(wxColor(32, 32, 32));
    m_button1->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button1, 0, wxALL, 2);

    m_button2 = new wxButton(this, wxID_ANY, _("DISABLE 24V "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button2->SetFont(bf);
    m_button2->SetBackgroundColour(wxColor(32, 32, 32));
    m_button2->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button2, 0, wxALL, 2);

    m_button3 = new wxButton(this, wxID_ANY, _(" ANT TUNE  "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button3->SetFont(bf);
    m_button3->SetBackgroundColour(wxColor(32, 32, 32));
    m_button3->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button3, 0, wxALL, 2);

    m_button4 = new wxButton(this, wxID_ANY, _(" TRANSMIT  "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button4->SetFont(bf);
    m_button4->SetBackgroundColour(wxColor(32, 32, 32));
    m_button4->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button4, 0, wxALL, 2);

    m_button5 = new wxButton(this, wxID_ANY, _(" RECEIVE  "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button5->SetFont(bf);
    m_button5->SetBackgroundColour(wxColor(32, 32, 32));
    m_button5->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button5, 0, wxALL, 2);

    m_buttonSync = new wxButton(this, wxID_ANY, _("   SYNC    "), wxDefaultPosition, wxSize(180, 40), 0);
    m_buttonSync->SetFont(bf);
    m_buttonSync->SetBackgroundColour(wxColor(32, 32, 32));
    m_buttonSync->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_buttonSync, 0, wxALL, 2);

    //wxStaticText* staticText1 = new wxStaticText(this, wxID_ANY, _("FREQUENCY"), wxDefaultPosition, wxSize(180, 40), 0);
    //staticText1->SetFont(bf);
    //gSizer1->Add(staticText1, 0, wxALL, 5);

    wxGridSizer* gSizer2;
    gSizer2 = new wxGridSizer(2, 2, 0, 0);
 

    m_textCtrl1 = new wxTextCtrl(this, wxID_ANY, _("14.25000"), wxDefaultPosition, wxSize(105,40), 0);
    m_textCtrl1->SetFont(bf);
    gSizer2->Add(m_textCtrl1, 0, wxALL, 5);

    m_button6 = new wxButton(this, wxID_ANY, _("MHz"), wxDefaultPosition, wxSize(60, 40), 0);
    m_button6->SetFont(bf);
    m_button6->SetBackgroundColour(wxColor(32, 32, 32));
    m_button6->SetForegroundColour(wxColor(255, 255, 128));
    gSizer2->Add(m_button6, 0, wxALL, 5);

    m_button7 = new wxButton(this, wxID_ANY, _(" - "), wxDefaultPosition, wxSize(60, 40), 0);
    m_button7->SetFont(bf);
    m_button7->SetBackgroundColour(wxColor(32, 32, 32));
    m_button7->SetForegroundColour(wxColor(255, 255, 128));
    gSizer2->Add(m_button7, 0, wxALL, 5);

    m_button8 = new wxButton(this, wxID_ANY, _(" + "), wxDefaultPosition, wxSize(60, 40), 0);
    m_button8->SetFont(bf);
    m_button8->SetBackgroundColour(wxColor(32, 32, 32));
    m_button8->SetForegroundColour(wxColor(255, 255, 128));
    gSizer2->Add(m_button8, 0, wxALL, 5);



    gSizer1->Add(gSizer2, 0, wxALL, 2);

    wxFlexGridSizer* gSizer3;
    gSizer3 = new wxFlexGridSizer(1, 3, 0, 0);
    gSizer3->SetFlexibleDirection(wxBOTH);
    gSizer3->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);


    //wxGridSizer* gSizer3;
    //gSizer3 = new wxGridSizer(1, 3, 0, 0);

    wxStaticText* staticText2 = new wxStaticText(this, wxID_ANY, _("STEP"), wxDefaultPosition, wxSize(50, 40), 0);
    staticText2->SetFont(bf);
    gSizer3->Add(staticText2, 0, wxALL, 2);

    m_textCtrl2 = new wxTextCtrl(this, wxID_ANY, _("1000"), wxDefaultPosition, wxSize(65, 40), 0);
    m_textCtrl2->SetFont(bf);
    gSizer3->Add(m_textCtrl2, 0, wxALL, 2);

    m_button9 = new wxButton(this, wxID_ANY, _("Hz"), wxDefaultPosition, wxSize(50, 40), 0);
    m_button9->SetFont(bf);
    m_button9->SetBackgroundColour(wxColor(32, 32, 32));
    m_button9->SetForegroundColour(wxColor(255, 255, 128));
    gSizer3->Add(m_button9, 0, wxALL, 2);

    //   m_button4 = new wxButton(this, wxID_ANY, _("MyButton"), wxDefaultPosition, wxDefaultSize, 0);
 //   gSizer1->Add(m_button4, 0, wxALL, 5);

 //   m_slider1 = new wxSlider(this, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
 //   gSizer1->Add(m_slider1, 0, wxALL, 5);
    gSizer1->Add(gSizer3, 0, wxALL, 2);

    m_button10 = new wxButton(this, wxID_ANY, _("Hotkeys = N"), wxDefaultPosition, wxSize(180, 40), 0);
    m_button10->SetFont(bf);
    m_button10->SetBackgroundColour(wxColor(32, 32, 32));
    m_button10->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button10, 0, wxALL, 2);

    
    m_textDebug = new wxTextCtrl(this, wxID_ANY, _("DEBUG"), wxDefaultPosition, wxSize(180, 40), 0);
    m_textDebug->SetFont(bf);
    gSizer1->Add(m_textDebug, 0, wxALL, 2);

    m_button11 = new wxButton(this, wxID_ANY, _("    LOG    "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button11->SetFont(bf);
    m_button11->SetBackgroundColour(wxColor(32, 64, 32));
    m_button11->SetForegroundColour(wxColor(128, 255, 128));
    gSizer1->Add(m_button11, 0, wxALL, 2);

    RemoteCallsign = wxEmptyString;

    bSizer1->Add(gSizer1, 0, wxALIGN_TOP, 5);


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

    SPtoTX = false;
    antTuneRun = false;
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
            m_button4->Enable(true);
        }
        prevMode = curMode;
    }

    myRadio->UpdatePlot();
    m_panel1->isPartial = true;
    m_panel1->audioModified = true;
    m_panel1->RFModified = true;
    m_panel1->Refresh(false);

}

void MyFrame::B1Click(wxCommandEvent& event) // CONNECT
{
    int retval = myRadio->Connect();

    if (retval)
    {
        m_button1->SetLabelText(_(" CONNECTED "));
        if (myRadio->LoadSettings("settings.json"))
        {
            m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
            m_myCallsign = wxString(myRadio->myCallsign, wxConvUTF8);
        }
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

void MyFrame::B6Click(wxCommandEvent& event) // MHz
{
    wxString value = m_textCtrl1->GetValue();
    double freq;
    value.ToDouble(&freq);
    if (freq < 14.150) freq = 14.150;
    if (freq > 14.347) freq = 14.347;
    myRadio->LOfreq = freq;
    m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
    myRadio->myStatus->UpdateText = true;
}

void MyFrame::B7Click(wxCommandEvent& event) // Step down
{
    myRadio->LOfreq -= myRadio->stepSize / 1.0e6;
    myRadio->LOfreq = round(myRadio->LOfreq * 10000.0) / 10000.0;
    if (myRadio->LOfreq < 14.150) myRadio->LOfreq = 14.150;
    m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
    myRadio->myStatus->UpdateText = true;
}

void MyFrame::B8Click(wxCommandEvent& event) // Step up
{
    myRadio->LOfreq += myRadio->stepSize / 1.0e6;
    myRadio->LOfreq = round(myRadio->LOfreq * 10000.0) / 10000.0;
    if (myRadio->LOfreq > 14.347) myRadio->LOfreq = 14.347;
    m_textCtrl1->SetValue(wxString::Format("%.4f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
    myRadio->myStatus->UpdateText = true;
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

void MyFrame::OnCharHook(wxKeyEvent& event)
{
    if (SPtoTX)
    {
        int key = event.GetKeyCode();
        if (key == 'T' && antTuneRun) { myRadio->myStatus->mode = TX_MODE; return; }
        if (key == 'R') { myRadio->myStatus->mode = RX_MODE; return; }
        if (key == WXK_RIGHT) { wxCommandEvent dummy; B8Click(dummy); return; }
        if (key == WXK_LEFT)  { wxCommandEvent dummy; B7Click(dummy); return; }
    }
    if (event.GetKeyCode() == 'L') { wxCommandEvent dummy; BLogClick(dummy); return; }
    event.Skip();
}

