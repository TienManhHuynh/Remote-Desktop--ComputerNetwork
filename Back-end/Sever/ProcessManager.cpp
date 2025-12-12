#include "ProcessManager.h"
#include <tlhelp32.h>
#include <sstream>
#include <shellapi.h> 
#include <map>       
#include <algorithm> 

#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace std;

string ToLowerCase(string str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

string GetProcessList() {
    string listResult = "";
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "Loi: Khong the chup snapshot.";

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "Loi: Khong lay duoc process dau tien.";
    }

    do {
        stringstream ss;
        ss << "[" << pe32.th32ProcessID << "] " << pe32.szExeFile << "\n";
        listResult += ss.str();

    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return listResult;
}

// KILL PROCESS
string KillProcessByID(int pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) return "Loi: Khong tim thay PID hoac thieu quyen Admin.";

    if (TerminateProcess(hProcess, 1)) {
        CloseHandle(hProcess);
        return "Da diet thanh cong PID: " + to_string(pid);
    }

    CloseHandle(hProcess);
    return "Loi: Khong the diet process (Access Denied).";
}

// START APP 
string StartApp(string appName) {
    map<string, string> aliasMap;

    aliasMap["word"] = "winword";
    aliasMap["excel"] = "excel";
    aliasMap["ppt"] = "powerpnt";
    aliasMap["powerpoint"] = "powerpnt";
    aliasMap["note"] = "notepad";
    aliasMap["paint"] = "mspaint";
    aliasMap["cmd"] = "cmd";
    aliasMap["maytinh"] = "calc";
    aliasMap["calc"] = "calc";

    aliasMap["chrome"] = "chrome";
    aliasMap["web"] = "https://google.com";
    aliasMap["youtube"] = "https://youtube.com";
    aliasMap["facebook"] = "https://facebook.com";
    aliasMap["music"] = "https://soundcloud.com";

    string inputLower = ToLowerCase(appName); 
    string finalCommand = appName;           

    if (aliasMap.find(inputLower) != aliasMap.end()) {
        finalCommand = aliasMap[inputLower];
    }

    HINSTANCE result = ShellExecuteA(NULL, "open", finalCommand.c_str(), NULL, NULL, SW_SHOW);

    if ((intptr_t)result > 32) {
        return "Thanh cong: Da mo [" + finalCommand + "]";
    }
    return "Loi: Khong tim thay chuong trinh [" + appName + "].";
}