#include "VideoRecorder.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <vector>
#include <opencv2/opencv.hpp>

using namespace std;
namespace fs = std::filesystem;

string GetTimeStrForFile() {
    auto now = chrono::system_clock::now();
    auto t = chrono::system_clock::to_time_t(now);
    tm tmv{};
    localtime_s(&tmv, &t);
    stringstream ss;
    ss << put_time(&tmv, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string RecordVideo(int seconds) {
    cout << "\n[RECORDER] === CHE DO: AUTO SYNC FPS ===" << endl;
    if (seconds <= 0) seconds = 5;

    // 1. Tạo thư mục
    if (fs::exists("temp_frames")) fs::remove_all("temp_frames");
    fs::create_directory("temp_frames");
    if (!fs::exists("captures")) fs::create_directory("captures");

    // 2. Mở Camera
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) return "Loi: Khong mo duoc Camera.";

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);

    cout << "[RECORDER] Bat dau chup anh trong " << seconds << "s..." << endl;

    auto startTime = chrono::steady_clock::now();
    cv::Mat frame;
    int frameCount = 0;

    while (true) {
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration_cast<chrono::milliseconds>(now - startTime).count() / 1000.0;
        if (elapsed >= seconds) break;

        if (cap.read(frame) && !frame.empty()) {
            stringstream ss;
            ss << "temp_frames/img_" << setfill('0') << setw(4) << frameCount << ".jpg";
            cv::imwrite(ss.str(), frame);
            frameCount++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    cap.release();

    if (frameCount == 0) return "Loi: Khong chup duoc frame nao.";

    auto endTime = chrono::steady_clock::now();
    double totalTime = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() / 1000.0;

    double realFPS = (double)frameCount / totalTime;

    cout << "[RECORDER] Tong so Frame: " << frameCount << endl;
    cout << "[RECORDER] Thoi gian thuc: " << totalTime << "s" << endl;
    cout << "[RECORDER] FPS Tinh toan: " << realFPS << " (Video se chay dung toc do nay)" << endl;

    string outputVideo = "captures/rec_" + GetTimeStrForFile() + ".mp4";

    stringstream cmdSS;
    cmdSS << "ffmpeg.exe -framerate " << realFPS << " -i \"temp_frames/img_%04d.jpg\" -c:v libx264 -pix_fmt yuv420p -y \"" << outputVideo << "\" > NUL 2>&1";

    string cmd = cmdSS.str();
    int result = system(cmd.c_str());

    if (result != 0) return "Loi: FFmpeg error.";

    fs::remove_all("temp_frames");
    cout << "[RECORDER] Xong! Video chuan thoi gian thuc." << endl;
    return outputVideo;
}