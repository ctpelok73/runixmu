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
    private const string ReadyXmlFolderName = "Data_XML_READY";
    private string _dataRoot;
    private readonly ObservableCollection<PropertyItem> _inspectorItems = new();
    private readonly Dictionary<string, string> _fileGuides = new(StringComparer.OrdinalIgnoreCase)
    {
        ["move.xml"] = "Move.xml: каждая строка это точка телепорта. Редактируйте Index, Gate, уровни и reset-ограничения. После правки жмите Validate, затем Backup и Save.",
        ["movesummon.xml"] = "MoveSummon.xml: зоны возврата талисманом. Проверяйте диапазоны X/Y и ограничения уровня/reset.",
        ["gate.xml"] = "Gate.xml: таблица ворот/точек выхода. Проверяйте диапазоны координат, TargetGate и ограничения по уровню/reset.",
        ["itemmove.xml"] = "ItemMove.xml: это флаги запретов/разрешений. 1 = разрешено, 0 = запрещено. Поддерживаются поля BanDrop/BanSell/BanTrade/BanVaul и AllowDrop/AllowSell/AllowTrade/AllowVault.",
        ["itemdrop.xml"] = "ItemDrop.xml: это правила дропа. Важно сохранять корректные диапазоны MonsterLevelMin/MonsterLevelMax и адекватный DropRate.",
        ["custommove.xml"] = "CustomMove.xml: команды телепорта. Проверяйте уникальность Name, карту, координаты и лимиты.",
        ["customnpcmove.xml"] = "CustomNpcMove.xml: перемещения через NPC. Проверяйте NPC-точку, карту назначения и лимиты.",
        ["customnpccommand.xml"] = "CustomNpcCommand.xml: команды через NPC. Проверяйте NPC-точку, Talk (0/1) и текст команды.",
        ["customcommanddescription.xml"] = "CustomCommandDescription.xml: описания команд. Проверяйте уникальность Command и длину Description.",
        ["customranking.xml"] = "CustomRanking.xml: вкладки рейтинга. Проверяйте уникальность Index и названия колонок.",
        ["customrankuser.xml"] = "CustomRankUser.xml: ранги игрока по Reset/MReset/Level. Проверяйте диапазоны ResetMin/ResetMax.",
        ["custommonster.xml"] = "CustomMonster.xml: модификаторы монстров. Проверяйте MapNumber, проценты статов и summon-поля.",
        ["customdeathmessage.xml"] = "CustomDeathMessage.xml: тексты при смерти от монстров. Проверяйте уникальность Index и непустой Text.",
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
        "MoveSummon.xml",
        "Gate.xml",
        "ItemDrop.xml",
        "ItemMove.xml",
        "CustomMove.xml",
        "CustomNpcMove.xml",
        "CustomNpcCommand.xml",
        "CustomCommandDescription.xml",
        "CustomRanking.xml",
        "CustomRankUser.xml",
        "CustomMonster.xml",
        "CustomDeathMessage.xml",
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
            ["Gate"] = "Номер gate из Gate.xml или Gate.txt."
        },
        ["movesummon.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Map"] = "Индекс карты.",
            ["X"] = "Начальная X координата разрешенной зоны.",
            ["Y"] = "Начальная Y координата разрешенной зоны.",
            ["TX"] = "Конечная X координата разрешенной зоны.",
            ["TY"] = "Конечная Y координата разрешенной зоны.",
            ["MinLevel"] = "Минимальный уровень, -1 отключает ограничение.",
            ["MaxLevel"] = "Максимальный уровень, -1 отключает ограничение.",
            ["MinReset"] = "Минимальный reset, -1 отключает ограничение.",
            ["MaxReset"] = "Максимальный reset, -1 отключает ограничение.",
            ["AccountLevel"] = "Уровень аккаунта для доступа.",
            ["PkMove"] = "0 запрещает move для PK >= 5, 1 разрешает."
        },
        ["gate.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Уникальный ID gate.",
            ["Flag"] = "Тип gate по логике сервера.",
            ["Map"] = "Индекс карты.",
            ["X"] = "X координата телепорта.",
            ["Y"] = "Y координата телепорта.",
            ["TX"] = "Целевая X зона входа в gate.",
            ["TY"] = "Целевая Y зона входа в gate.",
            ["TargetGate"] = "ID связанного gate (0 если прямой выход).",
            ["Dir"] = "Направление персонажа после телепорта.",
            ["MinLevel"] = "Минимальный уровень, -1 отключает ограничение.",
            ["MaxLevel"] = "Максимальный уровень, -1 отключает ограничение.",
            ["MinReset"] = "Минимальный reset, -1 отключает ограничение.",
            ["MaxReset"] = "Максимальный reset, -1 отключает ограничение.",
            ["AccountLevel"] = "Уровень аккаунта для доступа."
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
        },
        ["custommove.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Уникальный ID записи custom move.",
            ["Name"] = "Текст команды, например /arena1.",
            ["Map"] = "Индекс карты телепорта.",
            ["X"] = "X координата.",
            ["Y"] = "Y координата.",
            ["MinLevel"] = "Минимальный уровень, -1 отключает ограничение.",
            ["MaxLevel"] = "Максимальный уровень, -1 отключает ограничение.",
            ["MinReset"] = "Минимальный reset, -1 отключает ограничение.",
            ["MaxReset"] = "Максимальный reset, -1 отключает ограничение.",
            ["MinMReset"] = "Минимальный master reset, -1 отключает ограничение.",
            ["MaxMReset"] = "Максимальный master reset, -1 отключает ограничение.",
            ["AccountLevel"] = "Уровень аккаунта для доступа.",
            ["PkMove"] = "0 запрещает move для PK >= 5, 1 разрешает."
        },
        ["customnpcmove.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Уникальный ID записи NPC move.",
            ["MonsterClass"] = "Класс NPC.",
            ["Map"] = "Карта NPC.",
            ["X"] = "X координата NPC.",
            ["Y"] = "Y координата NPC.",
            ["MoveMap"] = "Карта назначения.",
            ["MoveX"] = "X координата назначения.",
            ["MoveY"] = "Y координата назначения.",
            ["MinLevel"] = "Минимальный уровень, -1 отключает ограничение.",
            ["MaxLevel"] = "Максимальный уровень, -1 отключает ограничение.",
            ["MinReset"] = "Минимальный reset, -1 отключает ограничение.",
            ["MaxReset"] = "Максимальный reset, -1 отключает ограничение.",
            ["MinMReset"] = "Минимальный master reset, -1 отключает ограничение.",
            ["MaxMReset"] = "Максимальный master reset, -1 отключает ограничение.",
            ["AccountLevel"] = "Уровень аккаунта для доступа.",
            ["PkMove"] = "0 запрещает move для PK >= 5, 1 разрешает."
        },
        ["customnpccommand.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Уникальный ID записи NPC command.",
            ["MonsterClass"] = "Класс NPC.",
            ["Map"] = "Карта NPC.",
            ["X"] = "X координата NPC.",
            ["Y"] = "Y координата NPC.",
            ["Talk"] = "1 отправляет как NPC-диалог, 0 как обычную команду.",
            ["Command"] = "Текст команды, которая выполнится при клике на NPC."
        },
        ["customcommanddescription.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Уникальный ID описания команды.",
            ["Command"] = "Команда игрока (например, /help).",
            ["Description"] = "Текст подсказки/описания, показываемый игроку."
        },
        ["customranking.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Номер вкладки рейтинга.",
            ["Name"] = "Название вкладки рейтинга.",
            ["Col1"] = "Заголовок первой колонки.",
            ["Col2"] = "Заголовок второй колонки."
        },
        ["customrankuser.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Номер ранга.",
            ["Name"] = "Название ранга.",
            ["ResetMin"] = "Минимальное значение для ранга.",
            ["ResetMax"] = "Максимальное значение для ранга, -1 без верхней границы.",
            ["Coin1"] = "Награда Coin1.",
            ["Coin2"] = "Награда Coin2.",
            ["Coin3"] = "Награда Coin3."
        },
        ["custommonster.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Класс монстра.",
            ["MapNumber"] = "Индекс карты, -1 для всех карт.",
            ["MaxLife"] = "Процент HP монстра, -1 без изменения.",
            ["DamageMin"] = "Процент минимального урона, -1 без изменения.",
            ["DamageMax"] = "Процент максимального урона, -1 без изменения.",
            ["Defense"] = "Процент защиты, -1 без изменения.",
            ["AttackRate"] = "Процент attack rate, -1 без изменения.",
            ["DefenseRate"] = "Процент defense rate, -1 без изменения.",
            ["ExperienceRate"] = "Процент опыта, -1 без изменения.",
            ["KillMessage"] = "ID сообщения в Message, -1 отключено.",
            ["InfoMessage"] = "ID личного сообщения, -1 отключено.",
            ["RewardValue1"] = "Параметр награды 1.",
            ["RewardValue2"] = "Параметр награды 2.",
            ["SummonMonster"] = "Класс призываемого монстра, -1 отключено.",
            ["SummonMonsterCount"] = "Количество призываемых монстров.",
            ["SummonMonsterRate"] = "Шанс призыва в процентах."
        },
        ["customdeathmessage.xml"] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["Index"] = "Класс монстра.",
            ["Text"] = "Текст, показываемый игроку при смерти от монстра."
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
    private readonly string[] _readyXmlRelativePaths =
    {
        Path.Combine("Move", "Move.xml"),
        Path.Combine("Move", "MoveSummon.xml"),
        Path.Combine("Move", "Gate.xml"),
        Path.Combine("Item", "ItemMove.xml"),
        Path.Combine("Item", "ItemDrop.xml"),
        Path.Combine("Custom", "CustomMove.xml"),
        Path.Combine("Custom", "CustomNpcMove.xml"),
        Path.Combine("Custom", "CustomNpcCommand.xml"),
        Path.Combine("Custom", "CustomCommandDescription.xml"),
        Path.Combine("Custom", "CustomRanking.xml"),
        Path.Combine("Custom", "CustomRankUser.xml"),
        Path.Combine("Custom", "CustomMonster.xml"),
        Path.Combine("Custom", "CustomDeathMessage.xml")
    };
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
        ["MoveSummon.xml"] = new List<PresetOption>
        {
            new("MoveSummon: Базовые ограничения", new Dictionary<string, string>
            {
                ["MinLevel"] = "1",
                ["MaxLevel"] = "400",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["AccountLevel"] = "0",
                ["PkMove"] = "0"
            }),
            new("MoveSummon: Свободный доступ", new Dictionary<string, string>
            {
                ["MinLevel"] = "-1",
                ["MaxLevel"] = "-1",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["AccountLevel"] = "0",
                ["PkMove"] = "1"
            })
        },
        ["Gate.xml"] = new List<PresetOption>
        {
            new("Gate: Базовый вход", new Dictionary<string, string>
            {
                ["Flag"] = "0",
                ["TargetGate"] = "0",
                ["Dir"] = "0",
                ["MinLevel"] = "-1",
                ["MaxLevel"] = "-1",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["AccountLevel"] = "0"
            })
        },
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
        },
        ["CustomMove.xml"] = new List<PresetOption>
        {
            new("CustomMove: Базовый доступ", new Dictionary<string, string>
            {
                ["MinLevel"] = "1",
                ["MaxLevel"] = "-1",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["MinMReset"] = "-1",
                ["MaxMReset"] = "-1",
                ["AccountLevel"] = "0",
                ["PkMove"] = "1"
            }),
            new("CustomMove: Ограничить PK", new Dictionary<string, string>
            {
                ["PkMove"] = "0"
            })
        },
        ["CustomNpcMove.xml"] = new List<PresetOption>
        {
            new("CustomNpcMove: Базовый доступ", new Dictionary<string, string>
            {
                ["MinLevel"] = "1",
                ["MaxLevel"] = "-1",
                ["MinReset"] = "-1",
                ["MaxReset"] = "-1",
                ["MinMReset"] = "-1",
                ["MaxMReset"] = "-1",
                ["AccountLevel"] = "0",
                ["PkMove"] = "1"
            }),
            new("CustomNpcMove: Ограничить PK", new Dictionary<string, string>
            {
                ["PkMove"] = "0"
            })
        },
        ["CustomNpcCommand.xml"] = new List<PresetOption>
        {
            new("CustomNpcCommand: Диалог через NPC", new Dictionary<string, string>
            {
                ["Talk"] = "1"
            }),
            new("CustomNpcCommand: Тихий вызов команды", new Dictionary<string, string>
            {
                ["Talk"] = "0"
            })
        },
        ["CustomCommandDescription.xml"] = new List<PresetOption>
        {
            new("CustomCommandDescription: Базовое описание", new Dictionary<string, string>
            {
                ["Command"] = "/help",
                ["Description"] = "Показывает доступные команды."
            })
        },
        ["CustomRanking.xml"] = new List<PresetOption>
        {
            new("CustomRanking: Базовая вкладка", new Dictionary<string, string>
            {
                ["Name"] = "Top Players",
                ["Col1"] = "Name",
                ["Col2"] = "Score"
            })
        },
        ["CustomRankUser.xml"] = new List<PresetOption>
        {
            new("CustomRankUser: Базовый ранг", new Dictionary<string, string>
            {
                ["ResetMin"] = "0",
                ["ResetMax"] = "-1",
                ["Coin1"] = "0",
                ["Coin2"] = "0",
                ["Coin3"] = "0"
            })
        },
        ["CustomMonster.xml"] = new List<PresetOption>
        {
            new("CustomMonster: Без изменений", new Dictionary<string, string>
            {
                ["MapNumber"] = "-1",
                ["MaxLife"] = "-1",
                ["DamageMin"] = "-1",
                ["DamageMax"] = "-1",
                ["Defense"] = "-1",
                ["AttackRate"] = "-1",
                ["DefenseRate"] = "-1",
                ["ExperienceRate"] = "-1",
                ["KillMessage"] = "-1",
                ["InfoMessage"] = "-1",
                ["RewardValue1"] = "0",
                ["RewardValue2"] = "0",
                ["SummonMonster"] = "-1",
                ["SummonMonsterCount"] = "-1",
                ["SummonMonsterRate"] = "100"
            })
        },
        ["CustomDeathMessage.xml"] = new List<PresetOption>
        {
            new("CustomDeathMessage: Базовое сообщение", new Dictionary<string, string>
            {
                ["Text"] = "I am very strong monster"
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
        SynchronizeDependencies(true);
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

    private void SyncDependenciesClick(object sender, RoutedEventArgs e)
    {
        SynchronizeDependencies(true);
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
                    Text = "Зависимость: Data\\Move\\Gate.xml (или Gate.txt)",
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
            else if (fileName.Equals("CustomMove.xml", StringComparison.OrdinalIgnoreCase) &&
                     column.ColumnName.Equals("Map", StringComparison.OrdinalIgnoreCase) &&
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
            else if (fileName.Equals("CustomNpcMove.xml", StringComparison.OrdinalIgnoreCase) &&
                     (column.ColumnName.Equals("Map", StringComparison.OrdinalIgnoreCase) ||
                      column.ColumnName.Equals("MoveMap", StringComparison.OrdinalIgnoreCase)) &&
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
            else if (fileName.Equals("CustomNpcCommand.xml", StringComparison.OrdinalIgnoreCase) &&
                     column.ColumnName.Equals("Map", StringComparison.OrdinalIgnoreCase) &&
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

        LoadGateIndexFromXml(Path.Combine(_dataRoot, "Move", "Gate.xml"));
        if (_gateIndexList.Count == 0)
        {
            LoadGateIndexFromTxt(Path.Combine(_dataRoot, "Move", "Gate.txt"));
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

    private void LoadGateIndexFromXml(string xmlPath)
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
                if (TryInt(info, "Index", out var gateIndex) && _gateIndexSet.Add(gateIndex))
                {
                    _gateIndexList.Add(gateIndex);
                }
            }
        }
        catch
        {
        }
    }

    private void LoadGateIndexFromTxt(string txtPath)
    {
        if (File.Exists(txtPath) == false)
        {
            return;
        }

        foreach (var line in File.ReadLines(txtPath))
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
            description += " [WARN: не найден в Gate.xml/Gate.txt]";
        }

        if (fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase) &&
            fieldName.Equals("MapNumber", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(fieldValue, out var mapValue) &&
            _mapNameByIndex.TryGetValue(mapValue, out var mapName))
        {
            description += $" [Map: {mapName}]";
        }

        if (fileName.Equals("CustomMove.xml", StringComparison.OrdinalIgnoreCase) &&
            fieldName.Equals("Map", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(fieldValue, out var customMapValue) &&
            _mapNameByIndex.TryGetValue(customMapValue, out var customMapName))
        {
            description += $" [Map: {customMapName}]";
        }

        if (fileName.Equals("CustomNpcMove.xml", StringComparison.OrdinalIgnoreCase) &&
            (fieldName.Equals("Map", StringComparison.OrdinalIgnoreCase) ||
             fieldName.Equals("MoveMap", StringComparison.OrdinalIgnoreCase)) &&
            int.TryParse(fieldValue, out var npcMapValue) &&
            _mapNameByIndex.TryGetValue(npcMapValue, out var npcMapName))
        {
            description += $" [Map: {npcMapName}]";
        }

        if (fileName.Equals("CustomNpcCommand.xml", StringComparison.OrdinalIgnoreCase) &&
            fieldName.Equals("Map", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(fieldValue, out var npcCommandMapValue) &&
            _mapNameByIndex.TryGetValue(npcCommandMapValue, out var npcCommandMapName))
        {
            description += $" [Map: {npcCommandMapName}]";
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
        else if (fileName.Equals("MoveSummon.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateMoveSummon(document, errors);
        }
        else if (fileName.Equals("CustomMove.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomMove(document, errors);
        }
        else if (fileName.Equals("CustomNpcMove.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomNpcMove(document, errors);
        }
        else if (fileName.Equals("CustomNpcCommand.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomNpcCommand(document, errors);
        }
        else if (fileName.Equals("CustomCommandDescription.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomCommandDescription(document, errors);
        }
        else if (fileName.Equals("CustomRanking.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomRanking(document, errors);
        }
        else if (fileName.Equals("CustomRankUser.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomRankUser(document, errors);
        }
        else if (fileName.Equals("CustomMonster.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomMonster(document, errors);
        }
        else if (fileName.Equals("CustomDeathMessage.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateCustomDeathMessage(document, errors);
        }
        else if (fileName.Equals("Gate.xml", StringComparison.OrdinalIgnoreCase))
        {
            ValidateGate(document, errors);
        }

        ValidateCrossFileDependencies(document, fileName, errors);

        return errors;
    }

    private void ValidateCrossFileDependencies(XDocument document, string fileName, List<string> errors)
    {
        if (fileName.Equals("MoveSummon.xml", StringComparison.OrdinalIgnoreCase))
        {
            foreach (var info in document.Root?.Elements("Info") ?? Enumerable.Empty<XElement>())
            {
                if (TryInt(info, "Map", out var map) &&
                    map != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(map) == false)
                {
                    errors.Add($"MoveSummon[{map}]: Map not found in MapManager.txt");
                }
            }
        }
        else if (fileName.Equals("CustomMove.xml", StringComparison.OrdinalIgnoreCase))
        {
            foreach (var info in document.Root?.Elements("Info") ?? Enumerable.Empty<XElement>())
            {
                if (TryInt(info, "Index", out var index) == false)
                {
                    continue;
                }

                if (TryInt(info, "Map", out var map) &&
                    map != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(map) == false)
                {
                    errors.Add($"CustomMove[{index}]: Map={map} not found in MapManager.txt");
                }
            }
        }
        else if (fileName.Equals("CustomNpcMove.xml", StringComparison.OrdinalIgnoreCase))
        {
            foreach (var info in document.Root?.Elements("Info") ?? Enumerable.Empty<XElement>())
            {
                if (TryInt(info, "Index", out var index) == false)
                {
                    continue;
                }

                if (TryInt(info, "Map", out var map) &&
                    map != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(map) == false)
                {
                    errors.Add($"CustomNpcMove[{index}]: Map={map} not found in MapManager.txt");
                }

                if (TryInt(info, "MoveMap", out var moveMap) &&
                    moveMap != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(moveMap) == false)
                {
                    errors.Add($"CustomNpcMove[{index}]: MoveMap={moveMap} not found in MapManager.txt");
                }
            }
        }
        else if (fileName.Equals("CustomNpcCommand.xml", StringComparison.OrdinalIgnoreCase))
        {
            foreach (var info in document.Root?.Elements("Info") ?? Enumerable.Empty<XElement>())
            {
                if (TryInt(info, "Index", out var index) == false)
                {
                    continue;
                }

                if (TryInt(info, "Map", out var map) &&
                    map != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(map) == false)
                {
                    errors.Add($"CustomNpcCommand[{index}]: Map={map} not found in MapManager.txt");
                }
            }
        }
        else if (fileName.Equals("ItemDrop.xml", StringComparison.OrdinalIgnoreCase))
        {
            foreach (var info in document.Root?.Elements("Info") ?? Enumerable.Empty<XElement>())
            {
                if (TryInt(info, "Index", out var index) == false)
                {
                    continue;
                }

                if (TryInt(info, "MapNumber", out var map) &&
                    map != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(map) == false)
                {
                    errors.Add($"ItemDrop[{index}]: MapNumber={map} not found in MapManager.txt");
                }
            }
        }
        else if (fileName.Equals("CustomMonster.xml", StringComparison.OrdinalIgnoreCase))
        {
            foreach (var info in document.Root?.Elements("Info") ?? Enumerable.Empty<XElement>())
            {
                if (TryInt(info, "Index", out var index) == false)
                {
                    continue;
                }

                if (TryInt(info, "MapNumber", out var map) &&
                    map != -1 &&
                    _mapIndexSet.Count > 0 &&
                    _mapIndexSet.Contains(map) == false)
                {
                    errors.Add($"CustomMonster[{index}]: MapNumber={map} not found in MapManager.txt");
                }
            }
        }
    }

    private void SynchronizeDependencies(bool reloadCurrentFile)
    {
        try
        {
            LoadDependencyData();

            var filesToValidate = _readyXmlRelativePaths
                .Select(relative => Path.Combine(_dataRoot, relative))
                .ToList();

            var scanned = 0;
            var warnings = new List<string>();

            foreach (var filePath in filesToValidate)
            {
                if (File.Exists(filePath) == false)
                {
                    continue;
                }

                scanned++;
                var doc = XDocument.Load(filePath);
                var fileWarnings = ValidateDocument(doc, Path.GetFileName(filePath));

                foreach (var warning in fileWarnings)
                {
                    warnings.Add($"{Path.GetFileName(filePath)}: {warning}");
                }
            }

            var mirrored = MirrorReadyXmlFiles(filesToValidate);

            if (reloadCurrentFile &&
                string.IsNullOrWhiteSpace(_currentFilePath) == false &&
                File.Exists(_currentFilePath))
            {
                LoadXmlFile(_currentFilePath);
            }

            if (warnings.Count == 0)
            {
                StatusText.Text = $"Sync OK: {scanned} files, mirrored: {mirrored}";
            }
            else
            {
                StatusText.Text = $"Sync warning: {warnings.Count}";
                MessageBox.Show(string.Join(Environment.NewLine, warnings.Take(30)), "Dependency Sync", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Sync error: {ex.Message}";
            MessageBox.Show(ex.Message, "Dependency Sync", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private int MirrorReadyXmlFiles(IEnumerable<string> sourceFiles)
    {
        var readyRoot = ResolveReadyXmlRoot();
        var copied = 0;

        foreach (var source in sourceFiles)
        {
            if (File.Exists(source) == false)
            {
                continue;
            }

            var relativePath = Path.GetRelativePath(_dataRoot, source);
            var destination = Path.Combine(readyRoot, relativePath);
            var destinationDir = Path.GetDirectoryName(destination);

            if (string.IsNullOrWhiteSpace(destinationDir) == false)
            {
                Directory.CreateDirectory(destinationDir);
            }

            File.Copy(source, destination, true);
            copied++;
        }

        return copied;
    }

    private string ResolveReadyXmlRoot()
    {
        var dataDirectory = new DirectoryInfo(_dataRoot);
        var gameServerRoot = dataDirectory.Parent?.FullName ?? _dataRoot;
        return Path.Combine(gameServerRoot, ReadyXmlFolderName);
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

        if (fileName.Equals("Gate.xml", StringComparison.OrdinalIgnoreCase))
        {
            return root.Elements("Info").ToList();
        }

        if (fileName.Equals("ItemMove.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("MoveSummon.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomMove.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomNpcMove.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomNpcCommand.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomCommandDescription.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomRanking.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomRankUser.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomMonster.xml", StringComparison.OrdinalIgnoreCase) ||
            fileName.Equals("CustomDeathMessage.xml", StringComparison.OrdinalIgnoreCase) ||
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
                errors.Add($"Move[{index}]: Gate={gate} not found in Gate.xml/Gate.txt");
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

    private static void ValidateMoveSummon(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Map", out var map) == false)
            {
                errors.Add("MoveSummon: invalid Map");
                continue;
            }

            TryInt(info, "X", out var x);
            TryInt(info, "Y", out var y);
            TryInt(info, "TX", out var tx);
            TryInt(info, "TY", out var ty);
            TryInt(info, "MinLevel", out var minLevel);
            TryInt(info, "MaxLevel", out var maxLevel);
            TryInt(info, "MinReset", out var minReset);
            TryInt(info, "MaxReset", out var maxReset);
            TryInt(info, "PkMove", out var pkMove);

            if (map < 0)
            {
                errors.Add("MoveSummon: Map < 0");
            }

            if (x < 0 || y < 0 || tx < 0 || ty < 0)
            {
                errors.Add($"MoveSummon[{map}]: negative coordinate");
            }

            if (x > tx || y > ty)
            {
                errors.Add($"MoveSummon[{map}]: invalid area range");
            }

            if (maxLevel != -1 && minLevel != -1 && minLevel > maxLevel)
            {
                errors.Add($"MoveSummon[{map}]: MinLevel > MaxLevel");
            }

            if (maxReset != -1 && minReset != -1 && minReset > maxReset)
            {
                errors.Add($"MoveSummon[{map}]: MinReset > MaxReset");
            }

            if (pkMove is not (0 or 1))
            {
                errors.Add($"MoveSummon[{map}]: invalid PkMove");
            }
        }
    }

    private static void ValidateCustomMove(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomMove: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomMove: duplicate Index={index}");
            }

            var name = info.Attribute("Name")?.Value ?? string.Empty;
            if (string.IsNullOrWhiteSpace(name))
            {
                errors.Add($"CustomMove[{index}]: empty Name");
            }
            else if (names.Add(name) == false)
            {
                errors.Add($"CustomMove: duplicate Name={name}");
            }

            TryInt(info, "Map", out var map);
            TryInt(info, "X", out var x);
            TryInt(info, "Y", out var y);
            TryInt(info, "MinLevel", out var minLevel);
            TryInt(info, "MaxLevel", out var maxLevel);
            TryInt(info, "MinReset", out var minReset);
            TryInt(info, "MaxReset", out var maxReset);
            TryInt(info, "MinMReset", out var minMReset);
            TryInt(info, "MaxMReset", out var maxMReset);
            TryInt(info, "PkMove", out var pkMove);

            if (map < 0)
            {
                errors.Add($"CustomMove[{index}]: Map < 0");
            }

            if (x < 0 || y < 0)
            {
                errors.Add($"CustomMove[{index}]: negative coordinate");
            }

            if (maxLevel != -1 && minLevel != -1 && minLevel > maxLevel)
            {
                errors.Add($"CustomMove[{index}]: MinLevel > MaxLevel");
            }

            if (maxReset != -1 && minReset != -1 && minReset > maxReset)
            {
                errors.Add($"CustomMove[{index}]: MinReset > MaxReset");
            }

            if (maxMReset != -1 && minMReset != -1 && minMReset > maxMReset)
            {
                errors.Add($"CustomMove[{index}]: MinMReset > MaxMReset");
            }

            if (pkMove is not (0 or 1))
            {
                errors.Add($"CustomMove[{index}]: invalid PkMove");
            }
        }
    }

    private static void ValidateCustomNpcMove(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomNpcMove: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomNpcMove: duplicate Index={index}");
            }

            TryInt(info, "MonsterClass", out var monsterClass);
            TryInt(info, "Map", out var map);
            TryInt(info, "X", out var x);
            TryInt(info, "Y", out var y);
            TryInt(info, "MoveMap", out var moveMap);
            TryInt(info, "MoveX", out var moveX);
            TryInt(info, "MoveY", out var moveY);
            TryInt(info, "MinLevel", out var minLevel);
            TryInt(info, "MaxLevel", out var maxLevel);
            TryInt(info, "MinReset", out var minReset);
            TryInt(info, "MaxReset", out var maxReset);
            TryInt(info, "MinMReset", out var minMReset);
            TryInt(info, "MaxMReset", out var maxMReset);
            TryInt(info, "PkMove", out var pkMove);

            if (monsterClass < 0)
            {
                errors.Add($"CustomNpcMove[{index}]: MonsterClass < 0");
            }

            if (map < 0 || moveMap < 0)
            {
                errors.Add($"CustomNpcMove[{index}]: Map/MoveMap < 0");
            }

            if (x < 0 || y < 0 || moveX < 0 || moveY < 0)
            {
                errors.Add($"CustomNpcMove[{index}]: negative coordinate");
            }

            if (maxLevel != -1 && minLevel != -1 && minLevel > maxLevel)
            {
                errors.Add($"CustomNpcMove[{index}]: MinLevel > MaxLevel");
            }

            if (maxReset != -1 && minReset != -1 && minReset > maxReset)
            {
                errors.Add($"CustomNpcMove[{index}]: MinReset > MaxReset");
            }

            if (maxMReset != -1 && minMReset != -1 && minMReset > maxMReset)
            {
                errors.Add($"CustomNpcMove[{index}]: MinMReset > MaxMReset");
            }

            if (pkMove is not (0 or 1))
            {
                errors.Add($"CustomNpcMove[{index}]: invalid PkMove");
            }
        }
    }

    private static void ValidateCustomNpcCommand(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomNpcCommand: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomNpcCommand: duplicate Index={index}");
            }

            TryInt(info, "MonsterClass", out var monsterClass);
            TryInt(info, "Map", out var map);
            TryInt(info, "X", out var x);
            TryInt(info, "Y", out var y);
            TryInt(info, "Talk", out var talk);

            var command = info.Attribute("Command")?.Value ?? string.Empty;

            if (monsterClass < 0)
            {
                errors.Add($"CustomNpcCommand[{index}]: MonsterClass < 0");
            }

            if (map < 0)
            {
                errors.Add($"CustomNpcCommand[{index}]: Map < 0");
            }

            if (x < 0 || y < 0)
            {
                errors.Add($"CustomNpcCommand[{index}]: negative coordinate");
            }

            if (talk is not (0 or 1))
            {
                errors.Add($"CustomNpcCommand[{index}]: invalid Talk");
            }

            if (string.IsNullOrWhiteSpace(command))
            {
                errors.Add($"CustomNpcCommand[{index}]: empty Command");
            }
        }
    }

    private static void ValidateCustomCommandDescription(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();
        var commands = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomCommandDescription: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomCommandDescription: duplicate Index={index}");
            }

            var command = info.Attribute("Command")?.Value ?? string.Empty;
            var description = info.Attribute("Description")?.Value ?? string.Empty;

            if (string.IsNullOrWhiteSpace(command))
            {
                errors.Add($"CustomCommandDescription[{index}]: empty Command");
            }
            else if (commands.Add(command) == false)
            {
                errors.Add($"CustomCommandDescription: duplicate Command={command}");
            }

            if (string.IsNullOrWhiteSpace(description))
            {
                errors.Add($"CustomCommandDescription[{index}]: empty Description");
            }
        }
    }

    private static void ValidateCustomRanking(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomRanking: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomRanking: duplicate Index={index}");
            }

            var name = info.Attribute("Name")?.Value ?? string.Empty;
            var col1 = info.Attribute("Col1")?.Value ?? string.Empty;
            var col2 = info.Attribute("Col2")?.Value ?? string.Empty;

            if (string.IsNullOrWhiteSpace(name))
            {
                errors.Add($"CustomRanking[{index}]: empty Name");
            }
            else if (names.Add(name) == false)
            {
                errors.Add($"CustomRanking: duplicate Name={name}");
            }

            if (string.IsNullOrWhiteSpace(col1))
            {
                errors.Add($"CustomRanking[{index}]: empty Col1");
            }

            if (string.IsNullOrWhiteSpace(col2))
            {
                errors.Add($"CustomRanking[{index}]: empty Col2");
            }
        }
    }

    private static void ValidateCustomRankUser(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomRankUser: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomRankUser: duplicate Index={index}");
            }

            var name = info.Attribute("Name")?.Value ?? string.Empty;
            TryInt(info, "ResetMin", out var resetMin);
            TryInt(info, "ResetMax", out var resetMax);
            TryInt(info, "Coin1", out var coin1);
            TryInt(info, "Coin2", out var coin2);
            TryInt(info, "Coin3", out var coin3);

            if (string.IsNullOrWhiteSpace(name))
            {
                errors.Add($"CustomRankUser[{index}]: empty Name");
            }

            if (resetMax != -1 && resetMin > resetMax)
            {
                errors.Add($"CustomRankUser[{index}]: ResetMin > ResetMax");
            }

            if (coin1 < 0 || coin2 < 0 || coin3 < 0)
            {
                errors.Add($"CustomRankUser[{index}]: negative Coin reward");
            }
        }
    }

    private static void ValidateCustomMonster(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<(int, int)>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomMonster: invalid Index");
                continue;
            }

            TryInt(info, "MapNumber", out var map);
            TryInt(info, "SummonMonsterRate", out var summonRate);

            if (indices.Add((index, map)) == false)
            {
                errors.Add($"CustomMonster: duplicate Index/MapNumber={index}/{map}");
            }

            if (map < -1)
            {
                errors.Add($"CustomMonster[{index}]: MapNumber < -1");
            }

            if (summonRate < 0 || summonRate > 100)
            {
                errors.Add($"CustomMonster[{index}]: SummonMonsterRate out of range 0..100");
            }
        }
    }

    private static void ValidateCustomDeathMessage(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("CustomDeathMessage: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"CustomDeathMessage: duplicate Index={index}");
            }

            var text = info.Attribute("Text")?.Value ?? string.Empty;

            if (string.IsNullOrWhiteSpace(text))
            {
                errors.Add($"CustomDeathMessage[{index}]: empty Text");
            }
        }
    }

    private static void ValidateGate(XDocument document, List<string> errors)
    {
        var infoList = document.Root?.Elements("Info").ToList() ?? new List<XElement>();
        var indices = new HashSet<int>();

        foreach (var info in infoList)
        {
            if (TryInt(info, "Index", out var index) == false)
            {
                errors.Add("Gate: invalid Index");
                continue;
            }

            if (indices.Add(index) == false)
            {
                errors.Add($"Gate: duplicate Index={index}");
            }

            TryInt(info, "Map", out var map);
            TryInt(info, "X", out var x);
            TryInt(info, "Y", out var y);
            TryInt(info, "TX", out var tx);
            TryInt(info, "TY", out var ty);
            TryInt(info, "TargetGate", out var targetGate);
            TryInt(info, "MinLevel", out var minLevel);
            TryInt(info, "MaxLevel", out var maxLevel);
            TryInt(info, "MinReset", out var minReset);
            TryInt(info, "MaxReset", out var maxReset);

            if (map < 0)
            {
                errors.Add($"Gate[{index}]: Map < 0");
            }

            if (x < 0 || y < 0 || tx < 0 || ty < 0)
            {
                errors.Add($"Gate[{index}]: negative coordinate");
            }

            if (targetGate < 0)
            {
                errors.Add($"Gate[{index}]: TargetGate < 0");
            }

            if (maxLevel != -1 && minLevel != -1 && minLevel > maxLevel)
            {
                errors.Add($"Gate[{index}]: MinLevel > MaxLevel");
            }

            if (maxReset != -1 && minReset != -1 && minReset > maxReset)
            {
                errors.Add($"Gate[{index}]: MinReset > MaxReset");
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
