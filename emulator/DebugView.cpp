/*  This file is part of NEMIGABTL.
    NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// DebugView.cpp

#include "stdafx.h"
#include <commctrl.h>
#include "Main.h"
#include "Views.h"
#include "ToolWindow.h"
#include "Emulator.h"
#include "emubase\Emubase.h"

//////////////////////////////////////////////////////////////////////


HWND g_hwndDebug = (HWND) INVALID_HANDLE_VALUE;  // Debug View window handle
WNDPROC m_wndprocDebugToolWindow = NULL;  // Old window proc address of the ToolWindow

HWND m_hwndDebugViewer = (HWND) INVALID_HANDLE_VALUE;
HWND m_hwndDebugToolbar = (HWND) INVALID_HANDLE_VALUE;

WORD m_wDebugCpuR[9];  // Old register values - R0..R7, PSW
BOOL m_okDebugCpuRChanged[9];  // Register change flags
WORD m_wDebugCpuPswOld;  // PSW value on previous step
WORD m_wDebugCpuR6Old;  // SP value on previous step

void DebugView_DoDraw(HDC hdc);
BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM lParam);
void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged, WORD oldPsw);
void DebugView_DrawMemoryForRegister(HDC hdc, int reg, const CProcessor* pProc, int x, int y, WORD oldValue);
BOOL DebugView_DrawWatchpoints(HDC hdc, const CProcessor* pProc, int x, int y);
void DebugView_DrawPorts(HDC hdc, const CMotherboard* pBoard, int x, int y);
BOOL DebugView_DrawBreakpoints(HDC hdc, int x, int y);
void DebugView_UpdateWindowText();


//////////////////////////////////////////////////////////////////////

BOOL DebugView_IsRegisterChanged(int regno)
{
    ASSERT(regno >= 0 && regno <= 8);
    return m_okDebugCpuRChanged[regno];
}

void DebugView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = DebugViewViewerWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_DEBUGVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void DebugView_Init()
{
    memset(m_wDebugCpuR, 255, sizeof(m_wDebugCpuR));
    memset(m_okDebugCpuRChanged, 1, sizeof(m_okDebugCpuRChanged));
    m_wDebugCpuPswOld = 0;
    m_wDebugCpuR6Old = 0;
}

void DebugView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndDebug = CreateWindow(
            CLASSNAME_TOOLWINDOW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
    DebugView_UpdateWindowText();

    // ToolWindow subclassing
    m_wndprocDebugToolWindow = (WNDPROC) LongToPtr( SetWindowLongPtr(
            g_hwndDebug, GWLP_WNDPROC, PtrToLong(DebugViewWndProc)) );

    RECT rcClient;  GetClientRect(g_hwndDebug, &rcClient);

    m_hwndDebugViewer = CreateWindowEx(
            WS_EX_STATICEDGE,
            CLASSNAME_DEBUGVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, rcClient.right, rcClient.bottom,
            g_hwndDebug, NULL, g_hInst, NULL);

    m_hwndDebugToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER | CCS_VERT,
            4, 4, 32, rcClient.bottom, m_hwndDebugViewer,
            (HMENU) 102,
            g_hInst, NULL);

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndDebugToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    SendMessage(m_hwndDebugToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndDebugToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBBUTTON buttons[3];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
        buttons[i].fsStyle = BTNS_BUTTON;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_VIEW_DEBUG;
    buttons[0].iBitmap = ToolbarImageDebugger;
    buttons[0].fsState = TBSTATE_ENABLED | TBSTATE_WRAP | TBSTATE_CHECKED;
    buttons[1].idCommand = ID_DEBUG_STEPINTO;
    buttons[1].iBitmap = ToolbarImageStepInto;
    buttons[2].idCommand = ID_DEBUG_STEPOVER;
    buttons[2].iBitmap = ToolbarImageStepOver;

    SendMessage(m_hwndDebugToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons);
}

void DebugView_Redraw()
{
    RedrawWindow(g_hwndDebug, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Adjust position of client windows
void DebugView_AdjustWindowLayout()
{
    RECT rc;  GetClientRect(g_hwndDebug, &rc);

    if (m_hwndDebugViewer != (HWND) INVALID_HANDLE_VALUE)
        SetWindowPos(m_hwndDebugViewer, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
}

LRESULT CALLBACK DebugViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    LRESULT lResult;
    switch (message)
    {
    case WM_DESTROY:
        g_hwndDebug = (HWND) INVALID_HANDLE_VALUE;  // We are closed! Bye-bye!..
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    case WM_SIZE:
        lResult = CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
        DebugView_AdjustWindowLayout();
        return lResult;
    default:
        return CallWindowProc(m_wndprocDebugToolWindow, hWnd, message, wParam, lParam);
    }
    //return (LRESULT)FALSE;
}

LRESULT CALLBACK DebugViewViewerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_COMMAND:
        ::PostMessage(g_hwnd, WM_COMMAND, wParam, lParam);
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            DebugView_DoDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        ::SetFocus(hWnd);
        break;
    case WM_KEYDOWN:
        return (LRESULT) DebugView_OnKeyDown(wParam, lParam);
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::InvalidateRect(hWnd, NULL, TRUE);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

BOOL DebugView_OnKeyDown(WPARAM vkey, LPARAM /*lParam*/)
{
    switch (vkey)
    {
    case VK_ESCAPE:
        ConsoleView_Activate();
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void DebugView_UpdateWindowText()
{
    ::SetWindowText(g_hwndDebug, _T("Debug"));
}


//////////////////////////////////////////////////////////////////////

// Update after Run or Step
void DebugView_OnUpdate()
{
    CProcessor* pCPU = g_pBoard->GetCPU();
    ASSERT(pCPU != nullptr);

    // Get new register values and set change flags
    m_wDebugCpuR6Old = m_wDebugCpuR[6];
    for (int r = 0; r < 8; r++)
    {
        WORD value = pCPU->GetReg(r);
        m_okDebugCpuRChanged[r] = (m_wDebugCpuR[r] != value);
        m_wDebugCpuR[r] = value;
    }
    WORD pswCPU = pCPU->GetPSW();
    m_okDebugCpuRChanged[8] = (m_wDebugCpuR[8] != pswCPU);
    m_wDebugCpuPswOld = m_wDebugCpuR[8];
    m_wDebugCpuR[8] = pswCPU;
}


//////////////////////////////////////////////////////////////////////
// Draw functions

void DebugView_DoDraw(HDC hdc)
{
    ASSERT(g_pBoard != nullptr);

    // Create and select font
    HFONT hFont = CreateMonospacedFont();
    HGDIOBJ hOldFont = SelectObject(hdc, hFont);
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    COLORREF colorBkOld = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    CProcessor* pDebugPU = g_pBoard->GetCPU();
    ASSERT(pDebugPU != nullptr);
    WORD* arrR = m_wDebugCpuR;
    BOOL* arrRChanged = m_okDebugCpuRChanged;
    WORD oldPsw = m_wDebugCpuPswOld;
    WORD oldSP = m_wDebugCpuR6Old;

    //TextOut(hdc, cxChar * 1, 2 + 1 * cyLine, _T("CPU"), 3);

    DebugView_DrawProcessor(hdc, pDebugPU, 30 + cxChar * 2, 2 + 1 * cyLine, arrR, arrRChanged, oldPsw);

    // Draw stack for the current processor
    DebugView_DrawMemoryForRegister(hdc, 6, pDebugPU, 30 + 36 * cxChar, 2 + 0 * cyLine, oldSP);

    BOOL okWatches = DebugView_DrawWatchpoints(hdc, pDebugPU, 30 + 56 * cxChar, 2 + 0 * cyLine);
    if (!okWatches)
        DebugView_DrawPorts(hdc, g_pBoard, 30 + 56 * cxChar, 2 + 0 * cyLine);

    DebugView_DrawBreakpoints(hdc, 30 + 85 * cxChar, 2 + 0 * cyLine);

    SetTextColor(hdc, colorOld);
    SetBkColor(hdc, colorBkOld);
    SelectObject(hdc, hOldFont);
    VERIFY(::DeleteObject(hFont));

    if (::GetFocus() == m_hwndDebugViewer)
    {
        RECT rcClient;
        GetClientRect(m_hwndDebugViewer, &rcClient);
        DrawFocusRect(hdc, &rcClient);
    }
}

void DebugView_DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2)
{
    HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetSysColorBrush(COLOR_BTNSHADOW));
    PatBlt(hdc, x1, y1, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x1, y1, 1, y2 - y1, PATCOPY);
    PatBlt(hdc, x1, y2, x2 - x1, 1, PATCOPY);
    PatBlt(hdc, x2, y1, 1, y2 - y1 + 1, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
}

void DebugView_DrawProcessor(HDC hdc, const CProcessor* pProc, int x, int y, WORD* arrR, BOOL* arrRChanged, WORD oldPsw)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    ::SetTextColor(hdc, colorText);

    DebugView_DrawRectangle(hdc, x - cxChar, y - 8, x + cxChar + 31 * cxChar, y + 8 + cyLine * 14);

    // Registers
    for (int r = 0; r < 8; r++)
    {
        ::SetTextColor(hdc, arrRChanged[r] ? colorChanged : colorText);

        LPCTSTR strRegName = REGISTER_NAME[r];
        TextOut(hdc, x, y + r * cyLine, strRegName, (int) _tcslen(strRegName));

        WORD value = arrR[r]; //pProc->GetReg(r);
        DrawOctalValue(hdc, x + cxChar * 3, y + r * cyLine, value);
        DrawHexValue(hdc, x + cxChar * 10, y + r * cyLine, value);
        DrawBinaryValue(hdc, x + cxChar * 15, y + r * cyLine, value);
    }
    ::SetTextColor(hdc, colorText);

    // PSW value
    ::SetTextColor(hdc, arrRChanged[8] ? colorChanged : colorText);
    TextOut(hdc, x, y + 9 * cyLine, _T("PS"), 2);
    WORD psw = arrR[8]; // pProc->GetPSW();
    DrawOctalValue(hdc, x + cxChar * 3, y + 9 * cyLine, psw);
    DrawHexValue(hdc, x + cxChar * 10, y + 9 * cyLine, psw);
    ::SetTextColor(hdc, colorText);
    TextOut(hdc, x + cxChar * 15, y + 8 * cyLine, _T("       HP  TNZVC"), 16);

    // PSW value bits colored bit-by-bit
    TCHAR buffera[2];  buffera[1] = 0;
    for (int i = 0; i < 16; i++)
    {
        WORD bitpos = 1 << i;
        buffera[0] = (psw & bitpos) ? '1' : '0';
        ::SetTextColor(hdc, ((psw & bitpos) != (oldPsw & bitpos)) ? colorChanged : colorText);
        TextOut(hdc, x + cxChar * (15 + 15 - i), y + 9 * cyLine, buffera, 1);
    }

    ::SetTextColor(hdc, colorText);

    // Processor mode - HALT or USER
    BOOL okHaltMode = pProc->IsHaltMode();
    TextOut(hdc, x, y + 11 * cyLine, okHaltMode ? _T("HALT") : _T("USER"), 4);

    // "Stopped" flag
    BOOL okStopped = pProc->IsStopped();
    if (okStopped)
        TextOut(hdc, x + 6 * cxChar, y + 11 * cyLine, _T("STOP"), 4);
}

void DebugView_DrawAddressAndValue(HDC hdc, const CProcessor* pProc, CMotherboard* pBoard, uint16_t address, int x, int y, int cxChar)
{
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    SetTextColor(hdc, colorText);
    DrawOctalValue(hdc, x + 0 * cxChar, y, address);
    x += 7 * cxChar;

    int addrtype = ADDRTYPE_DENY;
    uint16_t value = pBoard->GetWordView(address, pProc->IsHaltMode(), FALSE, &addrtype);
    if (addrtype == ADDRTYPE_RAM || addrtype == ADDRTYPE_HIRAM)
    {
        DrawOctalValue(hdc, x, y, value);
    }
    else if (addrtype == ADDRTYPE_ROM)
    {
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryRom));
        DrawOctalValue(hdc, x, y, value);
    }
    else if (addrtype == ADDRTYPE_IO || addrtype == ADDRTYPE_TERM)
    {
        value = pBoard->GetPortView(address);
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryIO));
        DrawOctalValue(hdc, x, y, value);
    }
    else //if (addrtype == ADDRTYPE_DENY)
    {
        SetTextColor(hdc, Settings_GetColor(ColorDebugMemoryNA));
        TextOut(hdc, x, y, _T("  NA  "), 6);
    }

    SetTextColor(hdc, colorText);
}

void DebugView_DrawMemoryForRegister(HDC hdc, int reg, const CProcessor* pProc, int x, int y, WORD oldValue)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);
    COLORREF colorPrev = Settings_GetColor(ColorDebugPrevious);
    COLORREF colorOld = SetTextColor(hdc, colorText);

    uint16_t current = pProc->GetReg(reg) & ~1;
    uint16_t previous = oldValue;
    bool okExec = (reg == 7);

    // Reading from memory into the buffer
    uint16_t memory[16];
    int addrtype[16];
    for (int idx = 0; idx < 16; idx++)
    {
        memory[idx] = g_pBoard->GetWordView(
                (uint16_t)(current + idx * 2 - 16), pProc->IsHaltMode(), okExec, addrtype + idx);
    }

    WORD address = current - 16;
    for (int index = 0; index < 16; index++)
    {
        DebugView_DrawAddressAndValue(hdc, pProc, g_pBoard, address, x + 3 * cxChar, y, cxChar);

        if (address == current)  // Current position
        {
            SetTextColor(hdc, colorText);
            TextOut(hdc, x + 2 * cxChar, y, _T(">"), 1);
            if (current != previous) SetTextColor(hdc, colorChanged);
            TextOut(hdc, x, y, REGISTER_NAME[reg], 2);
        }
        else if (address == previous)
        {
            SetTextColor(hdc, colorPrev);
            TextOut(hdc, x + 2 * cxChar, y, _T(">"), 1);
        }

        address += 2;
        y += cyLine;
    }

    SetTextColor(hdc, colorOld);
}

BOOL DebugView_DrawWatchpoints(HDC hdc, const CProcessor* pProc, int x, int y)
{
    const uint16_t* pws = Emulator_GetWatchpointList();
    if (*pws == 0177777)
        return FALSE;

    bool okHaltMode = pProc->IsHaltMode();

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);
    COLORREF colorText = Settings_GetColor(ColorDebugText);
    COLORREF colorChanged = Settings_GetColor(ColorDebugValueChanged);

    TextOut(hdc, x, y, _T("Watches:"), 8);
    y += cyLine;
    while (*pws != 0177777)
    {
        uint16_t address = *pws;

        //TODO: Replace it with call to DebugView_DrawAddressAndValue()
        SetTextColor(hdc, colorText);
        DrawOctalValue(hdc, x, y, address);
        int addrtype;
        uint16_t value = g_pBoard->GetWordView(address, okHaltMode, false, &addrtype);
        uint16_t wChanged = Emulator_GetChangeRamStatus(address);
        SetTextColor(hdc, (wChanged != 0) ? colorChanged : colorText);
        DrawOctalValue(hdc, x + 8 * cxChar, y, value);

        y += cyLine;
        pws++;
    }

    return TRUE;
}

void DebugView_DrawPorts(HDC hdc, const CMotherboard* /*pBoard*/, int x, int y)
{
    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    TextOut(hdc, x, y, _T("Port"), 6);

    WORD value;
    y += cyLine;
    value = g_pBoard->GetPortView(0170006);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0170006);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("System"), 6);
    y += cyLine;
    value = g_pBoard->GetPortView(0177660);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177572);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("VADDR"), 5);
    y += cyLine;
    value = g_pBoard->GetPortView(0177570);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177570);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("VDATA"), 5);
    y += cyLine;
    value = g_pBoard->GetPortView(0177100);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177100);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("FDD state"), 9);
    y += cyLine;
    value = g_pBoard->GetPortView(0177102);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177102);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("FDD data"), 8);
    y += cyLine;
    value = g_pBoard->GetPortView(0177106);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177106);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("FDD timer"), 9);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177712);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177712);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("timer manage"), 12);
    y += cyLine;
    value = g_pBoard->GetPortView(0177514);
    DrawOctalValue(hdc, x + 0 * cxChar, y, 0177514);
    DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    //DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    TextOut(hdc, x + 16 * cxChar, y, _T("parallel"), 8);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177716);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177716);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("system"), 6);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177130);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177130);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("floppy state"), 12);
    //y += cyLine;
    //value = g_pBoard->GetPortView(0177132);
    //DrawOctalValue(hdc, x + 0 * cxChar, y, 0177132);
    //DrawOctalValue(hdc, x + 8 * cxChar, y, value);
    ////DrawBinaryValue(hdc, x + 15 * cxChar, y, value);
    //TextOut(hdc, x + 16 * cxChar, y, _T("floppy data"), 11);
}

BOOL DebugView_DrawBreakpoints(HDC hdc, int x, int y)
{
    const uint16_t* pbps = Emulator_GetCPUBreakpointList();
    if (*pbps == 0177777)
        return FALSE;

    int cxChar, cyLine;  GetFontWidthAndHeight(hdc, &cxChar, &cyLine);

    TextOut(hdc, x, y, _T("Breakpts:"), 9);
    y += cyLine;
    while (*pbps != 0177777)
    {
        DrawOctalValue(hdc, x, y, *pbps);
        y += cyLine;
        pbps++;
    }
    return TRUE;
}


//////////////////////////////////////////////////////////////////////
