// Screenshot.cpp - ĐÃ FIX KÍCH THƯỚC VÀ CHẤT LƯỢNG CHỤP
#include "Screenshot.h"
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0; UINT  size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

string CaptureScreenshot() {
    int full_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int full_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x_start = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y_start = GetSystemMetrics(SM_YVIRTUALSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    int dest_width = full_width;
    int dest_height = full_height;

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, dest_width, dest_height);
    HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
    BOOL success = StretchBlt(
        hDC,           
        0,            
        0,              
        dest_width,    
        dest_height,    
        hScreen,       
        x_start,   
        y_start,      
        full_width,     
        full_height,    
        SRCCOPY         
    );

    if (!success) {
        SelectObject(hDC, old_obj);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        DeleteObject(hBitmap);
        return "Loi: StretchBlt failed.";
    }

    string filename = "screenshot.png";
    {
        Bitmap bitmap(hBitmap, NULL);
        CLSID clsid;
        GetEncoderClsid(L"image/png", &clsid);
        wstring wFilename(filename.begin(), filename.end());
        bitmap.Save(wFilename.c_str(), &clsid, NULL);
    }

    SelectObject(hDC, old_obj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);
    DeleteObject(hBitmap);

    return filename;
}