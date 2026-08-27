using System;
using System.Drawing;
using System.IO.Ports;
using System.Text;
using System.Windows.Forms;

namespace TM1638_GUI
{
    public partial class Form1 : Form
    {
        private SerialPort? serialPort;

        // Trạng thái hệ thống động cơ
        private bool isMotorRunning = false;
        private bool isDirCW = true;
        private bool isDriverEnabled = true;
        private uint currentSpeedSPS = 1600;
        private long currentPosition = 0;
        private int currentMode = 0; // 0: CONT, 1: POS, 2: JOG
        private byte currentLedMask = 0x00;
        private byte currentButtonMask = 0x00;

        // UI Controls - Kết nối
        private ComboBox cmbPorts = null!;
        private Button btnRefreshPorts = null!;
        private Button btnConnect = null!;
        private Label lblStatus = null!;
        private Panel pnlStatusIndicator = null!;
        private Button btnSync = null!;

        // UI Controls - Động cơ Master
        private Button btnMasterRun = null!;
        private Button btnMasterDir = null!;
        private Button btnMasterEnable = null!;
        private Button btnMasterZero = null!;
        private RadioButton rbModeCont = null!;
        private RadioButton rbModePos = null!;
        private RadioButton rbModeJog = null!;

        // UI Controls - Tốc độ & Định vị
        private TrackBar tbSpeed = null!;
        private Label lblSpeedVal = null!;
        private TextBox txtTargetSteps = null!;
        private Button btnMoveSteps = null!;
        private Button btnMovePlus1Rev = null!;
        private Button btnMoveMinus1Rev = null!;
        private Button btnMovePlus5Rev = null!;

        // UI Controls - Simulator LCD 1602 & TM1638
        private Label lblLcdLine1 = null!;
        private Label lblLcdLine2 = null!;
        private Label[] lblDigits = new Label[8];
        private Button[] btnLeds = new Button[8];
        private Label[] lblButtons = new Label[8];

        // UI Controls - Log & Custom Command
        private RichTextBox rtbLog = null!;
        private TextBox txtCustomCmd = null!;
        private Button btnSendCmd = null!;

        public Form1()
        {
            InitializeComponent();
            SetupCustomUI();
            RefreshComPorts();
        }

        private void SetupCustomUI()
        {
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);
            this.ForeColor = Color.White;
            this.Text = "ESP32-S3 + TM1638 & JMC-2HSS57 Motor Dashboard - Nguyễn Văn Lân";
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.ClientSize = new Size(1020, 780);
            this.BackColor = Color.FromArgb(20, 23, 30);

            // 1. HEADER
            Panel pnlHeader = new Panel
            {
                Location = new Point(0, 0),
                Size = new Size(1020, 65),
                BackColor = Color.FromArgb(28, 32, 44)
            };
            Label lblTitle = new Label
            {
                Text = "⚡ ESP32-S3 + TM1638 & JMC-2HSS57 HYBRID SERVO DASHBOARD",
                Font = new Font("Segoe UI", 12.5f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 220, 255),
                Location = new Point(20, 10),
                AutoSize = true
            };
            Label lblSub = new Label
            {
                Text = "Bảng Điều Khiển Động Cơ Bước Servo Lai Vòng Kín & Giám Sát Phần Cứng Thời Gian Thực | Tác giả: Nguyễn Văn Lân",
                Font = new Font("Segoe UI", 8.5f, FontStyle.Regular),
                ForeColor = Color.FromArgb(160, 175, 195),
                Location = new Point(22, 38),
                AutoSize = true
            };
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSub);
            this.Controls.Add(pnlHeader);

            // 2. KẾT NỐI SERIAL
            GroupBox gbConnect = CreateCard("1. KẾT NỐI CỔNG COM (ESP32-S3)", new Point(15, 75), new Size(990, 65));
            Label lblPort = new Label { Text = "Cổng COM:", Location = new Point(15, 26), AutoSize = true };
            cmbPorts = new ComboBox
            {
                Location = new Point(95, 23),
                Size = new Size(120, 28),
                DropDownStyle = ComboBoxStyle.DropDownList,
                BackColor = Color.FromArgb(40, 44, 58),
                ForeColor = Color.White
            };
            btnRefreshPorts = CreateButton("Làm Mới", new Point(225, 22), new Size(80, 30), Color.FromArgb(50, 55, 70));
            btnRefreshPorts.Click += (s, e) => RefreshComPorts();

            btnConnect = CreateButton("KẾT NỐI", new Point(315, 22), new Size(115, 30), Color.FromArgb(0, 160, 90));
            btnConnect.Click += BtnConnect_Click;

            btnSync = CreateButton("Đồng Bộ", new Point(440, 22), new Size(85, 30), Color.FromArgb(70, 80, 105));
            btnSync.Click += (s, e) => SendCommand("SYNC");

            pnlStatusIndicator = new Panel
            {
                Location = new Point(545, 29),
                Size = new Size(15, 15),
                BackColor = Color.Red
            };
            lblStatus = new Label
            {
                Text = "Chưa kết nối",
                Location = new Point(568, 27),
                AutoSize = true,
                ForeColor = Color.FromArgb(220, 100, 100)
            };

            gbConnect.Controls.AddRange(new Control[] { lblPort, cmbPorts, btnRefreshPorts, btnConnect, btnSync, pnlStatusIndicator, lblStatus });
            this.Controls.Add(gbConnect);

            // 3. KHỐI ĐIỀU KHIỂN TRUNG TÂM ĐỘNG CƠ (MOTOR MASTER CONTROLS)
            GroupBox gbMotor = CreateCard("2. ĐIỀU KHIỂN ĐỘNG CƠ (MOTOR MASTER)", new Point(15, 148), new Size(485, 230));

            btnMasterRun = CreateButton("▶ BẮT ĐẦU CHẠY (RUN)", new Point(20, 28), new Size(215, 45), Color.FromArgb(0, 160, 80));
            btnMasterRun.Font = new Font("Segoe UI", 10.5f, FontStyle.Bold);
            btnMasterRun.Click += BtnMasterRun_Click;

            btnMasterDir = CreateButton("↻ QUAY THUẬN (CW)", new Point(250, 28), new Size(215, 45), Color.FromArgb(0, 130, 200));
            btnMasterDir.Font = new Font("Segoe UI", 10.5f, FontStyle.Bold);
            btnMasterDir.Click += BtnMasterDir_Click;

            btnMasterEnable = CreateButton("🔒 KHÓA TRỤC (ENA ON)", new Point(20, 85), new Size(215, 40), Color.FromArgb(90, 60, 150));
            btnMasterEnable.Click += BtnMasterEnable_Click;

            btnMasterZero = CreateButton("↺ RESET VỊ TRÍ 0 (HOME)", new Point(250, 85), new Size(215, 40), Color.FromArgb(160, 80, 30));
            btnMasterZero.Click += (s, e) => SendCommand("MOTOR:ZERO");

            // Mode Selection
            Label lblModeTitle = new Label { Text = "Chế độ chạy:", Location = new Point(20, 140), AutoSize = true, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            rbModeCont = new RadioButton { Text = "Liên tục (CONT)", Location = new Point(20, 165), AutoSize = true, Checked = true };
            rbModePos  = new RadioButton { Text = "Định vị (POS)", Location = new Point(165, 165), AutoSize = true };
            rbModeJog  = new RadioButton { Text = "Nhấp tay (JOG)", Location = new Point(310, 165), AutoSize = true };

            rbModeCont.CheckedChanged += (s, e) => { if (rbModeCont.Checked) SendCommand("MOTOR:MODE:0"); };
            rbModePos.CheckedChanged  += (s, e) => { if (rbModePos.Checked)  SendCommand("MOTOR:MODE:1"); };
            rbModeJog.CheckedChanged  += (s, e) => { if (rbModeJog.Checked)  SendCommand("MOTOR:MODE:2"); };

            Label lblHint = new Label
            {
                Text = "💡 Phím tắt phần cứng TM1638: S1=RUN/STOP, S2=Đổi chiều, S3=Tăng tốc, S4=Giảm tốc",
                Location = new Point(15, 200),
                AutoSize = true,
                ForeColor = Color.FromArgb(140, 180, 220),
                Font = new Font("Segoe UI", 8f)
            };

            gbMotor.Controls.AddRange(new Control[] {
                btnMasterRun, btnMasterDir, btnMasterEnable, btnMasterZero,
                lblModeTitle, rbModeCont, rbModePos, rbModeJog, lblHint
            });
            this.Controls.Add(gbMotor);

            // 4. KHỐI TỐC ĐỘ VÀ ĐỊNH VỊ (SPEED & POSITIONING)
            GroupBox gbSpeedPos = CreateCard("3. TỐC ĐỘ & CHẠY ĐỊNH VỊ BƯỚC", new Point(510, 148), new Size(495, 230));

            lblSpeedVal = new Label
            {
                Text = "Tốc độ: 1600 SPS (~ 60.0 RPM)",
                Location = new Point(20, 26),
                AutoSize = true,
                ForeColor = Color.FromArgb(255, 220, 0),
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold)
            };

            tbSpeed = new TrackBar
            {
                Minimum = 200,
                Maximum = 16000,
                Value = 1600,
                TickFrequency = 1000,
                Location = new Point(15, 50),
                Size = new Size(465, 45)
            };
            tbSpeed.Scroll += (s, e) =>
            {
                uint spd = (uint)tbSpeed.Value;
                float rpm = (spd / 1600.0f) * 60.0f;
                lblSpeedVal.Text = $"Tốc độ: {spd} SPS (~ {rpm:F1} RPM)";
            };
            tbSpeed.MouseUp += (s, e) =>
            {
                SendCommand($"MOTOR:SPEED:{tbSpeed.Value}");
            };

            // Quick speed buttons
            int sx = 20;
            int[] quickSpds = { 400, 1600, 3200, 8000 };
            string[] quickLabels = { "15 RPM", "60 RPM", "120 RPM", "300 RPM" };
            for (int i = 0; i < quickSpds.Length; i++)
            {
                int val = quickSpds[i];
                Button b = CreateButton(quickLabels[i], new Point(sx, 95), new Size(80, 26), Color.FromArgb(50, 60, 80));
                b.Click += (s, e) =>
                {
                    tbSpeed.Value = Math.Min(tbSpeed.Maximum, Math.Max(tbSpeed.Minimum, val));
                    float rpm = (val / 1600.0f) * 60.0f;
                    lblSpeedVal.Text = $"Tốc độ: {val} SPS (~ {rpm:F1} RPM)";
                    SendCommand($"MOTOR:SPEED:{val}");
                };
                gbSpeedPos.Controls.Add(b);
                sx += 88;
            }

            // Move target steps
            Label lblMove = new Label { Text = "Chạy định vị:", Location = new Point(20, 132), AutoSize = true, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            txtTargetSteps = new TextBox
            {
                Text = "1600",
                Location = new Point(115, 130),
                Size = new Size(90, 26),
                BackColor = Color.FromArgb(40, 44, 58),
                ForeColor = Color.White
            };
            Label lblStepUnit = new Label { Text = "xung (bước)", Location = new Point(210, 133), AutoSize = true };

            btnMoveSteps = CreateButton("🚀 Chạy Bước", new Point(300, 128), new Size(110, 28), Color.FromArgb(0, 140, 200));
            btnMoveSteps.Click += (s, e) =>
            {
                if (int.TryParse(txtTargetSteps.Text, out int steps))
                {
                    SendCommand($"MOTOR:MOVE:{steps}");
                }
            };

            btnMoveMinus1Rev = CreateButton("↺ -1 Vòng", new Point(20, 170), new Size(100, 32), Color.FromArgb(70, 75, 95));
            btnMoveMinus1Rev.Click += (s, e) => SendCommand("MOTOR:MOVE:-1600");

            btnMovePlus1Rev = CreateButton("↻ +1 Vòng", new Point(130, 170), new Size(100, 32), Color.FromArgb(0, 130, 170));
            btnMovePlus1Rev.Click += (s, e) => SendCommand("MOTOR:MOVE:1600");

            btnMovePlus5Rev = CreateButton("↻ +5 Vòng", new Point(240, 170), new Size(100, 32), Color.FromArgb(0, 100, 160));
            btnMovePlus5Rev.Click += (s, e) => SendCommand("MOTOR:MOVE:8000");

            gbSpeedPos.Controls.AddRange(new Control[] {
                lblSpeedVal, tbSpeed, lblMove, txtTargetSteps, lblStepUnit, btnMoveSteps,
                btnMoveMinus1Rev, btnMovePlus1Rev, btnMovePlus5Rev
            });
            this.Controls.Add(gbSpeedPos);

            // 5. MÔ PHỎNG MÀN HÌNH LCD 1602 & 8 LED 7 ĐOẠN TM1638
            GroupBox gbSim = CreateCard("4. MÔ PHỎNG MÀN HÌNH PHẦN CỨNG (LCD 1602 & TM1638 7-SEG)", new Point(15, 385), new Size(990, 150));

            // LCD 1602 Simulator
            Panel pnlLcd = new Panel
            {
                Location = new Point(20, 28),
                Size = new Size(380, 75),
                BackColor = Color.FromArgb(10, 45, 60),
                BorderStyle = BorderStyle.Fixed3D
            };
            lblLcdLine1 = new Label
            {
                Text = "STOP    60RPM  CW",
                Font = new Font("Consolas", 13f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 255, 230),
                Location = new Point(10, 10),
                AutoSize = true
            };
            lblLcdLine2 = new Label
            {
                Text = "CONT P:+000000 ON",
                Font = new Font("Consolas", 13f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 255, 230),
                Location = new Point(10, 38),
                AutoSize = true
            };
            pnlLcd.Controls.Add(lblLcdLine1);
            pnlLcd.Controls.Add(lblLcdLine2);
            gbSim.Controls.Add(pnlLcd);

            Label lblLcdTag = new Label
            {
                Text = "Màn hình LCD 1602 (I2C)",
                Location = new Point(20, 108),
                AutoSize = true,
                ForeColor = Color.FromArgb(160, 180, 200)
            };
            gbSim.Controls.Add(lblLcdTag);

            // TM1638 8-Digit 7-Segment Simulator
            Panel pnl7Seg = new Panel
            {
                Location = new Point(420, 28),
                Size = new Size(545, 75),
                BackColor = Color.Black,
                BorderStyle = BorderStyle.FixedSingle
            };
            for (int i = 0; i < 8; i++)
            {
                lblDigits[i] = new Label
                {
                    Text = " ",
                    Font = new Font("Consolas", 22f, FontStyle.Bold),
                    ForeColor = Color.Lime,
                    Size = new Size(62, 58),
                    Location = new Point(6 + i * 67, 7),
                    TextAlign = ContentAlignment.MiddleCenter,
                    BackColor = Color.FromArgb(12, 18, 12)
                };
                pnl7Seg.Controls.Add(lblDigits[i]);
            }
            gbSim.Controls.Add(pnl7Seg);

            Label lbl7SegTag = new Label
            {
                Text = "Màn hình 8 LED 7 đoạn TM1638",
                Location = new Point(420, 108),
                AutoSize = true,
                ForeColor = Color.FromArgb(160, 180, 200)
            };
            gbSim.Controls.Add(lbl7SegTag);

            this.Controls.Add(gbSim);

            // 6. GIÁM SÁT 8 LED ĐƠN & 8 NÚT BẤM TM1638
            GroupBox gbHardware = CreateCard("5. GIÁM SÁT TRẠNG THÁI 8 ĐÈN LED & 8 NÚT BẤM TM1638", new Point(15, 542), new Size(990, 105));

            // 8 LEDs
            string[] ledNames = { "RUN", "DIR", "ENA", "SPD1", "SPD2", "SPD3", "SPD4", "SPD5" };
            for (int i = 0; i < 8; i++)
            {
                int idx = i;
                btnLeds[i] = new Button
                {
                    Text = $"{ledNames[i]}\nOFF",
                    Font = new Font("Segoe UI", 7.5f, FontStyle.Bold),
                    Size = new Size(54, 45),
                    Location = new Point(20 + i * 58, 25),
                    BackColor = Color.FromArgb(45, 48, 58),
                    ForeColor = Color.Gray,
                    FlatStyle = FlatStyle.Flat
                };
                btnLeds[i].FlatAppearance.BorderSize = 1;
                btnLeds[i].FlatAppearance.BorderColor = Color.FromArgb(70, 75, 88);
                gbHardware.Controls.Add(btnLeds[i]);
            }

            Label lblSep = new Label
            {
                BorderStyle = BorderStyle.Fixed3D,
                Location = new Point(495, 20),
                Size = new Size(2, 65)
            };
            gbHardware.Controls.Add(lblSep);

            // 8 Buttons
            for (int i = 0; i < 8; i++)
            {
                lblButtons[i] = new Label
                {
                    Text = $"S{i + 1}",
                    Font = new Font("Segoe UI", 8.5f, FontStyle.Bold),
                    Size = new Size(52, 45),
                    Location = new Point(515 + i * 57, 25),
                    TextAlign = ContentAlignment.MiddleCenter,
                    BackColor = Color.FromArgb(35, 40, 52),
                    ForeColor = Color.FromArgb(130, 145, 165),
                    BorderStyle = BorderStyle.FixedSingle
                };
                gbHardware.Controls.Add(lblButtons[i]);
            }

            Label lblLedDesc = new Label { Text = "8 Đèn LED Đơn TM1638", Location = new Point(20, 76), AutoSize = true, ForeColor = Color.FromArgb(140, 160, 180), Font = new Font("Segoe UI", 8f) };
            Label lblBtnDesc = new Label { Text = "8 Nút Bấm TM1638 (Sáng vàng khi nhấn trực tiếp trên mạch)", Location = new Point(515, 76), AutoSize = true, ForeColor = Color.FromArgb(140, 160, 180), Font = new Font("Segoe UI", 8f) };
            gbHardware.Controls.Add(lblLedDesc);
            gbHardware.Controls.Add(lblBtnDesc);

            this.Controls.Add(gbHardware);

            // 7. LOG TERMINAL & TẬP LỆNH TÙY BIẾN
            GroupBox gbLog = CreateCard("6. NHẬT KÝ TRUYỀN THÔNG SERIAL & GỬI LỆNH TÙY Ý", new Point(15, 652), new Size(990, 115));
            rtbLog = new RichTextBox
            {
                Location = new Point(15, 22),
                Size = new Size(620, 80),
                BackColor = Color.FromArgb(12, 15, 20),
                ForeColor = Color.FromArgb(0, 255, 170),
                Font = new Font("Consolas", 8.5f),
                ReadOnly = true
            };
            gbLog.Controls.Add(rtbLog);

            Label lblCmd = new Label { Text = "Lệnh Serial tùy ý:", Location = new Point(650, 24), AutoSize = true };
            txtCustomCmd = new TextBox
            {
                Location = new Point(650, 48),
                Size = new Size(220, 26),
                BackColor = Color.FromArgb(40, 44, 58),
                ForeColor = Color.White,
                Text = "MOTOR:RUN"
            };
            txtCustomCmd.KeyDown += (s, e) =>
            {
                if (e.KeyCode == Keys.Enter)
                {
                    SendCommand(txtCustomCmd.Text);
                    e.SuppressKeyPress = true;
                }
            };
            btnSendCmd = CreateButton("GỬI", new Point(880, 46), new Size(80, 28), Color.FromArgb(0, 120, 215));
            btnSendCmd.Click += (s, e) => SendCommand(txtCustomCmd.Text);

            gbLog.Controls.AddRange(new Control[] { lblCmd, txtCustomCmd, btnSendCmd });
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
                BackColor = Color.FromArgb(28, 32, 42)
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
                Font = new Font("Segoe UI", 9f, FontStyle.Bold)
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
                    if (cmbPorts.Items[i]?.ToString() == "COM7")
                    {
                        cmbPorts.SelectedIndex = i;
                        break;
                    }
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
                if (cmbPorts.SelectedItem == null)
                {
                    MessageBox.Show("Vui lòng chọn cổng COM trước!", "Thông báo", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                string portName = cmbPorts.SelectedItem.ToString() ?? "COM7";
                try
                {
                    serialPort = new SerialPort(portName, 115200, Parity.None, 8, StopBits.One)
                    {
                        ReadTimeout = 500,
                        WriteTimeout = 500
                    };
                    serialPort.DataReceived += SerialPort_DataReceived;
                    serialPort.Open();

                    btnConnect.Text = "NGẮT KẾT NỐI";
                    btnConnect.BackColor = Color.FromArgb(180, 50, 50);
                    pnlStatusIndicator.BackColor = Color.Lime;
                    lblStatus.Text = $"Đã kết nối {portName}";
                    lblStatus.ForeColor = Color.Lime;
                    LogMessage($"Đã mở cổng {portName} thành công (115200 baud).");

                    SendCommand("SYNC");
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Lỗi kết nối cổng {portName}:\n{ex.Message}", "Lỗi COM", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void DisconnectSerial()
        {
            try
            {
                if (serialPort != null)
                {
                    if (serialPort.IsOpen) serialPort.Close();
                    serialPort.Dispose();
                    serialPort = null;
                }
            }
            catch { }

            btnConnect.Text = "KẾT NỐI";
            btnConnect.BackColor = Color.FromArgb(0, 160, 90);
            pnlStatusIndicator.BackColor = Color.Red;
            lblStatus.Text = "Đã ngắt kết nối";
            lblStatus.ForeColor = Color.FromArgb(220, 100, 100);
            LogMessage("Đã đóng cổng COM.");
        }

        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                if (serialPort == null || !serialPort.IsOpen) return;
                string line = serialPort.ReadLine().Trim();

                if (this.IsDisposed || !this.IsHandleCreated) return;
                this.BeginInvoke(new Action(() => ProcessIncomingMessage(line)));
            }
            catch { }
        }

        private void ProcessIncomingMessage(string msg)
        {
            if (msg.StartsWith("MSTAT:"))
            {
                // Format: MSTAT:RUN=1,DIR=CW,SPD=1600,RPM=60.0,POS=3200,ENA=1,MODE=0
                ParseMotorStatus(msg.Substring(6));
            }
            else if (msg.StartsWith("BTN:"))
            {
                string hex = msg.Substring(4).Trim();
                if (byte.TryParse(hex, System.Globalization.NumberStyles.HexNumber, null, out byte mask))
                {
                    currentButtonMask = mask;
                    UpdateButtonsUI(mask);
                }
            }
            else if (msg.StartsWith("DISP:"))
            {
                string text = msg.Substring(5);
                UpdateDisplaySimulator(text);
            }
            else if (msg.StartsWith("LEDS:"))
            {
                string hex = msg.Substring(5).Trim();
                if (byte.TryParse(hex, System.Globalization.NumberStyles.HexNumber, null, out byte mask))
                {
                    currentLedMask = mask;
                    UpdateLedsUI(mask);
                }
            }
            else if (msg.StartsWith("OK:"))
            {
                LogMessage($"[THÀNH CÔNG] {msg}");
            }
            else
            {
                LogMessage($"[ESP32] {msg}");
            }
        }

        private void ParseMotorStatus(string data)
        {
            var parts = data.Split(',');
            foreach (var part in parts)
            {
                var kv = part.Split('=');
                if (kv.Length != 2) continue;
                string key = kv[0].Trim();
                string val = kv[1].Trim();

                switch (key)
                {
                    case "RUN":
                        isMotorRunning = (val == "1");
                        UpdateMasterRunButton();
                        break;
                    case "DIR":
                        isDirCW = (val == "CW");
                        UpdateMasterDirButton();
                        break;
                    case "SPD":
                        if (uint.TryParse(val, out uint spd))
                        {
                            currentSpeedSPS = spd;
                            if (spd >= tbSpeed.Minimum && spd <= tbSpeed.Maximum)
                            {
                                tbSpeed.Value = (int)spd;
                            }
                        }
                        break;
                    case "RPM":
                        if (float.TryParse(val, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float rpm))
                        {
                            lblSpeedVal.Text = $"Tốc độ: {currentSpeedSPS} SPS (~ {rpm:F1} RPM)";
                        }
                        break;
                    case "POS":
                        if (long.TryParse(val, out long pos))
                        {
                            currentPosition = pos;
                        }
                        break;
                    case "ENA":
                        isDriverEnabled = (val == "1");
                        UpdateMasterEnableButton();
                        break;
                    case "MODE":
                        if (int.TryParse(val, out int m))
                        {
                            currentMode = m;
                            if (m == 0) rbModeCont.Checked = true;
                            else if (m == 1) rbModePos.Checked = true;
                            else if (m == 2) rbModeJog.Checked = true;
                        }
                        break;
                }
            }

            UpdateLcdSimulator();
        }

        private void UpdateMasterRunButton()
        {
            if (isMotorRunning)
            {
                btnMasterRun.Text = "⏸ DỪNG LẠI (STOP)";
                btnMasterRun.BackColor = Color.FromArgb(220, 40, 40);
            }
            else
            {
                btnMasterRun.Text = "▶ BẮT ĐẦU CHẠY (RUN)";
                btnMasterRun.BackColor = Color.FromArgb(0, 160, 80);
            }
        }

        private void UpdateMasterDirButton()
        {
            if (isDirCW)
            {
                btnMasterDir.Text = "↻ QUAY THUẬN (CW)";
                btnMasterDir.BackColor = Color.FromArgb(0, 130, 200);
            }
            else
            {
                btnMasterDir.Text = "↺ QUAY NGHỊCH (CCW)";
                btnMasterDir.BackColor = Color.FromArgb(200, 120, 0);
            }
        }

        private void UpdateMasterEnableButton()
        {
            if (isDriverEnabled)
            {
                btnMasterEnable.Text = "🔒 KHÓA TRỤC (ENA ON)";
                btnMasterEnable.BackColor = Color.FromArgb(90, 60, 150);
            }
            else
            {
                btnMasterEnable.Text = "🔓 THẢ TỰ DO (FREE OFF)";
                btnMasterEnable.BackColor = Color.FromArgb(100, 100, 110);
            }
        }

        private void UpdateLcdSimulator()
        {
            float rpm = (currentSpeedSPS / 1600.0f) * 60.0f;
            string runStr = isMotorRunning ? "RUN " : "STOP";
            string dirStr = isDirCW ? "CW " : "CCW";
            lblLcdLine1.Text = $"{runStr,-4} {rpm,4:F0}RPM {dirStr,3}";

            string modeStr = (currentMode == 0) ? "CONT" : ((currentMode == 1) ? "POS " : "JOG ");
            lblLcdLine2.Text = $"{modeStr} P:{currentPosition,+7} {(isDriverEnabled ? "ON" : "OFF")}";
        }

        private void BtnMasterRun_Click(object? sender, EventArgs e)
        {
            if (isMotorRunning)
            {
                SendCommand("MOTOR:STOP");
            }
            else
            {
                SendCommand("MOTOR:RUN");
            }
        }

        private void BtnMasterDir_Click(object? sender, EventArgs e)
        {
            if (isDirCW)
            {
                SendCommand("MOTOR:DIR:CCW");
            }
            else
            {
                SendCommand("MOTOR:DIR:CW");
            }
        }

        private void BtnMasterEnable_Click(object? sender, EventArgs e)
        {
            if (isDriverEnabled)
            {
                SendCommand("MOTOR:ENA:0");
            }
            else
            {
                SendCommand("MOTOR:ENA:1");
            }
        }

        private void UpdateDisplaySimulator(string text)
        {
            for (int i = 0; i < 8; i++) lblDigits[i].Text = " ";

            int pos = 0;
            for (int i = 0; i < text.Length && pos < 8; i++)
            {
                char c = text[i];
                if (c == '.' && pos > 0)
                {
                    if (!lblDigits[pos - 1].Text.EndsWith("."))
                    {
                        lblDigits[pos - 1].Text += ".";
                    }
                    continue;
                }

                if (i + 1 < text.Length && text[i + 1] == '.')
                {
                    lblDigits[pos].Text = c.ToString() + ".";
                    i++;
                }
                else
                {
                    lblDigits[pos].Text = c.ToString();
                }
                pos++;
            }
        }

        private void UpdateLedsUI(byte mask)
        {
            string[] ledNames = { "RUN", "DIR", "ENA", "SPD1", "SPD2", "SPD3", "SPD4", "SPD5" };
            for (int i = 0; i < 8; i++)
            {
                bool isOn = (mask & (1 << i)) != 0;
                btnLeds[i].Text = $"{ledNames[i]}\n{(isOn ? "ON" : "OFF")}";
                btnLeds[i].BackColor = isOn ? Color.FromArgb(220, 30, 30) : Color.FromArgb(45, 48, 58);
                btnLeds[i].ForeColor = isOn ? Color.White : Color.Gray;
            }
        }

        private void UpdateButtonsUI(byte mask)
        {
            for (int i = 0; i < 8; i++)
            {
                bool isPressed = (mask & (1 << i)) != 0;
                lblButtons[i].Text = $"S{i + 1}\n{(isPressed ? "ON" : "OFF")}";
                lblButtons[i].BackColor = isPressed ? Color.FromArgb(255, 190, 0) : Color.FromArgb(35, 40, 52);
                lblButtons[i].ForeColor = isPressed ? Color.Black : Color.FromArgb(130, 145, 165);
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
                LogMessage($"[CHƯA KẾT NỐI] Không thể gửi lệnh '{cmd}'");
            }
        }

        private void LogMessage(string text)
        {
            if (rtbLog.IsDisposed) return;
            string time = DateTime.Now.ToString("HH:mm:ss");
            rtbLog.AppendText($"[{time}] {text}\n");
            rtbLog.SelectionStart = rtbLog.Text.Length;
            rtbLog.ScrollToCaret();
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            DisconnectSerial();
            base.OnFormClosing(e);
        }
    }
}
