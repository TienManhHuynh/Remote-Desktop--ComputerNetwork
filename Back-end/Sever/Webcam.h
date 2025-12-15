#pragma once

// --- BẮT BUỘC: Phải có winsock2.h để hiểu kiểu SOCKET ---
#include <winsock2.h>
#include <windows.h>
#include <string>

// Khai báo hàm quay video (trả về đường dẫn file)
std::string RecordWebcam(int seconds);

// Khai báo hàm Livestream (nhận tham số là SOCKET)
void StreamWebcam(SOCKET clientSocket);

// Khai báo hàm dừng camera
void StopWebcam();