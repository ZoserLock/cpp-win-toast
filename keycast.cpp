// Copyright © 2014 Brook Hong. All Rights Reserved.

#include "keycast.h"

#include <windowsx.h>
#include <Commctrl.h>
#include <stdio.h>

#include <gdiplus.h>

#include "resource.h"
#include "timer.h"

using namespace Gdiplus;

#define SHOWTIMER_INTERVAL 40

CTimer showTimer;
CTimer appTimer;

#define MAXCHARS 4096
WCHAR textBuffer[MAXCHARS];
LPCWSTR textBufferEnd = textBuffer + MAXCHARS;

struct KeyLabel {
    RectF rect;
    WCHAR *text;
    DWORD length;
    int time;
    BOOL fade;
    KeyLabel() {
        text = textBuffer;
        length = 0;
    }
};

struct LabelSettings 
{
    DWORD lingerTime;
    DWORD fadeDuration;
    LOGFONT font;
    COLORREF bgColor, textColor, borderColor;
    DWORD bgOpacity, textOpacity, borderOpacity;
    int borderSize;
    int cornerSize;
};

LabelSettings labelSettings;

Color clearColor(0, 127, 127, 127);

POINT deskOrigin;

KeyLabel keyLabel;

RECT desktopRect;
SIZE canvasSize;
POINT canvasOrigin;

WCHAR *szWinName = L"ZWinToast";

HWND hMainWnd;

HINSTANCE hInstance;

Graphics * gCanvas = NULL;
Font * fontPlus = NULL;

void showText(LPCWSTR text);

void updateLayeredWindow(HWND hwnd) 
{
    POINT ptSrc = {0, 0};
    BLENDFUNCTION blendFunction;
    blendFunction.AlphaFormat = AC_SRC_ALPHA;
    blendFunction.BlendFlags = 0;
    blendFunction.BlendOp = AC_SRC_OVER;
    blendFunction.SourceConstantAlpha = 255;
    HDC hdcBuf = gCanvas->GetHDC();
    HDC hdc = GetDC(hwnd);
    ::UpdateLayeredWindow(hwnd,hdc,&canvasOrigin,&canvasSize,hdcBuf,&ptSrc,0,&blendFunction,2);
    ReleaseDC(hwnd, hdc);
    gCanvas->ReleaseHDC(hdcBuf);
}

void eraseLabel() 
{
    RectF &rt = keyLabel.rect;
    RectF rc(rt.X-labelSettings.borderSize, rt.Y-labelSettings.borderSize, rt.Width+2*labelSettings.borderSize+1, rt.Height+2*labelSettings.borderSize+1);
    gCanvas->SetClip(rc);
    gCanvas->Clear(clearColor);
    gCanvas->ResetClip();
}

void drawLabelFrame(Graphics* g, const Pen* pen, const Brush* brush, RectF &rc, REAL cornerSize) 
{
    if(cornerSize > 0) 
    {
        GraphicsPath path;
        REAL dx = rc.Width - cornerSize, dy = rc.Height - cornerSize;
        path.AddArc(rc.X, rc.Y, cornerSize, cornerSize, 170, 90);
        path.AddArc(rc.X + dx, rc.Y, cornerSize, cornerSize, 270, 90);
        path.AddArc(rc.X + dx, rc.Y + dy, cornerSize, cornerSize, 0, 90);
        path.AddArc(rc.X, rc.Y + dy, cornerSize, cornerSize, 90, 90);
        path.CloseFigure();

        g->DrawPath(pen, &path);
        g->FillPath(brush, &path);
    } 
    else 
    {
        g->DrawRectangle(pen, rc.X, rc.Y, rc.Width, rc.Height);
        g->FillRectangle(brush, rc.X, rc.Y, rc.Width, rc.Height);
    }
}

#define BR(alpha, bgr) (alpha<<24|bgr>>16|(bgr&0x0000ff00)|(bgr&0x000000ff)<<16)

void updateLabel() 
{
    eraseLabel();

    if(keyLabel.length > 0) 
    {
        RectF &rc = keyLabel.rect;
        REAL r = 1.0f * keyLabel.time / labelSettings.fadeDuration;
        r = (r > 1.0f) ? 1.0f : r;
        PointF origin(rc.X, rc.Y);
        gCanvas->MeasureString(keyLabel.text, keyLabel.length, fontPlus, origin, &rc);
        rc.Width = (rc.Width < labelSettings.cornerSize) ? labelSettings.cornerSize : rc.Width;

        rc.X = canvasSize.cx - rc.Width - labelSettings.borderSize;
       
        rc.Height = (rc.Height < labelSettings.cornerSize) ? labelSettings.cornerSize : rc.Height;
        int bgAlpha = (int)(r*labelSettings.bgOpacity), textAlpha = (int)(r*labelSettings.textOpacity), borderAlpha = (int)(r*labelSettings.borderOpacity);
        Pen penPlus(Color::Color(BR(borderAlpha, labelSettings.borderColor)), labelSettings.borderSize+0.0f);
        SolidBrush brushPlus(Color::Color(BR(bgAlpha, labelSettings.bgColor)));
        drawLabelFrame(gCanvas, &penPlus, &brushPlus, rc, (REAL)labelSettings.cornerSize);
        SolidBrush textBrushPlus(Color(BR(textAlpha, labelSettings.textColor)));
        gCanvas->DrawString( keyLabel.text,
                keyLabel.length,
                fontPlus,
                PointF(rc.X, rc.Y),
                &textBrushPlus);
    }
}

static void close()
{
    PostMessage(hMainWnd, WM_CLOSE, 0, 0);
}

static void fadeUpdate() 
{
    DWORD i = 0;
    BOOL dirty = FALSE;

    RectF &rt = keyLabel.rect;
    if(keyLabel.time > labelSettings.fadeDuration) 
    {
        if(keyLabel.fade) 
        {
            keyLabel.time -= SHOWTIMER_INTERVAL;
        }
    } 
    else if(keyLabel.time >= SHOWTIMER_INTERVAL) 
    {
        if(keyLabel.fade) 
        {
            keyLabel.time -= SHOWTIMER_INTERVAL;
        }
        updateLabel();
        dirty = TRUE;
    } 
    else 
    {
        keyLabel.time = 0;
        if(keyLabel.length)
        {
            eraseLabel();
            keyLabel.length--;
            dirty = TRUE;
        }
    }
    
    if(dirty) 
    {
        updateLayeredWindow(hMainWnd);
    }
}

void showText(LPCWSTR text) 
{
    SetWindowPos(hMainWnd,HWND_TOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE);
    size_t newLen = wcslen(text);

    wcscpy_s(keyLabel.text, textBufferEnd - keyLabel.text, text);
    keyLabel.length = newLen;
    
    keyLabel.time = labelSettings.lingerTime + labelSettings.fadeDuration;
    keyLabel.fade = TRUE;

    updateLabel();

    updateLayeredWindow(hMainWnd);
}

void UpdateCanvasSize(const POINT &pt) 
{
    if (keyLabel.time > 0)
    {
        eraseLabel();
        keyLabel.time = 0;
    }
    
    canvasSize.cy = desktopRect.bottom - desktopRect.top;
    canvasOrigin.y = pt.y - desktopRect.bottom + desktopRect.top;
    canvasSize.cx = pt.x - desktopRect.left;
    canvasOrigin.x = desktopRect.left;
}

void CreateCanvas() 
{
    HDC hdc = GetDC(hMainWnd);
    HDC hdcBuffer = CreateCompatibleDC(hdc);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc, desktopRect.right - desktopRect.left, desktopRect.bottom - desktopRect.top);
    HBITMAP hBitmapOld = (HBITMAP)SelectObject(hdcBuffer, (HGDIOBJ)hbitmap);
    ReleaseDC(hMainWnd, hdc);
    DeleteObject(hBitmapOld);

    if(gCanvas) 
    {
        delete gCanvas;
    }

    gCanvas = new Graphics(hdcBuffer);
    gCanvas->SetSmoothingMode(SmoothingModeAntiAlias);
    gCanvas->SetTextRenderingHint(TextRenderingHintAntiAlias);
}

void PrepareLabels() 
{
    HDC hdc = GetDC(hMainWnd);
    HFONT hlabelFont = CreateFontIndirect(&labelSettings.font);
    HFONT hFontOld = (HFONT)SelectObject(hdc, hlabelFont);
    DeleteObject(hFontOld);

    if(fontPlus) 
    {
        delete fontPlus;
    }

    fontPlus = new Font(hdc, hlabelFont);
    ReleaseDC(hMainWnd, hdc);
    RectF box;
    PointF origin(0, 0);

    gCanvas->MeasureString(L"\u263b - KeyCastOW OFF", 16, fontPlus, origin, &box);

    REAL unitH = box.Height + 2 * labelSettings.borderSize;

    REAL paddingH = (desktopRect.bottom - desktopRect.top) - unitH;

    gCanvas->Clear(clearColor);

    keyLabel.rect.X = (REAL)labelSettings.borderSize;
    keyLabel.rect.Y = paddingH + labelSettings.borderSize;

    if (keyLabel.time > labelSettings.lingerTime + labelSettings.fadeDuration)
    {
        keyLabel.time = labelSettings.lingerTime + labelSettings.fadeDuration;
    }

    if (keyLabel.time > 0)
    {
        updateLabel();
    }
}

void GetWorkAreaByOrigin(const POINT &pt, MONITORINFO &mi)
{
    RECT rc = {pt.x-1, pt.y-1, pt.x+1, pt.y+1};
    HMONITOR hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
}

void FixDeskOrigin() 
{
    if(deskOrigin.x > desktopRect.right || deskOrigin.x < desktopRect.left + labelSettings.borderSize) 
    {
        deskOrigin.x = desktopRect.right - labelSettings.borderSize;
    }

    if(deskOrigin.y > desktopRect.bottom || deskOrigin.y < desktopRect.top + labelSettings.borderSize) 
    {
        deskOrigin.y = desktopRect.bottom;
    }
}

void LoadSettings() 
{
    labelSettings.lingerTime    = 1200;
    labelSettings.fadeDuration  = 310;
    labelSettings.bgColor       = RGB(75, 75, 75);
    labelSettings.textColor     = RGB(255, 255, 255);
    labelSettings.bgOpacity     = 200;
    labelSettings.textOpacity   = 255;
    labelSettings.borderOpacity = 200;
    labelSettings.borderColor   = RGB(0, 128, 255);
    labelSettings.borderSize    = 8;
    labelSettings.cornerSize    = 2;

    deskOrigin.x = 2;
    deskOrigin.y = 2;

    MONITORINFO mi;
    GetWorkAreaByOrigin(deskOrigin, mi);
    CopyMemory(&desktopRect, &mi.rcWork, sizeof(RECT));
    MoveWindow(hMainWnd, desktopRect.left, desktopRect.top, 1, 1, TRUE);

    FixDeskOrigin();

    memset(&labelSettings.font, 0, sizeof(labelSettings.font));

    labelSettings.font.lfCharSet        = DEFAULT_CHARSET;
    labelSettings.font.lfHeight         = -37;
    labelSettings.font.lfPitchAndFamily = DEFAULT_PITCH;
    labelSettings.font.lfWeight         = FW_BLACK;
    labelSettings.font.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    labelSettings.font.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    labelSettings.font.lfQuality        = ANTIALIASED_QUALITY;

    wcscpy_s(labelSettings.font.lfFaceName, LF_FACESIZE, TEXT("Arial Black"));
}

LRESULT CALLBACK WindowFunc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    switch(message) 
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

ATOM MyRegisterClassEx(HINSTANCE hInst, LPCWSTR className, WNDPROC wndProc) 
{
    WNDCLASSEX wcl;
    wcl.cbSize = sizeof(WNDCLASSEX);
    wcl.hInstance = hInst;
    wcl.lpszClassName = className;
    wcl.lpfnWndProc = wndProc;
    wcl.style = CS_DBLCLKS;
    wcl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcl.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
    wcl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcl.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcl.lpszMenuName = NULL;
    wcl.cbWndExtra = 0;
    wcl.cbClsExtra = 0;

    return RegisterClassEx(&wcl);
}

int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode)
{
    MSG msg;

    hInstance = hThisInst;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LINK_CLASS|ICC_LISTVIEW_CLASSES|ICC_PAGESCROLLER_CLASS
        |ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES|ICC_TAB_CLASSES|ICC_TREEVIEW_CLASSES
        |ICC_UPDOWN_CLASS|ICC_USEREX_CLASSES|ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    if(!MyRegisterClassEx(hThisInst, szWinName, WindowFunc)) 
    {
        MessageBox(NULL, L"Could not register window class", L"Error", MB_OK);
        return 0;
    }

    hMainWnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            szWinName,
            szWinName,
            WS_POPUP,
            0, 0,            //X and Y position of window
            1, 1,            //Width and height of window
            NULL,
            NULL,
            hThisInst,
            NULL
            );

    if( !hMainWnd)    
    {
        MessageBox(NULL, L"Could not create window", L"Error", MB_OK);
        return 0;
    }

    LPWSTR *szArgList;
    int argCount;
    szArgList = CommandLineToArgvW(GetCommandLine(), &argCount);

    LoadSettings();

    UpdateCanvasSize(deskOrigin);

    UpdateWindow(hMainWnd);

    CreateCanvas();

    PrepareLabels();

    ShowWindow(hMainWnd, SW_SHOW);

    showTimer.OnTimedEvent = fadeUpdate;
    showTimer.Start(SHOWTIMER_INTERVAL);

    if (argCount > 1)
    {
        appTimer.OnTimedEvent = close;
        appTimer.Start(2000, false, true);
        showText(szArgList[1]);
    }
    else
    {
        close();
    }

    while( GetMessage(&msg, NULL, 0, 0) )    
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete gCanvas;
    delete fontPlus;

    GdiplusShutdown(gdiplusToken);
    return msg.wParam;
}
