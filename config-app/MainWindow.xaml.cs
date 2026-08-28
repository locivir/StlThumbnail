using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Microsoft.Win32;

namespace StlThumbConfig;

public partial class MainWindow : Window
{
    // ---- native interop -------------------------------------------------
    [DllImport("StlThumbnail.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
    private static extern int StlRenderToFile(string stlPath, string bmpPath, int size, int[]? cfg);

    [DllImport("shell32.dll")]
    private static extern void SHChangeNotify(int eventId, uint flags, IntPtr item1, IntPtr item2);

    [DllImport("comdlg32.dll", CharSet = CharSet.Auto)]
    private static extern bool ChooseColor(ref CHOOSECOLOR cc);

    [StructLayout(LayoutKind.Sequential)]
    private struct CHOOSECOLOR
    {
        public int lStructSize;
        public IntPtr hwndOwner, hInstance;
        public uint rgbResult;
        public IntPtr lpCustColors;
        public uint Flags;
        public IntPtr lCustData, lpfnHook, lpTemplateName;
    }

    private const string RegKey = @"Software\StlThumbnail";

    private uint _modelColor = 0xD4AF37; // RGB
    private uint _bgColor = 0xFFFFFF;
    private string? _stlPath;
    private readonly string _previewBmp = Path.Combine(Path.GetTempPath(), "stlthumb_preview.bmp");
    private readonly DispatcherTimer _debounce;
    private bool _loading = true;
    private readonly IntPtr _custColors = Marshal.AllocHGlobal(16 * 4);

    public MainWindow()
    {
        InitializeComponent();
        // Make sure the DLL next to the exe (or in ..\build during dev) resolves.
        var exeDir = AppContext.BaseDirectory;
        foreach (var probe in new[] { exeDir, Path.GetFullPath(Path.Combine(exeDir, @"..\..\..\..\build")) })
            if (File.Exists(Path.Combine(probe, "StlThumbnail.dll"))) { SetDllDirectory(probe); break; }

        _debounce = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        _debounce.Tick += (_, _) => { _debounce.Stop(); RenderPreview(); };

        LoadFromRegistry();
        UpdateSwatches();
        _loading = false;

        // default sample: bundled torus if present
        var sample = Path.Combine(exeDir, "sample.stl");
        var devSample = @"D:\StlThumbnail\test\torus.stl";
        _stlPath = File.Exists(sample) ? sample : (File.Exists(devSample) ? devSample : null);
        StlName.Text = _stlPath != null ? Path.GetFileName(_stlPath) : "no STL loaded — click Open STL…";
        RenderPreview();
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern bool SetDllDirectory(string path);

    // ---- registry -------------------------------------------------------
    private void LoadFromRegistry()
    {
        using var k = Registry.CurrentUser.OpenSubKey(RegKey);
        if (k == null) return;
        int Get(string n, int d) => k.GetValue(n) is int v ? v : d;
        Yaw.Value = Get("Yaw", 30);
        Pitch.Value = Get("Pitch", 25);
        _modelColor = (uint)Get("ModelColor", unchecked((int)0xD4AF37));
        _bgColor = (uint)Get("BgColor", unchecked((int)0xFFFFFF));
        BgTransparent.IsChecked = Get("BgTransparent", 1) != 0;
        Ambient.Value = Get("Ambient", 30);
        Diffuse.Value = Get("Diffuse", 80);
        LightYaw.Value = Get("LightYaw", -40);
        LightPitch.Value = Get("LightPitch", 45);
    }

    private void SaveApply(object sender, RoutedEventArgs e)
    {
        using var k = Registry.CurrentUser.CreateSubKey(RegKey);
        k.SetValue("Yaw", (int)Yaw.Value, RegistryValueKind.DWord);
        k.SetValue("Pitch", (int)Pitch.Value, RegistryValueKind.DWord);
        k.SetValue("ModelColor", unchecked((int)_modelColor), RegistryValueKind.DWord);
        k.SetValue("BgColor", unchecked((int)_bgColor), RegistryValueKind.DWord);
        k.SetValue("BgTransparent", BgTransparent.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
        k.SetValue("Ambient", (int)Ambient.Value, RegistryValueKind.DWord);
        k.SetValue("Diffuse", (int)Diffuse.Value, RegistryValueKind.DWord);
        k.SetValue("LightYaw", (int)LightYaw.Value, RegistryValueKind.DWord);
        k.SetValue("LightPitch", (int)LightPitch.Value, RegistryValueKind.DWord);

        // Nudge Explorer: invalidate file association state so cached STL
        // thumbnails are regenerated with the new settings.
        SHChangeNotify(0x08000000 /*SHCNE_ASSOCCHANGED*/, 0x1000 /*SHCNF_IDLIST*/, IntPtr.Zero, IntPtr.Zero);
        Status.Text = "Saved. Explorer will regenerate STL thumbnails (existing cached ones may need a refresh with F5, or clear the thumbnail cache).";
    }

    private void ResetDefaults(object sender, RoutedEventArgs e)
    {
        _loading = true;
        Yaw.Value = 30; Pitch.Value = 25;
        _modelColor = 0xD4AF37; _bgColor = 0xFFFFFF;
        BgTransparent.IsChecked = true;
        Ambient.Value = 30; Diffuse.Value = 80;
        LightYaw.Value = -40; LightPitch.Value = 45;
        _loading = false;
        UpdateSwatches();
        RenderPreview();
    }

    // ---- UI events --------------------------------------------------------
    private void OnSetting(object sender, RoutedEventArgs e)
    {
        if (_loading) return;
        _debounce.Stop();
        _debounce.Start();
    }

    private void OpenStl(object sender, RoutedEventArgs e)
    {
        var dlg = new OpenFileDialog { Filter = "STL files (*.stl)|*.stl|All files|*.*" };
        if (dlg.ShowDialog(this) == true)
        {
            _stlPath = dlg.FileName;
            StlName.Text = Path.GetFileName(_stlPath);
            RenderPreview();
        }
    }

    private void PickModelColor(object sender, RoutedEventArgs e)
    {
        if (PickColor(ref _modelColor)) { UpdateSwatches(); RenderPreview(); }
    }

    private void PickBgColor(object sender, RoutedEventArgs e)
    {
        if (PickColor(ref _bgColor))
        {
            BgTransparent.IsChecked = false;
            UpdateSwatches();
            RenderPreview();
        }
    }

    private bool PickColor(ref uint rgb)
    {
        var cc = new CHOOSECOLOR
        {
            lStructSize = Marshal.SizeOf<CHOOSECOLOR>(),
            hwndOwner = new System.Windows.Interop.WindowInteropHelper(this).Handle,
            lpCustColors = _custColors,
            Flags = 0x103, // CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR
            rgbResult = ToColorref(rgb),
        };
        if (!ChooseColor(ref cc)) return false;
        rgb = FromColorref(cc.rgbResult);
        return true;
    }

    private static uint ToColorref(uint rgb) => ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | (rgb >> 16 & 0xFF);
    private static uint FromColorref(uint bgr) => ToColorref(bgr); // symmetric swap

    private void UpdateSwatches()
    {
        ModelSwatch.Fill = new SolidColorBrush(Color.FromRgb((byte)(_modelColor >> 16), (byte)(_modelColor >> 8), (byte)_modelColor));
        BgSwatch.Fill = new SolidColorBrush(Color.FromRgb((byte)(_bgColor >> 16), (byte)(_bgColor >> 8), (byte)_bgColor));
    }

    // ---- preview ----------------------------------------------------------
    private void RenderPreview()
    {
        if (_stlPath == null || !File.Exists(_stlPath)) return;
        var cfg = new int[]
        {
            (int)Yaw.Value, (int)Pitch.Value,
            unchecked((int)_modelColor), unchecked((int)_bgColor),
            BgTransparent.IsChecked == true ? 1 : 0,
            (int)Ambient.Value, (int)Diffuse.Value,
            (int)LightYaw.Value, (int)LightPitch.Value,
        };
        int rc = StlRenderToFile(_stlPath, _previewBmp, 512, cfg);
        if (rc != 0) { Status.Text = $"Render failed (code {rc})."; return; }

        var img = new BitmapImage();
        using (var fs = new FileStream(_previewBmp, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            img.BeginInit();
            img.CacheOption = BitmapCacheOption.OnLoad;
            img.StreamSource = fs;
            img.EndInit();
        }
        img.Freeze();
        Preview.Source = img;
        Status.Text = "";
    }
}
