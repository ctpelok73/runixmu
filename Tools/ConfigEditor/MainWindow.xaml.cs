using System.Collections.ObjectModel;
using System.Data;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using Microsoft.Win32;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace ConfigEditor;

public partial class MainWindow : Window
{
    private const string TextValueColumn = "_TextValue";
    private string _dataRoot;
    private readonly ObservableCollection<PropertyItem> _inspectorItems = new();
    private readonly Dictionary<string, string> _fileGuides = new(StringComparer.OrdinalIgnoreCase)
    {
        ["move.xml"] = "Move.xml: каждая строка это точка телепорта. Редактируйте Index, Gate, уровни и reset-ограничения. После правки жмите Validate, затем Backup и Save.",
        ["itemmove.xml"] = "ItemMove.xml: это флаги запретов/разрешений. 1 = разрешено, 0 = запрещено. Поддерживаются поля BanDrop/BanSell/BanTrade/BanVaul и AllowDrop/AllowSell/AllowTrade/AllowVault.",
        ["itemdrop.xml"] = "ItemDrop.xml: это правила дропа. Важно сохранять корректные диапазоны MonsterLevelMin/MonsterLevelMax и адекватный DropRate.",
        ["item.xml"] = "Item.xml: основной справочник предметов. Используется сервером напрямую, правки только осознанно.",
        ["eventname.xml"] = "EventName.xml: названия и строки событий для серверных уведомлений.",
        ["mixexpansion.xml"] = "MixExpansion.xml: конфиг расширенного микса GoblinMixExpansion.",
        ["petitemoption.xml"] = "PetItemOption.xml: параметры pet-опций предметов.",
        ["flagitemoption.xml"] = "FlagItemOption.xml: параметры flag-опций предметов.",
        ["earringitemoption.xml"] = "EarringItemOption.xml: параметры earring-опций предметов.",
        ["mucastledata.xml"] = "MuCastleData.xml: данные CastleSiege, грузится через CCastleSiege::LoadData.",
        ["custombuyvip.xml"] = "CustomBuyVip.xml: конфиг покупки VIP, грузится через CCustomBuyVip::LoadData.",
        ["message.xml"] = "Message.xml: системные тексты сервера (ищется в Data\\Lang и Data)."
    };
    private readonly HashSet<string> _serverUsedXmlFileNames = new(StringComparer.OrdinalIgnoreCase)
    {
        "Move.xml",
        "ItemDrop.xml",
        "ItemMove.xml",
        "Item.xml",
        "PetItemOption.xml",
        "FlagItemOption.xml",
        "EarringItemOption.xml",
        "EventName.xml",
        "MixExpansion.xml",
        "MuCastleData.xml",
        "CustomBuyVip.xml",
        "Message.xml"
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
    private readonly List<int> _mapIndexList = new();
    private readonly HashSet<int> _mapIndexSet = new();
    private readonly Dictionary<int, string> _mapNameByIndex = new();
    private readonly List<int> _itemIndexList = new();
    private readonly HashSet<int> _itemIndexSet = new();
    private readonly Dictionary<int, string> _itemNameByIndex = new();
    private readonly string[] _itemClassFields =
    {
        "DarkWizard",
        "DarkKnight",
        "FairyElf",
        "MagicGladiator",
        "DarkLord",
        "Summoner",
        "RageFighter",
        "WhiteWizard",
        "LemuriaMage",
        "IllusionKnight"
    };
    private readonly List<PresetOption> _itemClassPresets = new()
    {
        new("Классы: Только DK", new Dictionary<string, string>
        {
            ["DarkWizard"] = "0",
            ["DarkKnight"] = "1",
            ["FairyElf"] = "0",
            ["MagicGladiator"] = "0",
            ["DarkLord"] = "0",
            ["Summoner"] = "0",
            ["RageFighter"] = "0",
            ["WhiteWizard"] = "0",
            ["LemuriaMage"] = "0",
            ["IllusionKnight"] = "0"
        }),
        new("Классы: Только DW", new Dictionary<string, string>
        {
            ["DarkWizard"] = "1",
            ["DarkKnight"] = "0",
            ["FairyElf"] = "0",
            ["MagicGladiator"] = "0",
            ["DarkLord"] = "0",
            ["Summoner"] = "0",
            ["RageFighter"] = "0",
            ["WhiteWizard"] = "1",
            ["LemuriaMage"] = "1",
            ["IllusionKnight"] = "0"
        }),
        new("Классы: DK + DW + ELF", new Dictionary<string, string>
        {
            ["DarkWizard"] = "1",
            ["DarkKnight"] = "1",
            ["FairyElf"] = "1",
            ["MagicGladiator"] = "0",
            ["DarkLord"] = "0",
            ["Summoner"] = "0",
            ["RageFighter"] = "0",
            ["WhiteWizard"] = "1",
            ["LemuriaMage"] = "1",
            ["IllusionKnight"] = "0"
        }),
        new("Классы: Все S6 базовые", new Dictionary<string, string>
        {
            ["DarkWizard"] = "1",
            ["DarkKnight"] = "1",
            ["FairyElf"] = "1",
            ["MagicGladiator"] = "1",
            ["DarkLord"] = "1",
            ["Summoner"] = "1",
            ["RageFighter"] = "1",
            ["WhiteWizard"] = "1",
            ["LemuriaMage"] = "1",
            ["IllusionKnight"] = "1"
        }),
        new("Классы: Сброс", new Dictionary<string, string>
        {
            ["DarkWizard"] = "0",
            ["DarkKnight"] = "0",
            ["FairyElf"] = "0",
            ["MagicGladiator"] = "0",
            ["DarkLord"] = "0",
            ["Summoner"] = "0",
            ["RageFighter"] = "0",
            ["WhiteWizard"] = "0",
            ["LemuriaMage"] = "0",
            ["IllusionKnight"] = "0"
        })
    };
    private readonly Dictionary<string, List<PresetOption>> _presetsByFile = new(StringComparer.OrdinalIgnoreCase)
    {
        ["Move.xml"] = new List<PresetOption>
        {
            new("Move: Свободный доступ", new Dictionary<string, string>
            {
                ["Money"] = "0",
                ["MinLevel"] = "-1",
                ["MaxLevel"] = "-1",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["AccountLevel"] = "0"
            }),
            new("Move: Платный телепорт", new Dictionary<string, string>
            {
                ["Money"] = "10000",
                ["MinLevel"] = "50",
                ["MaxLevel"] = "-1",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["AccountLevel"] = "0"
            })
        },
        ["ItemMove.xml"] = new List<PresetOption>
        {
            new("ItemMove: Полный запрет перемещения", new Dictionary<string, string>
            {
                ["BanDrop"] = "0",
                ["BanSell"] = "0",
                ["BanTrade"] = "0",
                ["BanVaul"] = "0",
                ["BanVault"] = "0",
                ["AllowDrop"] = "0",
                ["AllowSell"] = "0",
                ["AllowTrade"] = "0",
                ["AllowVault"] = "0"
            }),
            new("ItemMove: Полное разрешение", new Dictionary<string, string>
            {
                ["BanDrop"] = "1",
                ["BanSell"] = "1",
                ["BanTrade"] = "1",
                ["BanVaul"] = "1",
                ["BanVault"] = "1",
                ["AllowDrop"] = "1",
                ["AllowSell"] = "1",
                ["AllowTrade"] = "1",
                ["AllowVault"] = "1"
            })
        },
        ["ItemDrop.xml"] = new List<PresetOption>
        {
            new("ItemDrop: Глобальный дроп", new Dictionary<string, string>
            {
                ["MapNumber"] = "-1",
                ["MonsterClass"] = "-1",
                ["MonsterLevelMin"] = "-1",
                ["MonsterLevelMax"] = "-1"
            }),
            new("ItemDrop: Средний шанс", new Dictionary<string, string>
            {
                ["DropRate"] = "1000"
            }),
            new("ItemDrop: Высокий шанс", new Dictionary<string, string>
            {
                ["DropRate"] = "5000"
            })
        }
    };
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
            var fileNode = new TreeViewItem
            {
                Header = IsServerUsedXml(file.Name) ? $"★ {file.Name}" : file.Name,
                Tag = file.FullName
            };
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
            RefreshPresetList();
            GuideText.Text = GetFileGuide(filePath);
            var usedState = IsServerUsedXml(Path.GetFileName(filePath)) ? "Server-used XML" : "Local XML";
            StatusText.Text = $"Loaded: {records.Count} records, {_recordsTable.Columns.Count} fields ({usedState})";
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

        var hasTextValue = records.Any(element => element.HasElements == false && string.IsNullOrWhiteSpace(element.Value) == false);

        if (hasTextValue)
        {
            _recordsTable.Columns.Add(TextValueColumn, typeof(string));
        }

        foreach (var element in records)
        {
            var row = _recordsTable.NewRow();

            foreach (DataColumn column in _recordsTable.Columns)
            {
                if (column.ColumnName == TextValueColumn)
                {
                    row[column.ColumnName] = element.Value;
                }
                else
                {
                    row[column.ColumnName] = element.Attribute(column.ColumnName)?.Value ?? string.Empty;
                }
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

            if (column.ColumnName == TextValueColumn)
            {
                gridColumn.Width = new DataGridLength(260);
                gridColumn.Header = "Text";
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

    private void ApplyPresetClick(object sender, RoutedEventArgs e)
    {
        if (RecordsGrid.SelectedItem is not DataRowView row)
        {
            StatusText.Text = "Выберите запись для применения шаблона";
            return;
        }

        if (PresetCombo.SelectedItem is not PresetOption preset)
        {
            StatusText.Text = "Выберите шаблон";
            return;
        }

        var appliedCount = 0;

        foreach (var pair in preset.Values)
        {
            if (_recordsTable.Columns.Contains(pair.Key))
            {
                row.Row[pair.Key] = pair.Value;
                appliedCount++;
            }
        }

        RefreshInspectorFromRow(row);
        BuildInteractiveEditor(row);
        StatusText.Text = $"Шаблон применен: {preset.Name} ({appliedCount} полей)";
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
                if (column.ColumnName == TextValueColumn)
                {
                    element.Value = row[column.ColumnName]?.ToString() ?? string.Empty;
                }
                else
                {
                    element.SetAttributeValue(column.ColumnName, row[column.ColumnName]?.ToString() ?? string.Empty);
                }
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
                Description = BuildFieldDescription(column.ColumnName, row.Row[column.ColumnName]?.ToString() ?? string.Empty)
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

        if (fileName.Equals("Item.xml", StringComparison.OrdinalIgnoreCase))
        {
            BuildItemClassEditor(row);
        }

        foreach (DataColumn column in _recordsTable.Columns)
        {
            if (fileName.Equals("Item.xml", StringComparison.OrdinalIgnoreCase) &&
                _itemClassFields.Contains(column.ColumnName, StringComparer.OrdinalIgnoreCase))
            {
                continue;
            }

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
            else if (fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase) &&
                     column.ColumnName.Equals("MapNumber", StringComparison.OrdinalIgnoreCase) &&
                     _mapIndexList.Count > 0)
            {
                var combo = new ComboBox
                {
                    ItemsSource = _mapIndexList,
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

                if (int.TryParse(currentValue, out var mapIndex) && _mapNameByIndex.TryGetValue(mapIndex, out var mapName))
                {
                    stack.Children.Add(new TextBlock
                    {
                        Text = $"Map: {mapName}",
                        Margin = new Thickness(0, 4, 0, 0),
                        Foreground = System.Windows.Media.Brushes.DimGray,
                        FontSize = 11
                    });
                }
            }
            else if ((fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase) ||
                      fileName.Equals("ItemMove.xml", StringComparison.OrdinalIgnoreCase)) &&
                     column.ColumnName.Equals("Index", StringComparison.OrdinalIgnoreCase) &&
                     _itemIndexList.Count > 0)
            {
                var combo = new ComboBox
                {
                    ItemsSource = _itemIndexList,
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

                if (int.TryParse(currentValue, out var itemIndex) && _itemNameByIndex.TryGetValue(itemIndex, out var itemName))
                {
                    stack.Children.Add(new TextBlock
                    {
                        Text = $"Item: {itemName}",
                        Margin = new Thickness(0, 4, 0, 0),
                        Foreground = System.Windows.Media.Brushes.DimGray,
                        FontSize = 11
                    });
                }
            }
            else if (IsBinaryField(row, column.ColumnName))
            {
                var check = new CheckBox
                {
                    IsChecked = row.Row[column.ColumnName]?.ToString() == "1",
                    Margin = new Thickness(0, 6, 0, 0),
                    Content = "Включено"
                };

                check.Checked += (_, _) => ApplyRowValue(row, column.ColumnName, "1");
                check.Unchecked += (_, _) => ApplyRowValue(row, column.ColumnName, "0");

                stack.Children.Add(check);
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

    private void BuildItemClassEditor(DataRowView row)
    {
        var classFields = _itemClassFields
            .Where(name => _recordsTable.Columns.Contains(name))
            .ToList();

        if (classFields.Count == 0)
        {
            return;
        }

        var card = new Border
        {
            BorderThickness = new Thickness(1),
            BorderBrush = System.Windows.Media.Brushes.SteelBlue,
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(10),
            Margin = new Thickness(0, 0, 0, 8)
        };

        var root = new StackPanel();
        root.Children.Add(new TextBlock
        {
            Text = "Классы и доступ",
            FontWeight = FontWeights.Bold
        });

        var presetPanel = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Thickness(0, 8, 0, 8)
        };

        var presetCombo = new ComboBox
        {
            Width = 240,
            ItemsSource = _itemClassPresets,
            DisplayMemberPath = "Name",
            SelectedIndex = 0
        };

        var presetButton = new Button
        {
            Content = "Применить набор",
            Margin = new Thickness(8, 0, 0, 0)
        };

        presetButton.Click += (_, _) =>
        {
            if (presetCombo.SelectedItem is not PresetOption preset)
            {
                return;
            }

            foreach (var pair in preset.Values)
            {
                if (_recordsTable.Columns.Contains(pair.Key))
                {
                    row.Row[pair.Key] = pair.Value;
                }
            }

            RefreshInspectorFromRow(row);
            BuildInteractiveEditor(row);
        };

        presetPanel.Children.Add(presetCombo);
        presetPanel.Children.Add(presetButton);
        root.Children.Add(presetPanel);

        var grid = new UniformGrid
        {
            Columns = 2
        };

        foreach (var field in classFields)
        {
            var itemPanel = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Margin = new Thickness(0, 4, 8, 4)
            };

            itemPanel.Children.Add(new TextBlock
            {
                Text = field,
                Width = 120,
                VerticalAlignment = VerticalAlignment.Center
            });

            var combo = new ComboBox
            {
                Width = 80,
                ItemsSource = new[] { "0", "1", "2", "3", "4" },
                SelectedItem = NormalizeClassValue(row.Row[field]?.ToString())
            };

            combo.SelectionChanged += (_, _) =>
            {
                ApplyRowValue(row, field, combo.SelectedItem?.ToString() ?? "0");
            };

            itemPanel.Children.Add(combo);
            grid.Children.Add(itemPanel);
        }

        root.Children.Add(grid);
        card.Child = root;
        EditorPanel.Children.Add(card);
    }

    private static string NormalizeClassValue(string? value)
    {
        return value is "1" or "2" or "3" or "4" ? value : "0";
    }

    private bool IsBinaryField(DataRowView row, string columnName)
    {
        if (_itemClassFields.Contains(columnName, StringComparer.OrdinalIgnoreCase))
        {
            return false;
        }

        var value = row.Row[columnName]?.ToString();

        return value is "0" or "1";
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
        _mapIndexList.Clear();
        _mapIndexSet.Clear();
        _mapNameByIndex.Clear();
        _itemIndexList.Clear();
        _itemIndexSet.Clear();
        _itemNameByIndex.Clear();

        var gatePath = Path.Combine(_dataRoot, "Move", "Gate.txt");

        if (File.Exists(gatePath))
        {
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
        }

        var mapManagerPath = Path.Combine(_dataRoot, "MapManager.txt");

        if (File.Exists(mapManagerPath))
        {
            foreach (var line in File.ReadLines(mapManagerPath))
            {
                var text = line.Trim();

                if (text.Length == 0 || text.StartsWith("//"))
                {
                    continue;
                }

                var tokens = text.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);

                if (tokens.Length == 0 || int.TryParse(tokens[0], out var mapIndex) == false)
                {
                    continue;
                }

                var mapName = string.Empty;
                var quoteStart = text.IndexOf('"');
                var quoteEnd = text.LastIndexOf('"');

                if (quoteStart >= 0 && quoteEnd > quoteStart)
                {
                    mapName = text.Substring(quoteStart + 1, quoteEnd - quoteStart - 1);
                }

                if (_mapIndexSet.Add(mapIndex))
                {
                    _mapIndexList.Add(mapIndex);
                }

                if (string.IsNullOrWhiteSpace(mapName) == false)
                {
                    _mapNameByIndex[mapIndex] = mapName;
                }
            }
        }

        LoadItemNameFromXml(Path.Combine(_dataRoot, "Item", "ItemDrop.xml"));
        LoadItemNameFromXml(Path.Combine(_dataRoot, "Item", "ItemMove.xml"));

        _gateIndexList.Sort();
        _mapIndexList.Sort();
        _itemIndexList.Sort();
    }

    private void LoadItemNameFromXml(string xmlPath)
    {
        if (File.Exists(xmlPath) == false)
        {
            return;
        }

        try
        {
            var doc = XDocument.Load(xmlPath);
            var infoElements = doc.Root?.Elements("Info") ?? Enumerable.Empty<XElement>();

            foreach (var info in infoElements)
            {
                if (TryInt(info, "Index", out var itemIndex) == false)
                {
                    continue;
                }

                if (_itemIndexSet.Add(itemIndex))
                {
                    _itemIndexList.Add(itemIndex);
                }

                var comment = info.Attribute("Comment")?.Value;

                if (string.IsNullOrWhiteSpace(comment) == false && _itemNameByIndex.ContainsKey(itemIndex) == false)
                {
                    _itemNameByIndex[itemIndex] = comment;
                }
            }
        }
        catch
        {
        }
    }

    private string GetFileGuide(string filePath)
    {
        var key = Path.GetFileName(filePath);
        return _fileGuides.TryGetValue(key, out var guide)
            ? guide
            : "Выберите XML в дереве слева. Далее: Validate -> Backup -> Save.";
    }

    private bool IsServerUsedXml(string? fileName)
    {
        if (string.IsNullOrWhiteSpace(fileName))
        {
            return false;
        }

        return _serverUsedXmlFileNames.Contains(fileName);
    }

    private void RefreshPresetList()
    {
        PresetCombo.ItemsSource = null;
        PresetCombo.SelectedItem = null;

        var fileName = Path.GetFileName(_currentFilePath ?? string.Empty);

        if (_presetsByFile.TryGetValue(fileName, out var presets))
        {
            PresetCombo.ItemsSource = presets;
            if (presets.Count > 0)
            {
                PresetCombo.SelectedIndex = 0;
            }
        }
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

    private string BuildFieldDescription(string fieldName, string fieldValue)
    {
        var description = GetFieldDescription(fieldName);

        if (string.IsNullOrWhiteSpace(_currentFilePath))
        {
            return description;
        }

        var fileName = Path.GetFileName(_currentFilePath);

        if (fileName.Equals("Move.xml", StringComparison.OrdinalIgnoreCase) &&
            fieldName.Equals("Gate", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(fieldValue, out var gateValue) &&
            _gateIndexSet.Contains(gateValue) == false)
        {
            description += " [WARN: не найден в Gate.txt]";
        }

        if (fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase) &&
            fieldName.Equals("MapNumber", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(fieldValue, out var mapValue) &&
            _mapNameByIndex.TryGetValue(mapValue, out var mapName))
        {
            description += $" [Map: {mapName}]";
        }

        if ((fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase) ||
             fileName.Equals("ItemMove.xml", StringComparison.OrdinalIgnoreCase)) &&
            fieldName.Equals("Index", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(fieldValue, out var itemValue) &&
            _itemNameByIndex.TryGetValue(itemValue, out var itemName))
        {
            description += $" [Item: {itemName}]";
        }

        return description;
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

        var leafRecords = root
            .Descendants()
            .Where(element => element.HasElements == false && (element.HasAttributes || string.IsNullOrWhiteSpace(element.Value) == false))
            .ToList();

        if (leafRecords.Count > 0)
        {
            return leafRecords;
        }

        var direct = root.Elements().ToList();

        if (direct.Count > 0)
        {
            return direct;
        }

        return new List<XElement>();
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

public sealed class PresetOption
{
    public PresetOption(string name, Dictionary<string, string> values)
    {
        Name = name;
        Values = values;
    }

    public string Name { get; }
    public Dictionary<string, string> Values { get; }
}
