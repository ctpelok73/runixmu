## Что именно ты просишь
- Вынести **полностью всё GM-меню (ImGui окно, вкладки Items/Monsters, диагностика/сканеры, превью, стейты/кэши)** из [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp) в отдельные файлы.
- По возможности **вернуть [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp) к исходнику** и оставить там только минимальные вызовы нового модуля + строго необходимые интеграции.
- Показать, какие файлы были затронуты/какие куски кода — через `git diff`/`git log` (это сделаю после подтверждения, т.к. сейчас plan mode).

## Текущее состояние (кратко)
- Фактически изменялся только [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp) (внутри `#ifdef EFFECT_MNG_HANDLE`). Остальные файлы я в основном только читал для трассировки.
- Внутри файла сейчас находятся добавленные блоки под превью/загрузку моделей (видно в районе [CGMEffectHandle.cpp:L214-L447](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp#L214-L447)).

## План работ (после подтверждения)
### 1) Восстановить исходник CGMEffectHandle.cpp
- С помощью git вернуть [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp) в состояние **до моих правок** (reset/checkout конкретного файла).
- Это даст тебе «чистую базу» и снимет проблему, что я “врезался” в файл.

### 2) Создать отдельный модуль целиком под GM-меню
- Добавить 2 новых файла в `Main5.2/source/` (это необходимо, потому что цель — вынести меню):
  - `CGMMenuEffectUI.h`
  - `CGMMenuEffectUI.cpp`
- В `CGMMenuEffectUI.cpp` перенести **всё содержимое GM-окна**:
  - логику `ImGui::Begin("RenderMesh Tools"...)`, вкладки, layout, preview capture rect, диагностику/сканеры, списки, кнопки Spawn и т.д. (то, что сейчас живёт в `RenderFrame()` и `RenderContents()` в [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp#L751-L1166)).
  - все `static`-состояния/кэши, относящиеся к меню (включая превью).
  - всю вспомогательную “внутреннюю” логику меню, которая сейчас оформлена как `static` функции в этом же файле.

### 3) Оставить в CGMEffectHandle.cpp только минимальные “хуки”
- В [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp) оставить:
  - `#include "CGMMenuEffectUI.h"`
  - В `CGFxEffectHandle::RenderFrame()` — один вызов типа `GMMenuEffectUI::Render(*this)`.
  - В местах закрытия окна / смены вкладок (или в `Unload/Reset`) — `GMMenuEffectUI::Reset()`.
- Всё остальное (именно UI и всё что к нему относится) — в новом модуле.

### 4) Внести только необходимые правки для превью (внутри нового модуля)
- Все текущие попытки фикса превью (подгрузка моделей, создание временного персонажа/рендер) будут жить **только** в `CGMMenuEffectUI.cpp`.
- В результате `CGMEffectHandle.cpp` останется максимально близким к исходнику.

### 5) Верификация и отчёт по git
- Собрать [Main.sln](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/Main.sln) (Release).
- Показать тебе:
  - список реально затронутых файлов
  - `git diff` (точно какие строки изменены)
  - `git log -p` по файлам (если нужно — по диапазону коммитов)

## Гарантии по минимизации изменений
- Цель: **CGMEffectHandle.cpp возвращается к почти исходному виду**, и там остаются только делегирование в новый модуль + необходимые include/Reset вызовы.

Если подтверждаешь этот план — я перейду к выносу меню целиком, откату файла к исходнику через git и затем аккуратно верну только минимальные точки интеграции.