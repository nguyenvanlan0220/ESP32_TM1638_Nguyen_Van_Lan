# Dự Án ESP32-S3 Điều Khiển Module TM1638 & LCD I2C

**Tác giả:** Nguyễn Văn Lân  
**Vi điều khiển:** ESP32-S3 (ESP32-S3-WROOM-1 / DevKitC-1)  

---

## 📂 1. Cấu Trúc Dự Án (2 Bộ Code Trong 1 Workspace)

Dự án được chia làm 2 thư mục chương trình Arduino/PlatformIO riêng biệt, dùng chung phần mềm giao diện C# Visual Studio GUI (`TM1638_GUI`):

```
ESP32_TM1638_Nguyen_Van_Lan-main/
│
├── 📂 ESP32_TM1638/                      --> [Dự án 1] ESP32-S3 + Module TM1638 (Cơ bản)
│   ├── ESP32_TM1638.ino                 --> Sketch Arduino chính cho dự án 1
│   ├── TM1638_Driver.cpp                --> Driver C++ TM1638
│   ├── TM1638_Driver.h                  --> Header Driver TM1638
│   └── platformio.ini                   --> Cấu hình PlatformIO cho dự án 1
│
├── 📂 ESP32_TM1638_V2_I2C/               --> [Dự án 2] ESP32-S3 + TM1638 + LCD I2C (Mở rộng)
│   ├── ESP32_TM1638_V2_I2C.ino          --> Sketch Arduino chính cho dự án 2
│   ├── TM1638_Driver.cpp                --> Driver C++ TM1638 (Dùng chung driver)
│   ├── TM1638_Driver.h                  --> Header Driver TM1638
│   └── platformio.ini                   --> Cấu hình PlatformIO cho dự án 2
│
├── 📂 TM1638_GUI/                        --> [Dùng chung] Giao diện máy tính C# WinForms (Visual Studio)
│   ├── Form1.cs
│   ├── Form1.Designer.cs
│   ├── Program.cs
│   ├── TM1638_GUI.csproj
│   └── TM1638_GUI.sln
│
├── Chay_Giao_Dien.bat                    --> Script kích chạy nhanh giao diện GUI máy tính
├── TM1638_Solution.slnx                  --> Solution Visual Studio 2022+
└── README.md                             --> Hướng dẫn dự án
```

---

## 📌 2. Sơ Đồ Đấu Nối Chân (Pinout Chi Tiết)

### A. Module TM1638 (Áp dụng cho cả Dự án 1 và Dự án 2):
| Chân Module TM1638 | Chân ESP32-S3 | Chức Năng |
| :--- | :--- | :--- |
| **VCC** | **5V** (hoặc **VIN / VBUS**) | Nguồn cấp 5V |
| **GND** | **GND** | Mass chung |
| **STB** (Strobe) | **GPIO 15** | Chân chốt dữ liệu |
| **CLK** (Clock) | **GPIO 16** | Xung nhịp đồng hồ |
| **DIO** (Data) | **GPIO 17** | Đường truyền dữ liệu |

### B. Màn hình LCD I2C 1602 / 2004 (Chỉ dùng cho Dự án 2 - `ESP32_TM1638_V2_I2C`):
| Chân Module I2C | Chân ESP32-S3 | Chức Năng |
| :--- | :--- | :--- |
| **VCC** | **5V** (hoặc **VIN**) | Nguồn cấp 5V |
| **GND** | **GND** | Mass chung |
| **SDA** | **GPIO 8** | Chân dữ liệu I2C |
| **SCL** | **GPIO 9** | Chân xung nhịp I2C |

---

## 🚀 3. Hướng Dẫn Sử Dụng & Kích Chạy

1. **Nạp Firmware lên ESP32-S3:**
   - Mở thư mục `ESP32_TM1638` (nếu chỉ dùng TM1638) hoặc `ESP32_TM1638_V2_I2C` (nếu dùng cả TM1638 + LCD I2C) bằng Arduino IDE hoặc VS Code (PlatformIO) để biên dịch và nạp code.
2. **Kích chạy Giao diện Máy tính:**
   - Nhấn đúp chuột vào file `Chay_Giao_Dien.bat` ở thư mục gốc để khởi chạy phần mềm C# GUI (`TM1638_GUI`).
   - Chọn đúng cổng COM của ESP32-S3 (Baudrate `115200`) và kết nối.
