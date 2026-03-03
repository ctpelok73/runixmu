# GameServer replay + regression isolation plan

Цель: заново перенести изменения **только для `GameServer/`** между
`6acf44b51df110321a02e3f70ae9f46a911468a4` и `8ff3fcbb00076b76920593ede1f456c1ed1166e5`
по одному коммиту, чтобы быстро найти момент появления проблемы.

## 1) Подготовка чистой ветки от baseline

```bash
git checkout -B gameserver-replay 6acf44b51df110321a02e3f70ae9f46a911468a4
```

## 2) Список только релевантных коммитов

```bash
git rev-list --reverse --ancestry-path \
  6acf44b51df110321a02e3f70ae9f46a911468a4..8ff3fcbb00076b76920593ede1f456c1ed1166e5 \
  -- GameServer
```

Эта команда исключает коммиты, которые не трогали `GameServer/`.

## 3) Переигрывание коммитов только для GameServer

Используй helper-скрипт:

```bash
./gameserver_replay_commits.sh --branch gameserver-replay
```

Если уже есть smoke-скрипт для GameServer, можно сразу ловить первый «плохой» шаг:

```bash
./gameserver_replay_commits.sh --branch gameserver-replay \
  --test-cmd "./scripts/test_gameserver.sh"
```

Скрипт остановится на первом упавшем шаге и покажет исходный SHA.
Если нужен полный проход со сбором всех падений — добавь `--keep-going`.

Что он делает:
- берёт список коммитов в диапазоне только по `GameServer/`;
- для каждого коммита применяет **только diff внутри `GameServer/`**;
- создаёт новый replay-коммит с ссылкой на исходный SHA.

Пробный прогон (без изменений):

```bash
./gameserver_replay_commits.sh --dry-run
```

## 4) Локализация проблемного коммита на replay-ветке

После каждого replay-коммита запускай smoke/regression-проверку GameServer.

Пример ручного цикла:

```bash
git log --oneline --reverse
# берём очередной коммит
# запускаем ваш тест/старт GameServer
```

Если хочется автоматизировать бинарный поиск:

```bash
git bisect start
git bisect bad
git bisect good <sha_где_ещё_всё_работает>
git bisect run ./scripts/test_gameserver.sh
```

> Если в репозитории нет `scripts/test_gameserver.sh`, создай временный скрипт,
> который поднимает GameServer и возвращает `0` при успехе и `1` при воспроизведении бага.

## 5) Практический workflow для безопасного переноса фиксов

1. Создать `gameserver-replay` от `6acf44...`.
2. Прогнать `--dry-run`, убедиться в порядке коммитов.
3. Запустить replay.
4. После каждого шага проверять GameServer smoke-тестом.
5. На первом «плохом» коммите:
   - открыть diff (`git show <sha>`),
   - сверить изменения по подсистемам (network/db/game loop),
   - сделать targeted fix отдельным коммитом.
6. После исправления — повторный прогон smoke/regression.

Такой подход даёт воспроизводимость и чёткий момент, где именно возникла регрессия.
