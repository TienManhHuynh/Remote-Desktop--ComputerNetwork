// Simple Windows utility: screenshot desktop and capture webcam, kèm HTTP server
// để khớp frontend (ping, /control, /camera, serve file).
//
// Build (example with MinGW + vcpkg for OpenCV):
//   g++ agent.cpp -o agent -DUSE_OPENCV -lgdiplus -lole32 -lShlwapi -lopencv_core -lopencv_videoio -lopencv_imgcodecs
// If OpenCV is not present, build without -DUSE_OPENCV: webcam sẽ bị vô hiệu hóa
//
// Yêu cầu: đặt file đơn header "httplib.h" (cpp-httplib) cạnh agent.cpp
//   https://github.com/yhirose/cpp-httplib/releases (copy httplib.h)

#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

#include "httplib.h" // single-header HTTP server/client

#ifdef _WIN32
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")
#endif

#ifdef USE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

using namespace Gdiplus;
namespace fs = std::filesystem;

namespace {

ULONG_PTR gdiToken;
fs::path baseDir = fs::current_path();

bool initGDI() {
    GdiplusStartupInput gdiplusStartupInput;
    return GdiplusStartup(&gdiToken, &gdiplusStartupInput, nullptr) == Ok;
}

void shutdownGDI() {
    GdiplusShutdown(gdiToken);
}

// ---- Screenshot (desktop) ----
bool saveScreenshot(const std::wstring &outputPath) {
    HDC hScreen = GetDC(nullptr);
    HDC hMemory = CreateCompatibleDC(hScreen);

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemory, hBitmap);
    BitBlt(hMemory, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);

    selectObject:
    SelectObject(hMemory, hOld);
    DeleteDC(hMemory);
    ReleaseDC(nullptr, hScreen);

    CLSID pngClsid;
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if(size == 0) return false;
    auto pInfo = (ImageCodecInfo*)malloc(size);
    if(!pInfo) return false;
    GetImageEncoders(num, size, pInfo);
    bool found = false;
    for(UINT j=0;j<num;++j){
        if(wcscmp(pInfo[j].MimeType, L"image/png") == 0){
            pngClsid = pInfo[j].Clsid;
            found = true;
            break;
        }
    }
    free(pInfo);
    if(!found) return false;

    Bitmap bmp(hBitmap, nullptr);
    Status st = bmp.Save(outputPath.c_str(), &pngClsid, nullptr);
    DeleteObject(hBitmap);
    return st == Ok;
}

// ---- Webcam snapshot ----
#ifdef USE_OPENCV
bool webcamSnapshot(const std::string &outputPath, int deviceIndex = 0) {
    cv::VideoCapture cap(deviceIndex, cv::CAP_DSHOW);
    if(!cap.isOpened()) return false;
    cv::Mat frame;
    if(!cap.read(frame)) return false;
    return cv::imwrite(outputPath, frame);
}

bool webcamRecord(const std::string &outputPath, int seconds, int fps = 20, int deviceIndex = 0) {
    cv::VideoCapture cap(deviceIndex, cv::CAP_DSHOW);
    if(!cap.isOpened()) return false;

    int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fourcc = cv::VideoWriter::fourcc('a','v','c','1'); // H.264 if available
    cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
    if(!writer.isOpened()) return false;

    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    cv::Mat frame;
    while(std::chrono::steady_clock::now() < end){
        if(!cap.read(frame)) break;
        writer.write(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000/fps));
    }
    return true;
}
#endif

} // namespace

std::string wstringToUtf8(const std::wstring &w){
    if(w.empty()) return {};
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &strTo[0], sizeNeeded, nullptr, nullptr);
    return strTo;
}

std::wstring utf8ToWstring(const std::string &s){
    if(s.empty()) return {};
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &wstr[0], sizeNeeded);
    return wstr;
}

fs::path makeCaptureDir(const std::string &name){
    fs::path p = baseDir / "captures" / name;
    fs::create_directories(p);
    return p;
}

std::string nowSuffix(){
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{};
    localtime_s(&tmv, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
    return buf;
}

// ---- HTTP handlers ----
std::string handleScreenshot(){
    fs::path dir = makeCaptureDir("screens");
    fs::path out = dir / ("screen_" + nowSuffix() + ".png");
    if(saveScreenshot(out.wstring())){
        return "/file/" + wstringToUtf8(out.wstring());
    }
    return "error: screenshot failed";
}

#ifdef USE_OPENCV
std::string handleWebcamSnap(int dev){
    fs::path dir = makeCaptureDir("webcam");
    fs::path out = dir / ("cam_" + nowSuffix() + ".jpg");
    if(webcamSnapshot(out.string(), dev)){
        return "/file/" + out.string();
    }
    return "error: webcam snapshot failed";
}

std::string handleWebcamRecord(int seconds, int dev){
    fs::path dir = makeCaptureDir("webcam");
    fs::path out = dir / ("record_" + nowSuffix() + ".mp4");
    if(webcamRecord(out.string(), seconds, 20, dev)){
        return "/file/" + out.string();
    }
    return "error: webcam record failed";
}
#endif

// naive parser for command JSON (only few fields)
std::string parseCommand(const std::string &body, std::string &cmd, int &seconds){
    cmd.clear(); seconds = 0;
    if(body.find("screenshot") != std::string::npos){
        cmd = "screenshot"; return {};
    }
    if(body.find("recordWebcam") != std::string::npos){
        cmd = "recordWebcam";
        auto pos = body.find("seconds");
        if(pos != std::string::npos){
            auto numPos = body.find_first_of("0123456789", pos);
            if(numPos != std::string::npos){
                seconds = std::stoi(body.substr(numPos));
            }
        }
        return {};
    }
    return "unknown";
}

std::string contentType(const fs::path &p){
    auto ext = p.extension().string();
    if(ext == ".png") return "image/png";
    if(ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if(ext == ".mp4") return "video/mp4";
    if(ext == ".webm") return "video/webm";
    return "application/octet-stream";
}

void runServer(){
    httplib::Server svr;

    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res){
        res.set_content("pong", "text/plain");
    });

    // serve files under /file/...
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

    // control endpoint
    svr.Post("/control", [](const httplib::Request& req, httplib::Response& res){
        std::string cmd; int seconds = 0;
        auto err = parseCommand(req.body, cmd, seconds);
        if(err == "unknown"){ res.status = 400; res.set_content("unknown command", "text/plain"); return; }

        if(cmd == "screenshot"){
            auto r = handleScreenshot();
            res.set_content(r, "text/plain");
        }
#ifdef USE_OPENCV
        else if(cmd == "recordWebcam"){
            if(seconds <=0) seconds = 5;
            auto r = handleWebcamRecord(seconds, 0);
            res.set_content(r, "text/plain");
        }
#else
        else if(cmd == "recordWebcam"){
            res.status = 501; res.set_content("webcam disabled (build with USE_OPENCV)", "text/plain");
        }
#endif
        else {
            res.status = 400; res.set_content("unknown command", "text/plain");
        }
    });

#ifdef USE_OPENCV
    // MJPEG stream
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
                return true; // continue
            },
            [](){});
    });
#else
    svr.Get("/camera", [](const httplib::Request&, httplib::Response& res){
        res.status = 501; res.set_content("webcam disabled (build with USE_OPENCV)", "text/plain");
    });
#endif

    std::cout << "HTTP server listening on 0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);
}

int wmain(int argc, wchar_t* argv[]){
    // determine executable directory to store captures
    wchar_t buf[MAX_PATH];
    if(GetModuleFileNameW(nullptr, buf, MAX_PATH)){
        baseDir = fs::path(buf).parent_path();
    }

    if(!initGDI()){
        std::wcerr << L"Failed to init GDI+\n";
        return 1;
    }

    if(argc >= 2){
        std::wstring cmd = argv[1];
        if(cmd == L"screenshot" || cmd == L"webcam-snap" || cmd == L"webcam-record"){
            // CLI mode (old behavior)
            int exitCode = 0;
            if(cmd == L"screenshot"){
                if(argc < 3){
                    std::wcerr << L"Missing output path\n";
                    exitCode = 1;
                } else {
                    std::wstring out = argv[2];
                    if(!PathMatchSpecW(out.c_str(), L"*.png")){
                        std::wcerr << L"Output should be .png\n";
                        exitCode = 1;
                    } else if(saveScreenshot(out)){
                        std::wcout << L"Saved screenshot to " << out << L"\n";
                    } else {
                        std::wcerr << L"Failed to capture screenshot\n";
                        exitCode = 1;
                    }
                }
            }
#ifdef USE_OPENCV
            else if(cmd == L"webcam-snap"){
                if(argc < 3){
                    std::wcerr << L"Missing output path\n";
                    exitCode = 1;
                } else {
                    int dev = (argc >= 4) ? _wtoi(argv[3]) : 0;
                    std::string out = std::string(argv[2], argv[2] + wcslen(argv[2]));
                    if(webcamSnapshot(out, dev)){
                        std::wcout << L"Saved webcam snapshot to " << argv[2] << L"\n";
                    } else {
                        std::wcerr << L"Failed to capture webcam snapshot\n";
                        exitCode = 1;
                    }
                }
            }
            else if(cmd == L"webcam-record"){
                if(argc < 4){
                    std::wcerr << L"Usage: webcam-record <seconds> <output.mp4> [deviceIndex]\n";
                    exitCode = 1;
                } else {
                    int seconds = _wtoi(argv[2]);
                    if(seconds <= 0){ std::wcerr << L"Seconds must be > 0\n"; exitCode = 1; }
                    else {
                        int dev = (argc >= 5) ? _wtoi(argv[4]) : 0;
                        std::string out = std::string(argv[3], argv[3] + wcslen(argv[3]));
                        if(webcamRecord(out, seconds, 20, dev)){
                            std::wcout << L"Recorded webcam to " << argv[3] << L"\n";
                        } else {
                            std::wcerr << L"Failed to record webcam\n";
                            exitCode = 1;
                        }
                    }
                }
            }
#endif
            shutdownGDI();
            return exitCode;
        }
    }

    // server mode (default)
    runServer();
    shutdownGDI();
    return 0;
}

