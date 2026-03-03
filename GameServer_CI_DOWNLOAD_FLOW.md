# GameServer CI build + download flow (без оплаты GitHub-hosted)

Ошибка вида:
`The job was not started because recent account payments have failed...`
означает, что запуск на GitHub-hosted runner заблокирован биллингом.

Чтобы это исключить, workflow переведён **только на self-hosted Windows runner**.

## Что теперь в репозитории

- `.github/workflows/gameserver-build.yml` запускается вручную (`workflow_dispatch`)
- job работает только на:
  - `runs-on: [self-hosted, windows]`

То есть платные `windows-latest` GitHub-hosted раннеры больше не используются.

## Как запустить сборку

1. На твоём Windows сервере/ПК в репозитории открой:
   **Settings -> Actions -> Runners -> New self-hosted runner**.
2. Выбери `Windows`, установи runner и запусти его как сервис.
3. Убедись, что установлены:
   - Visual Studio Build Tools (MSBuild)
   - NuGet
4. Открой **Actions -> GameServer Build Artifact (Self-Hosted) -> Run workflow**.
5. Выбери:
   - `configuration = Release` (или Debug)
   - `platform = Win32` (или x64)
6. После completion скачай артефакт `GameServer-...`.

## Для твоего replay-сценария

На каждый replay-коммит:
1) push коммита
2) ручной запуск workflow
3) скачивание `exe`
4) проверка good/bad

Так можно локализовать плохой коммит без упора в Billing & plans GitHub-hosted.
