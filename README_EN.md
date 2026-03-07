# RunixMU S6E3 Open Source

Open source Mu Online Season 6 Episode 3 codebase (server + client + tools), focused on stability, transparent development, and reproducible configuration workflows.

## Project Goals

- Keep a stable S6E3 stack without late-season content
- Prefer config-driven and production-safe changes
- Improve maintainability through an open GitHub workflow
- Lower the entry barrier for new contributors

## Repository Structure

- `GameServer/` — game server
- `DataServer/` — game data storage services
- `JoinServer/` — authentication and session services
- `ConnectServer/` — connection gateway service
- `Main5.2/` — client source
- `Launcher/` — launcher source
- `UpdaterService/` — update delivery service
- `Tools/` — utility tools

## Tech Stack

- C++ (Visual Studio toolchain)
- MSSQL
- XML/TXT/INI configuration files
- Node.js (for `UpdaterService`)

## Quick Start

1. Clone the repository
2. Open the required `.sln` file in Visual Studio
3. Configure MSSQL connection settings for server components
4. Verify game configs in related `Data` directories
5. Build and run services in order: Data/Join/Connect/Game

## Open Source Workflow

- Bugs and feature requests: GitHub Issues
- Contributions: Pull Requests targeting `main`
- PR style: small and atomic changes
- Before creating a PR: make sure local build passes

## Security

Do not post vulnerabilities in public issues. Follow `SECURITY.md`.

## License

This project is distributed under the MIT License. See `LICENSE`.

## Legal Note

This repository is intended for research, learning, and S6E3 server development. Use of names and assets related to Mu Online must comply with applicable rights and local laws.
