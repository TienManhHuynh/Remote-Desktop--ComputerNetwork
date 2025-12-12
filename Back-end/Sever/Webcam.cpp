#include "Webcam.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

// --- THƯ VIỆN OPENCV ---
// Yêu cầu: Đã cài gói NuGet "uny.OpenCV" hoặc tương đương
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

// Link thư viện Winsock (để dùng hàm send)
#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

// --- HÀM PHỤ TRỢ: LẤY THỜI GIAN HIỆN TẠI (ĐỂ ĐẶT TÊN FILE) ---
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
// 1. CHỨC NĂNG QUAY VIDEO (RECORD)
// =============================================================
string RecordWebcam(int seconds) {
    if (seconds <= 0) seconds = 5;

    // 1. Tạo thư mục lưu trữ nếu chưa có
    string dirName = "captures";
    if (!fs::exists(dirName)) {
        fs::create_directory(dirName);
    }

    // 2. Tạo tên file duy nhất (VD: captures/video_20231025_120000.mp4)
    string filename = dirName + "/video_" + GetCurrentTimeStr() + ".mp4";

    // 3. Mở Camera (Index 0 = Webcam mặc định)
    // Thêm cv::CAP_DSHOW để khởi động camera nhanh hơn trên Windows
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        return "Loi: Khong tim thay hoac khong the mo Camera.";
    }

    // 4. Cấu hình thông số video
    int width = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = 20.0;

    // Chọn Codec: 'mp4v' thường tương thích tốt, hoặc 'MJPG'
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');

    cv::VideoWriter writer(filename, fourcc, fps, cv::Size(width, height));
    if (!writer.isOpened()) {
        return "Loi: Khong the khoi tao bo ghi Video (Kiem tra Codec).";
    }

    // 5. Vòng lặp quay video
    auto endTime = chrono::steady_clock::now() + chrono::seconds(seconds);
    cv::Mat frame;

    while (chrono::steady_clock::now() < endTime) {
        if (!cap.read(frame)) break; // Đọc khung hình lỗi thì dừng

        writer.write(frame); // Ghi vào file

        // Sleep nhẹ để khớp với FPS (1000ms / 20fps = 50ms)
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    // 6. Giải phóng tài nguyên
    cap.release();
    writer.release();

    // Trả về tên file để Server gửi link cho Client tải về
    return filename;
}

// =============================================================
// 2. CHỨC NĂNG LIVESTREAM (MJPEG STREAMING)
// =============================================================
void StreamWebcam(SOCKET clientSocket) {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) return;

    // 1. Gửi Header HTTP đặc biệt cho luồng MJPEG
    // Báo cho trình duyệt biết đây là luồng ảnh liên tục (multipart/x-mixed-replace)
    string header = "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    send(clientSocket, header.c_str(), (int)header.length(), 0);

    cv::Mat frame;
    vector<uchar> buf; // Bộ nhớ đệm chứa ảnh JPG nén
    vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 50 }; // Nén chất lượng 50% cho mượt

    // 2. Vòng lặp vô tận (cho đến khi Client ngắt kết nối)
    while (true) {
        if (!cap.read(frame)) break;

        // Nén khung hình hiện tại thành định dạng .JPG
        if (!cv::imencode(".jpg", frame, buf, params)) break;

        // Tạo Header cho từng khung hình (Frame Header)
        string boundary = "--frame\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: " + to_string(buf.size()) + "\r\n"
            "\r\n";

        // Gửi Boundary
        if (send(clientSocket, boundary.c_str(), (int)boundary.length(), 0) == SOCKET_ERROR) break;

        // Gửi Dữ liệu ảnh
        if (send(clientSocket, (char*)buf.data(), (int)buf.size(), 0) == SOCKET_ERROR) break;

        // Gửi dòng ngắt kết thúc frame
        if (send(clientSocket, "\r\n", 2, 0) == SOCKET_ERROR) break;

        // Giới hạn tốc độ gửi (~20 FPS) để không làm nghẽn mạng
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    cap.release();
}