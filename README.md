# Dự Án ESP32-S3 Điều Khiển Module TM1638, LCD I2C & Động Cơ (Hybrid Servo JMC + RC Digital Servo)

**Tác giả:** Nguyễn Văn Lân  
**Vi điều khiển:** ESP32-S3 (ESP32-S3-WROOM-1 / DevKitC-1)  
**Driver & Động Cơ:** Driver JMC 2HSS57 (Hybrid Stepper Servo) & RC Digital Servos (MG996R, DS3218, SG90...)  

---

## 📂 1. Cấu Trúc Dự Án (Toàn Bộ Thư Mục Trên Repository)

```
ESP32_TM1638_Nguyen_Van_Lan/
│
├── 📂 ESP32_TM1638/                      --> [Dự án 1] ESP32-S3 + Module TM1638 (Cơ bản)
├── 📂 ESP32_TM1638_V2_I2C/               --> [Dự án 2] ESP32-S3 + TM1638 + LCD I2C (Mở rộng)
├── 📂 ESP32_TM1638_Motor_JMC/            --> [Dự án 3] ESP32-S3 + TM1638 + LCD I2C + 1 Động cơ JMC 2HSS57
├── 📂 ESP32_TM1638_DualMotor_JMC/        --> [Dự án 4] ESP32-S3 + TM1638 + LCD I2C + 2 Động cơ JMC 2HSS57
├── 📂 ESP32_TM1638_QuadMotor_JMC/        --> [Dự án 5] ESP32-S3 + TM1638 + LCD I2C + 4 Động cơ JMC 2HSS57
├── 📂 ESP32_TM1638_RCServo/              --> [Dự án 6] ESP32-S3 + TM1638 + LCD I2C + 4 Động Cơ RC Digital Servo
│
├── 📂 TM1638_GUI/                        --> [Dùng chung] Giao diện máy tính C# WinForms Dashboard (Visual Studio)
│   ├── Form1.cs
│   ├── Form1.Designer.cs
│   ├── Program.cs
│   └── TM1638_GUI.csproj
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

### B. Động Cơ RC Digital Servo (`ESP32_TM1638_RCServo` - Nhánh `feature/rc-digital-servo`):
| Động Cơ Servo | Chân Signal (Cam/Vàng) | Chân Nguồn VCC | Chân GND | Tần Số PWM |
| :--- | :--- | :--- | :--- | :--- |
| **Servo 1 (SV1)** | **GPIO 4** | **+5V đến +7.4V (Nguồn Rời)** | **GND Chung** | **50 Hz (500-2500µs)** |
| **Servo 2 (SV2)** | **GPIO 5** | **+5V đến +7.4V (Nguồn Rời)** | **GND Chung** | **50 Hz (500-2500µs)** |
| **Servo 3 (SV3)** | **GPIO 6** | **+5V đến +7.4V (Nguồn Rời)** | **GND Chung** | **50 Hz (500-2500µs)** |
| **Servo 4 (SV4)** | **GPIO 42** | **+5V đến +7.4V (Nguồn Rời)** | **GND Chung** | **50 Hz (500-2500µs)** |

*Lưu ý: Phải nối chung mass (GND) của nguồn Servo ngoài với chân GND của ESP32-S3.*

---

## 🚀 3. Hướng Dẫn Kích Chạy

1. **Chọn dự án cần nạp vào ESP32-S3:**
   - Điều khiển RC Digital Servo: Mở thư mục `ESP32_TM1638_RCServo`.
   - Điều khiển 4 Động cơ JMC 2HSS57: Mở thư mục `ESP32_TM1638_QuadMotor_JMC`.
   - Điều khiển 2 Động cơ JMC 2HSS57: Mở thư mục `ESP32_TM1638_DualMotor_JMC`.
2. **Kích chạy Giao diện Máy tính:**
   - Nhấn đúp chuột vào file `Chay_Giao_Dien.bat` ở thư mục gốc để khởi chạy phần mềm C# GUI (`TM1638_GUI`).
   - Chọn đúng cổng COM của ESP32-S3 (Baudrate `115200`) và bấm **KẾT NỐI**.
