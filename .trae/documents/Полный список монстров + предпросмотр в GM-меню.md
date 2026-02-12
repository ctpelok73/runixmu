## Цель
Сделать в GM-меню полноценный браузер монстров/NPC:
- показывает полный список (стандартные + кастомные)
- даёт стабильный 3D-предпросмотр
- выводит все характеристики (серверные параметры из Monster.txt)
- поддерживает фильтрацию/сортировку и диагностику проблемных моделей

## Текущее состояние и причина бага
- Сейчас список в GM-меню берётся из `GMMonsterMng->GetAll()` — это только кастомные записи `CUSTOM_MONSTER_INFO`, поэтому “стандартные” монстры не отображаются как список.
- Для стандартных монстров корректный путь создания/инициализации уже есть на клиенте: `CreateMonster(Type,...)` (внутри большой роутинг + `OpenMonsterModel/OpenNpc` + спец-случаи) и затем `RenderCharacter`.
- “Все характеристики” (Level/Life/Dmg/Defense/Resist/…) есть на сервере в `Monster\Monster.txt` (`CMonsterManager::m_MonsterInfo[]`). Клиент их не знает без синхронизации.

## Архитектура решения
### 1) Источник полного списка
- Источник истины по списку/характеристикам: **GameServer** (`gMonsterManager.GetInfo(i)` для `i=0..MAX_MONSTER_INFO-1`).
- Клиент запрашивает у сервера таблицу монстров (GM-only) и кэширует её для UI.
- Клиент дополнительно помечает “Custom” по наличию записи в `GMMonsterMng` (для фильтра «Standard/Custom»), но статы берёт с сервера.

### 2) Протокол (Client ↔ GameServer)
- Добавить новый head (например `0xFA`) с поддержкой C1/C2 пакетов.
  - **Request (C1)**: `0xFA:0x00` — запрос списка монстров.
  - **Begin (C1)**: `0xFA:0x01` — `totalCount`, версия/флаги.
  - **Data chunk (C2)**: `0xFA:0x02` — `start`, `count`, массив записей.
  - **End (C1)**: `0xFA:0x03` — завершение.
- Формат записи: `GM_MONSTER_INFO_NET` (packed) — поля из `MONSTER_INFO` (включая `Resistance[7]` и elemental поля; на серверах без elemental — заполнять 0).
- **Server-side**:
  - обработчик `CGGMMonsterDbRequest(aIndex)` в `Protocol.cpp`.
  - проверка прав GM (как в `CGGMItemSpawnRecv/CGGMClearInventoryRecv`).
  - отправка чанками (например по 20–30 записей на C2 пакет), чтобы не делать 1000 маленьких пакетов.
- **Client-side**:
  - приём в `ProtocolCoreEx` (Main5.2/source/Protocol.cpp) и сборка кэша.

## UI/UX в GM-меню (Monsters/NPCs)
### 3) Список
- Вместо `GMMonsterMng->GetAll()` использовать кэш “MonsterDB” от сервера.
- Колонки (ImGui Table): `Index`, `Name`, `Kind (Monster/NPC/Other)`, `Source (Std/Custom)`, `Level`, `Life`, `Dmg`, `Def`.
- Фильтры:
  - текстовый поиск (по имени/индексу)
  - выпадающий фильтр по типу: `All / Monster / NPC / Other`
  - выпадающий фильтр по источнику: `All / Standard / Custom`
  - чекбоксы: `Only with model`, `Only missing model` (по результатам проверки модели)
- Сортировка:
  - кликабельная сортировка по колонкам таблицы (Index/Name/Level/Life/…)

### 4) Предпросмотр
- Для выбранной строки предпросмотр строить через `CreateMonster(index,0,0,key)` (это покрывает стандартные и кастомные), далее `RenderCharacter`.
- Управление предпросмотром:
  - auto-rotate (уже есть)
  - ручной поворот/масштаб (ползунки)
  - выбор анимации (если есть действия у модели): безопасный selection (fallback на 0)
- Диагностика предпросмотра:
  - статус: `Model OK / Missing / Client not supporting`.

### 5) Панель характеристик
- Выводить все поля из `GM_MONSTER_INFO_NET`:
  - базовые: Level, Life, Mana, DamageMin/Max, Defense/MDefense, Attack/DefenseRate
  - скорости/дистанции: MoveSpeed, AttackSpeed, MoveRange, AttackRange, ViewRange
  - AI/Attribute/RegenTime/ItemRate/MoneyRate/MaxItemLevel
  - Resist[7]
  - Elemental поля (если применимо)

## Тестирование и “устранить баги стандартных монстров”
### 6) Встроенный тест/скан
- Кнопка `Scan all models`:
  - итерация по списку с лимитом по времени на кадр
  - попытка создать монстра (без рендера) и проверить готовность модели
  - подсчёты: missing model / client unsupported
  - опционально лог в файл (через `ErrorReport`) с индексом/именем/причиной

### 7) Проверки на практике
- Проверить:
  - количество записей в UI совпадает с валидными `gMonsterManager.GetInfo(i)`
  - стандартные монстры появляются и превью работает
  - кастомные монстры/NPC не ломаются
  - фильтрация/сортировка не лагует на 1000 записей

## Изменяемые файлы (ожидаемо)
- Client: `Main5.2/source/Protocol.h`, `Main5.2/source/Protocol.cpp`, `Main5.2/source/CGMEffectHandle_GMMenu.cpp` (+ при необходимости небольшой хелпер-файл под UI-модель данных).
- Server: `GameServer/GameServer/Protocol.cpp` (+ возможно `Protocol.h` для структур), и небольшой модуль/функции отправки MonsterDB.

Если утвердить план — начну с протокола (сервер+клиент), затем UI таблица/фильтры, затем предпросмотр и скан/логирование.