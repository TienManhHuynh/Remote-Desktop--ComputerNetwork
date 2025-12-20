# Đồ án Mạng Máy Tính: Remote Desktop System

* **GVHD:** Thầy Đỗ Hoàng Cường
* **Lớp:** 24TNT1

**Sinh viên thực hiện:**
1.  Thái Hữu Phúc (MSSV: 24122049)
2.  Nguyễn Việt Khoa (MSSV: 24122037)
3.  Huỳnh Tiến Mạnh (MSSV: 24122042)
---

## 1. Giới thiệu
Web Application cho phép điều khiển và giám sát máy tính từ xa thông qua trình duyệt Web (mô hình Client-Server).

 Sử dụng **C++ và Winsock2** để tự xử lý kết nối TCP/IP, ứng dụng Socket và cách xử lý đa luồng (Multi-threading) để truyền tải video.

**Công nghệ sử dụng:**
* **Backend:** C++ (Winsock2, OpenCV để xử lý Camera).
* **Frontend:** HTML5/ Javascript/ CSS3 (Sử dụng Fetch API để gửi lệnh).

---

## 2. Hướng dẫn Cài đặt & Chạy Demo. 
Nhóm đã tạo sẵn phiên bản **Relase** ở thanh giao diện bên phải của repo , người dùng **không cần cài Visual Studio và các thư viện đi kèm**, chỉ cần tải về và chạy thử demo. Hai máy chạy thử phải kết nối vào chung một mạng để app hoạt động được.

### Bước 1: Tải và Giải nén.
1. Vào mục **Releases** bên phải Github, tải file `.zip` mới nhất.
2. Giải nén ra một thư mục.

### Bước 2: Kiểm tra các file và chạy Demo.
**Back-end: chỉ cần chạy duy nhất file "Remote_desktop.exe" trên máy làm Sever, không cần chạy những file khác.**

**Front-end: chạy file "index.html" trong folder release trên máy làm Client.**

Trước khi chạy, hãy đảm bảo trong thư mục có đủ các file sau (nếu thiếu chương trình sẽ báo lỗi hoặc không quay được video):

```text
/RemoteControl_Backend_v1.1/
├── Remote_desktop.exe              # File chạy chính
├── opencv_world4120.dll            # Thư viện ảnh
├── ffmpeg.exe                      # Thư viện quay video
└── Các file .dll hệ thống khác (msvcp140, vcruntime140...)
