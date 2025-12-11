#pragma once
#include <string>
#include <windows.h>

// Lấy danh sách toàn bộ tiến trình chạy ngầm và nổi
std::string GetProcessList();

// Diệt tiến trình theo ID (PID)
std::string KillProcessByID(int pid);

// Khởi động ứng dụng (VD: notepad, calc, chrome)
std::string StartApp(std::string appName);