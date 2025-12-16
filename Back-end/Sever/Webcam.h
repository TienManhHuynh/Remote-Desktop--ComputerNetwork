#pragma once

#include <winsock2.h>
#include <windows.h>
#include <string>

std::string RecordWebcam(int seconds);

void StreamWebcam(SOCKET clientSocket);

void StopWebcam();