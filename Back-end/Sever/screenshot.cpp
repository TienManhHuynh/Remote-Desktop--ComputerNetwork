// screenshot_server.cpp (Windows version)
#include <cstdint>

static uint64_t to_big_endian(uint64_t x) {
    return ((x & 0x00000000000000FFull) << 56) |
           ((x & 0x000000000000FF00ull) << 40) |
           ((x & 0x0000000000FF0000ull) << 24) |
           ((x & 0x00000000FF000000ull) << 8)  |
           ((x & 0x000000FF00000000ull) >> 8)  |
           ((x & 0x0000FF0000000000ull) >> 24) |
           ((x & 0x00FF000000000000ull) >> 40) |
           ((x & 0xFF00000000000000ull) >> 56);
}

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Gdiplus.lib")

using namespace Gdiplus;
using namespace std;

ULONG_PTR gdiplusToken;

bool capture_screen_png(vector<BYTE>& out_png) {
    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);

    BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);

    //
    // Encode PNG
    //
    IStream* stream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);

    Bitmap bmp(hBitmap, NULL);

    CLSID clsidPng;
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    vector<BYTE> buffer(size);
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)buffer.data();
    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT i = 0; i < num; i++) {
        if (wcscmp(pImageCodecInfo[i].MimeType, L"image/png") == 0) {
            clsidPng = pImageCodecInfo[i].Clsid;
            break;
        }
    }

    if (bmp.Save(stream, &clsidPng, NULL) != Ok) return false;

    // Copy stream to vector
    STATSTG stats;
    stream->Stat(&stats, STATFLAG_NONAME);

    ULONG pngSize = stats.cbSize.LowPart;
    out_png.resize(pngSize);

    LARGE_INTEGER li = {};
    stream->Seek(li, STREAM_SEEK_SET, NULL);

    ULONG bytesRead;
    stream->Read(out_png.data(), pngSize, &bytesRead);

    stream->Release();
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    return true;
}

int main() {
    // GDI+
    GdiplusStartupInput gdiInput;
    GdiplusStartup(&gdiplusToken, &gdiInput, NULL);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4444);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, 5);

    cout << "Windows screenshot_server running on port 4444\n";

    while (true) {
        SOCKET client = accept(server, NULL, NULL);

        char buffer[128] = {};
        recv(client, buffer, sizeof(buffer), 0);

        if (strncmp(buffer, "SCREENSHOT", 10) == 0) {
            vector<BYTE> png;
            if (!capture_screen_png(png)) {
                string msg = "ERR capture failed\n";
                send(client, msg.c_str(), msg.size(), 0);
            } else {
                uint64_t L = png.size();
                uint64_t be = to_big_endian(L);

                send(client, (char*)&be, 8, 0);
                send(client, (char*)png.data(), png.size(), 0);
            }
        }

        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    GdiplusShutdown(gdiplusToken);
    return 0;
}
