#include "Webcam.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
using namespace std;

// --- THƯ VIỆN OPENCV ---
// Yêu cầu: Đã cài gói NuGet "uny.OpenCV" hoặc tương đương
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <atomic>
#include <opencv2/opencv.hpp>

// Trạng thái webcam
static std::atomic<bool> g_webcamRunning(false);

// Trạng thái record
static std::atomic<bool> g_isRecording(false);
static cv::VideoWriter g_writer;
static std::chrono::steady_clock::time_point g_recordEndTime;

// Link thư viện Winsock (để dùng hàm send)
#pragma comment(lib, "ws2_32.lib")

namespace fs = std::filesystem;
static std::atomic<bool> g_isRecording(false);
static std::atomic<int> g_recordSeconds(0);
static cv::VideoWriter g_writer;
static std::chrono::steady_clock::time_point g_recordEndTime;
static std::string g_lastRecordedFile = "";

// --- HÀM PHỤ TRỢ: LẤY THỜI GIAN HIỆN TẠI (ĐỂ ĐẶT TÊN FILE) ---
string GetCurrentTimeStr()
{
    auto now = chrono::system_clock::now();
    auto t = chrono::system_clock::to_time_t(now);
    tm tmv{};
    localtime_s(&tmv, &t);

    stringstream ss;
    ss << put_time(&tmv, "%Y%m%d_%H%M%S");
    return ss.str();
}
// 1. Hàm recordWebcam
void StartRecord(int seconds)
{
    if (seconds <= 0)
        seconds = 5;
    if (g_isRecording)
        return;

    if (!std::filesystem::exists("captures"))
        std::filesystem::create_directory("captures");

    g_lastRecordedFile =
        "captures/video_" + GetCurrentTimeStr() + ".mp4";

    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    double fps = 20.0;

    g_writer.open(g_lastRecordedFile, fourcc, fps, cv::Size(640, 480));
    if (!g_writer.isOpened())
        return;

    g_recordEndTime =
        std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    g_isRecording = true;
}
#include <fstream>

void SendVideoFile(SOCKET clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::string err =
            "HTTP/1.1 404 Not Found\r\n\r\nFile not found";
        send(clientSocket, err.c_str(), err.size(), 0);
        return;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: video/mp4\r\n"
        "Content-Length: " + std::to_string(fileSize) + "\r\n"
        "Content-Disposition: attachment; filename=\"video.mp4\"\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";

    send(clientSocket, header.c_str(), header.size(), 0);

    char buffer[8192];
    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        send(clientSocket, buffer, file.gcount(), 0);
    }

    file.close();
}
void SendLastRecordName(SOCKET clientSocket) {
    std::string body =
        "{ \"file\": \"" + g_lastRecordedFile + "\" }";

    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";

    send(clientSocket, header.c_str(), header.size(), 0);
    send(clientSocket, body.c_str(), body.size(), 0);
}


// =============================================================
// 2. CHỨC NĂNG LIVESTREAM (MJPEG STREAMING)
// =============================================================
void StreamWebcam(SOCKET clientSocket)
{
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened())
        return;

    g_webcamRunning = true;

    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";

    send(clientSocket, header.c_str(), header.size(), 0);

    cv::Mat frame;
    std::vector<uchar> buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 50};

    while (g_webcamRunning)
    {
        if (!cap.read(frame))
            break;

        // ===== RECORD MP4 =====
        if (g_isRecording)
        {
            cv::Mat resized;
            cv::resize(frame, resized, cv::Size(640, 480));
            g_writer.write(resized);

            if (std::chrono::steady_clock::now() >= g_recordEndTime)
            {
                g_isRecording = false;
                g_writer.release();
                std::cout << "Record xong\n";
            }
        }

        // ===== STREAM MJPEG =====
        if (!cv::imencode(".jpg", frame, buf, params))
            break;

        std::string boundary =
            "--frame\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: " +
            std::to_string(buf.size()) + "\r\n\r\n";

        if (send(clientSocket, boundary.c_str(), boundary.size(), 0) <= 0)
            break;
        if (send(clientSocket, (char *)buf.data(), buf.size(), 0) <= 0)
            break;
        if (send(clientSocket, "\r\n", 2, 0) <= 0)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ===== CLEAN UP =====
    cap.release();

    if (std::chrono::steady_clock::now() >= g_recordEndTime)
    {
        g_isRecording = false;
        g_writer.release();

        std::cout << "Record xong: " << g_lastRecordedFile << std::endl;
    }

    std::cout << "Webcam da tat\n";
}
void StopWebcam()
{
    g_webcamRunning = false;
    std::cout << "Yeu cau tat webcam\n";
}
