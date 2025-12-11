// Screenshot-only HTTP agent for Windows (no OpenCV needed).
// Endpoints:
//   GET  /ping         -> "pong"
//   POST /control      -> {"command":"screenshot"} returns /file/<path>
//   GET  /file/<path>  -> serve captured file
//
// Build (MinGW):
//   g++ screenshot_agent.cpp -o screenshot_agent.exe -lgdiplus -lgdi32 -luser32 -lole32 -lShlwapi -lWs2_32
//
// Requirements: place httplib.h next to this file.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <winsock2.h>
#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <string>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "httplib.h"

#ifdef _WIN32
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

using namespace Gdiplus;
namespace fs = std::filesystem;

namespace {

ULONG_PTR gdiToken;
fs::path baseDir = fs::current_path();

bool initGDI() {
    GdiplusStartupInput in;
    return GdiplusStartup(&gdiToken, &in, nullptr) == Ok;
}
void shutdownGDI() { GdiplusShutdown(gdiToken); }

bool saveScreenshot(const std::wstring &outputPath) {
    HDC hScreen = GetDC(nullptr);
    HDC hMemory = CreateCompatibleDC(hScreen);
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemory, hBitmap);
    BitBlt(hMemory, 0, 0, w, h, hScreen, 0, 0, SRCCOPY);
    SelectObject(hMemory, hOld);
    DeleteDC(hMemory);
    ReleaseDC(nullptr, hScreen);

    CLSID pngClsid;
    UINT num=0, size=0;
    GetImageEncodersSize(&num, &size);
    if(size==0) return false;
    auto pInfo = (ImageCodecInfo*)malloc(size);
    if(!pInfo) return false;
    GetImageEncoders(num, size, pInfo);
    bool found=false;
    for(UINT j=0;j<num;++j){
        if(wcscmp(pInfo[j].MimeType, L"image/png")==0){
            pngClsid = pInfo[j].Clsid; found=true; break;
        }
    }
    free(pInfo);
    if(!found) return false;

    Bitmap bmp(hBitmap, nullptr);
    Status st = bmp.Save(outputPath.c_str(), &pngClsid, nullptr);
    DeleteObject(hBitmap);
    return st==Ok;
}

std::string wstringToUtf8(const std::wstring &w){
    if(w.empty()) return {};
    int need = WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);
    std::string s(need,0);
    WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),s.data(),need,nullptr,nullptr);
    return s;
}

std::wstring utf8ToWstring(const std::string &s){
    if(s.empty()) return {};
    int need = MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0);
    std::wstring w(need,0);
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),w.data(),need);
    return w;
}

fs::path makeCaptureDir(){
    fs::path p = baseDir / "captures" / "screens";
    fs::create_directories(p);
    return p;
}

std::string nowSuffix(){
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{}; localtime_s(&tmv, &t);
    char buf[32]; strftime(buf,sizeof(buf),"%Y%m%d_%H%M%S",&tmv);
    return buf;
}

std::string contentType(const fs::path &p){
    auto ext = p.extension().string();
    if(ext==".png") return "image/png";
    return "application/octet-stream";
}

std::string handleScreenshot(){
    fs::path dir = makeCaptureDir();
    fs::path out = dir / ("screen_" + nowSuffix() + ".png");
    if(saveScreenshot(out.wstring())){
        return "/file/" + wstringToUtf8(out.wstring());
    }
    return "error: screenshot failed";
}

std::string parseCommand(const std::string &body){
    if(body.find("screenshot") != std::string::npos) return "screenshot";
    return "unknown";
}

void runServer(){
    httplib::Server svr;

    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res){
        res.set_content("pong", "text/plain");
    });

    svr.Get(R"(/file/(.+))", [](const httplib::Request& req, httplib::Response& res){
        fs::path target = utf8ToWstring(req.matches[1]);
        fs::path full = target.is_absolute() ? target : baseDir / target;
        if(!fs::exists(full) || !fs::is_regular_file(full)){
            res.status = 404; res.set_content("not found", "text/plain"); return;
        }
        std::ifstream f(full, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        res.set_content(std::move(data), contentType(full).c_str());
    });

    svr.Post("/control", [](const httplib::Request& req, httplib::Response& res){
        auto cmd = parseCommand(req.body);
        if(cmd == "screenshot"){
            res.set_content(handleScreenshot(), "text/plain");
        } else {
            res.status = 400; res.set_content("unknown command", "text/plain");
        }
    });

    std::cout << "Screenshot agent on 0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);
}

} // namespace

int main(){
    wchar_t buf[MAX_PATH];
    if(GetModuleFileNameW(nullptr, buf, MAX_PATH)) baseDir = fs::path(buf).parent_path();
    if(!initGDI()){ std::wcerr << L"Failed to init GDI+\n"; return 1; }
    runServer();
    shutdownGDI();
    return 0;
}

