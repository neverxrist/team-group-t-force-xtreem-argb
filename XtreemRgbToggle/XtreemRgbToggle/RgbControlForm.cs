using System.Drawing.Drawing2D;
using System.Threading;

namespace XtreemRgbToggle;

/// <summary>
/// Main RGB control form with comprehensive device control and DPI-aware layout.
/// </summary>
internal sealed class RgbControlForm : Form
{
    private readonly XtreemService _svc = new();
    private List<XtreemDevice> _cachedDevices = new();
    
    private readonly object _hardwareLock = new object();
    private CancellationTokenSource? _previewCancellation;
    private Task? _activePreviewTask;
    private DateTime _lastPreviewTime = DateTime.MinValue;
    private const int PreviewRateLimitMs = 100;
    
    private byte _pendingR, _pendingG, _pendingB;
    private bool _hasPendingColor;
    
    private bool _autostartStatusLoaded;
    private bool _autostartStatusCached;
    
    // Modern color palette
    private static readonly Color ColorPrimary = Color.FromArgb(24, 119, 242);
    private static readonly Color ColorSuccess = Color.FromArgb(76, 175, 80);
    private static readonly Color ColorDanger = Color.FromArgb(229, 57, 53);
    private static readonly Color ColorBackground = Color.FromArgb(250, 250, 250);
    private static readonly Color ColorText = Color.FromArgb(33, 33, 33);
    private static readonly Color ColorTextSecondary = Color.FromArgb(117, 117, 117);
    
    private readonly TableLayoutPanel _mainLayout;
    private readonly Label _statusLabel;
    private readonly Label _modulesLabel;
    private readonly Button _btnRescan;
    
    private readonly GroupBox _moduleSelectGroup;
    private readonly RadioButton _rbAllModules;
    private readonly List<RadioButton> _moduleRadioButtons = new();
    
    private readonly GroupBox _quickGroup;
    private readonly Button _btnOff;
    private readonly Button _btnOn;
    
    private readonly GroupBox _effectGroup;
    private readonly ComboBox _cboEffect;
    private readonly TrackBar _trackSpeed;
    private readonly Label _valSpeed;
    private readonly CheckBox _chkDirectionReverse;
    private readonly Button _btnApplyEffect;
    
    private readonly GroupBox _colorGroup;
    private readonly CheckBox _chkLivePreview;
    private readonly Button _btnPickColor;
    private readonly TrackBar _trackR, _trackG, _trackB;
    private readonly Label _valR, _valG, _valB;
    private readonly Panel _colorPreview;
    private readonly Button _btnApplyColor;
    
    private readonly GroupBox _autostartGroup;
    private readonly Button _btnInstallAutostart;
    private readonly Button _btnRemoveAutostart;
    
    private bool _busy;

    public RgbControlForm()
    {
        using var timer = DiagnosticTimer.Start("RgbControlForm.Constructor");
        
        // Enable diagnostic logging if env var set
        if (Environment.GetEnvironmentVariable("XTREEM_DIAG") == "1")
        {
            DiagnosticTimer.Enabled = true;
        }
        
        Text = "XTREEM RGB Control";
        AutoScaleMode = AutoScaleMode.Dpi;
        AutoScroll = true;
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(600, 400);
        BackColor = ColorBackground;
        Font = new Font("Segoe UI", 9f);
        Padding = new Padding(16);

        // Main layout container
        _mainLayout = new TableLayoutPanel
        {
            ColumnCount = 1,
            RowCount = 10,
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = ColorBackground
        };
        _mainLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));

        // Status
        _statusLabel = new Label
        {
            Text = "Status: Ready",
            AutoSize = true,
            Font = new Font("Segoe UI Semibold", 10f),
            ForeColor = ColorPrimary,
            Margin = new Padding(0, 0, 0, 4)
        };
        _mainLayout.Controls.Add(_statusLabel);

        _modulesLabel = new Label
        {
            Text = "Modules: Not detected",
            AutoSize = true,
            Font = new Font("Segoe UI", 9f),
            ForeColor = ColorTextSecondary,
            Margin = new Padding(0, 0, 0, 8)
        };
        _mainLayout.Controls.Add(_modulesLabel);

        _btnRescan = CreateButton("Rescan Modules", ColorPrimary);
        _btnRescan.Dock = DockStyle.Top;
        _btnRescan.Height = 36;
        _btnRescan.Margin = new Padding(0, 0, 0, 12);
        _btnRescan.Click += (_, _) => StartDetect();
        _mainLayout.Controls.Add(_btnRescan);

        // Module selection
        _moduleSelectGroup = CreateGroupBox("Module Selection");
        _moduleSelectGroup.Margin = new Padding(0, 0, 0, 12);
        _moduleSelectGroup.Padding = new Padding(12);
        _rbAllModules = new RadioButton
        {
            Text = "All modules",
            AutoSize = true,
            Checked = true,
            Font = new Font("Segoe UI", 9f),
            ForeColor = ColorText,
            Margin = new Padding(0, 4, 0, 0)
        };
        _rbAllModules.CheckedChanged += (_, _) => CancelPendingPreview();
        _moduleSelectGroup.Controls.Add(_rbAllModules);
        _mainLayout.Controls.Add(_moduleSelectGroup);

        // Quick control
        _quickGroup = CreateGroupBox("Quick Control");
        _quickGroup.Margin = new Padding(0, 0, 0, 12);
        _quickGroup.Height = 84;
        var quickLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            Padding = new Padding(8),
            CellBorderStyle = TableLayoutPanelCellBorderStyle.None
        };
        quickLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
        quickLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
        
        _btnOff = CreateButton("OFF", ColorDanger);
        _btnOff.Font = new Font("Segoe UI", 11f, FontStyle.Bold);
        _btnOff.Dock = DockStyle.Fill;
        _btnOff.Margin = new Padding(0, 0, 4, 0);
        _btnOff.Click += (_, _) => ApplyOff();
        quickLayout.Controls.Add(_btnOff, 0, 0);

        _btnOn = CreateButton("ON (T-Force Default)", ColorSuccess);
        _btnOn.Font = new Font("Segoe UI", 10f, FontStyle.Bold);
        _btnOn.Dock = DockStyle.Fill;
        _btnOn.Margin = new Padding(4, 0, 0, 0);
        _btnOn.Click += (_, _) => ApplyOn();
        quickLayout.Controls.Add(_btnOn, 1, 0);
        
        _quickGroup.Controls.Add(quickLayout);
        _mainLayout.Controls.Add(_quickGroup);

        // Effects
        _effectGroup = CreateGroupBox("Effects");
        _effectGroup.Margin = new Padding(0, 0, 0, 12);
        _effectGroup.Padding = new Padding(12);
        var effectLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 4,
            AutoSize = true,
            CellBorderStyle = TableLayoutPanelCellBorderStyle.None
        };
        effectLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 80f));
        effectLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));
        
        effectLayout.Controls.Add(new Label { Text = "Effect:", AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 0, 0) }, 0, 0);
        _cboEffect = new ComboBox
        {
            Dock = DockStyle.Top,
            DropDownStyle = ComboBoxStyle.DropDownList,
            Font = new Font("Segoe UI", 9f),
            Margin = new Padding(0, 0, 0, 8)
        };
        foreach (var mode in XtreemModes.AllModes)
            _cboEffect.Items.Add($"{mode.Name} (0x{mode.Value:X2})");
        _cboEffect.SelectedIndex = 25;
        _cboEffect.SelectedIndexChanged += OnEffectChanged;
        effectLayout.Controls.Add(_cboEffect, 1, 0);

        effectLayout.Controls.Add(new Label { Text = "Speed:", AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 0, 0) }, 0, 1);
        var speedPanel = new Panel { Dock = DockStyle.Top, Height = 45, Margin = new Padding(0, 0, 0, 8) };
        _trackSpeed = new TrackBar 
        { 
            Dock = DockStyle.Fill,
            Minimum = 0, 
            Maximum = 4, 
            Value = 2,
            TickFrequency = 1,
            Enabled = false
        };
        _trackSpeed.ValueChanged += (_, _) => UpdateSpeedLabel();
        _valSpeed = new Label 
        { 
            Text = "Normal",
            Dock = DockStyle.Right,
            Width = 60,
            TextAlign = ContentAlignment.MiddleRight,
            Font = new Font("Segoe UI Semibold", 9f),
            ForeColor = ColorPrimary
        };
        speedPanel.Controls.Add(_trackSpeed);
        speedPanel.Controls.Add(_valSpeed);
        effectLayout.Controls.Add(speedPanel, 1, 1);

        _chkDirectionReverse = new CheckBox
        {
            Text = "Reverse direction",
            AutoSize = true,
            Enabled = false,
            Font = new Font("Segoe UI", 9f),
            Margin = new Padding(0, 4, 0, 8),
            ForeColor = ColorText
        };
        effectLayout.SetColumnSpan(_chkDirectionReverse, 2);
        effectLayout.Controls.Add(_chkDirectionReverse, 0, 2);

        _btnApplyEffect = CreateButton("Apply Effect", ColorPrimary);
        _btnApplyEffect.Dock = DockStyle.Top;
        _btnApplyEffect.Height = 32;
        _btnApplyEffect.Click += (_, _) => ApplyEffect();
        effectLayout.SetColumnSpan(_btnApplyEffect, 2);
        effectLayout.Controls.Add(_btnApplyEffect, 0, 3);

        _effectGroup.Controls.Add(effectLayout);
        _mainLayout.Controls.Add(_effectGroup);

        // Static color
        _colorGroup = CreateGroupBox("Static Color (Live Preview)");
        _colorGroup.Margin = new Padding(0, 0, 0, 12);
        _colorGroup.Padding = new Padding(12);
        var colorLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 8,
            AutoSize = true,
            CellBorderStyle = TableLayoutPanelCellBorderStyle.None
        };
        colorLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));

        _chkLivePreview = new CheckBox
        {
            Text = "Live preview (updates hardware while dragging)",
            AutoSize = true,
            Checked = true,
            Font = new Font("Segoe UI", 9f),
            Margin = new Padding(0, 0, 0, 8),
            ForeColor = ColorText
        };
        colorLayout.Controls.Add(_chkLivePreview);

        _btnPickColor = CreateButton("🎨 Pick Color...", ColorPrimary);
        _btnPickColor.Dock = DockStyle.Top;
        _btnPickColor.Height = 36;
        _btnPickColor.Margin = new Padding(0, 0, 0, 8);
        _btnPickColor.Font = new Font("Segoe UI", 10f);
        _btnPickColor.Click += (_, _) => PickColor();
        colorLayout.Controls.Add(_btnPickColor);

        // RGB sliders
        var sliderPanel = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            ColumnCount = 3,
            RowCount = 3,
            AutoSize = true,
            Margin = new Padding(0, 0, 0, 8),
            CellBorderStyle = TableLayoutPanelCellBorderStyle.None
        };
        sliderPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 50f));
        sliderPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));
        sliderPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 50f));

        sliderPanel.Controls.Add(new Label { Text = "Red", AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 0, 0) }, 0, 0);
        _trackR = new TrackBar { Dock = DockStyle.Fill, Maximum = 255, Value = 0, TickFrequency = 32 };
        _trackR.ValueChanged += (_, _) => OnColorChanged();
        sliderPanel.Controls.Add(_trackR, 1, 0);
        _valR = new Label { Text = "0", Dock = DockStyle.Right, Width = 50, TextAlign = ContentAlignment.MiddleRight, Font = new Font("Segoe UI Semibold", 9f), ForeColor = ColorPrimary };
        sliderPanel.Controls.Add(_valR, 2, 0);

        sliderPanel.Controls.Add(new Label { Text = "Green", AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 0, 0) }, 0, 1);
        _trackG = new TrackBar { Dock = DockStyle.Fill, Maximum = 255, Value = 0, TickFrequency = 32 };
        _trackG.ValueChanged += (_, _) => OnColorChanged();
        sliderPanel.Controls.Add(_trackG, 1, 1);
        _valG = new Label { Text = "0", Dock = DockStyle.Right, Width = 50, TextAlign = ContentAlignment.MiddleRight, Font = new Font("Segoe UI Semibold", 9f), ForeColor = ColorPrimary };
        sliderPanel.Controls.Add(_valG, 2, 1);

        sliderPanel.Controls.Add(new Label { Text = "Blue", AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 6, 0, 0) }, 0, 2);
        _trackB = new TrackBar { Dock = DockStyle.Fill, Maximum = 255, Value = 255, TickFrequency = 32 };
        _trackB.ValueChanged += (_, _) => OnColorChanged();
        sliderPanel.Controls.Add(_trackB, 1, 2);
        _valB = new Label { Text = "255", Dock = DockStyle.Right, Width = 50, TextAlign = ContentAlignment.MiddleRight, Font = new Font("Segoe UI Semibold", 9f), ForeColor = ColorPrimary };
        sliderPanel.Controls.Add(_valB, 2, 2);

        colorLayout.Controls.Add(sliderPanel);

        _colorPreview = new Panel
        {
            Dock = DockStyle.Top,
            Height = 32,
            BackColor = Color.Blue,
            BorderStyle = BorderStyle.FixedSingle,
            Margin = new Padding(0, 0, 0, 8)
        };
        colorLayout.Controls.Add(_colorPreview);

        _btnApplyColor = CreateButton("Apply to Selected (force final update)", ColorPrimary);
        _btnApplyColor.Dock = DockStyle.Top;
        _btnApplyColor.Height = 28;
        _btnApplyColor.Font = new Font("Segoe UI", 8.5f);
        _btnApplyColor.Click += (_, _) => ApplyColorFinal();
        colorLayout.Controls.Add(_btnApplyColor);

        _colorGroup.Controls.Add(colorLayout);
        _mainLayout.Controls.Add(_colorGroup);

        // Autostart
        _autostartGroup = CreateGroupBox("Autostart (Lights OFF at Login)");
        _autostartGroup.Margin = new Padding(0, 0, 0, 12);
        _autostartGroup.Padding = new Padding(12);
        var autostartLayout = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            AutoSize = true,
            WrapContents = false
        };

        _btnInstallAutostart = CreateButton("Install Autostart", ColorSuccess);
        _btnInstallAutostart.Width = 536;
        _btnInstallAutostart.Height = 32;
        _btnInstallAutostart.Margin = new Padding(0, 0, 0, 8);
        _btnInstallAutostart.Click += (_, _) => InstallAutostart();
        autostartLayout.Controls.Add(_btnInstallAutostart);

        _btnRemoveAutostart = CreateButton("Remove Autostart", ColorDanger);
        _btnRemoveAutostart.Width = 536;
        _btnRemoveAutostart.Height = 32;
        _btnRemoveAutostart.Margin = new Padding(0, 0, 0, 8);
        _btnRemoveAutostart.Click += (_, _) => RemoveAutostart();
        autostartLayout.Controls.Add(_btnRemoveAutostart);

        var lblNote = new Label
        {
            Text = "Note: Elevated tasks do not appear in Task Manager Startup tab.",
            AutoSize = true,
            MaximumSize = new Size(536, 0),
            Font = new Font("Segoe UI", 7.5f, FontStyle.Italic),
            ForeColor = ColorTextSecondary
        };
        autostartLayout.Controls.Add(lblNote);

        _autostartGroup.Controls.Add(autostartLayout);
        _mainLayout.Controls.Add(_autostartGroup);

        Controls.Add(_mainLayout);

        // Size form to fit content
        Load += (_, _) =>
        {
            var workingArea = Screen.FromControl(this).WorkingArea;
            var preferredHeight = _mainLayout.GetPreferredSize(new Size(ClientSize.Width, 0)).Height + Padding.Vertical;
            ClientSize = new Size(600, Math.Min(preferredHeight, workingArea.Height - 100));
        };

        FormClosing += (_, _) => CancelPendingPreview();
        Shown += async (_, _) =>
        {
            DiagnosticTimer.Log("Form shown (first paint complete)");
            await InitializeAsync();
        };
    }

    private GroupBox CreateGroupBox(string title)
    {
        return new GroupBox
        {
            Text = title,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Font = new Font("Segoe UI Semibold", 9.5f),
            ForeColor = ColorText,
            FlatStyle = FlatStyle.Flat,
            Dock = DockStyle.Top
        };
    }

    private Button CreateButton(string text, Color bgColor)
    {
        var btn = new Button
        {
            Text = text,
            BackColor = bgColor,
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat,
            Font = new Font("Segoe UI", 9f),
            Cursor = Cursors.Hand
        };
        btn.FlatAppearance.BorderSize = 0;
        return btn;
    }

    private void PickColor()
    {
        using var dlg = new ColorDialog
        {
            Color = Color.FromArgb(255, _trackR.Value, _trackG.Value, _trackB.Value),
            FullOpen = true,
            AnyColor = true
        };
        
        if (dlg.ShowDialog(this) == DialogResult.OK)
        {
            _trackR.Value = dlg.Color.R;
            _trackG.Value = dlg.Color.G;
            _trackB.Value = dlg.Color.B;
            OnColorChanged();
        }
    }

    private void Ui(Action action)
    {
        if (IsDisposed) return;
        try
        {
            if (InvokeRequired) Invoke(action);
            else action();
        }
        catch { }
    }

    private void SetBusy(bool busy)
    {
        _busy = busy;
        
        _btnRescan.Enabled = !busy;
        _btnOff.Enabled = !busy && _cachedDevices.Count > 0;
        _btnOn.Enabled = !busy && _cachedDevices.Count > 0;
        _btnApplyColor.Enabled = !busy && _cachedDevices.Count > 0;
        _btnApplyEffect.Enabled = !busy && _cachedDevices.Count > 0;
        _btnPickColor.Enabled = !busy && _cachedDevices.Count > 0;
        _trackR.Enabled = !busy && _cachedDevices.Count > 0;
        _trackG.Enabled = !busy && _cachedDevices.Count > 0;
        _trackB.Enabled = !busy && _cachedDevices.Count > 0;
        _chkLivePreview.Enabled = !busy && _cachedDevices.Count > 0;
        _cboEffect.Enabled = !busy && _cachedDevices.Count > 0;
        
        RefreshAutostartButtons(busy);
        
        // Clear cursor on ALL controls
        Cursor targetCursor = busy ? Cursors.WaitCursor : Cursors.Default;
        Cursor = targetCursor;
        UseWaitCursor = busy;
        
        foreach (Control ctrl in GetAllControls(this))
        {
            ctrl.Cursor = targetCursor;
            ctrl.UseWaitCursor = busy;
        }
    }
    
    private IEnumerable<Control> GetAllControls(Control root)
    {
        foreach (Control ctrl in root.Controls)
        {
            yield return ctrl;
            foreach (var child in GetAllControls(ctrl))
                yield return child;
        }
    }
    
    private void RefreshAutostartButtons(bool busy)
    {
        if (!_autostartStatusLoaded)
        {
            // Status not yet loaded: show both buttons disabled
            _btnInstallAutostart.Enabled = false;
            _btnRemoveAutostart.Enabled = false;
            _btnInstallAutostart.Text = "Install Autostart (loading...)";
            _btnRemoveAutostart.Text = "Remove Autostart (loading...)";
            return;
        }
        
        // Status loaded: enable appropriate button
        bool isInstalled = _autostartStatusCached;
        _btnInstallAutostart.Enabled = !busy && !isInstalled;
        _btnRemoveAutostart.Enabled = !busy && isInstalled;
        _btnInstallAutostart.Text = "Install Autostart";
        _btnRemoveAutostart.Text = "Remove Autostart";
    }

    private void RefreshStatus()
    {
        var state = StateStore.Load();
        if (!_busy)
            _statusLabel.Text = $"Status: {state}";
        
        if (_cachedDevices.Count == 0)
        {
            _modulesLabel.Text = "Modules: none (check PawnIO, run as admin, stop Aura)";
            _moduleSelectGroup.Enabled = false;
        }
        else
        {
            _modulesLabel.Text = $"Modules: {_cachedDevices.Count} at " +
                                 string.Join(", ", _cachedDevices.Select(d => $"0x{d.Address7:X2}"));
            _moduleSelectGroup.Enabled = true;
        }
    }

    private async Task InitializeAsync()
    {
        using var timer = DiagnosticTimer.Start("InitializeAsync (total)");
        
        // Start both operations in parallel (don't block on either)
        var autostartTask = LoadAutostartStatusAsync();
        var detectTask = StartDetectAsync();
        
        // Wait for both
        await Task.WhenAll(autostartTask, detectTask);
        
        DiagnosticTimer.Log("InitializeAsync complete (form ready for commands)");
    }
    
    private async Task LoadAutostartStatusAsync()
    {
        using var timer = DiagnosticTimer.Start("LoadAutostartStatusAsync");
        
        try
        {
            bool status = await AutostartHelper.IsInstalledAsync();
            
            Ui(() =>
            {
                _autostartStatusCached = status;
                _autostartStatusLoaded = true;
                RefreshAutostartButtons(false);
            });
        }
        catch (Exception ex)
        {
            DiagnosticTimer.Log($"LoadAutostartStatusAsync failed: {ex.Message}");
            Ui(() =>
            {
                _autostartStatusLoaded = true; // Mark as loaded even on error
                RefreshAutostartButtons(false);
            });
        }
    }
    
    private async Task StartDetectAsync()
    {
        using var timer = DiagnosticTimer.Start("StartDetectAsync");
        
        Ui(() =>
        {
            if (_busy) return;
            CancelPendingPreview();
            SetBusy(true);
            _statusLabel.Text = "Status: Detecting modules...";
        });
        
        try
        {
            var found = await Task.Run(() =>
            {
                using var detectTimer = DiagnosticTimer.Start("Hardware detection");
                return _svc.DetectAndRemap(verbose: false);
            });
            
            Ui(() =>
            {
                try
                {
                    _cachedDevices = found;
                    UpdateModuleRadioButtons();
                    RefreshStatus();
                    _statusLabel.Text = found.Count > 0 ? "Status: Ready" : "Status: No modules";
                    DiagnosticTimer.Log($"Detection complete: {found.Count} modules found");
                }
                finally
                {
                    SetBusy(false);
                }
            });
        }
        catch (Exception ex)
        {
            DiagnosticTimer.Log($"Detection failed: {ex.Message}");
            Ui(() =>
            {
                try
                {
                    _statusLabel.Text = "Status: Detection failed";
                    MessageBox.Show(this, $"Detection error: {ex.Message}", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    RefreshStatus();
                }
                finally
                {
                    SetBusy(false);
                }
            });
        }
    }
    
    private void StartDetect()
    {
        // Synchronous wrapper for button click
        _ = StartDetectAsync();
    }

    private void UpdateModuleRadioButtons()
    {
        foreach (var rb in _moduleRadioButtons)
            _moduleSelectGroup.Controls.Remove(rb);
        _moduleRadioButtons.Clear();

        foreach (var device in _cachedDevices)
        {
            var rb = new RadioButton
            {
                Text = $"Module 0x{device.Address7:X2} (Port {device.BusPort})",
                AutoSize = true,
                Font = new Font("Segoe UI", 9f),
                ForeColor = ColorText,
                Margin = new Padding(0, 4, 0, 0)
            };
            rb.CheckedChanged += (_, _) => CancelPendingPreview();
            _moduleRadioButtons.Add(rb);
            _moduleSelectGroup.Controls.Add(rb);
        }
    }

    private List<XtreemDevice> GetSelectedModules()
    {
        if (_rbAllModules.Checked)
            return _cachedDevices.ToList();
        
        for (int i = 0; i < _moduleRadioButtons.Count && i < _cachedDevices.Count; i++)
        {
            if (_moduleRadioButtons[i].Checked)
                return new List<XtreemDevice> { _cachedDevices[i] };
        }
        
        return _cachedDevices.ToList();
    }

    private void OnColorChanged()
    {
        byte r = (byte)_trackR.Value;
        byte g = (byte)_trackG.Value;
        byte b = (byte)_trackB.Value;
        
        _valR.Text = r.ToString();
        _valG.Text = g.ToString();
        _valB.Text = b.ToString();
        _colorPreview.BackColor = Color.FromArgb(255, r, g, b);
        
        if (_chkLivePreview.Checked && _cachedDevices.Count > 0)
        {
            SchedulePreviewUpdate(r, g, b);
        }
    }

    private void SchedulePreviewUpdate(byte r, byte g, byte b)
    {
        lock (_hardwareLock)
        {
            _pendingR = r;
            _pendingG = g;
            _pendingB = b;
            _hasPendingColor = true;
            
            if (_activePreviewTask != null && !_activePreviewTask.IsCompleted)
                return;
            
            _previewCancellation?.Dispose();
            _previewCancellation = new CancellationTokenSource();
            var token = _previewCancellation.Token;
            
            _activePreviewTask = Task.Run(async () =>
            {
                while (true)
                {
                    byte currentR, currentG, currentB;
                    bool hasColor;
                    
                    lock (_hardwareLock)
                    {
                        if (token.IsCancellationRequested)
                            return;
                        
                        hasColor = _hasPendingColor;
                        currentR = _pendingR;
                        currentG = _pendingG;
                        currentB = _pendingB;
                        _hasPendingColor = false;
                    }
                    
                    if (!hasColor)
                    {
                        await Task.Delay(50, token);
                        continue;
                    }
                    
                    var elapsed = DateTime.Now - _lastPreviewTime;
                    if (elapsed.TotalMilliseconds < PreviewRateLimitMs)
                    {
                        var waitMs = PreviewRateLimitMs - (int)elapsed.TotalMilliseconds;
                        await Task.Delay(waitMs, token);
                    }
                    
                    if (token.IsCancellationRequested)
                        return;
                    
                    try
                    {
                        var devices = GetSelectedModules();
                        if (devices.Count > 0)
                        {
                            _svc.ApplyStaticColor(devices, currentR, currentG, currentB, verbose: false);
                            _lastPreviewTime = DateTime.Now;
                        }
                    }
                    catch { }
                }
            }, token);
        }
    }

    private void CancelPendingPreview()
    {
        lock (_hardwareLock)
        {
            _previewCancellation?.Cancel();
            _hasPendingColor = false;
        }
    }

    private void ApplyColorFinal()
    {
        CancelPendingPreview();
        
        byte r = (byte)_trackR.Value;
        byte g = (byte)_trackG.Value;
        byte b = (byte)_trackB.Value;
        
        ExecuteHardwareCommand($"Applying RGB({r},{g},{b})...", devices =>
        {
            _svc.ApplyStaticColor(devices, r, g, b, verbose: false);
            if (_rbAllModules.Checked)
                StateStore.Save(LightState.On);
        });
    }

    private void ApplyOff()
    {
        CancelPendingPreview();
        
        ExecuteHardwareCommand("Turning OFF...", devices =>
        {
            _svc.ApplyOff(devices, verbose: false);
            if (_rbAllModules.Checked)
                StateStore.Save(LightState.Off);
        });
    }

    private void ApplyOn()
    {
        CancelPendingPreview();
        
        ExecuteHardwareCommand("Turning ON (T-Force Default)...", devices =>
        {
            _svc.ApplyOn(devices, verbose: false);
            if (_rbAllModules.Checked)
                StateStore.Save(LightState.On);
        });
    }
    
    private void ApplyEffect()
    {
        if (_cboEffect.SelectedIndex < 0 || _cboEffect.SelectedIndex >= XtreemModes.AllModes.Length)
            return;
        
        CancelPendingPreview();
        
        var mode = XtreemModes.AllModes[_cboEffect.SelectedIndex];
        byte speed = GetSpeedValue();
        byte direction = _chkDirectionReverse.Checked ? XtreemModes.DIRECTION_REVERSE : XtreemModes.DIRECTION_FORWARD;
        
        ExecuteHardwareCommand($"Applying {mode.Name}...", devices =>
        {
            _svc.ApplyEffect(devices, mode.Value, speed, direction, verbose: false);
            if (_rbAllModules.Checked)
                StateStore.Save(mode.Value == XtreemModes.OFF ? LightState.Off : LightState.On);
        });
    }

    private void ExecuteHardwareCommand(string status, Action<List<XtreemDevice>> action)
    {
        if (_busy) return;
        
        SetBusy(true);
        _statusLabel.Text = status;
        
        Task.Run(() =>
        {
            try
            {
                lock (_hardwareLock)
                {
                    var devices = GetSelectedModules();
                    if (devices.Count == 0)
                        throw new InvalidOperationException("No modules selected. Click 'Rescan Modules'.");
                    
                    action(devices);
                }
                
                Ui(() =>
                {
                    try
                    {
                        _statusLabel.Text = "Status: Completed";
                        RefreshStatus();
                    }
                    finally
                    {
                        SetBusy(false);
                    }
                });
            }
            catch (Exception ex)
            {
                Ui(() =>
                {
                    try
                    {
                        _statusLabel.Text = "Status: Failed";
                        MessageBox.Show(this, $"Command failed:\n{ex.Message}", "Error",
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                        RefreshStatus();
                    }
                    finally
                    {
                        SetBusy(false);
                    }
                });
            }
        });
    }

    private async void InstallAutostart()
    {
        using var timer = DiagnosticTimer.Start("InstallAutostart");
        
        SetBusy(true);
        try
        {
            await Task.Run(() => AutostartHelper.Install());
            
            // Reload status
            _autostartStatusCached = await AutostartHelper.IsInstalledAsync();
            
            MessageBox.Show(this,
                "Autostart installed successfully.\n\n" +
                "Task: XtreemRgbToggle.exe off at login\n" +
                "Priority: Highest\n" +
                "Delay: None\n\n" +
                "Note: Elevated scheduled tasks do not appear in\n" +
                "Task Manager Startup tab. Managed via Task Scheduler.",
                "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            
            RefreshStatus();
            RefreshAutostartButtons(false);
        }
        catch (Exception ex)
        {
            DiagnosticTimer.Log($"InstallAutostart failed: {ex.Message}");
            MessageBox.Show(this, $"Installation failed:\n{ex.Message}", "Error",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            RefreshAutostartButtons(false);
        }
        finally
        {
            SetBusy(false);
        }
    }

    private async void RemoveAutostart()
    {
        using var timer = DiagnosticTimer.Start("RemoveAutostart");
        
        SetBusy(true);
        try
        {
            await Task.Run(() => AutostartHelper.Uninstall());
            
            // Reload status
            _autostartStatusCached = await AutostartHelper.IsInstalledAsync();
            
            MessageBox.Show(this, 
                "Autostart removed successfully.", 
                "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            
            RefreshStatus();
            RefreshAutostartButtons(false);
        }
        catch (Exception ex)
        {
            DiagnosticTimer.Log($"RemoveAutostart failed: {ex.Message}");
            MessageBox.Show(this, $"Removal failed:\n{ex.Message}", "Error",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
            RefreshAutostartButtons(false);
        }
        finally
        {
            SetBusy(false);
        }
    }
    
    private void OnEffectChanged(object? sender, EventArgs e)
    {
        if (_cboEffect.SelectedIndex < 0 || _cboEffect.SelectedIndex >= XtreemModes.AllModes.Length)
            return;
        
        var mode = XtreemModes.AllModes[_cboEffect.SelectedIndex];
        
        _trackSpeed.Enabled = mode.SupportsSpeed;
        _valSpeed.Enabled = mode.SupportsSpeed;
        
        _chkDirectionReverse.Enabled = mode.SupportsDirection;
        
        UpdateSpeedLabel();
    }
    
    private void UpdateSpeedLabel()
    {
        _valSpeed.Text = _trackSpeed.Value switch
        {
            0 => "Fastest",
            1 => "Fast",
            2 => "Normal",
            3 => "Slow",
            4 => "Slowest",
            _ => "Normal"
        };
    }
    
    private byte GetSpeedValue()
    {
        return _trackSpeed.Value switch
        {
            0 => XtreemModes.SPEED_FASTEST,
            1 => XtreemModes.SPEED_FAST,
            2 => XtreemModes.SPEED_NORMAL,
            3 => XtreemModes.SPEED_SLOW,
            4 => XtreemModes.SPEED_SLOWEST,
            _ => XtreemModes.SPEED_NORMAL
        };
    }
}
