#pragma once
#include <winsock2.h>
#include <string>
#include <atomic>
//Stop Webcam
void stopWecam();
// Bật record trong n giây
void StartRecord(int seconds);
// Hàm Livestream trực tiếp (MJPEG over HTTP)
void StreamWebcam(SOCKET clientSocket);