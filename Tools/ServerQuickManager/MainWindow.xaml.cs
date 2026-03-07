using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using Microsoft.Win32;
using Forms = System.Windows.Forms;

namespace ServerQuickManager;

public partial class MainWindow : Window
{
    private static readonly Regex Ipv4Regex = new(@"(?<!\d)(?<prefix>[Ss]?)(?<ip>(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d))\b", RegexOptions.Compiled);
    private static readonly Regex SqlHintRegex = new(@"(?i)(Data Source|Initial Catalog|UID|User Id|PWD|Password|Server=|Database=|Trusted_Connection|SQLNCLI|SQLOLEDB)", RegexOptions.Compiled);
    private static readonly Regex SqlPasswordRegex = new(@"(?im)(?<key>PWD|Password|Pass|SqlPassword)(?<sep>\s*[:=]\s*)(?<quote>[""']?)(?<val>[^;\r\n""']*)(?<endquote>[""']?)", RegexOptions.Compiled);
    private static readonly HashSet<string> ScanExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".ini", ".xml", ".txt", ".cfg", ".json", ".dat"
    };
    private static readonly HashSet<string> IpScanExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".ini", ".xml", ".txt", ".cfg", ".json", ".dat"
    };
    private static readonly HashSet<string> IgnoredFolders = new(StringComparer.OrdinalIgnoreCase)
    {
        ".git", "bin", "obj", ".vs", "packages", "node_modules"
    };
    private static readonly HttpClient HttpClient = new()
    {
        Timeout = TimeSpan.FromSeconds(6)
    };
    private readonly ObservableCollection<IpFileEntry> _ipFiles = new();
    private readonly ObservableCollection<ProcessEntry> _processes = new();
    private readonly ObservableCollection<SqlConfigFileEntry> _sqlConfigFiles = new();
    private readonly Dictionary<ProcessEntry, Process> _runningProcesses = new();
    private readonly DispatcherTimer _monitorTimer = new();
    private readonly DispatcherTimer _sqlBackupTimer = new();
    private readonly string _settingsPath;
    private ToolSettings _settings = new();
    private string _repoRoot = string.Empty;
    private bool _isSqlBackupRunning;
    private string? _sqlCmdPath;
    private readonly Dictionary<ProcessEntry, DateTime> _lastStartAttempts = new();
    private string? _localIpDetected;
    private string? _externalIpDetected;

    public MainWindow()
    {
        InitializeComponent();
        IpFilesGrid.ItemsSource = _ipFiles;
        ProcessesGrid.ItemsSource = _processes;
        SqlConfigFilesGrid.ItemsSource = _sqlConfigFiles;

        _repoRoot = DetectRepositoryRoot();
        ServerRootTextBox.Text = _repoRoot;

        _settingsPath = Path.Combine(AppContext.BaseDirectory, "serverquickmanager.settings.json");
        LoadSettings();

        _monitorTimer.Interval = TimeSpan.FromSeconds(3);
        _monitorTimer.Tick += MonitorTick;
        _monitorTimer.Start();

        _sqlBackupTimer.Tick += SqlBackupTick;
        RefreshSqlBackupTimer();
        EnableAutoBackupCheckBox.Checked += (_, _) => RefreshSqlBackupTimer();
        EnableAutoBackupCheckBox.Unchecked += (_, _) => RefreshSqlBackupTimer();
        AutoBackupMinutesTextBox.LostFocus += (_, _) => RefreshSqlBackupTimer();
        UseIntegratedAuthCheckBox.Checked += (_, _) => UpdateSqlAuthUi();
        UseIntegratedAuthCheckBox.Unchecked += (_, _) => UpdateSqlAuthUi();

        Closed += OnClosed;

        if (_settings.AutoRunOnLaunch && _settings.IpConfiguredAtUtc.HasValue)
        {
            Dispatcher.BeginInvoke(StartEnabledProcesses, DispatcherPriority.Background);
        }
    }

    private void PickRootClick(object sender, RoutedEventArgs e)
    {
        using var picker = new Forms.FolderBrowserDialog
        {
            Description = "Выберите корневую папку сервера",
            SelectedPath = Directory.Exists(_repoRoot) ? _repoRoot : string.Empty,
            ShowNewFolderButton = false,
            UseDescriptionForTitle = true
        };

        if (picker.ShowDialog() != Forms.DialogResult.OK)
        {
            return;
        }

        if (string.IsNullOrWhiteSpace(picker.SelectedPath))
        {
            return;
        }

        _repoRoot = ResolveServerRootFromPath(picker.SelectedPath);
        ServerRootTextBox.Text = _repoRoot;
        _settings.ServerRoot = _repoRoot;
        SaveSettings();
        StatusText.Text = $"Корень сервера: {_repoRoot}";
    }

    private async void ScanClick(object sender, RoutedEventArgs e)
    {
        if (Directory.Exists(_repoRoot) == false)
        {
            StatusText.Text = "Папка сервера не найдена";
            return;
        }

        StatusText.Text = "Сканирование IP...";
        ScanButton.IsEnabled = false;

        try
        {
            var found = await Task.Run(() =>
            {
                var list = new List<IpFileEntry>();
                foreach (var file in EnumerateIpConfigFiles(_repoRoot))
                {
                    if (TryReadFile(file, out var content, out _) == false || string.IsNullOrWhiteSpace(content))
                    {
                        continue;
                    }

                    var matches = Ipv4Regex.Matches(content);
                    if (matches.Count == 0)
                    {
                        continue;
                    }

                    list.Add(new IpFileEntry
                    {
                        IsSelected = true,
                        AbsolutePath = file,
                        RelativePath = Path.GetRelativePath(_repoRoot, file),
                        MatchCount = matches.Count,
                        SampleIp = matches[0].Value
                    });
                }

                return list;
            });

            _ipFiles.Clear();
            foreach (var item in found)
            {
                _ipFiles.Add(item);
            }
        }
        finally
        {
            ScanButton.IsEnabled = true;
        }

        StatusText.Text = $"Сканирование завершено. Файлов с IP: {_ipFiles.Count}";
    }

    private void ApplyIpClick(object sender, RoutedEventArgs e)
    {
        var targetIp = PublicIpTextBox.Text.Trim();

        if (IPAddress.TryParse(targetIp, out var parsed) == false || parsed.AddressFamily != AddressFamily.InterNetwork)
        {
            StatusText.Text = "Введите корректный IPv4";
            return;
        }

        var selected = _ipFiles.Where(x => x.IsSelected).ToList();
        if (selected.Count == 0)
        {
            StatusText.Text = "Нет выбранных файлов";
            return;
        }

        var changedFiles = 0;
        var changedValues = 0;

        foreach (var entry in selected)
        {
            if (TryReadFile(entry.AbsolutePath, out var content, out var encoding) == false)
            {
                continue;
            }

            var localChanged = 0;
            var replaced = Ipv4Regex.Replace(content, match =>
            {
                var prefix = match.Groups["prefix"].Value;
                var ipValue = match.Groups["ip"].Value;
                if (SkipLocalIpCheckBox.IsChecked == true && (ipValue == "127.0.0.1" || ipValue == "0.0.0.0"))
                {
                    return match.Value;
                }

                if (ipValue == targetIp)
                {
                    return match.Value;
                }

                localChanged++;
                return $"{prefix}{targetIp}";
            });

            if (localChanged == 0)
            {
                continue;
            }

            if (CreateBackupCheckBox.IsChecked == true)
            {
                BackupFile(entry.AbsolutePath, "_backup_iptool");
            }

            File.WriteAllText(entry.AbsolutePath, replaced, encoding);
            changedFiles++;
            changedValues += localChanged;
        }

        if (changedFiles > 0)
        {
            _settings.IpConfiguredAtUtc = DateTime.UtcNow;
            SaveSettings();
        }

        StatusText.Text = $"Готово. Изменено файлов: {changedFiles}, замен IP: {changedValues}";
    }

    private async void DetectIpClick(object sender, RoutedEventArgs e)
    {
        DetectIpButton.IsEnabled = false;
        StatusText.Text = "Определение IP...";

        try
        {
            _localIpDetected = GetLocalIpAddress();
            _externalIpDetected = await GetExternalIpAsync();
            UpdateDetectedIpUi();
        }
        finally
        {
            DetectIpButton.IsEnabled = true;
        }

        if (string.IsNullOrWhiteSpace(_localIpDetected) && string.IsNullOrWhiteSpace(_externalIpDetected))
        {
            StatusText.Text = "IP не найден";
        }
        else
        {
            StatusText.Text = "IP определен";
        }
    }

    private void DetectedIpChoiceChanged(object sender, RoutedEventArgs e)
    {
        ApplyDetectedIpSelection();
    }

    private void UpdateDetectedIpUi()
    {
        LocalIpText.Text = _localIpDetected ?? string.Empty;
        ExternalIpText.Text = _externalIpDetected ?? string.Empty;

        if (string.IsNullOrWhiteSpace(_externalIpDetected) == false)
        {
            UseExternalIpRadio.IsChecked = true;
        }
        else if (string.IsNullOrWhiteSpace(_localIpDetected) == false)
        {
            UseLocalIpRadio.IsChecked = true;
        }

        ApplyDetectedIpSelection();
    }

    private void ApplyDetectedIpSelection()
    {
        if (UseExternalIpRadio.IsChecked == true && string.IsNullOrWhiteSpace(_externalIpDetected) == false)
        {
            PublicIpTextBox.Text = _externalIpDetected;
        }
        else if (UseLocalIpRadio.IsChecked == true && string.IsNullOrWhiteSpace(_localIpDetected) == false)
        {
            PublicIpTextBox.Text = _localIpDetected;
        }
    }

    private static string? GetLocalIpAddress()
    {
        try
        {
            var host = Dns.GetHostEntry(Dns.GetHostName());
            foreach (var address in host.AddressList)
            {
                if (address.AddressFamily != AddressFamily.InterNetwork)
                {
                    continue;
                }

                var text = address.ToString();
                if (text.StartsWith("169.254.", StringComparison.Ordinal))
                {
                    continue;
                }

                return text;
            }
        }
        catch
        {
        }

        return null;
    }

    private static async Task<string?> GetExternalIpAsync()
    {
        try
        {
            var text = await HttpClient.GetStringAsync("https://api.ipify.org");
            var value = text.Trim();
            if (IPAddress.TryParse(value, out var parsed) && parsed.AddressFamily == AddressFamily.InterNetwork)
            {
                return value;
            }
        }
        catch
        {
        }

        return null;
    }

    private void DetectSqlServerClick(object sender, RoutedEventArgs e)
    {
        try
        {
            var servers = DetectSqlServers().ToList();
            if (servers.Count == 0)
            {
                StatusText.Text = "SQL Server не найден";
                return;
            }

            SqlServerTextBox.Text = servers[0];
            StatusText.Text = $"SQL Server: {string.Join(", ", servers)}";
        }
        catch (Exception ex)
        {
            StatusText.Text = $"SQL автоопределение ошибка: {ex.Message}";
        }
    }

    private static IEnumerable<string> DetectSqlServers()
    {
        var results = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        AddSqlInstancesFromRegistry(RegistryHive.LocalMachine, RegistryView.Registry64, results);
        AddSqlInstancesFromRegistry(RegistryHive.LocalMachine, RegistryView.Registry32, results);
        AddSqlInstancesFromRegistry(RegistryHive.CurrentUser, RegistryView.Registry64, results);
        AddSqlInstancesFromRegistry(RegistryHive.CurrentUser, RegistryView.Registry32, results);
        AddInstalledSqlInstances(RegistryHive.LocalMachine, RegistryView.Registry64, results);
        AddInstalledSqlInstances(RegistryHive.LocalMachine, RegistryView.Registry32, results);
        AddInstalledSqlInstances(RegistryHive.CurrentUser, RegistryView.Registry64, results);
        AddInstalledSqlInstances(RegistryHive.CurrentUser, RegistryView.Registry32, results);

        var ordered = results
            .OrderBy(value => value == "." ? 0 : 1)
            .ThenBy(value => value, StringComparer.OrdinalIgnoreCase)
            .ToList();

        return ordered;
    }

    private static void AddInstalledSqlInstances(RegistryHive hive, RegistryView view, HashSet<string> results)
    {
        try
        {
            using var baseKey = RegistryKey.OpenBaseKey(hive, view);
            using var key = baseKey.OpenSubKey(@"SOFTWARE\Microsoft\Microsoft SQL Server");
            var instances = key?.GetValue("InstalledInstances") as string[];
            if (instances == null)
            {
                return;
            }

            foreach (var name in instances)
            {
                if (string.Equals(name, "MSSQLSERVER", StringComparison.OrdinalIgnoreCase))
                {
                    results.Add(".");
                }
                else if (string.IsNullOrWhiteSpace(name) == false)
                {
                    results.Add(@".\" + name);
                }
            }
        }
        catch
        {
        }
    }

    private static void AddSqlInstancesFromRegistry(RegistryHive hive, RegistryView view, HashSet<string> results)
    {
        try
        {
            using var baseKey = RegistryKey.OpenBaseKey(hive, view);
            using var key = baseKey.OpenSubKey(@"SOFTWARE\Microsoft\Microsoft SQL Server\Instance Names\SQL");
            if (key == null)
            {
                return;
            }

            foreach (var name in key.GetValueNames())
            {
                if (string.Equals(name, "MSSQLSERVER", StringComparison.OrdinalIgnoreCase))
                {
                    results.Add(".");
                }
                else if (string.IsNullOrWhiteSpace(name) == false)
                {
                    results.Add(@".\" + name);
                }
            }
        }
        catch
        {
        }
    }

    private void AddExeClick(object sender, RoutedEventArgs e)
    {
        var picker = new Microsoft.Win32.OpenFileDialog
        {
            Filter = "Executable (*.exe)|*.exe",
            CheckFileExists = true,
            Multiselect = true
        };

        if (Directory.Exists(_repoRoot))
        {
            picker.InitialDirectory = _repoRoot;
        }

        if (picker.ShowDialog(this) != true)
        {
            return;
        }

        foreach (var file in picker.FileNames)
        {
            if (_processes.Any(x => string.Equals(ResolveAbsolutePath(x), file, StringComparison.OrdinalIgnoreCase)))
            {
                continue;
            }

            _processes.Add(new ProcessEntry
            {
                IsEnabled = true,
                AutoRestart = true,
                Name = Path.GetFileNameWithoutExtension(file),
                RelativePath = ToDisplayPath(file),
                Status = "Stopped"
            });
        }

        SaveSettings();
        StatusText.Text = $"Добавлено процессов: {picker.FileNames.Length}";
    }

    private void RemoveExeClick(object sender, RoutedEventArgs e)
    {
        if (ProcessesGrid.SelectedItem is not ProcessEntry entry)
        {
            StatusText.Text = "Выберите процесс";
            return;
        }

        StopProcess(entry);
        _processes.Remove(entry);
        SaveSettings();
        StatusText.Text = "Процесс удален из списка";
    }

    private void StartAllClick(object sender, RoutedEventArgs e)
    {
        StartEnabledProcesses();
    }

    private void StopAllClick(object sender, RoutedEventArgs e)
    {
        foreach (var process in _processes.ToList())
        {
            StopProcess(process);
        }

        StatusText.Text = "Все процессы остановлены";
    }

    private void SaveProcessesClick(object sender, RoutedEventArgs e)
    {
        SaveSettings();
        StatusText.Text = "Список процессов сохранен";
    }

    private void AutoStartSettingChanged(object sender, RoutedEventArgs e)
    {
        _settings.AutoRunOnLaunch = EnableAutoRunOnLaunchCheckBox.IsChecked == true;
        SaveSettings();
    }

    private void MonitorTick(object? sender, EventArgs e)
    {
        foreach (var entry in _processes.ToList())
        {
            if (_runningProcesses.TryGetValue(entry, out var process))
            {
                if (process.HasExited)
                {
                    _runningProcesses.Remove(entry);
                    entry.Status = "Stopped";

                    if (entry.IsEnabled && entry.AutoRestart)
                    {
                        StartProcess(entry);
                    }
                }
                else
                {
                    entry.Status = "Running";
                }
            }
            else
            {
                entry.Status = File.Exists(ResolveAbsolutePath(entry)) ? "Stopped" : "Missing";
                if (entry.IsEnabled && entry.AutoRestart)
                {
                    StartProcess(entry);
                }
            }
        }
    }

    private void StartEnabledProcesses()
    {
        var started = 0;
        foreach (var entry in _processes.Where(x => x.IsEnabled))
        {
            if (StartProcess(entry))
            {
                started++;
            }
        }

        StatusText.Text = $"Запущено процессов: {started}";
    }

    private bool StartProcess(ProcessEntry entry)
    {
        if (_runningProcesses.TryGetValue(entry, out var current) && current.HasExited == false)
        {
            entry.Status = "Running";
            return false;
        }

        if (IsStartCooldownActive(entry))
        {
            entry.Status = "Cooldown";
            return false;
        }

        var absolutePath = ResolveAbsolutePath(entry);
        if (File.Exists(absolutePath) == false)
        {
            entry.Status = "Missing";
            return false;
        }

        try
        {
            var process = new Process
            {
                StartInfo = new ProcessStartInfo
                {
                    FileName = absolutePath,
                    WorkingDirectory = Path.GetDirectoryName(absolutePath) ?? _repoRoot,
                    UseShellExecute = true
                },
                EnableRaisingEvents = true
            };

            if (process.Start() == false)
            {
                entry.Status = "StartFail";
                return false;
            }

            _lastStartAttempts[entry] = DateTime.UtcNow;

            process.Exited += (_, _) =>
            {
                Dispatcher.Invoke(() =>
                {
                    _runningProcesses.Remove(entry);
                    entry.Status = "Stopped";

                    if (entry.IsEnabled && entry.AutoRestart)
                    {
                        StartProcess(entry);
                    }
                });
            };

            _runningProcesses[entry] = process;
            entry.Status = "Running";
            return true;
        }
        catch
        {
            entry.Status = "StartFail";
            return false;
        }
    }

    private void StopProcess(ProcessEntry entry)
    {
        if (_runningProcesses.TryGetValue(entry, out var process) == false)
        {
            entry.Status = "Stopped";
            return;
        }

        try
        {
            if (process.HasExited == false)
            {
                process.Kill(true);
            }
        }
        catch
        {
        }

        _runningProcesses.Remove(entry);
        entry.Status = "Stopped";
    }

    private async void TestSqlConnectionClick(object sender, RoutedEventArgs e)
    {
        try
        {
            if (EnsureSqlCmdAvailable() == false)
            {
                return;
            }
            StatusText.Text = "Проверка SQL...";
            var result = await RunSqlQueryAsync("SELECT 1 AS Ping");
            if (result.ok)
            {
                StatusText.Text = "SQL подключение успешно";
            }
            else
            {
                StatusText.Text = $"SQL ошибка: {result.error}";
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"SQL ошибка: {ex.Message}";
        }
    }

    private void PickBackupFolderClick(object sender, RoutedEventArgs e)
    {
        using var picker = new Forms.FolderBrowserDialog
        {
            Description = "Выберите папку для backup",
            SelectedPath = Directory.Exists(_repoRoot) ? _repoRoot : string.Empty,
            ShowNewFolderButton = true,
            UseDescriptionForTitle = true
        };

        if (picker.ShowDialog() != Forms.DialogResult.OK)
        {
            return;
        }

        if (string.IsNullOrWhiteSpace(picker.SelectedPath))
        {
            return;
        }

        BackupFolderTextBox.Text = picker.SelectedPath;
        SaveSettings();
    }

    private void SaveSqlSettingsClick(object sender, RoutedEventArgs e)
    {
        SaveSettings();
        RefreshSqlBackupTimer();
        StatusText.Text = "SQL настройки сохранены";
    }

    private async void BackupNowClick(object sender, RoutedEventArgs e)
    {
        if (await BackupDatabasesAsync("manual") == false)
        {
            return;
        }

        StatusText.Text = "Backup завершен";
    }

    private async void SqlBackupTick(object? sender, EventArgs e)
    {
        await BackupDatabasesAsync("auto");
    }

    private async Task<bool> BackupDatabasesAsync(string mode)
    {
        if (_isSqlBackupRunning)
        {
            return false;
        }
        if (EnsureSqlCmdAvailable() == false)
        {
            return false;
        }

        var backupFolder = BackupFolderTextBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(backupFolder))
        {
            StatusText.Text = "Укажите папку backup";
            return false;
        }

        Directory.CreateDirectory(backupFolder);

        var databases = ParseDatabaseList(DatabaseListTextBox.Text);
        if (databases.Count == 0)
        {
            StatusText.Text = "Укажите хотя бы одну базу";
            return false;
        }

        _isSqlBackupRunning = true;
        try
        {
            var okCount = await Task.Run(() =>
            {
                var count = 0;
                foreach (var database in databases)
                {
                    var fileName = $"{database}_{DateTime.Now:yyyyMMdd_HHmmss}_{mode}.bak";
                    var filePath = Path.Combine(backupFolder, fileName);
                    var sql = $"BACKUP DATABASE [{database}] TO DISK = N'{EscapeSqlLiteral(filePath)}' WITH INIT, CHECKSUM";
                    if (RunSqlQuery(sql, out _, out _))
                    {
                        count++;
                    }
                }

                return count;
            });

            StatusText.Text = $"Backup баз: {okCount}/{databases.Count}";
            return okCount == databases.Count;
        }
        finally
        {
            _isSqlBackupRunning = false;
        }
    }

    private void PickBakFileClick(object sender, RoutedEventArgs e)
    {
        var picker = new Microsoft.Win32.OpenFileDialog
        {
            Filter = "Backup (*.bak)|*.bak|All files (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };

        var backupFolder = BackupFolderTextBox.Text.Trim();
        if (Directory.Exists(backupFolder))
        {
            picker.InitialDirectory = backupFolder;
        }
        else if (Directory.Exists(_repoRoot))
        {
            picker.InitialDirectory = _repoRoot;
        }

        if (picker.ShowDialog(this) != true)
        {
            return;
        }

        RestoreBakPathTextBox.Text = picker.FileName;
    }

    private async void RestoreDatabaseClick(object sender, RoutedEventArgs e)
    {
        if (EnsureSqlCmdAvailable() == false)
        {
            return;
        }
        var bakPath = RestoreBakPathTextBox.Text.Trim();
        var database = RestoreDatabaseTextBox.Text.Trim();

        if (File.Exists(bakPath) == false)
        {
            StatusText.Text = "Файл .bak не найден";
            return;
        }

        if (string.IsNullOrWhiteSpace(database))
        {
            StatusText.Text = "Укажите базу для восстановления";
            return;
        }

        var sql = $"ALTER DATABASE [{database}] SET SINGLE_USER WITH ROLLBACK IMMEDIATE; RESTORE DATABASE [{database}] FROM DISK = N'{EscapeSqlLiteral(bakPath)}' WITH REPLACE, RECOVERY; ALTER DATABASE [{database}] SET MULTI_USER;";
        StatusText.Text = $"Restore: {database}...";
        var result = await RunSqlQueryAsync(sql);
        if (result.ok)
        {
            StatusText.Text = $"Restore завершен: {database}";
        }
        else
        {
            StatusText.Text = $"Restore ошибка: {result.error}";
        }
    }

    private void ApplyRegistryClick(object sender, RoutedEventArgs e)
    {
        try
        {
            var sqlRoot = Registry.CurrentUser.CreateSubKey(@"Software\RunixMU\ServerQuickManager\Sql");
            sqlRoot?.SetValue("Server", SqlServerTextBox.Text.Trim());
            sqlRoot?.SetValue("Login", SqlLoginTextBox.Text.Trim());
            sqlRoot?.SetValue("UseIntegratedAuth", UseIntegratedAuthCheckBox.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
            sqlRoot?.SetValue("BackupFolder", BackupFolderTextBox.Text.Trim());
            sqlRoot?.SetValue("Databases", DatabaseListTextBox.Text.Trim());

            var odbcSource = Registry.CurrentUser.CreateSubKey(@"Software\ODBC\ODBC.INI\ODBC Data Sources");
            odbcSource?.SetValue("RunixMuMSSQL", "SQL Server");

            var odbcDsn = Registry.CurrentUser.CreateSubKey(@"Software\ODBC\ODBC.INI\RunixMuMSSQL");
            odbcDsn?.SetValue("Database", ParseDatabaseList(DatabaseListTextBox.Text).FirstOrDefault() ?? "master");
            odbcDsn?.SetValue("Driver", "SQL Server");
            odbcDsn?.SetValue("Server", SqlServerTextBox.Text.Trim());
            odbcDsn?.SetValue("LastUser", SqlLoginTextBox.Text.Trim());

            SaveSettings();
            StatusText.Text = "Реестр обновлен";
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Ошибка реестра: {ex.Message}";
        }
    }

    private async void ScanSqlConfigClick(object sender, RoutedEventArgs e)
    {
        if (Directory.Exists(_repoRoot) == false)
        {
            StatusText.Text = "Папка сервера не найдена";
            return;
        }

        StatusText.Text = "Сканирование SQL конфигов...";
        ScanSqlConfigButton.IsEnabled = false;
        try
        {
            var found = await Task.Run(() =>
            {
                var list = new List<SqlConfigFileEntry>();
                foreach (var file in EnumerateConfigFiles(_repoRoot))
                {
                    if (TryReadFile(file, out var content, out _) == false || string.IsNullOrWhiteSpace(content))
                    {
                        continue;
                    }

                    var hintMatches = SqlHintRegex.Matches(content);
                    if (hintMatches.Count == 0)
                    {
                        continue;
                    }

                    list.Add(new SqlConfigFileEntry
                    {
                        IsSelected = true,
                        AbsolutePath = file,
                        RelativePath = Path.GetRelativePath(_repoRoot, file),
                        MatchCount = hintMatches.Count,
                        SampleHint = hintMatches[0].Value
                    });
                }

                return list;
            });

            _sqlConfigFiles.Clear();
            foreach (var item in found)
            {
                _sqlConfigFiles.Add(item);
            }
        }
        finally
        {
            ScanSqlConfigButton.IsEnabled = true;
        }

        StatusText.Text = $"Найдено SQL конфигов: {_sqlConfigFiles.Count}";
    }

    private void ApplySqlPasswordInConfigsClick(object sender, RoutedEventArgs e)
    {
        var newPassword = NewSqlConfigPasswordBox.Password;
        var oldPassword = OldSqlConfigPasswordBox.Password;

        if (string.IsNullOrWhiteSpace(newPassword))
        {
            StatusText.Text = "Введите новый пароль";
            return;
        }

        var selected = _sqlConfigFiles.Where(x => x.IsSelected).ToList();
        if (selected.Count == 0)
        {
            StatusText.Text = "Нет выбранных SQL конфигов";
            return;
        }

        var changedFiles = 0;
        var changedPasswords = 0;

        foreach (var file in selected)
        {
            if (TryReadFile(file.AbsolutePath, out var content, out var encoding) == false)
            {
                continue;
            }

            var localChanges = 0;
            var updated = SqlPasswordRegex.Replace(content, match =>
            {
                var key = match.Groups["key"].Value;
                var sep = match.Groups["sep"].Value;
                var quote = match.Groups["quote"].Value;
                var endQuote = match.Groups["endquote"].Value;
                var value = match.Groups["val"].Value.Trim();
                var trimmed = value.Trim('"', '\'');

                if (string.IsNullOrWhiteSpace(oldPassword) == false && string.Equals(trimmed, oldPassword, StringComparison.Ordinal) == false)
                {
                    return match.Value;
                }

                localChanges++;
                return $"{key}{sep}{quote}{newPassword}{endQuote}";
            });

            if (localChanges == 0)
            {
                continue;
            }

            BackupFile(file.AbsolutePath, "_backup_sqlconfig");
            File.WriteAllText(file.AbsolutePath, updated, encoding);
            changedFiles++;
            changedPasswords += localChanges;
        }

        SqlPasswordBox.Password = newPassword;
        SaveSettings();
        StatusText.Text = $"Обновлено SQL паролей: {changedPasswords} в файлах: {changedFiles}";
    }

    private async Task<(bool ok, string error)> RunSqlQueryAsync(string query)
    {
        return await Task.Run(() =>
        {
            var ok = RunSqlQuery(query, out _, out var error);
            return (ok, error);
        });
    }

    private bool RunSqlQuery(string query, out string output, out string error)
    {
        output = string.Empty;
        error = string.Empty;

        var server = SqlServerTextBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(server))
        {
            error = "SQL Server пуст";
            return false;
        }

        var useIntegratedAuth = UseIntegratedAuthCheckBox.IsChecked == true;
        var login = SqlLoginTextBox.Text.Trim();
        var password = SqlPasswordBox.Password;

        if (!useIntegratedAuth && (string.IsNullOrWhiteSpace(login) || string.IsNullOrWhiteSpace(password)))
        {
            error = "Укажите SQL логин и пароль";
            return false;
        }

        var sqlcmd = ResolveSqlCmdPath();
        if (string.IsNullOrWhiteSpace(sqlcmd))
        {
            error = "sqlcmd не найден";
            return false;
        }

        var args = new StringBuilder();
        args.Append("-b ");
        args.Append("-S ").Append(QuoteArgument(server)).Append(' ');
        if (useIntegratedAuth)
        {
            args.Append("-E ");
        }
        else
        {
            args.Append("-U ").Append(QuoteArgument(login)).Append(' ');
            args.Append("-P ").Append(QuoteArgument(password)).Append(' ');
        }
        args.Append("-Q ").Append(QuoteArgument(query));

        try
        {
            using var process = new Process();
            process.StartInfo = new ProcessStartInfo
            {
                FileName = sqlcmd,
                Arguments = args.ToString(),
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            process.Start();
            output = process.StandardOutput.ReadToEnd();
            error = process.StandardError.ReadToEnd();
            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                if (string.IsNullOrWhiteSpace(error))
                {
                    error = output;
                }
                return false;
            }

            return true;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    private static string QuoteArgument(string value)
    {
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }

    private static string EscapeSqlLiteral(string value)
    {
        return value.Replace("'", "''");
    }

    private static List<string> ParseDatabaseList(string value)
    {
        return value
            .Split([';', ',', '\n', '\r'], StringSplitOptions.RemoveEmptyEntries)
            .Select(x => x.Trim())
            .Where(x => x.Length > 0)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    private void RefreshSqlBackupTimer()
    {
        if (EnableAutoBackupCheckBox.IsChecked != true)
        {
            _sqlBackupTimer.Stop();
            return;
        }

        if (int.TryParse(AutoBackupMinutesTextBox.Text.Trim(), out var minutes) == false || minutes <= 0)
        {
            _sqlBackupTimer.Stop();
            return;
        }

        _sqlBackupTimer.Interval = TimeSpan.FromMinutes(minutes);
        _sqlBackupTimer.Start();
    }

    private bool IsStartCooldownActive(ProcessEntry entry)
    {
        if (_lastStartAttempts.TryGetValue(entry, out var lastAttempt))
        {
            if ((DateTime.UtcNow - lastAttempt) < TimeSpan.FromSeconds(10))
            {
                return true;
            }
        }

        return false;
    }

    private void OnClosed(object? sender, EventArgs e)
    {
        SaveSettings();
        _monitorTimer.Stop();
        _sqlBackupTimer.Stop();
    }

    private void UpdateSqlAuthUi()
    {
        var integrated = UseIntegratedAuthCheckBox.IsChecked == true;
        SqlLoginTextBox.IsEnabled = integrated == false;
        SqlPasswordBox.IsEnabled = integrated == false;
    }

    private IEnumerable<string> EnumerateConfigFiles(string root)
    {
        var stack = new Stack<string>();
        stack.Push(root);

        while (stack.Count > 0)
        {
            var current = stack.Pop();
            IEnumerable<string> directories;
            IEnumerable<string> files;

            try
            {
                directories = Directory.EnumerateDirectories(current);
                files = Directory.EnumerateFiles(current);
            }
            catch
            {
                continue;
            }

            foreach (var dir in directories)
            {
                var name = Path.GetFileName(dir);
                if (IgnoredFolders.Contains(name))
                {
                    continue;
                }

                stack.Push(dir);
            }

            foreach (var file in files)
            {
                var extension = Path.GetExtension(file);
                if (ScanExtensions.Contains(extension) == false)
                {
                    continue;
                }

                FileInfo info;
                try
                {
                    info = new FileInfo(file);
                }
                catch
                {
                    continue;
                }

                if (info.Length > 4 * 1024 * 1024)
                {
                    continue;
                }

                yield return file;
            }
        }
    }

    private IEnumerable<string> EnumerateIpConfigFiles(string root)
    {
        foreach (var file in EnumerateConfigFiles(root))
        {
            if (IsTargetIpConfigFile(file))
            {
                yield return file;
            }
        }
    }

    private static bool IsTargetIpConfigFile(string path)
    {
        var name = Path.GetFileNameWithoutExtension(path);
        if (string.IsNullOrWhiteSpace(name))
        {
            return false;
        }

        var extension = Path.GetExtension(path);
        if (IpScanExtensions.Contains(extension) == false)
        {
            return false;
        }

        return string.Equals(name, "MapServerInfo", StringComparison.OrdinalIgnoreCase)
               || string.Equals(name, "ServerList", StringComparison.OrdinalIgnoreCase);
    }

    private static bool TryReadFile(string path, out string content, out Encoding encoding)
    {
        content = string.Empty;
        encoding = Encoding.UTF8;

        try
        {
            using var reader = new StreamReader(path, detectEncodingFromByteOrderMarks: true);
            content = reader.ReadToEnd();
            encoding = reader.CurrentEncoding;
            return true;
        }
        catch
        {
            return false;
        }
    }

    private void BackupFile(string path, string backupFolderName)
    {
        var stamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
        var relative = Path.GetRelativePath(_repoRoot, path);
        var backupPath = Path.Combine(_repoRoot, backupFolderName, stamp, relative);
        var backupDirectory = Path.GetDirectoryName(backupPath);

        if (string.IsNullOrWhiteSpace(backupDirectory) == false)
        {
            Directory.CreateDirectory(backupDirectory);
        }

        File.Copy(path, backupPath, true);
    }

    private string ResolveAbsolutePath(ProcessEntry entry)
    {
        if (Path.IsPathRooted(entry.RelativePath))
        {
            return entry.RelativePath;
        }

        var value = entry.RelativePath.Replace('/', '\\');
        if (value.StartsWith(".\\"))
        {
            return Path.GetFullPath(Path.Combine(_repoRoot, value[2..]));
        }

        return Path.GetFullPath(Path.Combine(_repoRoot, value));
    }

    private string ToDisplayPath(string absolutePath)
    {
        try
        {
            var relative = Path.GetRelativePath(_repoRoot, absolutePath);
            if (relative.StartsWith(".."))
            {
                return absolutePath;
            }

            return $".\\{relative}";
        }
        catch
        {
            return absolutePath;
        }
    }

    private string DetectRepositoryRoot()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);

        while (current != null)
        {
            var hasGameServer = Directory.Exists(Path.Combine(current.FullName, "GameServer"));
            var hasJoinServer = Directory.Exists(Path.Combine(current.FullName, "JoinServer"));
            var hasTools = Directory.Exists(Path.Combine(current.FullName, "Tools"));

            if (hasGameServer && hasJoinServer && hasTools)
            {
                return current.FullName;
            }

            current = current.Parent;
        }

        return AppContext.BaseDirectory;
    }

    private string ResolveServerRootFromPath(string path)
    {
        var directory = new DirectoryInfo(path);
        while (directory != null)
        {
            var hasGameServer = Directory.Exists(Path.Combine(directory.FullName, "GameServer"));
            var hasJoinServer = Directory.Exists(Path.Combine(directory.FullName, "JoinServer"));
            var hasTools = Directory.Exists(Path.Combine(directory.FullName, "Tools"));
            if (hasGameServer && hasJoinServer && hasTools)
            {
                return directory.FullName;
            }

            directory = directory.Parent;
        }

        return path;
    }

    private void LoadSettings()
    {
        try
        {
            if (File.Exists(_settingsPath))
            {
                var json = File.ReadAllText(_settingsPath);
                var loaded = JsonSerializer.Deserialize<ToolSettings>(json);
                if (loaded != null)
                {
                    _settings = loaded;
                }
            }
        }
        catch
        {
            _settings = new ToolSettings();
        }

        if (string.IsNullOrWhiteSpace(_settings.ServerRoot) == false && Directory.Exists(_settings.ServerRoot))
        {
            _repoRoot = _settings.ServerRoot;
            ServerRootTextBox.Text = _repoRoot;
        }
        else
        {
            _settings.ServerRoot = _repoRoot;
        }

        EnableAutoRunOnLaunchCheckBox.IsChecked = _settings.AutoRunOnLaunch;
        PublicIpTextBox.Text = _settings.LastPublicIp ?? string.Empty;
        SqlServerTextBox.Text = string.IsNullOrWhiteSpace(_settings.SqlServer) ? @".\SQLEXPRESS" : _settings.SqlServer;
        SqlLoginTextBox.Text = string.IsNullOrWhiteSpace(_settings.SqlLogin) ? "sa" : _settings.SqlLogin;
        var restoredPassword = string.Empty;
        if (string.IsNullOrWhiteSpace(_settings.SqlPasswordProtected) == false)
        {
            restoredPassword = UnprotectString(_settings.SqlPasswordProtected);
        }
        else if (string.IsNullOrWhiteSpace(_settings.SqlPassword) == false)
        {
            restoredPassword = _settings.SqlPassword;
        }
        SqlPasswordBox.Password = restoredPassword;
        UseIntegratedAuthCheckBox.IsChecked = _settings.UseIntegratedAuth;
        DatabaseListTextBox.Text = string.IsNullOrWhiteSpace(_settings.DatabaseList) ? "MuOnline;Me_MuOnline" : _settings.DatabaseList;
        BackupFolderTextBox.Text = string.IsNullOrWhiteSpace(_settings.BackupFolder) ? Path.Combine(_repoRoot, "SqlBackup") : _settings.BackupFolder;
        EnableAutoBackupCheckBox.IsChecked = _settings.EnableAutoBackup;
        AutoBackupMinutesTextBox.Text = (_settings.AutoBackupMinutes <= 0 ? 60 : _settings.AutoBackupMinutes).ToString();
        RestoreDatabaseTextBox.Text = ParseDatabaseList(DatabaseListTextBox.Text).FirstOrDefault() ?? "MuOnline";
        UpdateSqlAuthUi();

        _processes.Clear();
        var fromSettings = _settings.Processes ?? new List<ProcessItemState>();
        if (fromSettings.Count == 0)
        {
            fromSettings = BuildDefaultProcesses();
        }

        foreach (var item in fromSettings)
        {
            _processes.Add(new ProcessEntry
            {
                IsEnabled = item.IsEnabled,
                AutoRestart = item.AutoRestart,
                Name = item.Name,
                RelativePath = item.RelativePath,
                Status = "Stopped"
            });
        }

        SaveSettings();
    }

    private void SaveSettings()
    {
        _settings.ServerRoot = _repoRoot;
        _settings.LastPublicIp = PublicIpTextBox.Text.Trim();
        _settings.AutoRunOnLaunch = EnableAutoRunOnLaunchCheckBox.IsChecked == true;
        _settings.Processes = _processes.Select(x => new ProcessItemState
        {
            IsEnabled = x.IsEnabled,
            AutoRestart = x.AutoRestart,
            Name = x.Name,
            RelativePath = x.RelativePath
        }).ToList();
        _settings.SqlServer = SqlServerTextBox.Text.Trim();
        _settings.SqlLogin = SqlLoginTextBox.Text.Trim();
        var rawPassword = SqlPasswordBox.Password;
        _settings.SqlPassword = string.Empty;
        _settings.SqlPasswordProtected = string.IsNullOrWhiteSpace(rawPassword) ? string.Empty : ProtectString(rawPassword);
        _settings.UseIntegratedAuth = UseIntegratedAuthCheckBox.IsChecked == true;
        _settings.DatabaseList = DatabaseListTextBox.Text.Trim();
        _settings.BackupFolder = BackupFolderTextBox.Text.Trim();
        _settings.EnableAutoBackup = EnableAutoBackupCheckBox.IsChecked == true;
        _settings.AutoBackupMinutes = int.TryParse(AutoBackupMinutesTextBox.Text.Trim(), out var minutes) ? Math.Max(minutes, 1) : 60;

        var json = JsonSerializer.Serialize(_settings, new JsonSerializerOptions
        {
            WriteIndented = true
        });
        File.WriteAllText(_settingsPath, json, Encoding.UTF8);
    }

    private static List<ProcessItemState> BuildDefaultProcesses()
    {
        return
        [
            new ProcessItemState { Name = "ConnectServer", RelativePath = @".\1.ConnectServer\ConnectServer.exe", IsEnabled = true, AutoRestart = true },
            new ProcessItemState { Name = "DataServer", RelativePath = @".\2.DataServer\DataServer.exe", IsEnabled = true, AutoRestart = true },
            new ProcessItemState { Name = "JoinServer", RelativePath = @".\3.JoinServer\JoinServer.exe", IsEnabled = true, AutoRestart = true },
            new ProcessItemState { Name = "GameServer", RelativePath = @".\4.MuServer\Sub-1\GameServer\GameServer.exe", IsEnabled = true, AutoRestart = true },
            new ProcessItemState { Name = "GameServerCS", RelativePath = @".\4.MuServer\Sub-1\GameServerCS\GameServer.exe", IsEnabled = false, AutoRestart = true }
        ];
    }

    private bool EnsureSqlCmdAvailable()
    {
        if (string.IsNullOrWhiteSpace(ResolveSqlCmdPath()))
        {
            StatusText.Text = "sqlcmd не найден";
            return false;
        }

        return true;
    }

    private string? ResolveSqlCmdPath()
    {
        if (string.IsNullOrWhiteSpace(_sqlCmdPath) == false && File.Exists(_sqlCmdPath))
        {
            return _sqlCmdPath;
        }

        var pathEnv = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (var part in pathEnv.Split(';'))
        {
            var candidate = part.Trim();
            if (candidate.Length == 0)
            {
                continue;
            }

            try
            {
                var full = Path.Combine(candidate, "sqlcmd.exe");
                if (File.Exists(full))
                {
                    _sqlCmdPath = full;
                    return _sqlCmdPath;
                }
            }
            catch
            {
            }
        }

        var roots = new[]
        {
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86)
        };

        foreach (var root in roots)
        {
            if (string.IsNullOrWhiteSpace(root))
            {
                continue;
            }

            var baseDir = Path.Combine(root, "Microsoft SQL Server");
            if (Directory.Exists(baseDir) == false)
            {
                continue;
            }

            try
            {
                foreach (var path in Directory.EnumerateFiles(baseDir, "sqlcmd.exe", SearchOption.AllDirectories))
                {
                    _sqlCmdPath = path;
                    return _sqlCmdPath;
                }
            }
            catch
            {
            }
        }

        return null;
    }

    private static string ProtectString(string value)
    {
        var bytes = Encoding.UTF8.GetBytes(value);
        var protectedBytes = ProtectedData.Protect(bytes, Encoding.UTF8.GetBytes("ServerQuickManager"), DataProtectionScope.CurrentUser);
        return Convert.ToBase64String(protectedBytes);
    }

    private static string UnprotectString(string value)
    {
        try
        {
            var bytes = Convert.FromBase64String(value);
            var unprotectedBytes = ProtectedData.Unprotect(bytes, Encoding.UTF8.GetBytes("ServerQuickManager"), DataProtectionScope.CurrentUser);
            return Encoding.UTF8.GetString(unprotectedBytes);
        }
        catch
        {
            return string.Empty;
        }
    }
}

public class IpFileEntry : INotifyPropertyChanged
{
    private bool _isSelected;
    private string _relativePath = string.Empty;
    private int _matchCount;
    private string _sampleIp = string.Empty;

    public bool IsSelected
    {
        get => _isSelected;
        set
        {
            if (_isSelected == value) return;
            _isSelected = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsSelected)));
        }
    }

    public string AbsolutePath { get; set; } = string.Empty;

    public string RelativePath
    {
        get => _relativePath;
        set
        {
            if (_relativePath == value) return;
            _relativePath = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(RelativePath)));
        }
    }

    public int MatchCount
    {
        get => _matchCount;
        set
        {
            if (_matchCount == value) return;
            _matchCount = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(MatchCount)));
        }
    }

    public string SampleIp
    {
        get => _sampleIp;
        set
        {
            if (_sampleIp == value) return;
            _sampleIp = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SampleIp)));
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
}

public class SqlConfigFileEntry : INotifyPropertyChanged
{
    private bool _isSelected;
    private string _relativePath = string.Empty;
    private int _matchCount;
    private string _sampleHint = string.Empty;

    public bool IsSelected
    {
        get => _isSelected;
        set
        {
            if (_isSelected == value) return;
            _isSelected = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsSelected)));
        }
    }

    public string AbsolutePath { get; set; } = string.Empty;

    public string RelativePath
    {
        get => _relativePath;
        set
        {
            if (_relativePath == value) return;
            _relativePath = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(RelativePath)));
        }
    }

    public int MatchCount
    {
        get => _matchCount;
        set
        {
            if (_matchCount == value) return;
            _matchCount = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(MatchCount)));
        }
    }

    public string SampleHint
    {
        get => _sampleHint;
        set
        {
            if (_sampleHint == value) return;
            _sampleHint = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(SampleHint)));
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
}

public class ProcessEntry : INotifyPropertyChanged
{
    private bool _isEnabled;
    private bool _autoRestart;
    private string _name = string.Empty;
    private string _relativePath = string.Empty;
    private string _status = "Stopped";

    public bool IsEnabled
    {
        get => _isEnabled;
        set
        {
            if (_isEnabled == value) return;
            _isEnabled = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsEnabled)));
        }
    }

    public bool AutoRestart
    {
        get => _autoRestart;
        set
        {
            if (_autoRestart == value) return;
            _autoRestart = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(AutoRestart)));
        }
    }

    public string Name
    {
        get => _name;
        set
        {
            if (_name == value) return;
            _name = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Name)));
        }
    }

    public string RelativePath
    {
        get => _relativePath;
        set
        {
            if (_relativePath == value) return;
            _relativePath = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(RelativePath)));
        }
    }

    public string Status
    {
        get => _status;
        set
        {
            if (_status == value) return;
            _status = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Status)));
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
}

public class ToolSettings
{
    public string ServerRoot { get; set; } = string.Empty;
    public string? LastPublicIp { get; set; }
    public DateTime? IpConfiguredAtUtc { get; set; }
    public bool AutoRunOnLaunch { get; set; }
    public List<ProcessItemState>? Processes { get; set; }
    public string SqlServer { get; set; } = string.Empty;
    public string SqlLogin { get; set; } = string.Empty;
    public string? SqlPassword { get; set; }
    public string? SqlPasswordProtected { get; set; }
    public bool UseIntegratedAuth { get; set; }
    public string DatabaseList { get; set; } = string.Empty;
    public string BackupFolder { get; set; } = string.Empty;
    public bool EnableAutoBackup { get; set; }
    public int AutoBackupMinutes { get; set; } = 60;
}

public class ProcessItemState
{
    public bool IsEnabled { get; set; }
    public bool AutoRestart { get; set; }
    public string Name { get; set; } = string.Empty;
    public string RelativePath { get; set; } = string.Empty;
}
