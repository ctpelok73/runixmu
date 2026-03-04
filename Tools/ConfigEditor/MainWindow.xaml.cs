using System.Collections.ObjectModel;
using System.Data;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using Microsoft.Win32;
using System.Windows;
using System.Windows.Controls;

namespace ConfigEditor;

public partial class MainWindow : Window
{
    private string _dataRoot;
    private readonly ObservableCollection<PropertyItem> _inspectorItems = new();
    private readonly Dictionary<string, string> _fileGuides = new(StringComparer.OrdinalIgnoreCase)
    {
        ["move.xml"] = "Move.xml: каждая строка это точка телепорта. Редактируйте Index, Gate, уровни и reset-ограничения. После правки жмите Validate, затем Backup и Save.",
        ["itemmove.xml"] = "ItemMove.xml: это флаги запретов/разрешений. 1 = разрешено, 0 = запрещено. Поддерживаются поля BanDrop/BanSell/BanTrade/BanVaul и AllowDrop/AllowSell/AllowTrade/AllowVault.",
        ["itemdrop.xml"] = "ItemDrop.xml: это правила дропа. Важно сохранять корректные диапазоны MonsterLevelMin/MonsterLevelMax и адекватный DropRate."
    };
    private readonly Dictionary<string, Dictionary<string, string>> _fieldGuides = new(StringComparer.OrdinalIgnoreCase)
    {
        ["move.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Уникальный ID записи телепорта.",
            ["MainName"] = "Основное имя в списке Move.",
            ["SubName"] = "Подпись/подназвание в списке Move.",
            ["Money"] = "Стоимость телепорта в zen.",
            ["MinLevel"] = "Минимальный уровень, -1 отключает ограничение.",
            ["MaxLevel"] = "Максимальный уровень, -1 отключает ограничение.",
            ["MinReset"] = "Минимальный reset, -1 отключает ограничение.",
            ["MaxReset"] = "Максимальный reset, -1 отключает ограничение.",
            ["AccountLevel"] = "Уровень аккаунта для доступа.",
            ["Gate"] = "Номер gate из Gate.txt."
        },
        ["itemmove.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "ID предмета (GET_ITEM packed index).",
            ["BanDrop"] = "1/0: разрешить/запретить выброс предмета.",
            ["BanSell"] = "1/0: разрешить/запретить продажу в NPC.",
            ["BanTrade"] = "1/0: разрешить/запретить трейд.",
            ["BanVaul"] = "1/0: разрешить/запретить склад.",
            ["BanVault"] = "Альтернативное имя поля для склада.",
            ["AllowDrop"] = "Альтернативное имя поля выброса.",
            ["AllowSell"] = "Альтернативное имя поля продажи.",
            ["AllowTrade"] = "Альтернативное имя поля трейда.",
            ["AllowVault"] = "Альтернативное имя поля склада."
        },
        ["itemdrop.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "ID предмета (GET_ITEM packed index).",
            ["Level"] = "Уровень предмета при дропе.",
            ["Grade"] = "Значение excellent/new option по логике сервера.",
            ["Option0"] = "Шаблон опций уровня.",
            ["Option1"] = "Шаблон опции luck/skill.",
            ["Option2"] = "Шаблон опции add.",
            ["Option3"] = "Шаблон excellent-шансов.",
            ["Option4"] = "Шаблон excellent-значения.",
            ["Option5"] = "Шаблон set-опции.",
            ["Option6"] = "Шаблон socket-опции.",
            ["Duration"] = "Срок жизни предмета в секундах (0 = без срока).",
            ["MapNumber"] = "Карта дропа, -1 на всех картах.",
            ["MonsterClass"] = "Класс монстра, -1 любой.",
            ["MonsterLevelMin"] = "Минимальный уровень монстра, -1 отключает ограничение.",
            ["MonsterLevelMax"] = "Максимальный уровень монстра, -1 отключает ограничение.",
            ["DropRate"] = "Шанс по внутренней шкале сервера.",
            ["Comment"] = "Только подпись для удобства."
        }
    };
    private readonly DataTable _recordsTable = new();
    private readonly List<XElement> _recordElements = new();
    private readonly List<int> _gateIndexList = new();
    private readonly HashSet<int> _gateIndexSet = new();
    private XDocument? _currentDocument;
    private string? _currentFilePath;

    public MainWindow()
    {
        InitializeComponent();
        InspectorGrid.ItemsSource = _inspectorItems;
        _dataRoot = ResolveDataRoot();
        LoadFileTree();
        LoadDependencyData();
        StatusText.Text = Directory.Exists(_dataRoot) ? $"Data root: {_dataRoot}" : "Data root not found";
    }

    private static string ResolveDataRoot()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);

        while (directory != null)
        {
            var candidate = Path.Combine(directory.FullName, "GameServer", "Data");

            if (Directory.Exists(candidate))
            {
                return candidate;
            }

            directory = directory.Parent;
        }

        return Path.Combine(AppContext.BaseDirectory, "GameServer", "Data");
    }

    private void LoadFileTree()
    {
        FilesTree.Items.Clear();

        if (Directory.Exists(_dataRoot) == false)
        {
            return;
        }

        var root = BuildDirectoryNode(new DirectoryInfo(_dataRoot));
        FilesTree.Items.Add(root);
        root.IsExpanded = true;
    }

    private TreeViewItem BuildDirectoryNode(DirectoryInfo directory)
    {
        var node = new TreeViewItem { Header = directory.Name, Tag = directory.FullName };

        foreach (var childDirectory in directory.GetDirectories().OrderBy(dir => dir.Name))
        {
            node.Items.Add(BuildDirectoryNode(childDirectory));
        }

        foreach (var file in directory.GetFiles("*.xml").OrderBy(file => file.Name))
        {
            var fileNode = new TreeViewItem { Header = file.Name, Tag = file.FullName };
            node.Items.Add(fileNode);
        }

        return node;
    }

    private void FilesTreeSelected(object sender, RoutedPropertyChangedEventArgs<object> e)
    {
        if (e.NewValue is not TreeViewItem node || node.Tag is not string path)
        {
            return;
        }

        if (File.Exists(path) == false || Path.GetExtension(path).Equals(".xml", StringComparison.OrdinalIgnoreCase) == false)
        {
            return;
        }

        LoadXmlFile(path);
    }

    private void LoadXmlFile(string filePath)
    {
        try
        {
            _currentDocument = XDocument.Load(filePath);
            _currentFilePath = filePath;
            CurrentFileText.Text = filePath;

            var records = ResolveRecordElements(_currentDocument, Path.GetFileName(filePath));

            _recordElements.Clear();
            _recordElements.AddRange(records);

            BuildRecordsTable(records);
            RecordsGrid.ItemsSource = _recordsTable.DefaultView;
            if (RecordsGrid.Items.Count > 0)
            {
                RecordsGrid.SelectedIndex = 0;
            }
            GuideText.Text = GetFileGuide(filePath);
            StatusText.Text = $"Loaded: {records.Count} records, {_recordsTable.Columns.Count} fields";
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Load error: {ex.Message}";
            MessageBox.Show(ex.Message, "Load XML", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void BuildRecordsTable(List<XElement> records)
    {
        _recordsTable.Clear();
        _recordsTable.Columns.Clear();

        var names = new List<string>();
        var first = records.FirstOrDefault();

        if (first != null)
        {
            foreach (var attribute in first.Attributes())
            {
                names.Add(attribute.Name.LocalName);
            }
        }

        foreach (var name in records
            .SelectMany(element => element.Attributes().Select(attribute => attribute.Name.LocalName))
            .Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (names.Contains(name, StringComparer.OrdinalIgnoreCase) == false)
            {
                names.Add(name);
            }
        }

        foreach (var name in names)
        {
            _recordsTable.Columns.Add(name, typeof(string));
        }

        foreach (var element in records)
        {
            var row = _recordsTable.NewRow();

            foreach (DataColumn column in _recordsTable.Columns)
            {
                row[column.ColumnName] = element.Attribute(column.ColumnName)?.Value ?? string.Empty;
            }

            _recordsTable.Rows.Add(row);
        }

        ConfigureRecordsGridColumns();
    }

    private void ConfigureRecordsGridColumns()
    {
        RecordsGrid.Columns.Clear();

        foreach (DataColumn column in _recordsTable.Columns)
        {
            var gridColumn = new DataGridTextColumn
            {
                Header = column.ColumnName,
                Binding = new System.Windows.Data.Binding(column.ColumnName),
                MinWidth = 110,
                Width = new DataGridLength(140)
            };

            if (column.ColumnName.Equals("Index", StringComparison.OrdinalIgnoreCase))
            {
                gridColumn.Width = new DataGridLength(90);
            }

            RecordsGrid.Columns.Add(gridColumn);
        }
    }

    private void RecordsSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (RecordsGrid.SelectedItem is not DataRowView row)
        {
            _inspectorItems.Clear();
            EditorPanel.Children.Clear();
            return;
        }

        RefreshInspectorFromRow(row);
        BuildInteractiveEditor(row);
    }

    private void OpenClick(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "XML files (*.xml)|*.xml|All files (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };

        if (Directory.Exists(_dataRoot))
        {
            dialog.InitialDirectory = _dataRoot;
        }

        var result = dialog.ShowDialog(this);

        if (result != true || string.IsNullOrWhiteSpace(dialog.FileName))
        {
            StatusText.Text = "Open canceled";
            return;
        }

        _dataRoot = ResolveDataRootFromFile(dialog.FileName);
        LoadFileTree();
        LoadDependencyData();
        LoadXmlFile(dialog.FileName);
        StatusText.Text = $"Opened: {dialog.FileName}";
    }

    private void ValidateClick(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_currentFilePath) || File.Exists(_currentFilePath) == false)
        {
            StatusText.Text = "No file selected";
            return;
        }

        try
        {
            XDocument.Load(_currentFilePath);
            if (_currentDocument == null)
            {
                _currentDocument = XDocument.Load(_currentFilePath);
            }

            var errors = ValidateDocument(_currentDocument, Path.GetFileName(_currentFilePath));

            if (errors.Count == 0)
            {
                StatusText.Text = "Validate OK";
            }
            else
            {
                StatusText.Text = $"Validate warning: {errors.Count}";
                MessageBox.Show(string.Join(Environment.NewLine, errors.Take(20)), "Validate Rules", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Validate error: {ex.Message}";
            MessageBox.Show(ex.Message, "Validate XML", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void SaveClick(object sender, RoutedEventArgs e)
    {
        if (_currentDocument == null || string.IsNullOrWhiteSpace(_currentFilePath))
        {
            StatusText.Text = "No file selected";
            return;
        }

        if (RecordsGrid.ItemsSource is not DataView rows)
        {
            StatusText.Text = "No records loaded";
            return;
        }

        if (rows.Count != _recordElements.Count)
        {
            StatusText.Text = "Record mapping error";
            return;
        }

        for (var i = 0; i < rows.Count; i++)
        {
            var row = rows[i];
            var element = _recordElements[i];

            foreach (DataColumn column in _recordsTable.Columns)
            {
                element.SetAttributeValue(column.ColumnName, row[column.ColumnName]?.ToString() ?? string.Empty);
            }
        }

        _currentDocument.Save(_currentFilePath);
        StatusText.Text = "Saved";
    }

    private void BackupClick(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_currentFilePath) || File.Exists(_currentFilePath) == false)
        {
            StatusText.Text = "No file selected";
            return;
        }

        try
        {
            var relativePath = Path.GetRelativePath(_dataRoot, _currentFilePath);
            var backupRoot = Path.Combine(_dataRoot, "_backup", DateTime.Now.ToString("yyyyMMdd_HHmm"));
            var backupFile = Path.Combine(backupRoot, relativePath);
            var backupDir = Path.GetDirectoryName(backupFile);

            if (string.IsNullOrWhiteSpace(backupDir) == false)
            {
                Directory.CreateDirectory(backupDir);
            }

            File.Copy(_currentFilePath, backupFile, true);
            StatusText.Text = $"Backup created: {backupFile}";
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Backup error: {ex.Message}";
            MessageBox.Show(ex.Message, "Backup", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void RefreshInspectorFromRow(DataRowView row)
    {
        _inspectorItems.Clear();

        foreach (DataColumn column in _recordsTable.Columns)
        {
            _inspectorItems.Add(new PropertyItem
            {
                Name = column.ColumnName,
                Value = row.Row[column.ColumnName]?.ToString() ?? string.Empty,
                Description = GetFieldDescription(column.ColumnName)
            });
        }
    }

    private void BuildInteractiveEditor(DataRowView row)
    {
        EditorPanel.Children.Clear();

        if (_recordsTable.Columns.Count == 0)
        {
            return;
        }

        var fileName = Path.GetFileName(_currentFilePath ?? string.Empty);

        foreach (DataColumn column in _recordsTable.Columns)
        {
            var card = new Border
            {
                BorderThickness = new Thickness(1),
                BorderBrush = System.Windows.Media.Brushes.LightGray,
                CornerRadius = new CornerRadius(8),
                Padding = new Thickness(10),
                Margin = new Thickness(0, 0, 0, 8)
            };

            var stack = new StackPanel();
            var label = new TextBlock
            {
                Text = column.ColumnName,
                FontWeight = FontWeights.SemiBold
            };

            stack.Children.Add(label);

            var currentValue = row.Row[column.ColumnName]?.ToString() ?? string.Empty;

            if (fileName.Equals("Move.xml", StringComparison.OrdinalIgnoreCase) &&
                column.ColumnName.Equals("Gate", StringComparison.OrdinalIgnoreCase) &&
                _gateIndexList.Count > 0)
            {
                var combo = new ComboBox
                {
                    ItemsSource = _gateIndexList,
                    IsEditable = true,
                    Margin = new Thickness(0, 6, 0, 0),
                    Text = currentValue
                };

                combo.SelectionChanged += (_, _) =>
                {
                    ApplyRowValue(row, column.ColumnName, combo.Text);
                };

                combo.LostKeyboardFocus += (_, _) =>
                {
                    ApplyRowValue(row, column.ColumnName, combo.Text);
                };

                stack.Children.Add(combo);

                var info = new TextBlock
                {
                    Text = "Зависимость: Data\\Move\\Gate.txt",
                    Margin = new Thickness(0, 4, 0, 0),
                    Foreground = System.Windows.Media.Brushes.DimGray,
                    FontSize = 11
                };
                stack.Children.Add(info);
            }
            else
            {
                var box = new TextBox
                {
                    Text = currentValue,
                    Margin = new Thickness(0, 6, 0, 0)
                };

                box.LostKeyboardFocus += (_, _) =>
                {
                    ApplyRowValue(row, column.ColumnName, box.Text);
                };

                stack.Children.Add(box);
            }

            card.Child = stack;
            EditorPanel.Children.Add(card);
        }
    }

    private void ApplyRowValue(DataRowView row, string columnName, string? value)
    {
        row.Row[columnName] = value ?? string.Empty;
        RefreshInspectorFromRow(row);
    }

    private void LoadDependencyData()
    {
        _gateIndexList.Clear();
        _gateIndexSet.Clear();

        var gatePath = Path.Combine(_dataRoot, "Move", "Gate.txt");

        if (File.Exists(gatePath) == false)
        {
            return;
        }

        foreach (var line in File.ReadLines(gatePath))
        {
            var text = line.Trim();

            if (text.Length == 0 || text.StartsWith("//"))
            {
                continue;
            }

            var firstToken = text.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();

            if (int.TryParse(firstToken, out var gateIndex) && _gateIndexSet.Add(gateIndex))
            {
                _gateIndexList.Add(gateIndex);
            }
        }

        _gateIndexList.Sort();
    }

    private string GetFileGuide(string filePath)
    {
        var key = Path.GetFileName(filePath);
        return _fileGuides.TryGetValue(key, out var guide)
            ? guide
            : "Выберите XML в дереве слева. Далее: Validate -> Backup -> Save.";
    }

    private string GetFieldDescription(string fieldName)
    {
        if (string.IsNullOrWhiteSpace(_currentFilePath))
        {
            return string.Empty;
        }

        var key = Path.GetFileName(_currentFilePath);

        if (_fieldGuides.TryGetValue(key, out var fieldMap) && fieldMap.TryGetValue(fieldName, out var description))
        {
            return description;
        }

        return string.Empty;
    }

    private List<string> ValidateDocument(XDocument document, string fileName)
    {
        var errors = new List<string>();

        if (fileName.Equals("Move.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateMove(document, errors);
        }
        else if (fileName.Equals("ItemMove.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateItemMove(document, errors);
        }
        else if (fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateItemDrop(document, errors);
        }

        return errors;
    }

    private static List<XElement> ResolveRecordElements(XDocument document, string fileName)
    {
        var root = document.Root;

        if (root == null)
        {
            return new List<XElement>();
        }

        if (fileName.Equals("Move.xml", StringComparison.OrdinalIgnoreCase))
        {
            return root.Elements("info").ToList();
        }

        if (fileName.Equals("ItemMove.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase))
        {
            return root.Elements("Info").ToList();
        }

        var direct = root.Elements().ToList();

        if (direct.Count > 0 && direct.Any(element => element.HasAttributes))
        {
            return direct;
        }

        var secondLevel = root.Elements().SelectMany(element => element.Elements()).ToList();

        return secondLevel;
    }

    private void ValidateMove(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("Move: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"Move: duplicate Index={index}");
            }

            TryInt(info, "Gate", out var gate);
            TryInt(info, "MinLevel", out var minLevel);
            TryInt(info, "MaxLevel", out var maxLevel);
            TryInt(info, "MinReset", out var minReset);
            TryInt(info, "MaxReset", out var maxReset);

            if (gate < 0)
            {
                errors.Add($"Move[{index}]: Gate < 0");
            }

            if (gate >= 0 && _gateIndexSet.Count > 0 && _gateIndexSet.Contains(gate) == false)
            {
                errors.Add($"Move[{index}]: Gate={gate} not found in Gate.txt");
            }

            if (maxLevel != -1 && minLevel != -1 && minLevel > maxLevel)
            {
                errors.Add($"Move[{index}]: MinLevel > MaxLevel");
            }

            if (maxReset != -1 && minReset != -1 && minReset > maxReset)
            {
                errors.Add($"Move[{index}]: MinReset > MaxReset");
            }
        }
    }

    private static void ValidateItemMove(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("ItemMove: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"ItemMove: duplicate Index={index}");
            }

            var drop = GetFlagValue(info, "AllowDrop", "BanDrop");
            var sell = GetFlagValue(info, "AllowSell", "BanSell");
            var trade = GetFlagValue(info, "AllowTrade", "BanTrade");
            var vault = GetFlagValue(info, "AllowVault", "BanVaul", "BanVault");

            if (drop is not (0 or 1)) errors.Add($"ItemMove[{index}]: invalid Drop flag");
            if (sell is not (0 or 1)) errors.Add($"ItemMove[{index}]: invalid Sell flag");
            if (trade is not (0 or 1)) errors.Add($"ItemMove[{index}]: invalid Trade flag");
            if (vault is not (0 or 1)) errors.Add($"ItemMove[{index}]: invalid Vault flag");
        }
    }

    private static void ValidateItemDrop(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("ItemDrop: invalid Index");
                continue;
            }

            TryInt(info, "MonsterLevelMin", out var minLevel);
            TryInt(info, "MonsterLevelMax", out var maxLevel);
            TryInt(info, "DropRate", out var dropRate);

            if (dropRate < -1)
            {
                errors.Add($"ItemDrop[{index}]: DropRate < -1");
            }

            if (maxLevel != -1 && minLevel != -1 && minLevel > maxLevel)
            {
                errors.Add($"ItemDrop[{index}]: MonsterLevelMin > MonsterLevelMax");
            }
        }
    }

    private static bool TryInt(XElement element, string attributeName, out int value)
    {
        value = 0;
        var attr = element.Attribute(attributeName);

        return attr != null && int.TryParse(attr.Value, out value);
    }

    private static int GetFlagValue(XElement element, params string[] names)
    {
        foreach (var name in names)
        {
            var attr = element.Attribute(name);

            if (attr != null && int.TryParse(attr.Value, out var value))
            {
                return value;
            }
        }

        return int.MinValue;
    }

    private static string ResolveDataRootFromFile(string filePath)
    {
        var directory = new DirectoryInfo(Path.GetDirectoryName(filePath) ?? string.Empty);

        while (directory != null)
        {
            if (directory.Name.Equals("Data", StringComparison.OrdinalIgnoreCase))
            {
                return directory.FullName;
            }

            directory = directory.Parent;
        }

        return Path.GetDirectoryName(filePath) ?? string.Empty;
    }
}

public sealed class PropertyItem
{
    public string Name { get; set; } = string.Empty;
    public string Value { get; set; } = string.Empty;
    public string Description { get; set; } = string.Empty;
}
