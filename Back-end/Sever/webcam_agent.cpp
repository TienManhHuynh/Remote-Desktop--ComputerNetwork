// Webcam-only HTTP agent for Windows (requires OpenCV).
// Endpoints:
//   GET  /ping         -> "pong"
//   GET  /camera       -> MJPEG stream
//   POST /control      -> {"command":"recordWebcam","seconds":N} returns /file/<path>
//   GET  /file/<path>  -> serve captured file
//
// Build (MinGW + vcpkg OpenCV):
//   g++ webcam_agent.cpp -o webcam_agent.exe -DUSE_OPENCV `
//     -I"C:\vcpkg\installed\x64-mingw-static\include" `
//     -L"C:\vcpkg\installed\x64-mingw-static\lib" `
//     -lgdiplus -lole32 -lShlwapi -lWs2_32 `
//     -lopencv_core -lopencv_videoio -lopencv_imgcodecs
//
// Requirements: place httplib.h next to this file. Needs Windows 10+.

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
#include <thread>
#include <filesystem>
#include <fstream>
#include <vector>

#include "httplib.h"

#ifdef _WIN32
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

namespace fs = std::filesystem;

namespace {

fs::path baseDir = fs::current_path();

std::string utf8(const std::wstring &w){
    if(w.empty()) return {};
    int need = WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);
    std::string s(need,0);
    WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),s.data(),need,nullptr,nullptr);
    return s;
}
std::wstring wstr(const std::string &s){
    if(s.empty()) return {};
    int need = MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0);
    std::wstring w(need,0);
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),w.data(),need);
    return w;
}

fs::path makeCaptureDir(const std::string &name){
    fs::path p = baseDir / "captures" / name;
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
    if(ext==".jpg" || ext==".jpeg") return "image/jpeg";
    if(ext==".mp4") return "video/mp4";
    return "application/octet-stream";
}

std::string parseCommand(const std::string &body, int &seconds){
    seconds = 0;
    if(body.find("recordWebcam") != std::string::npos){
        auto pos = body.find("seconds");
        if(pos != std::string::npos){
            auto numPos = body.find_first_of("0123456789", pos);
            if(numPos != std::string::npos){
                seconds = std::stoi(body.substr(numPos));
            }
        }
        return "recordWebcam";
    }
    return "unknown";
}

std::string handleRecord(int seconds, int dev=0){
    if(seconds<=0) seconds = 5;
    fs::path dir = makeCaptureDir("webcam");
    fs::path out = dir / ("record_" + nowSuffix() + ".mp4");

    cv::VideoCapture cap(dev, cv::CAP_DSHOW);
    if(!cap.isOpened()) return "error: cannot open camera";

    int w = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int h = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    int fps = 20;
    int fourcc = cv::VideoWriter::fourcc('a','v','c','1');
    cv::VideoWriter writer(out.string(), fourcc, fps, cv::Size(w,h));
    if(!writer.isOpened()) return "error: cannot open writer";

    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    cv::Mat frame;
    while(std::chrono::steady_clock::now() < end){
        if(!cap.read(frame)) break;
        writer.write(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000/fps));
    }
    return "/file/" + out.string();
}

void runServer(){
    httplib::Server svr;

    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res){
        res.set_content("pong", "text/plain");
    });

    svr.Get(R"(/file/(.+))", [](const httplib::Request& req, httplib::Response& res){
        fs::path target = wstr(req.matches[1]);
        fs::path full = target.is_absolute() ? target : baseDir / target;
        if(!fs::exists(full) || !fs::is_regular_file(full)){
            res.status = 404; res.set_content("not found", "text/plain"); return;
        }
        std::ifstream f(full, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        res.set_content(std::move(data), contentType(full).c_str());
    });

    svr.Post("/control", [](const httplib::Request& req, httplib::Response& res){
        int seconds = 0;
        auto cmd = parseCommand(req.body, seconds);
        if(cmd == "recordWebcam"){
            auto r = handleRecord(seconds, 0);
            res.set_content(r, "text/plain");
        } else {
            res.status = 400; res.set_content("unknown command", "text/plain");
        }
    });

    // MJPEG live stream
    svr.Get("/camera", [](const httplib::Request&, httplib::Response& res){
        cv::VideoCapture cap(0, cv::CAP_DSHOW);
        if(!cap.isOpened()){
            res.status = 500; res.set_content("cannot open camera", "text/plain"); return;
        }
        res.set_header("Connection", "close");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame");
        res.set_chunked_content_provider("multipart/x-mixed-replace; boundary=frame",
            [&cap](size_t, httplib::DataSink &sink) {
                cv::Mat frame;
                if(!cap.read(frame)) return false;
                std::vector<uchar> buf;
                if(!cv::imencode(".jpg", frame, buf)) return false;
                std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(buf.size()) + "\r\n\r\n";
                sink.write(header.data(), header.size());
                sink.write(reinterpret_cast<const char*>(buf.data()), buf.size());
                sink.write("\r\n", 2);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return true;
            },
            [](){});
    });

    std::cout << "Webcam agent on 0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);
}

} // namespace

int wmain(){
    wchar_t buf[MAX_PATH];
    if(GetModuleFileNameW(nullptr, buf, MAX_PATH)) baseDir = fs::path(buf).parent_path();
    runServer();
    return 0;
}

