#pragma once
#include <string>
#include <windows.h>

std::string GetProcessList();

std::string KillProcessByID(int pid);

std::string StartApp(std::string appName);