# ServerQuickManager.Qt

Каркас нового GUI на Qt 6 (Widgets) для будущей миграции `ServerQuickManager`.

## Что уже есть

- Окно с 3 вкладками: `Публичный IP`, `Config SQL`, `Автозапуск`
- Базовая современная компоновка через `QTabWidget + QGroupBox + QTableWidget`
- Подготовленный `CMakeLists.txt` с зависимостями `Qt6::Widgets`, `Qt6::Network`, `Qt6::Sql`

## Требования

- Qt 6 с компонентами Widgets, Network, Sql
- CMake 3.21+
- Visual Studio 2022 Build Tools

## Сборка

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64"
cmake --build build --config Release
```

Если Qt установлен в другом пути, укажите свой `CMAKE_PREFIX_PATH`.

## Следующий шаг миграции

1. Перенести модели данных и настройки (`QSettings`)
2. Перенести запуск процессов (`QProcess`)
3. Перенести SQL/ODBC проверку (`QSqlDatabase`)
4. Перенести сканирование/массовое обновление конфигов
