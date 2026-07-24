///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/slider.h>
#include <wx/frame.h>
#include <cstring>

class CRadio;

///////////////////////////////////////////////////////////////////////////
class BasicDrawPane : public wxPanel
{

public:
    BasicDrawPane(wxWindow* parent, void *vpRadio,
        wxWindowID winid = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTAB_TRAVERSAL | wxNO_BORDER,
        const wxString& name = wxASCII_STR(wxPanelNameStr))
    {
        Create(parent, winid, pos, size, style, name);
        pRadio = (CRadio *) vpRadio;
        textModified = true;
        audioModified = true;
        VSWRModified = true;
        RFModified = true;
        isPartial = false;
        memset(wfPixels, 0, sizeof(wfPixels));
    }

    static constexpr int WF_ROWS = 100;
    static constexpr int WF_BINS = 250;
    unsigned char wfPixels[WF_ROWS * WF_BINS * 3];

    bool textModified;
    bool audioModified;
    bool RFModified;
    bool VSWRModified;
    bool isPartial;

    void paintEvent(wxPaintEvent& evt);
    void paintNow();
    void OnSize(wxSizeEvent& event);

    void render(wxDC& dc);
    CRadio* pRadio;
    // some useful events
    /*
     void mouseMoved(wxMouseEvent& event);
     void mouseDown(wxMouseEvent& event);
     void mouseWheelMoved(wxMouseEvent& event);
     void mouseReleased(wxMouseEvent& event);
     void rightClick(wxMouseEvent& event);
     void mouseLeftWindow(wxMouseEvent& event);
     void keyPressed(wxKeyEvent& event);
     void keyReleased(wxKeyEvent& event);
     */
    DECLARE_EVENT_TABLE()

};

BEGIN_EVENT_TABLE(BasicDrawPane, wxPanel)
// some useful events
/*
 EVT_MOTION(BasicDrawPane::mouseMoved)
 EVT_LEFT_DOWN(BasicDrawPane::mouseDown)
 EVT_LEFT_UP(BasicDrawPane::mouseReleased)
 EVT_RIGHT_DOWN(BasicDrawPane::rightClick)
 EVT_LEAVE_WINDOW(BasicDrawPane::mouseLeftWindow)
 EVT_KEY_DOWN(BasicDrawPane::keyPressed)
 EVT_KEY_UP(BasicDrawPane::keyReleased)
 EVT_MOUSEWHEEL(BasicDrawPane::mouseWheelMoved)
 */

 // catch paint events
EVT_PAINT(BasicDrawPane::paintEvent)
EVT_SIZE(BasicDrawPane::OnSize)

END_EVENT_TABLE()

#define TIMER_ID 1
enum {
    ID_RADIO_SET_COM   = wxID_HIGHEST + 1,
    ID_SWEEP_OPEN,
    ID_SWEEP_SHORT,
    ID_SWEEP_LOAD,
    ID_SWEEP_TUNER,
    ID_SWEEP_ANTENNA,
    ID_AUDIO_MAX_MIC_GAIN,
    ID_AUDIO_CESSB_SETPOINT,
    ID_PLOT_SETTINGS,
    ID_MY_CALLSIGN,
    ID_IMAGE_REJECT,
    ID_CW_MODE,
};
///////////////////////////////////////////////////////////////////////////////
/// Class MyFrame1
///////////////////////////////////////////////////////////////////////////////
class MyFrame : public wxFrame
{
private:

protected:
    BasicDrawPane* m_panel1;
	wxButton* m_button1;
	wxButton* m_button2;
	wxButton* m_button3;
	wxButton* m_button4;
    wxButton* m_button5;
    wxButton* m_button6;
    wxButton* m_button7;
    wxButton* m_button8;
    wxButton* m_button9;
    wxButton* m_button10;
    wxButton* m_button11;
    wxButton* m_buttonSync;
    wxButton* m_buttonViewLog;
    wxSlider* m_slider1;
    wxTextCtrl* m_textCtrl1;
    wxTextCtrl* m_textCtrl2;
    wxTextCtrl* m_textDebug;


public:

	MyFrame(
        wxWindow* parent, 
        wxWindowID id = wxID_ANY, 
        const wxString& title = wxEmptyString, 
        const wxPoint& pos = wxDefaultPosition, 
        const wxSize& size = wxSize(1280, 800), 
        long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL);

    wxTimer m_timer;

	~MyFrame();
    void B1Click(wxCommandEvent& event);
    void B2Click(wxCommandEvent& event);
    void B3Click(wxCommandEvent& event);
    void B4Click(wxCommandEvent& event);
    void B5Click(wxCommandEvent& event);
    void B6Click(wxCommandEvent& event);
    void B7Click(wxCommandEvent& event);
    void B8Click(wxCommandEvent& event);
    void B9Click(wxCommandEvent& event);
    void B10Click(wxCommandEvent& event);
    void OnCharHook(wxKeyEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnFileSave(wxCommandEvent& event);
    void OnFileLoad(wxCommandEvent& event);
    void OnSetComPort(wxCommandEvent& event);
    void OnSweepOpen(wxCommandEvent& event);
    void OnSweepShort(wxCommandEvent& event);
    void OnSweepLoad(wxCommandEvent& event);
    void OnSweepTuner(wxCommandEvent& event);
    void OnSweepAntenna(wxCommandEvent& event);
    void OnAudioMaxMicGain(wxCommandEvent& event);
    void OnAudioCESSBSetpoint(wxCommandEvent& event);
    void OnPlotSettings(wxCommandEvent& event);
    void OnMyCallsign(wxCommandEvent& event);
    void OnImageReject(wxCommandEvent& event);
    void OnCWMode(wxCommandEvent& event);
    void BLogClick(wxCommandEvent& event);
    void BSyncClick(wxCommandEvent& event);
    void BViewLogClick(wxCommandEvent& event);
    void UpdateDebugText(char* text);

    CRadio*   myRadio;
    bool      SPtoTX;
    bool      antTuneRun;
    wxString  RemoteCallsign;
    wxString  m_myCallsign;
    bool      m_potaChecked = false;
    wxString  m_potaPark;
    bool      m_p2pChecked = false;
    wxString  m_p2pPark;
    wxDECLARE_EVENT_TABLE();

};
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_TIMER(TIMER_ID, MyFrame::OnTimer)
wxEND_EVENT_TABLE()


#pragma once
