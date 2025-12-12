// Screenshot.cpp - ĐÃ FIX KÍCH THƯỚC VÀ CHẤT LƯỢNG CHỤP
#include "Screenshot.h"
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

// Hàm hỗ trợ lấy mã encoder (PNG/JPG) - Giữ nguyên
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
    // 1. Lấy kích thước TOÀN BỘ MÀN HÌNH ẢO
    int full_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int full_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x_start = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y_start = GetSystemMetrics(SM_YVIRTUALSCREEN);

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);

    // Kích thước Bitmap đích (Có thể là Full HD 1920x1080 nếu cần ép)
    // Tuy nhiên, ta nên dùng kích thước VIRTUAL SCREEN để không bị bóp méo.
    int dest_width = full_width;
    int dest_height = full_height;

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, dest_width, dest_height);
    HGDIOBJ old_obj = SelectObject(hDC, hBitmap);

    // SỬ DỤNG STRETCHBLT ĐỂ ĐẢM BẢO COPY TOÀN BỘ VÙNG VÀ CÓ THỂ ÉP KÍCH THƯỚC
    BOOL success = StretchBlt(
        hDC,            // Destination DC
        0,              // Destination X
        0,              // Destination Y
        dest_width,     // Destination Width
        dest_height,    // Destination Height
        hScreen,        // Source DC
        x_start,        // Source X (Bắt đầu từ đâu)
        y_start,        // Source Y (Bắt đầu từ đâu)
        full_width,     // Source Width
        full_height,    // Source Height
        SRCCOPY         // Raster Operation
    );

    if (!success) {
        // Xử lý lỗi nếu StretchBlt thất bại
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