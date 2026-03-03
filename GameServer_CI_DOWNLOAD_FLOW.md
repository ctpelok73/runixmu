# GameServer CI build + download flow

Если GitHub пишет про оплату, значит workflow пытается стартовать на
`github-hosted` раннере (`windows-latest`) и закончились/недоступны минуты.

Ниже настройка без оплаты: **через self-hosted Windows runner**.

## Что изменено в workflow

- `.github/workflows/gameserver-build.yml` теперь запускается **только вручную** (`workflow_dispatch`)
- добавлен выбор `runner_type`:
  - `self-hosted` (по умолчанию, без GitHub-hosted billing)
  - `github-hosted` (если у тебя есть оплаченные минуты)

## Как запустить без оплаты (рекомендуется)

1. На своей Windows-машине открой репозиторий -> **Settings -> Actions -> Runners**.
2. Добавь новый self-hosted runner (OS: Windows) и запусти его как сервис.
3. Убедись, что на машине есть Visual Studio Build Tools + NuGet/MSBuild.
4. В Actions запусти `GameServer Build Artifact`:
   - `runner_type = self-hosted`
   - `configuration = Release`
   - `platform = Win32` (или `x64`)
5. После успешной сборки скачай артефакт `GameServer-...`.

## Проверка по шагам replay

- делаешь следующий replay-коммит по `GameServer`
- запускаешь workflow на `self-hosted`
- скачиваешь exe и проверяешь
- отмечаешь good/bad
- переходишь к следующему коммиту

Так ты находишь проблемный коммит без ограничений платных GitHub-hosted минут.
