1. Context: Mu Online Season 6 Episode 3 (eX603) server development.
2. Core Tech: C++, SQL (MSSQL), XML/TXT/INI server configuration files.
3. Version Limits: Stick strictly to S6E3. Classes: DK, DW, Elf, MG, DL, SU, RF. Max 3rd wings. No Grow Lancer or later content.
4. File Structure: Follow Louis Emulator, IGCN, or OpenMu structures (Data/Item, Data/Monster, Data/Events).
5. Code Style: Prioritize config-based changes. Suggest C++ source edits only if the task cannot be solved via .txt/.xml configs.
6. Minimalism: Do not suggest custom features, new events, or client mods unless explicitly requested. Keep it stable and "Retail-like".
7. SQL Safety: Use standard tables (Character, Memb_Info, AccountCharacter). Ensure queries are injection-proof and always warn to backup DB before execution.
8. Efficiency: Provide direct file paths and concise code snippets. No game design theory.
