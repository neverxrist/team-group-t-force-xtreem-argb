namespace XtreemRgbToggle;

internal sealed class TrayApplicationContext : ApplicationContext
{
    private readonly NotifyIcon _notifyIcon;
    private readonly ContextMenuStrip _contextMenu;
    private readonly XtreemService _svc = new();

    public TrayApplicationContext()
    {
        _contextMenu = new ContextMenuStrip();
        _contextMenu.Items.Add("Toggle Lights", null, OnToggle);
        _contextMenu.Items.Add("OFF", null, OnOff);
        _contextMenu.Items.Add("ON", null, OnOn);
        _contextMenu.Items.Add(new ToolStripSeparator());
        _contextMenu.Items.Add("Open GUI", null, OnOpenGui);
        _contextMenu.Items.Add(new ToolStripSeparator());
        _contextMenu.Items.Add("Install Autostart", null, OnInstallAutostart);
        _contextMenu.Items.Add("Remove Autostart", null, OnRemoveAutostart);
        _contextMenu.Items.Add(new ToolStripSeparator());
        _contextMenu.Items.Add("Exit", null, OnExit);

        _notifyIcon = new NotifyIcon
        {
            Icon = SystemIcons.Application,
            ContextMenuStrip = _contextMenu,
            Text = "XTREEM RGB Toggle",
            Visible = true
        };

        _notifyIcon.MouseClick += OnIconClick;
        UpdateAutostartMenu();
    }

    private void OnIconClick(object? sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Left)
        {
            DoToggle();
        }
    }

    private void OnToggle(object? sender, EventArgs e) => DoToggle();
    private void OnOff(object? sender, EventArgs e) => DoSet(wantOff: true);
    private void OnOn(object? sender, EventArgs e) => DoSet(wantOff: false);

    private void OnOpenGui(object? sender, EventArgs e)
    {
        try
        {
            var form = new RgbControlForm();
            form.ShowDialog();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to open GUI: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OnInstallAutostart(object? sender, EventArgs e)
    {
        try
        {
            AutostartHelper.Install();
            MessageBox.Show("Autostart installed successfully.", "Autostart", MessageBoxButtons.OK, MessageBoxIcon.Information);
            UpdateAutostartMenu();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to install autostart: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OnRemoveAutostart(object? sender, EventArgs e)
    {
        try
        {
            AutostartHelper.Uninstall();
            MessageBox.Show("Autostart removed successfully.", "Autostart", MessageBoxButtons.OK, MessageBoxIcon.Information);
            UpdateAutostartMenu();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to remove autostart: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OnExit(object? sender, EventArgs e)
    {
        _notifyIcon.Visible = false;
        Application.Exit();
    }

    private void DoToggle()
    {
        var current = StateStore.Load();
        bool goOff = current != LightState.Off;
        DoSet(goOff);
    }

    private void DoSet(bool wantOff)
    {
        Task.Run(() =>
        {
            try
            {
                var devices = _svc.DetectAndRemap(verbose: false);
                if (devices.Count == 0)
                {
                    _contextMenu.Invoke(() => MessageBox.Show("No XTREEM modules found.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error));
                    return;
                }

                if (wantOff)
                {
                    _svc.ApplyOff(devices, verbose: false);
                    StateStore.Save(LightState.Off);
                }
                else
                {
                    _svc.ApplyOn(devices, verbose: false);
                    StateStore.Save(LightState.On);
                }

                _contextMenu.Invoke(() => _notifyIcon.ShowBalloonTip(2000, "XTREEM RGB", wantOff ? "Lights OFF" : "Lights ON", ToolTipIcon.Info));
            }
            catch (Exception ex)
            {
                _contextMenu.Invoke(() => MessageBox.Show($"Error: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error));
            }
        });
    }

    private void UpdateAutostartMenu()
    {
        bool hasAutostart = AutostartHelper.IsInstalled();
        _contextMenu.Items[6].Enabled = !hasAutostart; // Install
        _contextMenu.Items[7].Enabled = hasAutostart;  // Remove
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _notifyIcon.Dispose();
            _contextMenu.Dispose();
        }
        base.Dispose(disposing);
    }
}
