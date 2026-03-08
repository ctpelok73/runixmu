# ConfigEditor (S6E3) — как редактировать XML

## Что это
GUI-редактор конфигов `GameServer\Data\*.xml` для S6E3.

## Базовый сценарий
1. В дереве слева выберите файл (`Move.xml`, `ItemMove.xml`, `ItemDrop.xml`).
2. В центре измените значения в таблице `Records`.
3. Справа проверьте расшифровку поля в `Property Inspector`.
4. Нажмите `Validate`.
5. Нажмите `Backup`.
6. Нажмите `Save`.

## Что означают кнопки
- `Open`: обновляет дерево XML файлов.
- `Validate`: проверка XML и базовых правил S6E3.
- `Backup`: копия текущего файла в `Data\_backup\yyyyMMdd_HHmm\...`.
- `Save`: сохраняет текущие значения обратно в XML.
- `Sync`: проверяет зависимости и зеркалит готовые XML в `GameServer\Data_XML_READY\...`.

## Правила по файлам

### Move.xml
- `Index` должен быть уникальным.
- `Gate` должен быть `>= 0`.
- Если `MinLevel` и `MaxLevel` не `-1`, то `MinLevel <= MaxLevel`.
- Если `MinReset` и `MaxReset` не `-1`, то `MinReset <= MaxReset`.

### ItemMove.xml
- `Index` должен быть уникальным.
- Флаги должны быть `0` или `1`.
- Поддерживаются оба формата полей:
  - `BanDrop/BanSell/BanTrade/BanVaul`
  - `AllowDrop/AllowSell/AllowTrade/AllowVault`

### ItemDrop.xml
- Проверяется корректность числовых полей.
- `DropRate` не должен быть меньше `-1`.
- Если `MonsterLevelMin` и `MonsterLevelMax` не `-1`, то `Min <= Max`.

## Примечание
XSD-шаблоны для первой волны лежат в `Tools\ConfigEditor\Schema`.
Готовые для выкладки XML автоматически копируются в `GameServer\Data_XML_READY` с сохранением структуры папок (`Move\...`, `Item\...`, `Custom\...`).
