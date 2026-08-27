# Dự Án ESP32-S3 Điều Khiển Module TM1638, LCD I2C & Driver Động Cơ JMC-2HSS57

**Tác giả:** Nguyễn Văn Lân  
**Vi điều khiển:** ESP32-S3 (ESP32-S3-WROOM-1 / DevKitC-1)  
**Driver Động Cơ:** JMC 2HSS57 (Hybrid Stepper Servo Drive) + Động Cơ Bước YAKO  

---

## 📂 1. Cấu Trúc Dự Án (Toàn Bộ Thư Mục Đã Tích Hợp Trên Trang Chính GitHub)

Dự án được tổng hợp trực tiếp trên trang chính repository GitHub với 4 thư mục chương trình Arduino/PlatformIO riêng biệt và 1 giao diện C# Visual Studio GUI (`TM1638_GUI`):

```
ESP32_TM1638_Nguyen_Van_Lan/
│
├── 📂 ESP32_TM1638/                      --> [Dự án 1] ESP32-S3 + Module TM1638 (Cơ bản)
├── 📂 ESP32_TM1638_V2_I2C/               --> [Dự án 2] ESP32-S3 + TM1638 + LCD I2C (Mở rộng)
├── 📂 ESP32_TM1638_Motor_JMC/            --> [Dự án 3] ESP32-S3 + TM1638 + LCD I2C + 1 Động cơ JMC 2HSS57
├── 📂 ESP32_TM1638_DualMotor_JMC/        --> [Dự án 4] ESP32-S3 + TM1638 + LCD I2C + 2 Động cơ JMC 2HSS57 (Độc lập & Đồng bộ)
│
├── 📂 TM1638_GUI/                        --> [Dùng chung] Giao diện máy tính C# WinForms Dashboard (Visual Studio)
│   ├── Form1.cs
│   ├── Form1.Designer.cs
│   ├── Program.cs
│   ├── TM1638_GUI.csproj
│   └── TM1638_GUI.sln
│
├── Chay_Giao_Dien.bat                    --> Script kích chạy nhanh giao diện GUI máy tính
├── TM1638_Solution.slnx                  --> Solution Visual Studio 2022+
└── README.md                             --> Hướng dẫn chi tiết dự án
```

---

## 📌 2. Sơ Đồ Đấu Nối Chân (Pinout Chi Tiết)

### A. Module TM1638 & LCD I2C:
| Thiết bị | Chân Module | Chân ESP32-S3 | Chức Năng |
| :--- | :--- | :--- | :--- |
| **TM1638** | **STB** / **CLK** / **DIO** | **GPIO 15 / 16 / 17** | Chốt / Xung nhịp / Dữ liệu |
| **TM1638** | **VCC** / **GND** | **5V / GND** | Nguồn 5V và Mass chung |
| **LCD 1602 I2C** | **SDA** / **SCL** | **GPIO 8 / GPIO 9** | Bus dữ liệu I2C |
| **LCD 1602 I2C** | **VCC** / **GND** | **5V / GND** | Nguồn 5V và Mass chung |

### B. Driver Động Cơ JMC 2HSS57 (Kiểu Common Cathode):
| Driver | Chân Driver | Chân ESP32-S3 | Chức Năng |
| :--- | :--- | :--- | :--- |
| **Động cơ 1 (M1)** | **PUL1+ / DIR1+ / ENA1+** | **GPIO 4 / GPIO 5 / GPIO 6** | Xung bước / Hướng / Khóa lực |
| **Động cơ 1 (M1)** | **PUL1- / DIR1- / ENA1-** | **GND** | Nối âm chung với ESP32 |
| **Động cơ 2 (M2)** | **PUL2+ / DIR2+ / ENA2+** | **GPIO 18 / GPIO 19 / GPIO 20** | Xung bước / Hướng / Khóa lực |
| **Động cơ 2 (M2)** | **PUL2- / DIR2- / ENA2-** | **GND** | Nối âm chung với ESP32 |

---

## 🚀 3. Hướng Dẫn Sử Dụng & Kích Chạy

1. **Chọn dự án cần sử dụng:**
   - Điều khiển 1 động cơ: Mở thư mục `ESP32_TM1638_Motor_JMC`.
   - Điều khiển 2 động cơ: Mở thư mục `ESP32_TM1638_DualMotor_JMC`.
2. **Kích chạy Giao diện Máy tính:**
   - Nhấn đúp chuột vào file `Chay_Giao_Dien.bat` ở thư mục gốc để khởi chạy phần mềm C# GUI (`TM1638_GUI`).
   - Chọn đúng cổng COM của ESP32-S3 (Baudrate `115200`) và bấm **KẾT NỐI**.
