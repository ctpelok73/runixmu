## Что нужно почистить
- Удалить неиспользуемые include в [CGMEffectHandle.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle.cpp) (там больше нет ImGui/GM-меню, остался runtime effect/спавн-пакеты).
- Удалить неиспользуемые include в [CGMEffectHandle_GMMenu.cpp](file:///Y:/main5.2/SRC%205.2%20BASE/SRCMainGS/Source/Main5.2/source/CGMEffectHandle_GMMenu.cpp) (там уже есть только меню).

## Результаты анализа (по текущему состоянию файлов)
- В CGMEffectHandle.cpp ImGui используется только в include-строках: можно убрать `imgui_impl_win32.h`/`imgui_impl_opengl2.h`.
- В CGMEffectHandle.cpp нет `std::set`, `va_list`, `std::algorithm`-вызовов: можно убрать `<set>`, `<algorithm>`, `<cstdarg>`.
- В CGMEffectHandle.cpp больше не используется GM-меню/монстры: можно убрать `CSItemOption.h`, `CSParts.h`, `ZzzCharacter.h`, `SocketSystem.h`, `CGMMonsterMng.h`, `supportingfeature.h`, `ErrorReport.h`, `muConsoleDebug.h`.
- В CGMEffectHandle_GMMenu.cpp `MonkSystem.h`, `SocketSystem.h`, `ZzzTexture.h`, `<algorithm>` не используются; при этом `CSItemOption.h/CSParts.h`, `ZzzOpenData.h`, `ZzzCharacter.h`, `ZzzOpenglUtil.h`, `<set>`, `<string>`, `<vector>`, `<cstdarg>` нужны.

## Изменения в коде
1. В CGMEffectHandle.cpp оставить только реально нужные заголовки (ориентир: `stdafx.h`, `ZzzTexture.h`, `NewUISystem.h`, `CGMEffectHandle.h`, `MonkSystem.h`, `Protocol.h`, `ZzzInventory.h`, `ZzzOpenglUtil.h`, плюс минимум STL: `<string>`, `<vector>` если требуется напрямую).
2. В CGMEffectHandle_GMMenu.cpp удалить неиспользуемые include (как минимум `ZzzTexture.h`, `MonkSystem.h`, `SocketSystem.h`, `<algorithm>`), остальные оставить.

## Проверка
- Собрать через бат: `build_main_x86.bat Release x86`.
- Если сборка упадёт из-за недостающего forward-declare/инклюда (редко, но возможно из-за PCH), точечно вернуть один нужный заголовок.

## Отчёт
- После сборки приложить `git diff --stat` и при необходимости `git diff` по двум cpp + двум vcxproj файлам (если их трогать не придётся, их не меняем).