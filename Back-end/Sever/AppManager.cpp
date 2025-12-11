#include "AppManager.h"
#include <windows.h>
#include <sstream>

#pragma comment(lib, "User32.lib") // Quan trọng cho EnumWindows

using namespace std;

string globalAppList = "";

// Hàm callback (xử lý từng cửa sổ tìm được)
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsWindowVisible(hwnd)) {
        int length = GetWindowTextLengthA(hwnd); // Dùng bản A (ANSI)
        if (length > 0) {
            char* buffer = new char[length + 1];
            GetWindowTextA(hwnd, buffer, length + 1); // Dùng bản A (ANSI)

            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);

            string title = buffer;
            // Lọc bớt rác hệ thống
            if (title != "Program Manager" && title != "Default IME" && title != "MSCTFIME UI") {
                stringstream ss;
                ss << "PID: " << pid << " | App: " << title << "\n";
                globalAppList += ss.str();
            }
            delete[] buffer;
        }
    }
    return TRUE;
}

string GetAppList() {
    globalAppList = "";
    EnumWindows(EnumWindowsProc, 0);
    if (globalAppList == "") return "Khong co App nao dang mo.";
    return globalAppList;
}