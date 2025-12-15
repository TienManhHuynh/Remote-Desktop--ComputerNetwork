#pragma once
#include <string>

// Hàm quay video độc lập
// Input: Số giây muốn quay
// Output: Đường dẫn file video đã lưu (.avi)
std::string RecordVideo(int seconds);