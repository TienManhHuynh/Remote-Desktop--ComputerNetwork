#pragma once
#include <winsock2.h>
#include <string>

// Hàm quay video trong n giây, lưu file và trả về tên file
std::string RecordWebcam(int seconds);

// Hàm Livestream trực tiếp (MJPEG over HTTP)
void StreamWebcam(SOCKET clientSocket);