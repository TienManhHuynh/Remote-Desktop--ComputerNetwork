#include "Webcam.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>

// THƯ VIỆN OPENCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

// BIẾN TOÀN CỤC
static std::atomic<bool> g_isStreaming(false);
static cv::Mat g_currentFrame;
static std::mutex g_frameMutex;

string GetCurrentTimeStr() {
    auto now = chrono::system_clock::now();
    auto t = chrono::system_clock::to_time_t(now);
    tm tmv{};
    localtime_s(&tmv, &t);
    stringstream ss;
    ss << put_time(&tmv, "%Y%m%d_%H%M%S");
    return ss.str();
}

// =============================================================
// HÀM RECORD WEBCAM (PHIÊN BẢN FIX LỖI SIZE + CODEC)
// =============================================================
string RecordWebcam(int seconds) {
    if (seconds <= 0) seconds = 5;

    // 1. Tạo thư mục
    if (!fs::exists("captures")) fs::create_directory("captures");
    string fileName = "captures/video_" + GetCurrentTimeStr() + ".avi";

    // 2. LẤY MẪU KHUNG HÌNH TRƯỚC (QUAN TRỌNG ĐỂ FIX LỖI CRASH)
    // Ta phải biết chính xác Camera đang chạy độ phân giải nào
    cv::Mat sampleFrame;
    cv::VideoCapture tempCap;
    bool usingTempCap = false;

    // Nếu đang Stream, lấy mẫu từ luồng Stream
    if (g_isStreaming) {
        int retries = 0;
        while (retries < 20) { // Thử 20 lần (1 giây)
            {
                lock_guard<mutex> lock(g_frameMutex);
                if (!g_currentFrame.empty()) {
                    sampleFrame = g_currentFrame.clone();
                    break;
                }
            }
            this_thread::sleep_for(chrono::milliseconds(50));
            retries++;
        }
    }
    // Nếu không Stream, tự mở Camera lấy mẫu
    else {
        tempCap.open(0, cv::CAP_DSHOW);
        if (tempCap.isOpened()) {
            tempCap.read(sampleFrame);
            usingTempCap = true;
        }
    }

    if (sampleFrame.empty()) return "Loi: Khong the lay mau Camera de khoi tao Video.";

    // 3. Khởi tạo VideoWriter với ĐÚNG KÍCH THƯỚC của Camera
    cv::VideoWriter writer;
    int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G'); // Codec AVI an toàn nhất
    double fps = 20.0;
    cv::Size frameSize = sampleFrame.size(); // <--- FIX LỖI Ở ĐÂY (Không dùng 640x480 cứng nữa)

    cout << "[INFO] Camera Size: " << frameSize.width << "x" << frameSize.height << endl;

    // Thử mở bằng FFMPEG trước
    writer.open(fileName, cv::CAP_FFMPEG, fourcc, fps, frameSize);

    // Nếu lỗi, thử fallback về mặc định
    if (!writer.isOpened()) {
        cout << "[WARN] FFMPEG failed, trying default..." << endl;
        writer.open(fileName, fourcc, fps, frameSize);
    }

    if (!writer.isOpened()) {
        if (usingTempCap) tempCap.release();
        return "Loi: VideoWriter crash (Kiem tra lai Debug/Release mode).";
    }

    cout << "[RECORD] Dang ghi file: " << fileName << endl;

    // 4. Bắt đầu vòng lặp ghi
    auto startTime = chrono::steady_clock::now();
    while (chrono::steady_clock::now() - startTime <= chrono::seconds(seconds)) {
        cv::Mat frame;
        bool hasFrame = false;

        if (g_isStreaming) {
            lock_guard<mutex> lock(g_frameMutex);
            if (!g_currentFrame.empty()) {
                frame = g_currentFrame.clone();
                hasFrame = true;
            }
        }
        else {
            if (usingTempCap && tempCap.isOpened()) {
                if (tempCap.read(frame)) hasFrame = true;
            }
            else {
                // Trường hợp mất kết nối camera giữa chừng, thử mở lại
                if (tempCap.open(0, cv::CAP_DSHOW)) tempCap.read(frame);
            }
        }

        if (hasFrame && !frame.empty()) {
            // Resize về đúng kích thước ban đầu (để tránh lỗi nếu camera đổi độ phân giải)
            if (frame.size() != frameSize) {
                cv::resize(frame, frame, frameSize);
            }
            writer.write(frame);
        }
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    writer.release();
    if (usingTempCap) tempCap.release();

    cout << "[RECORD] Thanh cong: " << fileName << endl;
    return fileName;
}

// HÀM STREAM GIỮ NGUYÊN
void StreamWebcam(SOCKET clientSocket) {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) return;

    g_isStreaming = true;
    string header = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    send(clientSocket, header.c_str(), (int)header.length(), 0);

    cv::Mat frame;
    vector<uchar> buf;
    vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 50 };

    while (cap.read(frame)) {
        {
            lock_guard<mutex> lock(g_frameMutex);
            g_currentFrame = frame.clone();
        }
        if (!cv::imencode(".jpg", frame, buf, params)) break;
        string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(buf.size()) + "\r\n\r\n";
        if (send(clientSocket, boundary.c_str(), (int)boundary.size(), 0) <= 0) break;
        if (send(clientSocket, (char*)buf.data(), (int)buf.size(), 0) <= 0) break;
        if (send(clientSocket, "\r\n", 2, 0) <= 0) break;
        this_thread::sleep_for(chrono::milliseconds(50));
    }
    g_isStreaming = false;
    cap.release();
}

void StopWebcam() {}