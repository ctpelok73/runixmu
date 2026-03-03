# GameServer CI build + download flow

Да, это можно автоматизировать: репозиторий сам собирает `GameServer.exe`,
а ты просто скачиваешь артефакт из GitHub Actions и проверяешь.

## Что уже добавлено

- workflow: `.github/workflows/gameserver-build.yml`
- триггеры:
  - `push` в ветки `work` и `gameserver-replay*` (если изменился `GameServer/**`)
  - ручной запуск `workflow_dispatch` с выбором `Configuration`/`Platform`

## Как использовать по шагам

1. Запускаешь replay-перенос (или руками делаешь следующий commit только для GameServer).
2. Пушишь ветку в GitHub (например `gameserver-replay-run`).
3. Ждёшь, пока workflow `GameServer Build Artifact` завершится.
4. Открываешь run -> `Artifacts` -> скачиваешь архив `GameServer-...`.
5. Проверяешь exe локально.
6. Если всё ок — идёшь к следующему commit.

## Рекомендуемый цикл под твою задачу

- на каждый replay-commit:
  1) push
  2) скачать артефакт
  3) проверить
  4) отметить good/bad

Так ты получишь точный commit, где возникает поломка, без ручной локальной сборки каждый раз.
