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

        // Trạng thái 2 Động cơ
        private bool isRunM1 = false, isDirCW1 = true, isEna1 = true;
        private uint speedSPS1 = 1600;
        private long pos1 = 0;

        private bool isRunM2 = false, isDirCW2 = true, isEna2 = true;
        private uint speedSPS2 = 1600;
        private long pos2 = 0;

        private int selectedTarget = 0; // 0: M1, 1: M2, 2: BOTH
        private int currentMode = 0;    // 0: CONT, 1: POS, 2: JOG

        private byte currentLedMask = 0x00;
        private byte currentButtonMask = 0x00;

        // UI Controls - Connection
        private ComboBox cmbPorts = null!;
        private Button btnRefreshPorts = null!;
        private Button btnConnect = null!;
        private Label lblStatus = null!;
        private Panel pnlStatusIndicator = null!;

        // UI Controls - Motor 1 & Motor 2
        private Button btnRunM1 = null!, btnDirM1 = null!;
        private TrackBar tbSpeedM1 = null!;
        private Label lblSpeedValM1 = null!;
        private TextBox txtMoveM1 = null!;

        private Button btnRunM2 = null!, btnDirM2 = null!;
        private TrackBar tbSpeedM2 = null!;
        private Label lblSpeedValM2 = null!;
        private TextBox txtMoveM2 = null!;

        private Button btnRunAll = null!, btnStopAll = null!, btnZeroAll = null!;
        private RadioButton rbSelM1 = null!, rbSelM2 = null!, rbSelBoth = null!;

        // Simulator
        private Label lblLcdLine1 = null!, lblLcdLine2 = null!;
        private Label[] lblDigits = new Label[8];
        private Button[] btnLeds = new Button[8];
        private Label[] lblButtons = new Label[8];

        private RichTextBox rtbLog = null!;
        private TextBox txtCustomCmd = null!;

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
            this.Text = "ESP32-S3 Dual Motor Controller Dashboard - Nguyễn Văn Lân";
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.ClientSize = new Size(1040, 800);
            this.BackColor = Color.FromArgb(20, 23, 30);

            // 1. HEADER
            Panel pnlHeader = new Panel
            {
                Location = new Point(0, 0),
                Size = new Size(1040, 65),
                BackColor = Color.FromArgb(28, 32, 44)
            };
            Label lblTitle = new Label
            {
                Text = "⚡ ESP32-S3 DUAL MOTOR (M1 + M2) HYBRID SERVO DASHBOARD",
                Font = new Font("Segoe UI", 12.5f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 220, 255),
                Location = new Point(20, 10),
                AutoSize = true
            };
            Label lblSub = new Label
            {
                Text = "Điều Khiển Độc Lập & Đồng Thời 2 Động Cơ Bước JMC-2HSS57 + TM1638 + LCD I2C | Nguyễn Văn Lân",
                Font = new Font("Segoe UI", 8.5f, FontStyle.Regular),
                ForeColor = Color.FromArgb(160, 175, 195),
                Location = new Point(22, 38),
                AutoSize = true
            };
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSub);
            this.Controls.Add(pnlHeader);

            // 2. KẾT NỐI SERIAL
            GroupBox gbConnect = CreateCard("1. KẾT NỐI CỔNG COM", new Point(15, 75), new Size(1010, 65));
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

            Button btnSync = CreateButton("Đồng Bộ", new Point(440, 22), new Size(85, 30), Color.FromArgb(70, 80, 105));
            btnSync.Click += (s, e) => SendCommand("SYNC");

            pnlStatusIndicator = new Panel { Location = new Point(545, 29), Size = new Size(15, 15), BackColor = Color.Red };
            lblStatus = new Label { Text = "Chưa kết nối", Location = new Point(568, 27), AutoSize = true, ForeColor = Color.FromArgb(220, 100, 100) };

            gbConnect.Controls.AddRange(new Control[] { lblPort, cmbPorts, btnRefreshPorts, btnConnect, btnSync, pnlStatusIndicator, lblStatus });
            this.Controls.Add(gbConnect);

            // 3. ĐIỀU KHIỂN ĐỘNG CƠ 1 (M1)
            GroupBox gbM1 = CreateCard("2. ĐIỀU KHIỂN ĐỘNG CƠ 1 (MOTOR 1 - M1)", new Point(15, 148), new Size(495, 240));

            btnRunM1 = CreateButton("▶ CHẠY M1", new Point(20, 28), new Size(140, 42), Color.FromArgb(0, 160, 80));
            btnRunM1.Click += (s, e) => SendCommand(isRunM1 ? "M1:STOP" : "M1:RUN");

            btnDirM1 = CreateButton("↻ THUẬN (CW)", new Point(170, 28), new Size(140, 42), Color.FromArgb(0, 130, 200));
            btnDirM1.Click += (s, e) => SendCommand(isDirCW1 ? "M1:DIR:CCW" : "M1:DIR:CW");

            Button btnZeroM1 = CreateButton("↺ RESET M1", new Point(320, 28), new Size(155, 42), Color.FromArgb(140, 70, 30));
            btnZeroM1.Click += (s, e) => SendCommand("M1:MOVE:0");

            lblSpeedValM1 = new Label { Text = "Tốc độ M1: 1600 SPS (~ 60 RPM)", Location = new Point(20, 78), AutoSize = true, ForeColor = Color.Yellow, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            tbSpeedM1 = new TrackBar { Minimum = 200, Maximum = 16000, Value = 1600, TickFrequency = 1000, Location = new Point(15, 98), Size = new Size(460, 45) };
            tbSpeedM1.Scroll += (s, e) => { float rpm = (tbSpeedM1.Value / 1600.0f) * 60.0f; lblSpeedValM1.Text = $"Tốc độ M1: {tbSpeedM1.Value} SPS (~ {rpm:F0} RPM)"; };
            tbSpeedM1.MouseUp += (s, e) => SendCommand($"M1:SPEED:{tbSpeedM1.Value}");

            Label lblMove1 = new Label { Text = "Quay M1:", Location = new Point(20, 148), AutoSize = true };
            txtMoveM1 = new TextBox { Text = "1600", Location = new Point(90, 146), Size = new Size(70, 26), BackColor = Color.FromArgb(40, 44, 58), ForeColor = Color.White };
            Button btnStepM1 = CreateButton("Chạy Bước", new Point(170, 144), new Size(95, 28), Color.FromArgb(0, 140, 190));
            btnStepM1.Click += (s, e) => { if (int.TryParse(txtMoveM1.Text, out int st)) SendCommand($"M1:MOVE:{st}"); };

            Button btnRev1M1 = CreateButton("+1 Vòng", new Point(275, 144), new Size(90, 28), Color.FromArgb(60, 70, 90));
            btnRev1M1.Click += (s, e) => SendCommand("M1:MOVE:1600");

            Button btnRev5M1 = CreateButton("+5 Vòng", new Point(375, 144), new Size(100, 28), Color.FromArgb(60, 70, 90));
            btnRev5M1.Click += (s, e) => SendCommand("M1:MOVE:8000");

            gbM1.Controls.AddRange(new Control[] { btnRunM1, btnDirM1, btnZeroM1, lblSpeedValM1, tbSpeedM1, lblMove1, txtMoveM1, btnStepM1, btnRev1M1, btnRev5M1 });
            this.Controls.Add(gbM1);

            // 4. ĐIỀU KHIỂN ĐỘNG CƠ 2 (M2)
            GroupBox gbM2 = CreateCard("3. ĐIỀU KHIỂN ĐỘNG CƠ 2 (MOTOR 2 - M2)", new Point(530, 148), new Size(495, 240));

            btnRunM2 = CreateButton("▶ CHẠY M2", new Point(20, 28), new Size(140, 42), Color.FromArgb(0, 160, 80));
            btnRunM2.Click += (s, e) => SendCommand(isRunM2 ? "M2:STOP" : "M2:RUN");

            btnDirM2 = CreateButton("↻ THUẬN (CW)", new Point(170, 28), new Size(140, 42), Color.FromArgb(0, 130, 200));
            btnDirM2.Click += (s, e) => SendCommand(isDirCW2 ? "M2:DIR:CCW" : "M2:DIR:CW");

            Button btnZeroM2 = CreateButton("↺ RESET M2", new Point(320, 28), new Size(155, 42), Color.FromArgb(140, 70, 30));
            btnZeroM2.Click += (s, e) => SendCommand("M2:MOVE:0");

            lblSpeedValM2 = new Label { Text = "Tốc độ M2: 1600 SPS (~ 60 RPM)", Location = new Point(20, 78), AutoSize = true, ForeColor = Color.Yellow, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            tbSpeedM2 = new TrackBar { Minimum = 200, Maximum = 16000, Value = 1600, TickFrequency = 1000, Location = new Point(15, 98), Size = new Size(460, 45) };
            tbSpeedM2.Scroll += (s, e) => { float rpm = (tbSpeedM2.Value / 1600.0f) * 60.0f; lblSpeedValM2.Text = $"Tốc độ M2: {tbSpeedM2.Value} SPS (~ {rpm:F0} RPM)"; };
            tbSpeedM2.MouseUp += (s, e) => SendCommand($"M2:SPEED:{tbSpeedM2.Value}");

            Label lblMove2 = new Label { Text = "Quay M2:", Location = new Point(20, 148), AutoSize = true };
            txtMoveM2 = new TextBox { Text = "1600", Location = new Point(90, 146), Size = new Size(70, 26), BackColor = Color.FromArgb(40, 44, 58), ForeColor = Color.White };
            Button btnStepM2 = CreateButton("Chạy Bước", new Point(170, 144), new Size(95, 28), Color.FromArgb(0, 140, 190));
            btnStepM2.Click += (s, e) => { if (int.TryParse(txtMoveM2.Text, out int st)) SendCommand($"M2:MOVE:{st}"); };

            Button btnRev1M2 = CreateButton("+1 Vòng", new Point(275, 144), new Size(90, 28), Color.FromArgb(60, 70, 90));
            btnRev1M2.Click += (s, e) => SendCommand("M2:MOVE:1600");

            Button btnRev5M2 = CreateButton("+5 Vòng", new Point(375, 144), new Size(100, 28), Color.FromArgb(60, 70, 90));
            btnRev5M2.Click += (s, e) => SendCommand("M2:MOVE:8000");

            gbM2.Controls.AddRange(new Control[] { btnRunM2, btnDirM2, btnZeroM2, lblSpeedValM2, tbSpeedM2, lblMove2, txtMoveM2, btnStepM2, btnRev1M2, btnRev5M2 });
            this.Controls.Add(gbM2);

            // 5. ĐIỀU KHIỂN DUAL & MỤC TIÊU TM1638
            GroupBox gbMasterAll = CreateCard("4. ĐIỀU KHIỂN TỔNG & CHỌN ĐỘNG CƠ CỦA TM1638", new Point(15, 395), new Size(1010, 65));

            btnRunAll = CreateButton("▶ CHẠY CẢ 2 MOTOR", new Point(20, 22), new Size(185, 32), Color.FromArgb(0, 160, 80));
            btnRunAll.Click += (s, e) => SendCommand("MOTOR:RUN_ALL");

            btnStopAll = CreateButton("⏸ DỪNG CẢ 2 MOTOR", new Point(215, 22), new Size(185, 32), Color.FromArgb(200, 40, 40));
            btnStopAll.Click += (s, e) => SendCommand("MOTOR:STOP_ALL");

            btnZeroAll = CreateButton("↺ RESET CẢ 2 VỀ 0", new Point(410, 22), new Size(160, 32), Color.FromArgb(150, 70, 30));
            btnZeroAll.Click += (s, e) => SendCommand("MOTOR:ZERO_ALL");

            Label lblSelTag = new Label { Text = "Mục tiêu TM1638:", Location = new Point(590, 27), AutoSize = true, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            rbSelM1 = new RadioButton { Text = "M1", Location = new Point(710, 26), AutoSize = true, Checked = true };
            rbSelM2 = new RadioButton { Text = "M2", Location = new Point(765, 26), AutoSize = true };
            rbSelBoth = new RadioButton { Text = "Cả 2 (BOTH)", Location = new Point(820, 26), AutoSize = true };

            rbSelM1.CheckedChanged += (s, e) => { if (rbSelM1.Checked) SendCommand("MOTOR:SEL:0"); };
            rbSelM2.CheckedChanged += (s, e) => { if (rbSelM2.Checked) SendCommand("MOTOR:SEL:1"); };
            rbSelBoth.CheckedChanged += (s, e) => { if (rbSelBoth.Checked) SendCommand("MOTOR:SEL:2"); };

            gbMasterAll.Controls.AddRange(new Control[] { btnRunAll, btnStopAll, btnZeroAll, lblSelTag, rbSelM1, rbSelM2, rbSelBoth });
            this.Controls.Add(gbMasterAll);

            // 6. SIMULATOR MÀN HÌNH HARDWARE
            GroupBox gbSim = CreateCard("5. MÔ PHỎNG MÀN HÌNH PHẦN CỨNG (LCD 1602 & TM1638)", new Point(15, 468), new Size(1010, 145));

            Panel pnlLcd = new Panel { Location = new Point(20, 25), Size = new Size(390, 75), BackColor = Color.FromArgb(10, 45, 60), BorderStyle = BorderStyle.Fixed3D };
            lblLcdLine1 = new Label { Text = "M1:STOP   60CW  *", Font = new Font("Consolas", 13f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 230), Location = new Point(10, 10), AutoSize = true };
            lblLcdLine2 = new Label { Text = "M2:STOP  120CCW  ", Font = new Font("Consolas", 13f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 230), Location = new Point(10, 38), AutoSize = true };
            pnlLcd.Controls.Add(lblLcdLine1); pnlLcd.Controls.Add(lblLcdLine2);
            gbSim.Controls.Add(pnlLcd);

            Panel pnl7Seg = new Panel { Location = new Point(430, 25), Size = new Size(560, 75), BackColor = Color.Black, BorderStyle = BorderStyle.FixedSingle };
            for (int i = 0; i < 8; i++)
            {
                lblDigits[i] = new Label { Text = " ", Font = new Font("Consolas", 22f, FontStyle.Bold), ForeColor = Color.Lime, Size = new Size(63, 58), Location = new Point(6 + i * 69, 7), TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.FromArgb(12, 18, 12) };
                pnl7Seg.Controls.Add(lblDigits[i]);
            }
            gbSim.Controls.Add(pnl7Seg);

            Label lblLcdTag = new Label { Text = "Màn hình LCD 1602 (I2C)", Location = new Point(20, 108), AutoSize = true, ForeColor = Color.FromArgb(160, 180, 200) };
            Label lbl7SegTag = new Label { Text = "Màn hình 8 LED 7 đoạn TM1638", Location = new Point(430, 108), AutoSize = true, ForeColor = Color.FromArgb(160, 180, 200) };
            gbSim.Controls.AddRange(new Control[] { lblLcdTag, lbl7SegTag });
            this.Controls.Add(gbSim);

            // 7. GIÁM SÁT LED & PHÍM TM1638
            GroupBox gbHardware = CreateCard("6. GIÁM SÁT 8 ĐÈN LED & 8 NÚT BẤM TM1638", new Point(15, 620), new Size(1010, 90));
            string[] ledNames = { "M1:RUN", "M2:RUN", "M1:CW", "M2:CW", "TARGET", "SPD1", "SPD2", "SPD3" };
            for (int i = 0; i < 8; i++)
            {
                btnLeds[i] = new Button { Text = $"{ledNames[i]}\nOFF", Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), Size = new Size(54, 42), Location = new Point(20 + i * 58, 24), BackColor = Color.FromArgb(45, 48, 58), ForeColor = Color.Gray, FlatStyle = FlatStyle.Flat };
                gbHardware.Controls.Add(btnLeds[i]);
            }

            Label lblSep = new Label { BorderStyle = BorderStyle.Fixed3D, Location = new Point(495, 18), Size = new Size(2, 55) };
            gbHardware.Controls.Add(lblSep);

            string[] btnNames = { "S1:RUN", "S2:SEL", "S3:SPD+", "S4:SPD-", "S5:M1DIR", "S6:M2DIR", "S7:MODE", "S8:ZERO" };
            for (int i = 0; i < 8; i++)
            {
                lblButtons[i] = new Label { Text = btnNames[i], Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), Size = new Size(55, 42), Location = new Point(515 + i * 59, 24), TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.FromArgb(35, 40, 52), ForeColor = Color.FromArgb(130, 145, 165), BorderStyle = BorderStyle.FixedSingle };
                gbHardware.Controls.Add(lblButtons[i]);
            }
            this.Controls.Add(gbHardware);

            // 8. LOG
            GroupBox gbLog = CreateCard("7. NHẬT KÝ SERIAL", new Point(15, 715), new Size(1010, 75));
            rtbLog = new RichTextBox { Location = new Point(15, 20), Size = new Size(650, 48), BackColor = Color.FromArgb(12, 15, 20), ForeColor = Color.FromArgb(0, 255, 170), Font = new Font("Consolas", 8.5f), ReadOnly = true };
            gbLog.Controls.Add(rtbLog);

            txtCustomCmd = new TextBox { Location = new Point(680, 30), Size = new Size(220, 26), BackColor = Color.FromArgb(40, 44, 58), ForeColor = Color.White, Text = "MOTOR:RUN_ALL" };
            Button btnSendCmd = CreateButton("GỬI LỆNH", new Point(910, 28), new Size(85, 28), Color.FromArgb(0, 120, 215));
            btnSendCmd.Click += (s, e) => SendCommand(txtCustomCmd.Text);
            gbLog.Controls.AddRange(new Control[] { txtCustomCmd, btnSendCmd });
            this.Controls.Add(gbLog);
        }

        private GroupBox CreateCard(string title, Point location, Size size)
        {
            return new GroupBox { Text = title, Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 210, 255), Location = location, Size = size, BackColor = Color.FromArgb(28, 32, 42) };
        }

        private Button CreateButton(string text, Point loc, Size size, Color bg)
        {
            var btn = new Button { Text = text, Location = loc, Size = size, BackColor = bg, ForeColor = Color.White, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand, Font = new Font("Segoe UI", 8.5f, FontStyle.Bold) };
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
            if (serialPort != null && serialPort.IsOpen) DisconnectSerial();
            else
            {
                if (cmbPorts.SelectedItem == null) return;
                string portName = cmbPorts.SelectedItem.ToString() ?? "COM7";
                try
                {
                    serialPort = new SerialPort(portName, 115200, Parity.None, 8, StopBits.One) { ReadTimeout = 500, WriteTimeout = 500 };
                    serialPort.DataReceived += SerialPort_DataReceived;
                    serialPort.Open();

                    btnConnect.Text = "NGẮT KẾT NỐI"; btnConnect.BackColor = Color.FromArgb(180, 50, 50);
                    pnlStatusIndicator.BackColor = Color.Lime;
                    lblStatus.Text = $"Đã kết nối {portName}"; lblStatus.ForeColor = Color.Lime;
                    LogMessage($"Đã mở cổng {portName} thành công.");
                    SendCommand("SYNC");
                }
                catch (Exception ex) { MessageBox.Show(ex.Message, "Lỗi COM", MessageBoxButtons.OK, MessageBoxIcon.Error); }
            }
        }

        private void DisconnectSerial()
        {
            try { if (serialPort != null) { if (serialPort.IsOpen) serialPort.Close(); serialPort.Dispose(); serialPort = null; } } catch { }
            btnConnect.Text = "KẾT NỐI"; btnConnect.BackColor = Color.FromArgb(0, 160, 90);
            pnlStatusIndicator.BackColor = Color.Red; lblStatus.Text = "Đã ngắt kết nối"; lblStatus.ForeColor = Color.FromArgb(220, 100, 100);
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
            if (msg.StartsWith("DUALSTAT:")) ParseDualStatus(msg.Substring(9));
            else if (msg.StartsWith("BTN:")) UpdateButtonsUI(byte.Parse(msg.Substring(4).Trim(), System.Globalization.NumberStyles.HexNumber));
            else if (msg.StartsWith("DISP:")) UpdateDisplaySimulator(msg.Substring(5));
            else if (msg.StartsWith("LEDS:")) UpdateLedsUI(byte.Parse(msg.Substring(5).Trim(), System.Globalization.NumberStyles.HexNumber));
            else LogMessage($"[ESP32] {msg}");
        }

        private void ParseDualStatus(string data)
        {
            var parts = data.Split(',');
            foreach (var part in parts)
            {
                var kv = part.Split('='); if (kv.Length != 2) continue;
                string k = kv[0].Trim(), v = kv[1].Trim();
                switch (k)
                {
                    case "M1_RUN": isRunM1 = (v == "1"); UpdateM1UI(); break;
                    case "M1_DIR": isDirCW1 = (v == "CW"); UpdateM1UI(); break;
                    case "M1_SPD": if (uint.TryParse(v, out uint s1)) { speedSPS1 = s1; if (s1 >= tbSpeedM1.Minimum && s1 <= tbSpeedM1.Maximum) tbSpeedM1.Value = (int)s1; } break;
                    case "M1_POS": if (long.TryParse(v, out long p1)) pos1 = p1; break;

                    case "M2_RUN": isRunM2 = (v == "1"); UpdateM2UI(); break;
                    case "M2_DIR": isDirCW2 = (v == "CW"); UpdateM2UI(); break;
                    case "M2_SPD": if (uint.TryParse(v, out uint s2)) { speedSPS2 = s2; if (s2 >= tbSpeedM2.Minimum && s2 <= tbSpeedM2.Maximum) tbSpeedM2.Value = (int)s2; } break;
                    case "M2_POS": if (long.TryParse(v, out long p2)) pos2 = p2; break;

                    case "SEL":
                        if (int.TryParse(v, out int sel))
                        {
                            selectedTarget = sel;
                            if (sel == 0) rbSelM1.Checked = true;
                            else if (sel == 1) rbSelM2.Checked = true;
                            else rbSelBoth.Checked = true;
                        }
                        break;
                }
            }

            UpdateLcdSimulator();
        }

        private void UpdateM1UI()
        {
            btnRunM1.Text = isRunM1 ? "⏸ DỪNG M1" : "▶ CHẠY M1";
            btnRunM1.BackColor = isRunM1 ? Color.FromArgb(220, 40, 40) : Color.FromArgb(0, 160, 80);
            btnDirM1.Text = isDirCW1 ? "↻ THUẬN (CW)" : "↺ NGHỊCH (CCW)";
            btnDirM1.BackColor = isDirCW1 ? Color.FromArgb(0, 130, 200) : Color.FromArgb(200, 120, 0);
        }

        private void UpdateM2UI()
        {
            btnRunM2.Text = isRunM2 ? "⏸ DỪNG M2" : "▶ CHẠY M2";
            btnRunM2.BackColor = isRunM2 ? Color.FromArgb(220, 40, 40) : Color.FromArgb(0, 160, 80);
            btnDirM2.Text = isDirCW2 ? "↻ THUẬN (CW)" : "↺ NGHỊCH (CCW)";
            btnDirM2.BackColor = isDirCW2 ? Color.FromArgb(0, 130, 200) : Color.FromArgb(200, 120, 0);
        }

        private void UpdateLcdSimulator()
        {
            float rpm1 = (speedSPS1 / 1600.0f) * 60.0f;
            float rpm2 = (speedSPS2 / 1600.0f) * 60.0f;
            lblLcdLine1.Text = $"M1:{(isRunM1 ? "RUN " : "STOP")}{rpm1,4:F0}{(isDirCW1 ? "CW " : "CCW")} {(selectedTarget != 1 ? '*' : ' ')}";
            lblLcdLine2.Text = $"M2:{(isRunM2 ? "RUN " : "STOP")}{rpm2,4:F0}{(isDirCW2 ? "CW " : "CCW")} {(selectedTarget != 0 ? '*' : ' ')}";
        }

        private void UpdateDisplaySimulator(string text)
        {
            for (int i = 0; i < 8; i++) lblDigits[i].Text = " ";
            int pos = 0;
            for (int i = 0; i < text.Length && pos < 8; i++)
            {
                char c = text[i];
                if (c == '.' && pos > 0) { if (!lblDigits[pos - 1].Text.EndsWith(".")) lblDigits[pos - 1].Text += "."; continue; }
                if (i + 1 < text.Length && text[i + 1] == '.') { lblDigits[pos].Text = c.ToString() + "."; i++; }
                else lblDigits[pos].Text = c.ToString();
                pos++;
            }
        }

        private void UpdateLedsUI(byte mask)
        {
            string[] ledNames = { "M1:RUN", "M2:RUN", "M1:CW", "M2:CW", "TARGET", "SPD1", "SPD2", "SPD3" };
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
            string[] btnNames = { "S1:RUN", "S2:SEL", "S3:SPD+", "S4:SPD-", "S5:M1DIR", "S6:M2DIR", "S7:MODE", "S8:ZERO" };
            for (int i = 0; i < 8; i++)
            {
                bool isPressed = (mask & (1 << i)) != 0;
                lblButtons[i].Text = $"{btnNames[i]}\n{(isPressed ? "ON" : "OFF")}";
                lblButtons[i].BackColor = isPressed ? Color.FromArgb(255, 190, 0) : Color.FromArgb(35, 40, 52);
                lblButtons[i].ForeColor = isPressed ? Color.Black : Color.FromArgb(130, 145, 165);
            }
        }

        public void SendCommand(string cmd)
        {
            if (serialPort != null && serialPort.IsOpen)
            {
                try { serialPort.WriteLine(cmd); LogMessage($"[GỬI] {cmd}"); }
                catch (Exception ex) { LogMessage($"[LỖI] {ex.Message}"); }
            }
        }

        private void LogMessage(string text)
        {
            if (rtbLog.IsDisposed) return;
            rtbLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {text}\n");
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
