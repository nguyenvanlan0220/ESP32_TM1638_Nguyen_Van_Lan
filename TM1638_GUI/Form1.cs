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
        private StringBuilder rxBuffer = new StringBuilder();
        private readonly object lockObj = new object();

        // Cấu trúc trạng thái cho 4 Động cơ
        private class MotorUIState
        {
            public bool IsRunning = false;
            public bool IsDirCW = true;
            public bool IsEnabled = true;
            public uint SpeedSPS = 1600;
            public long CurrentPos = 0;

            public bool UI_Dirty = true; // Cờ báo cần vẽ lại UI

            public Button BtnRun = null!;
            public Button BtnDir = null!;
            public Button BtnEna = null!;
            public TrackBar TbSpeed = null!;
            public Label LblSpeedVal = null!;
            public Label LblPos = null!;
            public TextBox TxtMove = null!;
        }

        private MotorUIState[] motors = new MotorUIState[4];

        private int selectedTarget = 0; // 0: M1, 1: M2, 2: M3, 3: M4, 4: ALL
        private int currentMode = 0;    // 0: CONT, 1: POS, 2: JOG

        private byte currentLedMask = 0x00;
        private byte currentButtonMask = 0x00;
        private string current7SegText = "        ";

        private bool stateDirty = true;

        // UI Controls - Connection
        private ComboBox cmbPorts = null!;
        private Button btnRefreshPorts = null!;
        private Button btnConnect = null!;
        private Label lblStatus = null!;
        private Panel pnlStatusIndicator = null!;

        // Master Control
        private Button btnRunAll = null!, btnStopAll = null!, btnZeroAll = null!;
        private RadioButton[] rbTargets = new RadioButton[5];
        private RadioButton[] rbModes = new RadioButton[3];

        // Hardware Simulator
        private Label lblLcdLine1 = null!, lblLcdLine2 = null!;
        private Label[] lblDigits = new Label[8];
        private Button[] btnLeds = new Button[8];
        private Label[] lblButtons = new Label[8];

        private RichTextBox rtbLog = null!;
        private TextBox txtCustomCmd = null!;

        // Timer Render UI mượt mà 20 FPS (50ms)
        private System.Windows.Forms.Timer uiRenderTimer = null!;

        public Form1()
        {
            InitializeComponent();
            this.DoubleBuffered = true;
            this.SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint, true);

            for (int i = 0; i < 4; i++)
            {
                motors[i] = new MotorUIState();
            }
            SetupCustomUI();
            RefreshComPorts();

            // Khởi tạo Timer Render UI
            uiRenderTimer = new System.Windows.Forms.Timer();
            uiRenderTimer.Interval = 50; // 20 FPS refresh mượt mà
            uiRenderTimer.Tick += UiRenderTimer_Tick;
            uiRenderTimer.Start();
        }

        private void SetupCustomUI()
        {
            this.Font = new Font("Segoe UI", 9f, FontStyle.Regular);
            this.ForeColor = Color.White;
            this.Text = "ESP32-S3 QUAD & DUAL MOTOR CONTROL DASHBOARD - NGUYỄN VĂN LÂN";
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.ClientSize = new Size(1180, 940);
            this.BackColor = Color.FromArgb(16, 19, 26);

            // 1. HEADER PANEL
            Panel pnlHeader = new Panel
            {
                Location = new Point(0, 0),
                Size = new Size(1180, 65),
                BackColor = Color.FromArgb(24, 29, 42)
            };
            Label lblTitle = new Label
            {
                Text = "⚡ ESP32-S3 QUAD & DUAL MOTOR HYBRID SERVO DASHBOARD",
                Font = new Font("Segoe UI", 13f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 225, 255),
                Location = new Point(20, 10),
                AutoSize = true
            };
            Label lblSub = new Label
            {
                Text = "Hệ thống điều khiển Đa Động Cơ JMC 2HSS57 + Module TM1638 + LCD 1602 I2C | Tác giả: Nguyễn Văn Lân",
                Font = new Font("Segoe UI", 8.5f, FontStyle.Regular),
                ForeColor = Color.FromArgb(150, 170, 195),
                Location = new Point(22, 38),
                AutoSize = true
            };
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSub);
            this.Controls.Add(pnlHeader);

            // 2. KẾT NỐI SERIAL
            GroupBox gbConnect = CreateCard("1. KẾT NỐI CỔNG COM / SERIAL", new Point(15, 75), new Size(1150, 65));
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

            // 3. ĐIỀU KHIỂN 4 ĐỘNG CƠ (2x2 GRID)
            int cardW = 568;
            int cardH = 220;
            Point[] cardLocs = new Point[] {
                new Point(15, 148),
                new Point(597, 148),
                new Point(15, 376),
                new Point(597, 376)
            };

            for (int i = 0; i < 4; i++)
            {
                int motorIdx = i;
                GroupBox gbM = CreateCard($"2.{i + 1} ĐIỀU KHIỂN ĐỘNG CƠ {i + 1} (MOTOR {i + 1} - M{i + 1})", cardLocs[i], new Size(cardW, cardH));

                motors[i].BtnRun = CreateButton($"▶ CHẠY M{i + 1}", new Point(15, 25), new Size(125, 36), Color.FromArgb(0, 160, 80));
                motors[i].BtnRun.Click += (s, e) => SendCommand(motors[motorIdx].IsRunning ? $"M{motorIdx + 1}:STOP" : $"M{motorIdx + 1}:RUN");

                motors[i].BtnDir = CreateButton("↻ THUẬN (CW)", new Point(148, 25), new Size(130, 36), Color.FromArgb(0, 130, 200));
                motors[i].BtnDir.Click += (s, e) => SendCommand(motors[motorIdx].IsDirCW ? $"M{motorIdx + 1}:DIR:CCW" : $"M{motorIdx + 1}:DIR:CW");

                motors[i].BtnEna = CreateButton("🔒 KHÓA TRỤC", new Point(286, 25), new Size(130, 36), Color.FromArgb(100, 50, 150));
                motors[i].BtnEna.Click += (s, e) => SendCommand(motors[motorIdx].IsEnabled ? $"M{motorIdx + 1}:DISABLE" : $"M{motorIdx + 1}:ENABLE");

                Button btnZeroM = CreateButton("↺ RESET 0", new Point(424, 25), new Size(128, 36), Color.FromArgb(150, 70, 30));
                btnZeroM.Click += (s, e) => SendCommand($"M{motorIdx + 1}:MOVE:0");

                motors[i].LblSpeedVal = new Label { Text = $"Tốc độ M{i + 1}: 1600 SPS (~ 60 RPM)", Location = new Point(15, 68), AutoSize = true, ForeColor = Color.Yellow, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
                motors[i].LblPos = new Label { Text = "Vị trí: 0 xung", Location = new Point(340, 68), AutoSize = true, ForeColor = Color.FromArgb(0, 225, 255), Font = new Font("Segoe UI", 9f, FontStyle.Bold) };

                motors[i].TbSpeed = new TrackBar { Minimum = 200, Maximum = 16000, Value = 1600, TickFrequency = 1000, Location = new Point(10, 88), Size = new Size(545, 45) };
                motors[i].TbSpeed.Scroll += (s, e) => {
                    float rpm = (motors[motorIdx].TbSpeed.Value / 1600.0f) * 60.0f;
                    motors[motorIdx].LblSpeedVal.Text = $"Tốc độ M{motorIdx + 1}: {motors[motorIdx].TbSpeed.Value} SPS (~ {rpm:F0} RPM)";
                };
                motors[i].TbSpeed.MouseUp += (s, e) => SendCommand($"M{motorIdx + 1}:SPEED:{motors[motorIdx].TbSpeed.Value}");

                Label lblMove = new Label { Text = "Bước xung:", Location = new Point(15, 138), AutoSize = true };
                motors[i].TxtMove = new TextBox { Text = "1600", Location = new Point(90, 135), Size = new Size(80, 26), BackColor = Color.FromArgb(36, 42, 56), ForeColor = Color.White };
                
                Button btnStep = CreateButton("Chạy Bước", new Point(180, 134), new Size(105, 28), Color.FromArgb(0, 140, 190));
                btnStep.Click += (s, e) => { if (int.TryParse(motors[motorIdx].TxtMove.Text, out int st)) SendCommand($"M{motorIdx + 1}:MOVE:{st}"); };

                Button btnRev1 = CreateButton("+1 Vòng", new Point(295, 134), new Size(120, 28), Color.FromArgb(55, 65, 85));
                btnRev1.Click += (s, e) => SendCommand($"M{motorIdx + 1}:MOVE:1600");

                Button btnRev5 = CreateButton("+5 Vòng", new Point(425, 134), new Size(127, 28), Color.FromArgb(55, 65, 85));
                btnRev5.Click += (s, e) => SendCommand($"M{motorIdx + 1}:MOVE:8000");

                gbM.Controls.AddRange(new Control[] {
                    motors[i].BtnRun, motors[i].BtnDir, motors[i].BtnEna, btnZeroM,
                    motors[i].LblSpeedVal, motors[i].LblPos, motors[i].TbSpeed,
                    lblMove, motors[i].TxtMove, btnStep, btnRev1, btnRev5
                });
                this.Controls.Add(gbM);
            }

            // 4. ĐIỀU KHIỂN TỔNG & CẤU HÌNH MODE / TARGET
            GroupBox gbMaster = CreateCard("3. ĐIỀU KHIỂN TỔNG & CẤU HÌNH TM1638", new Point(15, 604), new Size(1150, 68));

            btnRunAll = CreateButton("▶ CHẠY TẤT CẢ", new Point(15, 22), new Size(150, 34), Color.FromArgb(0, 160, 80));
            btnRunAll.Click += (s, e) => SendCommand("MOTOR:RUN_ALL");

            btnStopAll = CreateButton("⏸ DỪNG TẤT CẢ", new Point(175, 22), new Size(150, 34), Color.FromArgb(200, 40, 40));
            btnStopAll.Click += (s, e) => SendCommand("MOTOR:STOP_ALL");

            btnZeroAll = CreateButton("↺ RESET TẤT CẢ VỀ 0", new Point(335, 22), new Size(170, 34), Color.FromArgb(150, 70, 30));
            btnZeroAll.Click += (s, e) => SendCommand("MOTOR:ZERO_ALL");

            Label lblSelTag = new Label { Text = "Mục tiêu TM1638:", Location = new Point(520, 29), AutoSize = true, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            string[] tgtNames = { "M1", "M2", "M3", "M4", "Cả 4 (ALL)" };
            int posX = 645;
            for (int i = 0; i < 5; i++)
            {
                int tgtIdx = i;
                rbTargets[i] = new RadioButton { Text = tgtNames[i], Location = new Point(posX, 27), AutoSize = true, Checked = (i == 0) };
                rbTargets[i].CheckedChanged += (s, e) => { if (rbTargets[tgtIdx].Checked) SendCommand($"MOTOR:SEL:{tgtIdx}"); };
                gbMaster.Controls.Add(rbTargets[i]);
                posX += (i == 4) ? 100 : 55;
            }

            Label lblModeTag = new Label { Text = "Chế độ:", Location = new Point(950, 29), AutoSize = true, Font = new Font("Segoe UI", 9f, FontStyle.Bold) };
            string[] modeNames = { "CONT", "POS", "JOG" };
            posX = 1005;
            for (int i = 0; i < 3; i++)
            {
                int modeIdx = i;
                rbModes[i] = new RadioButton { Text = modeNames[i], Location = new Point(posX, 27), AutoSize = true, Checked = (i == 0) };
                rbModes[i].CheckedChanged += (s, e) => { if (rbModes[modeIdx].Checked) SendCommand($"MOTOR:MODE:{modeIdx}"); };
                gbMaster.Controls.Add(rbModes[i]);
                posX += 45;
            }

            gbMaster.Controls.AddRange(new Control[] { btnRunAll, btnStopAll, btnZeroAll, lblSelTag, lblModeTag });
            this.Controls.Add(gbMaster);

            // 5. MÔ PHỎNG PHẦN CỨNG (LCD 1602 & TM1638 LED 7 ĐOẠN)
            GroupBox gbSim = CreateCard("4. MÔ PHỎNG MÀN HÌNH HARDWARE (LCD 1602 & TM1638)", new Point(15, 680), new Size(1150, 135));

            Panel pnlLcd = new Panel { Location = new Point(15, 23), Size = new Size(460, 78), BackColor = Color.FromArgb(8, 40, 52), BorderStyle = BorderStyle.Fixed3D };
            lblLcdLine1 = new Label { Text = "1*+ 60  2:+ 60", Font = new Font("Consolas", 13f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 230), Location = new Point(8, 10), AutoSize = true };
            lblLcdLine2 = new Label { Text = "3:+ 60  4:+ 60", Font = new Font("Consolas", 13f, FontStyle.Bold), ForeColor = Color.FromArgb(0, 255, 230), Location = new Point(8, 40), AutoSize = true };
            pnlLcd.Controls.Add(lblLcdLine1); pnlLcd.Controls.Add(lblLcdLine2);
            gbSim.Controls.Add(pnlLcd);

            Panel pnl7Seg = new Panel { Location = new Point(490, 23), Size = new Size(645, 78), BackColor = Color.Black, BorderStyle = BorderStyle.FixedSingle };
            for (int i = 0; i < 8; i++)
            {
                lblDigits[i] = new Label { Text = " ", Font = new Font("Consolas", 24f, FontStyle.Bold), ForeColor = Color.Lime, Size = new Size(73, 62), Location = new Point(6 + i * 79, 7), TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.FromArgb(12, 20, 12) };
                pnl7Seg.Controls.Add(lblDigits[i]);
            }
            gbSim.Controls.Add(pnl7Seg);

            Label lblLcdTag = new Label { Text = "Màn hình LCD 1602 (I2C)", Location = new Point(15, 106), AutoSize = true, ForeColor = Color.FromArgb(150, 175, 195) };
            Label lbl7SegTag = new Label { Text = "Màn hình 8 LED 7 đoạn TM1638", Location = new Point(490, 106), AutoSize = true, ForeColor = Color.FromArgb(150, 175, 195) };
            gbSim.Controls.AddRange(new Control[] { lblLcdTag, lbl7SegTag });
            this.Controls.Add(gbSim);

            // 6. GIÁM SÁT 8 LED & 8 PHÍM TM1638
            GroupBox gbHardware = CreateCard("5. GIÁM SÁT 8 ĐÈN LED & 8 NÚT BẤM TM1638", new Point(15, 822), new Size(660, 105));
            string[] ledNames = { "M1 RUN", "M2 RUN", "M3 RUN", "M4 RUN", "SEL 1", "SEL 2", "SEL 3", "SEL 4" };
            for (int i = 0; i < 8; i++)
            {
                btnLeds[i] = new Button { Text = $"{ledNames[i]}\nOFF", Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), Size = new Size(68, 44), Location = new Point(15 + i * 78, 24), BackColor = Color.FromArgb(42, 46, 58), ForeColor = Color.Gray, FlatStyle = FlatStyle.Flat };
                btnLeds[i].FlatAppearance.BorderSize = 0;
                gbHardware.Controls.Add(btnLeds[i]);
            }

            string[] btnNames = { "S1:RUN", "S2:SEL", "S3:SPD+", "S4:SPD-", "S5:DIR", "S6:ENA", "S7:MODE", "S8:ZERO" };
            for (int i = 0; i < 8; i++)
            {
                lblButtons[i] = new Label { Text = btnNames[i], Font = new Font("Segoe UI", 7.5f, FontStyle.Bold), Size = new Size(68, 22), Location = new Point(15 + i * 78, 73), TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.FromArgb(32, 38, 50), ForeColor = Color.FromArgb(140, 155, 175), BorderStyle = BorderStyle.FixedSingle };
                gbHardware.Controls.Add(lblButtons[i]);
            }
            this.Controls.Add(gbHardware);

            // 7. NHẬT KÝ SERIAL LOG
            GroupBox gbLog = CreateCard("6. NHẬT KÝ LỆNH SERIAL", new Point(685, 822), new Size(480, 105));
            rtbLog = new RichTextBox { Location = new Point(12, 22), Size = new Size(456, 44), BackColor = Color.FromArgb(10, 14, 20), ForeColor = Color.FromArgb(0, 255, 170), Font = new Font("Consolas", 8.5f), ReadOnly = true };
            
            txtCustomCmd = new TextBox { Location = new Point(12, 71), Size = new Size(335, 26), BackColor = Color.FromArgb(36, 42, 56), ForeColor = Color.White, Text = "MOTOR:RUN_ALL" };
            Button btnSendCmd = CreateButton("GỬI LỆNH", new Point(355, 70), new Size(113, 28), Color.FromArgb(0, 120, 215));
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

        // Đọc dữ liệu Asynchronous không gây nghẽn Thread UI
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
            if (msg.StartsWith("QUADSTAT:")) ParseQuadStatusData(msg.Substring(9));
            else if (msg.StartsWith("DUALSTAT:")) ParseDualStatusData(msg.Substring(9));
            else if (msg.StartsWith("BTN:"))
            {
                if (byte.TryParse(msg.Substring(4).Trim(), System.Globalization.NumberStyles.HexNumber, null, out byte b))
                {
                    currentButtonMask = b;
                    stateDirty = true;
                }
            }
            else if (msg.StartsWith("DISP:"))
            {
                current7SegText = msg.Substring(5);
                stateDirty = true;
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

        private void ParseQuadStatusData(string data)
        {
            var parts = data.Split(',');
            foreach (var part in parts)
            {
                var kv = part.Split('='); if (kv.Length != 2) continue;
                string k = kv[0].Trim(), v = kv[1].Trim();

                for (int m = 0; m < 4; m++)
                {
                    int mNum = m + 1;
                    if (k == $"M{mNum}_RUN") { motors[m].IsRunning = (v == "1"); motors[m].UI_Dirty = true; }
                    else if (k == $"M{mNum}_DIR") { motors[m].IsDirCW = (v == "CW"); motors[m].UI_Dirty = true; }
                    else if (k == $"M{mNum}_ENA") { motors[m].IsEnabled = (v == "1"); motors[m].UI_Dirty = true; }
                    else if (k == $"M{mNum}_SPD")
                    {
                        if (uint.TryParse(v, out uint s))
                        {
                            motors[m].SpeedSPS = s;
                            motors[m].UI_Dirty = true;
                        }
                    }
                    else if (k == $"M{mNum}_POS")
                    {
                        if (long.TryParse(v, out long p))
                        {
                            motors[m].CurrentPos = p;
                            motors[m].UI_Dirty = true;
                        }
                    }
                }

                if (k == "SEL" && int.TryParse(v, out int sel) && sel >= 0 && sel < 5)
                {
                    selectedTarget = sel;
                    stateDirty = true;
                }
                else if (k == "MODE" && int.TryParse(v, out int mode) && mode >= 0 && mode < 3)
                {
                    currentMode = mode;
                    stateDirty = true;
                }
            }
        }

        private void ParseDualStatusData(string data)
        {
            var parts = data.Split(',');
            foreach (var part in parts)
            {
                var kv = part.Split('='); if (kv.Length != 2) continue;
                string k = kv[0].Trim(), v = kv[1].Trim();

                for (int m = 0; m < 2; m++)
                {
                    int mNum = m + 1;
                    if (k == $"M{mNum}_RUN") { motors[m].IsRunning = (v == "1"); motors[m].UI_Dirty = true; }
                    else if (k == $"M{mNum}_DIR") { motors[m].IsDirCW = (v == "CW"); motors[m].UI_Dirty = true; }
                    else if (k == $"M{mNum}_SPD")
                    {
                        if (uint.TryParse(v, out uint s))
                        {
                            motors[m].SpeedSPS = s;
                            motors[m].UI_Dirty = true;
                        }
                    }
                    else if (k == $"M{mNum}_POS")
                    {
                        if (long.TryParse(v, out long p))
                        {
                            motors[m].CurrentPos = p;
                            motors[m].UI_Dirty = true;
                        }
                    }
                }

                if (k == "SEL" && int.TryParse(v, out int sel) && sel >= 0 && sel < 5)
                {
                    selectedTarget = sel;
                    stateDirty = true;
                }
            }
        }

        // HÀM RENDER UI ĐỊNH KỲ MƯỢT MÀ (20 FPS - DÙNG TIMER KHÔNG KHỰNG KHÓA UI)
        private void UiRenderTimer_Tick(object? sender, EventArgs e)
        {
            for (int i = 0; i < 4; i++)
            {
                if (motors[i].UI_Dirty)
                {
                    motors[i].UI_Dirty = false;
                    UpdateMotorCardRender(i);
                }
            }

            if (stateDirty)
            {
                stateDirty = false;
                UpdateGlobalStateRender();
            }
        }

        private void UpdateMotorCardRender(int idx)
        {
            var m = motors[idx];
            m.BtnRun.Text = m.IsRunning ? $"⏸ DỪNG M{idx + 1}" : $"▶ CHẠY M{idx + 1}";
            m.BtnRun.BackColor = m.IsRunning ? Color.FromArgb(220, 40, 40) : Color.FromArgb(0, 160, 80);

            m.BtnDir.Text = m.IsDirCW ? "↻ THUẬN (CW)" : "↺ NGHỊCH (CCW)";
            m.BtnDir.BackColor = m.IsDirCW ? Color.FromArgb(0, 130, 200) : Color.FromArgb(200, 120, 0);

            m.BtnEna.Text = m.IsEnabled ? "🔒 KHÓA TRỤC" : "🔓 MỞ TRỤC";
            m.BtnEna.BackColor = m.IsEnabled ? Color.FromArgb(100, 50, 150) : Color.FromArgb(80, 85, 95);

            m.LblPos.Text = $"Vị trí: {m.CurrentPos} xung";

            // Chỉ cập nhật TrackBar khi người dùng không kéo giữ chuột
            if (!m.TbSpeed.Capture && m.TbSpeed.Value != (int)m.SpeedSPS)
            {
                if (m.SpeedSPS >= m.TbSpeed.Minimum && m.SpeedSPS <= m.TbSpeed.Maximum)
                {
                    m.TbSpeed.Value = (int)m.SpeedSPS;
                    float rpm = (m.SpeedSPS / 1600.0f) * 60.0f;
                    m.LblSpeedVal.Text = $"Tốc độ M{idx + 1}: {m.SpeedSPS} SPS (~ {rpm:F0} RPM)";
                }
            }
        }

        private void UpdateGlobalStateRender()
        {
            // Update Radio Target
            if (selectedTarget >= 0 && selectedTarget < 5 && !rbTargets[selectedTarget].Checked)
            {
                rbTargets[selectedTarget].Checked = true;
            }

            // Update Radio Mode
            if (currentMode >= 0 && currentMode < 3 && !rbModes[currentMode].Checked)
            {
                rbModes[currentMode].Checked = true;
            }

            // Update LCD Simulator
            float rpm1 = (motors[0].SpeedSPS / 1600.0f) * 60.0f;
            float rpm2 = (motors[1].SpeedSPS / 1600.0f) * 60.0f;
            float rpm3 = (motors[2].SpeedSPS / 1600.0f) * 60.0f;
            float rpm4 = (motors[3].SpeedSPS / 1600.0f) * 60.0f;

            lblLcdLine1.Text = string.Format("1{0}{1}{2,3:F0} 2{3}{4}{5,3:F0}",
                (selectedTarget == 0 || selectedTarget == 4) ? '*' : ':',
                motors[0].IsRunning ? (motors[0].IsDirCW ? '+' : '-') : 'S', rpm1,
                (selectedTarget == 1 || selectedTarget == 4) ? '*' : ':',
                motors[1].IsRunning ? (motors[1].IsDirCW ? '+' : '-') : 'S', rpm2);

            lblLcdLine2.Text = string.Format("3{0}{1}{2,3:F0} 4{3}{4}{5,3:F0}",
                (selectedTarget == 2 || selectedTarget == 4) ? '*' : ':',
                motors[2].IsRunning ? (motors[2].IsDirCW ? '+' : '-') : 'S', rpm3,
                (selectedTarget == 3 || selectedTarget == 4) ? '*' : ':',
                motors[3].IsRunning ? (motors[3].IsDirCW ? '+' : '-') : 'S', rpm4);

            // Update 7-Seg Simulator
            for (int i = 0; i < 8; i++) lblDigits[i].Text = " ";
            int pos = 0;
            for (int i = 0; i < current7SegText.Length && pos < 8; i++)
            {
                char c = current7SegText[i];
                if (c == '.' && pos > 0) { if (!lblDigits[pos - 1].Text.EndsWith(".")) lblDigits[pos - 1].Text += "."; continue; }
                if (i + 1 < current7SegText.Length && current7SegText[i + 1] == '.') { lblDigits[pos].Text = c.ToString() + "."; i++; }
                else lblDigits[pos].Text = c.ToString();
                pos++;
            }

            // Update LEDs
            string[] ledNames = { "M1 RUN", "M2 RUN", "M3 RUN", "M4 RUN", "SEL 1", "SEL 2", "SEL 3", "SEL 4" };
            for (int i = 0; i < 8; i++)
            {
                bool isOn = (currentLedMask & (1 << i)) != 0;
                btnLeds[i].Text = $"{ledNames[i]}\n{(isOn ? "ON" : "OFF")}";
                btnLeds[i].BackColor = isOn ? Color.FromArgb(230, 35, 35) : Color.FromArgb(42, 46, 58);
                btnLeds[i].ForeColor = isOn ? Color.White : Color.Gray;
            }

            // Update Buttons
            string[] btnNames = { "S1:RUN", "S2:SEL", "S3:SPD+", "S4:SPD-", "S5:DIR", "S6:ENA", "S7:MODE", "S8:ZERO" };
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
            if (rtbLog.TextLength > 10000) rtbLog.Clear(); // Giới hạn kích thước log tránh giật
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
