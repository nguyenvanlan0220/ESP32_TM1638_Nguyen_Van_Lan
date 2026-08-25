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
        private byte currentLedMask = 0x00;
        private byte currentButtonMask = 0x00;

        // UI Controls
        private ComboBox cmbPorts = null!;
        private Button btnRefreshPorts = null!;
        private Button btnConnect = null!;
        private Label lblStatus = null!;
        private Panel pnlStatusIndicator = null!;

        private Label[] lblDigits = new Label[8];
        private TextBox txtInputText = null!;
        private Button btnSendText = null!;
        private Button btnStartCounter = null!;
        private Button btnStopCounter = null!;
        private Button btnResetCounter = null!;

        private Button[] btnLeds = new Button[8];
        private Button btnAllLedsOn = null!;
        private Button btnAllLedsOff = null!;

        private Label[] lblButtons = new Label[8];

        private TrackBar tbBrightness = null!;
        private Label lblBrightnessVal = null!;

        private RichTextBox rtbLog = null!;

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
            this.Text = "ESP32-S3 + TM1638 Dual Controller - Nguyễn Văn Lân";
            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;

            // 1. HEADER
            Panel pnlHeader = new Panel
            {
                Location = new Point(0, 0),
                Size = new Size(920, 60),
                BackColor = Color.FromArgb(32, 35, 45)
            };
            Label lblTitle = new Label
            {
                Text = "⚡ ESP32-S3 & TM1638 DUAL CONTROLLER",
                Font = new Font("Segoe UI", 13f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 220, 255),
                Location = new Point(20, 8),
                AutoSize = true
            };
            Label lblSub = new Label
            {
                Text = "Giao diện điều khiển máy tính song song với phần cứng TM1638 | Tác giả: Nguyễn Văn Lân",
                Font = new Font("Segoe UI", 8.5f, FontStyle.Regular),
                ForeColor = Color.FromArgb(170, 180, 195),
                Location = new Point(22, 34),
                AutoSize = true
            };
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSub);
            this.Controls.Add(pnlHeader);

            // 2. KẾT NỐI SERIAL CARD
            GroupBox gbConnect = CreateCard("Kết Nối Cổng COM (ESP32-S3)", new Point(20, 70), new Size(880, 65));
            Label lblPort = new Label { Text = "Cổng COM:", Location = new Point(20, 26), AutoSize = true };
            cmbPorts = new ComboBox
            {
                Location = new Point(100, 23),
                Size = new Size(130, 28),
                DropDownStyle = ComboBoxStyle.DropDownList,
                BackColor = Color.FromArgb(45, 48, 60),
                ForeColor = Color.White
            };
            btnRefreshPorts = CreateButton("Làm Mới", new Point(240, 22), new Size(85, 30), Color.FromArgb(50, 55, 70));
            btnRefreshPorts.Click += (s, e) => RefreshComPorts();

            btnConnect = CreateButton("KẾT NỐI", new Point(340, 22), new Size(120, 30), Color.FromArgb(0, 160, 90));
            btnConnect.Click += BtnConnect_Click;

            pnlStatusIndicator = new Panel
            {
                Location = new Point(480, 28),
                Size = new Size(16, 16),
                BackColor = Color.Red
            };
            lblStatus = new Label
            {
                Text = "Chưa kết nối",
                Location = new Point(505, 26),
                AutoSize = true,
                ForeColor = Color.FromArgb(200, 100, 100)
            };

            gbConnect.Controls.AddRange(new Control[] { lblPort, cmbPorts, btnRefreshPorts, btnConnect, pnlStatusIndicator, lblStatus });
            this.Controls.Add(gbConnect);

            // 3. MÀN HÌNH LED 7 ĐOẠN SIMULATOR & CONTROL
            GroupBox gbDisplay = CreateCard("Màn Hình 8 LED 7 Đoạn (TM1638 Display)", new Point(20, 145), new Size(880, 155));

            // Visual 8 Digits Box
            Panel pnlDisplaySim = new Panel
            {
                Location = new Point(20, 25),
                Size = new Size(480, 60),
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
                    Size = new Size(55, 50),
                    Location = new Point(5 + i * 59, 5),
                    TextAlign = ContentAlignment.MiddleCenter,
                    BackColor = Color.FromArgb(10, 15, 10)
                };
                pnlDisplaySim.Controls.Add(lblDigits[i]);
            }
            gbDisplay.Controls.Add(pnlDisplaySim);

            // Quick Presets
            int px = 520;
            string[] presets = { "HELLO", "ESP32-S3", "12345678", "CLEAR" };
            foreach (var p in presets)
            {
                Button btnPreset = CreateButton(p, new Point(px, 35), new Size(80, 35), Color.FromArgb(60, 70, 90));
                string textToSend = p == "CLEAR" ? "        " : p;
                btnPreset.Click += (s, e) => SendTextToDevice(textToSend);
                gbDisplay.Controls.Add(btnPreset);
                px += 88;
            }

            // Input Text & Send
            Label lblInput = new Label { Text = "Nhập chữ/số:", Location = new Point(20, 102), AutoSize = true };
            txtInputText = new TextBox
            {
                Location = new Point(115, 100),
                Size = new Size(200, 26),
                MaxLength = 16,
                Text = "HELLO",
                BackColor = Color.FromArgb(45, 48, 60),
                ForeColor = Color.White
            };
            // Nhấn Enter trong ô nhập để gửi nhanh
            txtInputText.KeyDown += (s, e) =>
            {
                if (e.KeyCode == Keys.Enter)
                {
                    SendTextToDevice(txtInputText.Text);
                    e.SuppressKeyPress = true;
                }
            };

            btnSendText = CreateButton("Gửi Màn Hình", new Point(325, 98), new Size(110, 30), Color.FromArgb(0, 120, 215));
            btnSendText.Click += (s, e) => SendTextToDevice(txtInputText.Text);

            btnStartCounter = CreateButton("▶ Bắt đầu đếm", new Point(450, 98), new Size(120, 30), Color.FromArgb(30, 140, 70));
            btnStartCounter.Click += (s, e) => SendCommand("COUNTER:START");

            btnStopCounter = CreateButton("⏸ Dừng", new Point(580, 98), new Size(80, 30), Color.FromArgb(170, 120, 20));
            btnStopCounter.Click += (s, e) => SendCommand("COUNTER:STOP");

            btnResetCounter = CreateButton("↺ Reset 0", new Point(670, 98), new Size(90, 30), Color.FromArgb(160, 40, 40));
            btnResetCounter.Click += (s, e) => SendCommand("COUNTER:RESET");

            gbDisplay.Controls.AddRange(new Control[] { lblInput, txtInputText, btnSendText, btnStartCounter, btnStopCounter, btnResetCounter });
            this.Controls.Add(gbDisplay);

            // 4. ĐIỀU KHIỂN 8 ĐÈN LED ĐƠN
            GroupBox gbLeds = CreateCard("Điều Khiển 8 LED Đơn (LED 1 - LED 8)", new Point(20, 310), new Size(540, 140));
            for (int i = 0; i < 8; i++)
            {
                int ledIndex = i;
                btnLeds[i] = new Button
                {
                    Text = $"LED {i + 1}\nOFF",
                    Font = new Font("Segoe UI", 8.5f, FontStyle.Bold),
                    Size = new Size(55, 55),
                    Location = new Point(20 + i * 63, 28),
                    BackColor = Color.FromArgb(50, 50, 60),
                    ForeColor = Color.Gray,
                    FlatStyle = FlatStyle.Flat,
                    Cursor = Cursors.Hand
                };
                btnLeds[i].FlatAppearance.BorderSize = 1;
                btnLeds[i].FlatAppearance.BorderColor = Color.FromArgb(80, 80, 90);
                btnLeds[i].Click += (s, e) => ToggleLed(ledIndex);
                gbLeds.Controls.Add(btnLeds[i]);
            }

            btnAllLedsOn = CreateButton("Bật Tất Cả LED", new Point(20, 95), new Size(130, 30), Color.FromArgb(0, 140, 80));
            btnAllLedsOn.Click += (s, e) => SetAllLeds(0xFF);

            btnAllLedsOff = CreateButton("Tắt Tất Cả LED", new Point(160, 95), new Size(130, 30), Color.FromArgb(140, 40, 40));
            btnAllLedsOff.Click += (s, e) => SetAllLeds(0x00);

            gbLeds.Controls.AddRange(new Control[] { btnAllLedsOn, btnAllLedsOff });
            this.Controls.Add(gbLeds);

            // 5. GIÁM SÁT 8 NÚT NHẤN PHẦN CỨNG (S1 - S8)
            GroupBox gbButtons = CreateCard("Trạng Thái 8 Nút Bấm TM1638 (S1 - S8)", new Point(570, 310), new Size(330, 140));
            for (int i = 0; i < 8; i++)
            {
                int col = i % 4;
                int row = i / 4;
                lblButtons[i] = new Label
                {
                    Text = $"S{i + 1}: OFF",
                    Font = new Font("Segoe UI", 9f, FontStyle.Bold),
                    Size = new Size(68, 38),
                    Location = new Point(18 + col * 75, 30 + row * 45),
                    TextAlign = ContentAlignment.MiddleCenter,
                    BackColor = Color.FromArgb(40, 45, 55),
                    ForeColor = Color.FromArgb(140, 150, 165),
                    BorderStyle = BorderStyle.FixedSingle
                };
                gbButtons.Controls.Add(lblButtons[i]);
            }
            this.Controls.Add(gbButtons);

            // 6. ĐỘ SÁNG & LOG TERMINAL
            GroupBox gbBrightness = CreateCard("Độ Sáng", new Point(20, 460), new Size(300, 180));
            tbBrightness = new TrackBar
            {
                Minimum = 0,
                Maximum = 7,
                Value = 7,
                TickFrequency = 1,
                Location = new Point(20, 35),
                Size = new Size(250, 45)
            };
            lblBrightnessVal = new Label
            {
                Text = "Mức độ sáng: 7 / 7 (Tối đa)",
                Location = new Point(25, 85),
                AutoSize = true,
                ForeColor = Color.Yellow
            };
            tbBrightness.Scroll += (s, e) =>
            {
                lblBrightnessVal.Text = $"Mức độ sáng: {tbBrightness.Value} / 7";
                SendCommand($"BRIGHTNESS:{tbBrightness.Value}");
            };
            gbBrightness.Controls.AddRange(new Control[] { tbBrightness, lblBrightnessVal });
            this.Controls.Add(gbBrightness);

            GroupBox gbLog = CreateCard("Nhật Ký Giao Tiếp Serial (Log Console)", new Point(330, 460), new Size(570, 180));
            rtbLog = new RichTextBox
            {
                Location = new Point(15, 25),
                Size = new Size(540, 140),
                BackColor = Color.FromArgb(15, 18, 24),
                ForeColor = Color.FromArgb(0, 255, 180),
                Font = new Font("Consolas", 9f),
                ReadOnly = true
            };
            gbLog.Controls.Add(rtbLog);
            this.Controls.Add(gbLog);
        }

        private GroupBox CreateCard(string title, Point location, Size size)
        {
            return new GroupBox
            {
                Text = title,
                Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
                ForeColor = Color.FromArgb(0, 200, 255),
                Location = location,
                Size = size,
                BackColor = Color.FromArgb(32, 35, 45)
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
            lblStatus.ForeColor = Color.FromArgb(200, 100, 100);
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
            if (msg.StartsWith("BTN:"))
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
            else
            {
                LogMessage($"[ESP32] {msg}");
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
            for (int i = 0; i < 8; i++)
            {
                bool isOn = (mask & (1 << i)) != 0;
                btnLeds[i].Text = $"LED {i + 1}\n{(isOn ? "ON" : "OFF")}";
                btnLeds[i].BackColor = isOn ? Color.FromArgb(200, 20, 20) : Color.FromArgb(50, 50, 60);
                btnLeds[i].ForeColor = isOn ? Color.White : Color.Gray;
            }
        }

        private void UpdateButtonsUI(byte mask)
        {
            for (int i = 0; i < 8; i++)
            {
                bool isPressed = (mask & (1 << i)) != 0;
                lblButtons[i].Text = $"S{i + 1}: {(isPressed ? "ON" : "OFF")}";
                lblButtons[i].BackColor = isPressed ? Color.FromArgb(255, 190, 0) : Color.FromArgb(40, 45, 55);
                lblButtons[i].ForeColor = isPressed ? Color.Black : Color.FromArgb(140, 150, 165);
            }
        }

        // Loại bỏ dấu tiếng Việt và chuẩn hóa chữ HOA để hiển thị chuẩn nhất trên LED 7 đoạn
        private static string RemoveVietnameseAccents(string text)
        {
            if (string.IsNullOrEmpty(text)) return string.Empty;
            string normalized = text.Normalize(NormalizationForm.FormD);
            var sb = new StringBuilder();
            foreach (char c in normalized)
            {
                var uc = System.Globalization.CharUnicodeInfo.GetUnicodeCategory(c);
                if (uc != System.Globalization.UnicodeCategory.NonSpacingMark)
                {
                    if (c == 'đ' || c == 'Đ') sb.Append('D');
                    else sb.Append(c);
                }
            }
            return sb.ToString().Normalize(NormalizationForm.FormC).ToUpper();
        }

        private void SendTextToDevice(string rawText)
        {
            string cleanText = RemoveVietnameseAccents(rawText);
            UpdateDisplaySimulator(cleanText);
            SendCommand($"TEXT:{cleanText}");
        }

        private void ToggleLed(int index)
        {
            currentLedMask ^= (byte)(1 << index);
            UpdateLedsUI(currentLedMask);
            SendCommand($"LEDS:{currentLedMask:X2}");
        }

        private void SetAllLeds(byte mask)
        {
            currentLedMask = mask;
            UpdateLedsUI(mask);
            SendCommand($"LEDS:{mask:X2}");
        }

        private void SendCommand(string cmd)
        {
            if (serialPort != null && serialPort.IsOpen)
            {
                try
                {
                    serialPort.WriteLine(cmd);
                    LogMessage($"[TX] {cmd}");
                }
                catch (Exception ex)
                {
                    LogMessage($"[LỖI GỬI] {ex.Message}");
                }
            }
            else
            {
                LogMessage($"[Chưa kết nối COM] Lệnh: {cmd}");
            }
        }

        private void LogMessage(string text)
        {
            if (rtbLog.IsDisposed) return;
            string time = DateTime.Now.ToString("HH:mm:ss");
            
            if (rtbLog.Lines.Length > 150)
            {
                rtbLog.Clear();
            }
            
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
