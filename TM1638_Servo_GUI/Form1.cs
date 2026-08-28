using System;
using System.Drawing;
using System.IO.Ports;
using System.Text;
using System.Windows.Forms;

namespace TM1638_Servo_GUI
{
    public partial class Form1 : Form
    {
        private SerialPort? serialPort;
        private StringBuilder rxBuffer = new StringBuilder();
        private readonly object lockObj = new object();

        // Trạng thái Động cơ Servo 1
        private int servoAngle = 90;
        private bool isSweepMode = false;
        private bool stateDirty = true;

        private byte currentLedMask = 0x00;
        private byte currentButtonMask = 0x00;

        // UI Controls - Connection
        private ComboBox cmbPorts = null!;
        private Button btnRefreshPorts = null!;
        private Button btnConnect = null!;
        private Label lblStatus = null!;
        private Panel pnlStatusIndicator = null!;

        // UI Controls - Servo Control
        private Label lblAngleValue = null!;
        private TrackBar tbServoAngle = null!;
        private Button btnSweepToggle = null!;

        // Hardware Simulator
        private Label lblLcdLine1 = null!, lblLcdLine2 = null!;
        private Label[] lblDigits = new Label[8];
        private Button[] btnLeds = new Button[8];
        private Label[] lblButtons = new Label[8];

        private RichTextBox rtbLog = null!;
        private TextBox txtCustomCmd = null!;

        // Timer Render UI 20 FPS (50ms) mượt mà
        private System.Windows.Forms.Timer uiRenderTimer = null!;

        public Form1()
        {
            InitializeComponent();
            this.DoubleBuffered = true;
            this.SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint, true);

            SetupCustomUI();
            RefreshComPorts();

            uiRenderTimer = new System.Windows.Forms.Timer();
            uiRenderTimer.Interval = 50; // 20 FPS
            uiRenderTimer.Tick += UiRenderTimer_Tick;
            uiRenderTimer.Start();
        }

        private void SetupCustomUI()
        {
            this.Font = new Font("Segoe UI", 9f, FontStyle.Regular);
            this.ForeColor = Color.White;
            this.Text = "ESP32-S3 RC DIGITAL SERVO DASHBOARD - NGUYỄN VĂN LÂN";
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.ClientSize = new Size(1000, 880);
            this.BackColor = Color.FromArgb(16, 19, 26);

            // 1. HEADER PANEL
            Panel pnlHeader = new Panel
            {
                Location = new Point(0, 0),
                Size = new Size(1000, 65),
                BackColor = Color.FromArgb(24, 29, 42)
            };
            Label lblTitle = new Label
            {
                Text = "🤖 ESP32-S3 RC DIGITAL SERVO CONTROLLER DASHBOARD",
                Font = new Font("Segoe UI", 13f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 225, 255),
                Location = new Point(20, 10),
                AutoSize = true
            };
            Label lblSub = new Label
            {
                Text = "Giao diện chuyên dụng điều khiển Động Cơ RC Digital Servo (50Hz PWM, 0° - 180°) | Nguyễn Văn Lân",
                Font = new Font("Segoe UI", 8.5f, FontStyle.Regular),
                ForeColor = Color.FromArgb(150, 170, 195),
                Location = new Point(22, 38),
                AutoSize = true
            };
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSub);
            this.Controls.Add(pnlHeader);

            // 2. KẾT NỐI SERIAL COM
            GroupBox gbConnect = CreateCard("1. KẾT NỐI CỔNG COM / SERIAL", new Point(15, 75), new Size(970, 65));
            Label lblPort = new Label { Text = "Cổng COM:", Location = new Point(15, 26), AutoSize = true };
            cmbPorts = new ComboBox
            {
                Location = new Point(95, 23),
                Size = new Size(130, 28),
                DropDownStyle = ComboBoxStyle.DropDownList,
                BackColor = Color.FromArgb(36, 42, 56),
                ForeColor = Color.White
            };
            btnRefreshPorts = CreateButton("Làm Mới", new Point(235, 22), new Size(85, 30), Color.FromArgb(48, 56, 74));
            btnRefreshPorts.Click += (s, e) => RefreshComPorts();

            btnConnect = CreateButton("KẾT NỐI", new Point(330, 22), new Size(125, 30), Color.FromArgb(0, 170, 95));
            btnConnect.Click += BtnConnect_Click;

            Button btnSync = CreateButton("Đồng Bộ (SYNC)", new Point(465, 22), new Size(130, 30), Color.FromArgb(65, 78, 105));
            btnSync.Click += (s, e) => SendCommand("SYNC");

            pnlStatusIndicator = new Panel { Location = new Point(620, 29), Size = new Size(15, 15), BackColor = Color.Red };
            lblStatus = new Label { Text = "Chưa kết nối COM", Location = new Point(643, 27), AutoSize = true, ForeColor = Color.FromArgb(230, 90, 90), Font = new Font("Segoe UI", 9f, FontStyle.Bold) };

            gbConnect.Controls.AddRange(new Control[] { lblPort, cmbPorts, btnRefreshPorts, btnConnect, btnSync, pnlStatusIndicator, lblStatus });
            this.Controls.Add(gbConnect);

            // 3. BẢNG ĐIỀU KHIỂN SERVO 1 (SERVO 1 - SV1)
            GroupBox gbServo = CreateCard("2. ĐIỀU KHIỂN GÓC QUAY RC SERVO 1 (GPIO 4)", new Point(15, 148), new Size(970, 240));

            // Hiển thị góc quay kỹ thuật số lớn
            lblAngleValue = new Label
            {
                Text = "GÓC QUAY: 90°",
                Font = new Font("Segoe UI", 20f, FontStyle.Bold),
                ForeColor = Color.FromArgb(255, 190, 0),
                Location = new Point(20, 25),
                AutoSize = true
            };

            // Nút Sweep Mode
            btnSweepToggle = CreateButton("🔄 BẬT TỰ ĐỘNG QUÉT (SWEEP)", new Point(660, 25), new Size(280, 42), Color.FromArgb(0, 140, 200));
            btnSweepToggle.Click += (s, e) => SendCommand(isSweepMode ? "SERVO:SWEEP:0" : "SERVO:SWEEP:1");

            // Thanh trượt điều chỉnh góc (0 -> 180 độ)
            tbServoAngle = new TrackBar
            {
                Minimum = 0,
                Maximum = 180,
                Value = 90,
                TickFrequency = 10,
                Location = new Point(15, 75),
                Size = new Size(935, 45)
            };
            tbServoAngle.Scroll += (s, e) => {
                lblAngleValue.Text = $"GÓC QUAY: {tbServoAngle.Value}°";
            };
            tbServoAngle.MouseUp += (s, e) => SendCommand($"SV1:{tbServoAngle.Value}");

            // Các nút góc đặt nhanh (Preset Buttons)
            int btnY = 135;
            Button btn0 = CreateButton("0° (Min)", new Point(20, btnY), new Size(110, 36), Color.FromArgb(60, 70, 90));
            btn0.Click += (s, e) => SendCommand("SV1:0");

            Button btn45 = CreateButton("45°", new Point(140, btnY), new Size(110, 36), Color.FromArgb(60, 70, 90));
            btn45.Click += (s, e) => SendCommand("SV1:45");

            Button btn90 = CreateButton("90° (Trung Tâm)", new Point(260, btnY), new Size(150, 36), Color.FromArgb(0, 160, 90));
            btn90.Click += (s, e) => SendCommand("SV1:90");

            Button btn135 = CreateButton("135°", new Point(420, btnY), new Size(110, 36), Color.FromArgb(60, 70, 90));
            btn135.Click += (s, e) => SendCommand("SV1:135");

            Button btn180 = CreateButton("180° (Max)", new Point(540, btnY), new Size(110, 36), Color.FromArgb(60, 70, 90));
            btn180.Click += (s, e) => SendCommand("SV1:180");

            // Nút tinh chỉnh góc (+- 1° và +- 5°)
            Button btnDec5 = CreateButton("-5°", new Point(670, btnY), new Size(60, 36), Color.FromArgb(180, 80, 40));
            btnDec5.Click += (s, e) => SendCommand($"SV1:{Math.Max(0, servoAngle - 5)}");

            Button btnDec1 = CreateButton("-1°", new Point(738, btnY), new Size(60, 36), Color.FromArgb(140, 70, 30));
            btnDec1.Click += (s, e) => SendCommand($"SV1:{Math.Max(0, servoAngle - 1)}");

            Button btnInc1 = CreateButton("+1°", new Point(806, btnY), new Size(60, 36), Color.FromArgb(140, 70, 30));
            btnInc1.Click += (s, e) => SendCommand($"SV1:{Math.Min(180, servoAngle + 1)}");

            Button btnInc5 = CreateButton("+5°", new Point(874, btnY), new Size(65, 36), Color.FromArgb(180, 80, 40));
            btnInc5.Click += (s, e) => SendCommand($"SV1:{Math.Min(180, servoAngle + 5)}");

            gbServo.Controls.AddRange(new Control[] {
                lblAngleValue, btnSweepToggle, tbServoAngle,
                btn0, btn45, btn90, btn135, btn180,
                btnDec5, btnDec1, btnInc1, btnInc5
            });
            this.Controls.Add(gbServo);

            // 4. MÔ PHỎNG MÀN HÌNH PHẦN CỨNG (LCD 1602 & TM1638 8-LED 7 SEGMENT)
            GroupBox gbSim = CreateCard("3. MÔ PHỎNG MÀN HÌNH HARDWARE (LCD 1602 & TM1638)", new Point(15, 398), new Size(970, 135));

            Panel pnlLcd = new Panel { Location = new Point(15, 23), Size = new Size(400, 78), BackColor = Color.FromArgb(8, 40, 52), BorderStyle = BorderStyle.Fixed3D };
            lblLcdLine1 = new Label { Text = "1x RC SERVO S1 ", Font = new Font("Consolas", 13f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 230), Location = new Point(8, 10), AutoSize = true };
            lblLcdLine2 = new Label { Text = "GOC: 90\xDF  [MANUAL]", Font = new Font("Consolas", 13f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 230), Location = new Point(8, 40), AutoSize = true };
            pnlLcd.Controls.Add(lblLcdLine1); pnlLcd.Controls.Add(lblLcdLine2);
            gbSim.Controls.Add(pnlLcd);

            Panel pnl7Seg = new Panel { Location = new Point(430, 23), Size = new Size(520, 78), BackColor = Color.Black, BorderStyle = BorderStyle.FixedSingle };
            for (int i = 0; i < 8; i++)
            {
                lblDigits[i] = new Label { Text = " ", Font = new Font("Consolas", 24f, FontStyle.Bold), ForeColor = Color.Lime, Size = new Size(58, 62), Location = new Point(5 + i * 64, 7), TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.FromArgb(12, 20, 12) };
                pnl7Seg.Controls.Add(lblDigits[i]);
            }
            gbSim.Controls.Add(pnl7Seg);

            Label lblLcdTag = new Label { Text = "Màn hình LCD 1602 (I2C)", Location = new Point(15, 106), AutoSize = true, ForeColor = Color.FromArgb(150, 175, 195) };
            Label lbl7SegTag = new Label { Text = "Màn hình 8 LED 7 đoạn TM1638", Location = new Point(430, 106), AutoSize = true, ForeColor = Color.FromArgb(150, 175, 195) };
            gbSim.Controls.AddRange(new Control[] { lblLcdTag, lbl7SegTag });
            this.Controls.Add(gbSim);

            // 5. GIÁM SÁT 8 LED & 8 NÚT BẤM TM1638
            GroupBox gbHardware = CreateCard("4. GIÁM SÁT 8 ĐÈN LED & 8 NÚT BẤM TM1638", new Point(15, 540), new Size(970, 105));
            string[] ledNames = { "SWEEP", "BAR 1", "BAR 2", "BAR 3", "BAR 4", "BAR 5", "BAR 6", "BAR 7" };
            for (int i = 0; i < 8; i++)
            {
                btnLeds[i] = new Button { Text = $"{ledNames[i]}\nOFF", Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), Size = new Size(106, 44), Location = new Point(15 + i * 118, 24), BackColor = Color.FromArgb(42, 46, 58), ForeColor = Color.Gray, FlatStyle = FlatStyle.Flat };
                btnLeds[i].FlatAppearance.BorderSize = 0;
                gbHardware.Controls.Add(btnLeds[i]);
            }

            string[] btnNames = { "S1:+5°", "S2:-5°", "S3:0°", "S4:45°", "S5:90°", "S6:135°", "S7:180°", "S8:SWEEP" };
            for (int i = 0; i < 8; i++)
            {
                lblButtons[i] = new Label { Text = btnNames[i], Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), Size = new Size(106, 22), Location = new Point(15 + i * 118, 73), TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.FromArgb(32, 38, 50), ForeColor = Color.FromArgb(140, 155, 175), BorderStyle = BorderStyle.FixedSingle };
                gbHardware.Controls.Add(lblButtons[i]);
            }
            this.Controls.Add(gbHardware);

            // 6. NHẬT KÝ SERIAL LOG
            GroupBox gbLog = CreateCard("5. NHẬT KÝ LỆNH SERIAL", new Point(15, 650), new Size(970, 210));
            rtbLog = new RichTextBox { Location = new Point(15, 25), Size = new Size(940, 130), BackColor = Color.FromArgb(10, 14, 20), ForeColor = Color.FromArgb(0, 255, 170), Font = new Font("Consolas", 9f), ReadOnly = true };
            
            txtCustomCmd = new TextBox { Location = new Point(15, 168), Size = new Size(780, 26), BackColor = Color.FromArgb(36, 42, 56), ForeColor = Color.White, Text = "SV1:90" };
            Button btnSendCmd = CreateButton("GỬI LỆNH", new Point(805, 166), new Size(150, 30), Color.FromArgb(0, 120, 215));
            btnSendCmd.Click += (s, e) => SendCommand(txtCustomCmd.Text);

            gbLog.Controls.AddRange(new Control[] { rtbLog, txtCustomCmd, btnSendCmd });
            this.Controls.Add(gbLog);
        }

        private GroupBox CreateCard(string title, Point location, Size size)
        {
            return new GroupBox
            {
                Text = title,
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 210, 255),
                Location = location,
                Size = size,
                BackColor = Color.FromArgb(24, 29, 40)
            };
        }

        private Button CreateButton(string text, Point loc, Size size, Color bg)
        {
            var btn = new Button
            {
                Text = text,
                Location = loc,
                Size = size,
                BackColor = bg,
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Cursor = Cursors.Hand,
                Font = new Font("Segoe UI", 8.5f, FontStyle.Bold)
            };
            btn.FlatAppearance.BorderSize = 0;
            return btn;
        }

        private void RefreshComPorts()
        {
            cmbPorts.Items.Clear();
            string[] ports = SerialPort.GetPortNames();
            cmbPorts.Items.AddRange(ports);
            if (cmbPorts.Items.Count > 0)
            {
                cmbPorts.SelectedIndex = 0;
                for (int i = 0; i < cmbPorts.Items.Count; i++)
                {
                    if (cmbPorts.Items[i]?.ToString() == "COM7") { cmbPorts.SelectedIndex = i; break; }
                }
            }
        }

        private void BtnConnect_Click(object? sender, EventArgs e)
        {
            if (serialPort != null && serialPort.IsOpen)
            {
                DisconnectSerial();
            }
            else
            {
                if (cmbPorts.SelectedItem == null) return;
                string portName = cmbPorts.SelectedItem.ToString() ?? "COM7";
                try
                {
                    serialPort = new SerialPort(portName, 115200, Parity.None, 8, StopBits.One)
                    {
                        ReadTimeout = 200,
                        WriteTimeout = 200,
                        ReadBufferSize = 8192
                    };
                    serialPort.DataReceived += SerialPort_DataReceived;
                    serialPort.Open();

                    btnConnect.Text = "NGẮT KẾT NỐI";
                    btnConnect.BackColor = Color.FromArgb(180, 45, 45);
                    pnlStatusIndicator.BackColor = Color.Lime;
                    lblStatus.Text = $"Đã kết nối {portName}";
                    lblStatus.ForeColor = Color.Lime;
                    LogMessage($"Đã mở cổng {portName} thành công.");
                    SendCommand("SYNC");
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Lỗi Kết Nối COM", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void DisconnectSerial()
        {
            try
            {
                if (serialPort != null)
                {
                    if (serialPort.IsOpen)
                    {
                        serialPort.DataReceived -= SerialPort_DataReceived;
                        serialPort.Close();
                    }
                    serialPort.Dispose();
                    serialPort = null;
                }
            }
            catch { }

            btnConnect.Text = "KẾT NỐI";
            btnConnect.BackColor = Color.FromArgb(0, 170, 95);
            pnlStatusIndicator.BackColor = Color.Red;
            lblStatus.Text = "Đã ngắt kết nối";
            lblStatus.ForeColor = Color.FromArgb(230, 90, 90);
        }

        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                if (serialPort == null || !serialPort.IsOpen) return;
                string incoming = serialPort.ReadExisting();
                
                lock (lockObj)
                {
                    rxBuffer.Append(incoming);
                    string fullStr = rxBuffer.ToString();
                    int lastNL = fullStr.LastIndexOf('\n');
                    if (lastNL >= 0)
                    {
                        string processChunk = fullStr.Substring(0, lastNL);
                        rxBuffer.Remove(0, lastNL + 1);

                        string[] lines = processChunk.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
                        foreach (var line in lines)
                        {
                            ParsePacketInMemory(line.Trim());
                        }
                    }
                }
            }
            catch { }
        }

        private void ParsePacketInMemory(string msg)
        {
            if (msg.StartsWith("SERVOSTAT:")) ParseServoStatusData(msg.Substring(10));
            else if (msg.StartsWith("BTN:"))
            {
                if (byte.TryParse(msg.Substring(4).Trim(), System.Globalization.NumberStyles.HexNumber, null, out byte b))
                {
                    currentButtonMask = b;
                    stateDirty = true;
                }
            }
            else if (msg.StartsWith("LEDS:"))
            {
                if (byte.TryParse(msg.Substring(5).Trim(), System.Globalization.NumberStyles.HexNumber, null, out byte l))
                {
                    currentLedMask = l;
                    stateDirty = true;
                }
            }
        }

        private void ParseServoStatusData(string data)
        {
            var parts = data.Split(',');
            foreach (var part in parts)
            {
                var kv = part.Split('='); if (kv.Length != 2) continue;
                string k = kv[0].Trim(), v = kv[1].Trim();

                if (k == "S1" && int.TryParse(v, out int deg))
                {
                    servoAngle = deg;
                    stateDirty = true;
                }
                else if (k == "SWEEP" && int.TryParse(v, out int swp))
                {
                    isSweepMode = (swp == 1);
                    stateDirty = true;
                }
            }
        }

        private void UiRenderTimer_Tick(object? sender, EventArgs e)
        {
            if (stateDirty)
            {
                stateDirty = false;
                UpdateServoRender();
            }
        }

        private void UpdateServoRender()
        {
            lblAngleValue.Text = $"GÓC QUAY: {servoAngle}°";
            
            btnSweepToggle.Text = isSweepMode ? "⏸ DỪNG TỰ ĐỘNG QUÉT (SWEEP)" : "🔄 BẬT TỰ ĐỘNG QUÉT (SWEEP)";
            btnSweepToggle.BackColor = isSweepMode ? Color.FromArgb(220, 40, 40) : Color.FromArgb(0, 140, 200);

            if (!tbServoAngle.Capture && tbServoAngle.Value != servoAngle)
            {
                if (servoAngle >= tbServoAngle.Minimum && servoAngle <= tbServoAngle.Maximum)
                {
                    tbServoAngle.Value = servoAngle;
                }
            }

            // LCD Simulator
            lblLcdLine1.Text = "1x RC SERVO S1 ";
            lblLcdLine2.Text = $"GOC:{servoAngle,3}\xDF  {(isSweepMode ? "[SWEEP]" : "[MANUAL]")}";

            // 7-Seg Simulator
            string dispStr = string.Format("S1-{0,3}\xDF  ", servoAngle);
            for (int i = 0; i < 8; i++) lblDigits[i].Text = " ";
            for (int i = 0; i < dispStr.Length && i < 8; i++)
            {
                lblDigits[i].Text = dispStr[i].ToString();
            }

            // LEDs Bar
            byte ledMask = 0;
            if (isSweepMode) ledMask |= 1;
            int levelBars = (servoAngle * 7) / 180;
            for (int i = 0; i <= levelBars && i < 7; i++) ledMask |= (byte)(1 << (1 + i));

            string[] ledNames = { "SWEEP", "BAR 1", "BAR 2", "BAR 3", "BAR 4", "BAR 5", "BAR 6", "BAR 7" };
            for (int i = 0; i < 8; i++)
            {
                bool isOn = (ledMask & (1 << i)) != 0;
                btnLeds[i].Text = $"{ledNames[i]}\n{(isOn ? "ON" : "OFF")}";
                btnLeds[i].BackColor = isOn ? Color.FromArgb(230, 35, 35) : Color.FromArgb(42, 46, 58);
                btnLeds[i].ForeColor = isOn ? Color.White : Color.Gray;
            }

            // Buttons
            string[] btnNames = { "S1:+5°", "S2:-5°", "S3:0°", "S4:45°", "S5:90°", "S6:135°", "S7:180°", "S8:SWEEP" };
            for (int i = 0; i < 8; i++)
            {
                bool isPressed = (currentButtonMask & (1 << i)) != 0;
                lblButtons[i].Text = $"{btnNames[i]}";
                lblButtons[i].BackColor = isPressed ? Color.FromArgb(255, 185, 0) : Color.FromArgb(32, 38, 50);
                lblButtons[i].ForeColor = isPressed ? Color.Black : Color.FromArgb(140, 155, 175);
            }
        }

        public void SendCommand(string cmd)
        {
            if (serialPort != null && serialPort.IsOpen)
            {
                try
                {
                    serialPort.WriteLine(cmd);
                    LogMessage($"[GỬI] {cmd}");
                }
                catch (Exception ex)
                {
                    LogMessage($"[LỖI GỬI] {ex.Message}");
                }
            }
            else
            {
                LogMessage($"[CHƯA KẾT NỐI] {cmd}");
            }
        }

        private void LogMessage(string text)
        {
            if (rtbLog.IsDisposed) return;
            if (rtbLog.TextLength > 10000) rtbLog.Clear();
            rtbLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {text}\n");
            rtbLog.SelectionStart = rtbLog.Text.Length;
            rtbLog.ScrollToCaret();
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            uiRenderTimer?.Stop();
            DisconnectSerial();
            base.OnFormClosing(e);
        }
    }
}
