# Contributing

Thanks for contributing to RunixMU.

## How to Propose a Change

1. Create an issue describing the bug or idea
2. Fork the repository and create a dedicated branch
3. Make atomic changes
4. Verify local build for affected components
5. Open a Pull Request with a clear description

## Contribution Requirements

- Keep compatibility with Mu Online S6E3
- Do not add content from later seasons
- Prefer configuration-driven changes
- Do not commit secrets, credentials, keys, or private database dumps
- Describe config migrations and SQL changes in the PR

## Pull Request Standard

- One PR = one logical task
- Add manual verification steps
- Attach screenshots or logs for UI/runtime changes when relevant
- Include risk notes and rollback plan

## Commit Messages

Recommended format:

`type(scope): short description`

Examples:

- `fix(gameserver): validate move gate range`
- `feat(config): add xml schema check for itemdrop`
