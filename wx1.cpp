// Start of wxWidgets "Hello World" Program
#include <wx/wx.h>
#include <wx/numdlg.h>
#include <cmath>
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

    dc.DrawText(text, xofs + 10, yofs + 3);
    yofs += 24;
    ysize -= 24;

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
    for (int i = 0; i < 36; i++)
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
    yofs += 27;
    xsize -= 6;
    ysize -= 30;
    int radius = xsize >> 1;
    int xmid = xofs + radius;
    int ymid = yofs + radius;
    int lastx = 0, lasty = 0, x1 = 0, y1 = 0;

    dc.SetPen(wxPen(wxColor(160, 255, 160), 2)); // green pen
    for (int i = 0; i < 36; i++)
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

//Plot widget (float)
//inputs: array size, array pointer, x offset, y offset, x size, y size, y min, y max, horz grat count, vert grat count
void Plot(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float ymin, float ymax, int hgrat, int ygrat, wchar_t* text, int wfallPix = 0)
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

    dc.DrawText(text, xofs + 10, yofs + 3);
    yofs += 24;
    ysize -= 24;

    if (wfallPix > 0)
    {
        dc.SetPen(wxPen(wxColor(32, 32, 32), 0));
        dc.SetBrush(wxBrush(wxColor(0, 32, 192))); //
        dc.DrawRectangle(xofs, yofs + ysize - wfallPix + 2, xsize, wfallPix - 4); // Draw outer perimeter
        ysize -= wfallPix; // Leave room for waterfall if specified
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

    dc.SetPen(wxPen(wxColor(255, 255, 128), 2)); // yellow pen
    int xposLast = 0;
    int yposLast = 0;
    for (int i = 0; i < numElements; i++)
    {
        int xpos = (int)(xofs + 1.0 * i * xsize / (numElements-1));
        int ypos = (int)(yofs + (ymax - data[i] ) * ysize / (ymax - ymin));
        if (ypos < yofs) ypos = yofs;
        if (ypos > (yofs + ysize)) ypos = yofs + ysize;

        if(i > 0)
            dc.DrawLine(xposLast, yposLast, xpos, ypos); // draw line across the rectangle
        xposLast = xpos;
        yposLast = ypos;
    }

    // dc.SetPen(wxPen(wxColor(255, 0, 0), 1)); // 1-pixel-thick graticule

}

void Plot2(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float ymin, float ymax, int hgrat, int ygrat, wchar_t* text, float* data2)
{
    Plot(dc, xofs, yofs, xsize, ysize, numElements, data, ymin, ymax, hgrat, ygrat, text);

    xofs += 3; // Border and scale to match "plot"
    yofs += 27;
    xsize -= 6;
    ysize -= 30;
 
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

    f.SetPointSize(12);
    dc.SetFont(f);
    dc.SetTextForeground(wxColor(255, 255, 0));

    if (VSWRModified)
    {
        VSWRModified = false;
        wchar_t plot1Label[36] = _T("VSWR 14-14.35 MHz 0.5/");
        Plot2(dc, x3, midY, plotW, midH, 36, pRadio->myStatus->SWRUntuned, 1.0, 3.0, 4, 7, plot1Label, pRadio->myStatus->SWRTuned);

        wchar_t plot8Label[24] = _T("Antenna Impedance");
        SmithPlot2(dc, x4, midY, smithW, midH, 36, pRadio->myStatus->SmithChartUntuned, pRadio->myStatus->SmithChartTuned, plot8Label);
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
        sprintf_s(rfText, "RF Power vs Freq, %.3f - %.3f MHz, 5 kHz/, 10 dB/", pRadio->myStatus->TunerFreq - 0.02, pRadio->myStatus->TunerFreq + 0.02);
        wchar_t plot7Label[64];
        mbstowcs(plot7Label, rfText, 64);
        Plot(dc, margin, botY, botW, botH, 250, pRadio->myStatus->RFFreqPlot, -5.0, 3.0, 8, 8, plot7Label, 0);
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
    gSizer1 = new wxFlexGridSizer(10, 1, 5, 5);
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
    gSizer1->Add(m_button1, 0, wxALL, 5);

    m_button2 = new wxButton(this, wxID_ANY, _("DISABLE 24V "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button2->SetFont(bf);
    m_button2->SetBackgroundColour(wxColor(32, 32, 32));
    m_button2->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button2, 0, wxALL, 5);

    m_button3 = new wxButton(this, wxID_ANY, _(" ANT TUNE  "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button3->SetFont(bf);
    m_button3->SetBackgroundColour(wxColor(32, 32, 32));
    m_button3->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button3, 0, wxALL, 5);

    m_button4 = new wxButton(this, wxID_ANY, _(" TRANSMIT  "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button4->SetFont(bf);
    m_button4->SetBackgroundColour(wxColor(32, 32, 32));
    m_button4->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button4, 0, wxALL, 5);

    m_button5 = new wxButton(this, wxID_ANY, _(" RECEIVE  "), wxDefaultPosition, wxSize(180, 40), 0);
    m_button5->SetFont(bf);
    m_button5->SetBackgroundColour(wxColor(32, 32, 32));
    m_button5->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button5, 0, wxALL, 5);

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



    gSizer1->Add(gSizer2, 0, wxALL, 5);

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
    gSizer1->Add(gSizer3, 0, wxALL, 5);

    m_button10 = new wxButton(this, wxID_ANY, _("Hotkeys = N"), wxDefaultPosition, wxSize(180, 40), 0);
    m_button10->SetFont(bf);
    m_button10->SetBackgroundColour(wxColor(32, 32, 32));
    m_button10->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button10, 0, wxALL, 2);

    
    m_textDebug = new wxTextCtrl(this, wxID_ANY, _("DEBUG"), wxDefaultPosition, wxSize(180, 40), 0);
    m_textDebug->SetFont(bf);
    gSizer1->Add(m_textDebug, 0, wxALL, 2);

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

    SPtoTX = false;
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
        m_textCtrl1->SetValue(wxString::Format("%.5f", myRadio->LOfreq));
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
    wchar_t label[16];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    mbstowcs(label, myRadio->dbgText, 16);
    m_textDebug->SetLabelText(label);

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
        m_button1->SetLabelText(_(" CONNECTED "));

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

void MyFrame::B6Click(wxCommandEvent& event) // MHz
{
    wxString value = m_textCtrl1->GetValue();
    double freq;
    value.ToDouble(&freq);
    myRadio->LOfreq = freq;
    myRadio->NewLOFreq = true;
}

void MyFrame::B7Click(wxCommandEvent& event) // Step down
{
    myRadio->LOfreq -= myRadio->stepSize / 1e6f;
    m_textCtrl1->SetValue(wxString::Format("%.5f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
}

void MyFrame::B8Click(wxCommandEvent& event) // Step up
{
    myRadio->LOfreq += myRadio->stepSize / 1e6f;
    m_textCtrl1->SetValue(wxString::Format("%.5f", myRadio->LOfreq));
    myRadio->NewLOFreq = true;
}

void MyFrame::B9Click(wxCommandEvent& event) // Hz (update step size)
{
    wxString value = m_textCtrl2->GetValue();
    double step;
    if (value.ToDouble(&step))
        myRadio->stepSize = (float)step;
}

void MyFrame::B10Click(wxCommandEvent& event)
{
    SPtoTX = !SPtoTX;
    m_button10->SetLabelText(SPtoTX ? _("Hotkeys = Y") : _("Hotkeys = N"));
}

void MyFrame::OnCharHook(wxKeyEvent& event)
{
    if (SPtoTX)
    {
        int key = event.GetKeyCode();
        if (key == 'T') { myRadio->myStatus->mode = TX_MODE; return; }
        if (key == 'R') { myRadio->myStatus->mode = RX_MODE; return; }
        if (key == WXK_RIGHT) { wxCommandEvent dummy; B8Click(dummy); return; }
        if (key == WXK_LEFT)  { wxCommandEvent dummy; B7Click(dummy); return; }
    }
    event.Skip();
}

