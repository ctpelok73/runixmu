# GameServer regression replay notes

Проведён фактический replay только для `GameServer/` в диапазоне:
`6acf44b51df110321a02e3f70ae9f46a911468a4..8ff3fcbb00076b76920593ede1f456c1ed1166e5`.

## Что было переиграно

На ветке `gameserver-replay-run` успешно применились 5 коммитов:

1. `7f4d282` — GM menu item spawn
2. `c73f434` — clear inventory command
3. `c5f228b` — monster database sync packets
4. `77fc6e4` — update character stats on equipment change
5. `8ff3fcb` — integer shift + random device init

## Наиболее рискованный шаг (по коду)

Без runtime-теста нельзя строго доказать «первый плохой» SHA,
но по коду наиболее чувствительный шаг — `8ff3fcb`, т.к. он меняет:

- генерацию seed через `std::random_device` в `Util.cpp`;
- арифметику цены предмета в `Item.cpp` (`1 << level` -> `1LL << level`);
- тип счётчика инвентаря holy-item в `GMHolyItem.cpp`.

Это изменения в базовой логике/утилитах, а не только в GM packet handlers.

## Как найти точный commit автоматически

Запусти replay с тест-командой:

```bash
./gameserver_replay_commits.sh --branch gameserver-replay \
  --test-cmd "./scripts/test_gameserver.sh"
```

Скрипт остановится на первом падении и покажет исходный SHA,
после чего можно сделать targeted-fix поверх replay-ветки.
