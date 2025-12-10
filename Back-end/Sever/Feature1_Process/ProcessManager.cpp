#include "ProcessManager.h"
#include <tlhelp32.h>
#include <sstream>
#include <shellapi.h> // Cần cho ShellExecute
#include <map>        // Cần cho từ điển Alias
#include <algorithm>  // Cần cho hàm transform (chữ thường)

// Tự động link thư viện
#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace std;

// --- HÀM PHỤ TRỢ: CHUYỂN CHỮ THƯỜNG ---
string ToLowerCase(string str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// --- 1. LẤY DANH SÁCH PROCESS ---
string GetProcessList() {
    string listResult = "";
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    // Chụp ảnh danh sách process
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "Loi: Khong the chup snapshot.";

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "Loi: Khong lay duoc process dau tien.";
    }

    do {
        // Gom thông tin: [PID] Tên_File
        stringstream ss;
        ss << "[" << pe32.th32ProcessID << "] " << pe32.szExeFile << "\n";
        listResult += ss.str();

    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return listResult;
}

// --- 2. DIỆT PROCESS ---
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

// --- 3. START APP THÔNG MINH (CẬP NHẬT MỚI) ---
string StartApp(string appName) {
    // A. TẠO TỪ ĐIỂN "MẬT KHẨU"
    // Bạn có thể thêm bất cứ cái gì bạn thích vào đây
    map<string, string> aliasMap;

    // Văn phòng
    aliasMap["word"] = "winword";
    aliasMap["excel"] = "excel";
    aliasMap["ppt"] = "powerpnt";
    aliasMap["powerpoint"] = "powerpnt";
    aliasMap["note"] = "notepad";
    aliasMap["paint"] = "mspaint";
    aliasMap["cmd"] = "cmd";
    aliasMap["maytinh"] = "calc";
    aliasMap["calc"] = "calc";

    // Web & Mạng xã hội (Mở luôn trình duyệt)
    aliasMap["chrome"] = "chrome";
    aliasMap["web"] = "https://google.com";
    aliasMap["youtube"] = "https://youtube.com";
    aliasMap["facebook"] = "https://facebook.com";
    aliasMap["music"] = "https://soundcloud.com";

    // B. XỬ LÝ ĐẦU VÀO
    string inputLower = ToLowerCase(appName); // Chuyển về chữ thường hết
    string finalCommand = appName;            // Mặc định là giữ nguyên (nếu gõ notepad, calc...)

    // Nếu tìm thấy trong từ điển -> Thay thế bằng lệnh chuẩn
    if (aliasMap.find(inputLower) != aliasMap.end()) {
        finalCommand = aliasMap[inputLower];
    }

    // C. GỌI LỆNH CHẠY (Dùng ShellExecuteA để hỗ trợ ANSI string)
    HINSTANCE result = ShellExecuteA(NULL, "open", finalCommand.c_str(), NULL, NULL, SW_SHOW);

    if ((intptr_t)result > 32) {
        return "Thanh cong: Da mo [" + finalCommand + "]";
    }
    return "Loi: Khong tim thay chuong trinh [" + appName + "].";
}