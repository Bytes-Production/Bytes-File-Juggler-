# Bytes — File Juggler & Cleaner

*Organize smarter. Clean safer.*

Bytes is a native Windows desktop application (C++20 / Qt 6 / SQLite) that
automatically organizes files dropped into folders you choose, and safely
cleans TEMP files and the Recycle Bin on a schedule. It runs quietly in the
system tray, does all of its work locally, and is built around one rule
above all others: **Bytes must never cause data loss.**

## Highlights

- **File Juggler** — watches folders with `ReadDirectoryChangesW` (no
  polling), waits out a configurable grace period + file-stability check so
  it never touches a file that's still downloading, then sorts it into
  category subfolders (Images, Videos, Documents/PDF, Installers, ...).
- **Rule Engine** — user-defined rules (extension, filename pattern, size,
  source folder, date) evaluated in priority order, with an "Auto Optimize
  Priority" button and a Rule Tester dialog.
- **Duplicate-safe** — never overwrites by default; duplicates are renamed
  `name (1).ext`, `name (2).ext`, ... automatically.
- **Undo** — every move is recorded and reversible, including from a
  collision (`name (restored).ext`) that never overwrites anything else.
- **Cleaner** — User Temp, Windows Temp, and the Recycle Bin, each with a
  size preview, manual cleanup, and an optional daily/weekly/monthly
  schedule via the Windows Task Scheduler. Recycle Bin auto-cleaning is off
  by default and always asks for confirmation.
- **Markdown activity log** — a human-readable daily log of every move,
  skip, error, cleanup, and undo, at `%LOCALAPPDATA%\Bytes\Logs\`.
- **System tray + Windows startup** — minimizes to tray, single-instance
  enforced, optional "Start with Windows" via the per-user Run key (no
  admin rights required).
- **Local-only** — no cloud, no telemetry, no file uploads, no execution of
  organized files.

## Building

Prerequisites:

- Visual Studio 2022 (or newer) with the **Desktop development with C++**
  workload
- [CMake](https://cmake.org/) 3.21+
- [Qt 6.8](https://www.qt.io/) for MSVC 2022 x64 (Core, Gui, Widgets, Sql, Test)
- [Ninja](https://ninja-build.org/) (or use the Visual Studio generator instead)
- [Inno Setup](https://jrsoftware.org/isinfo.php) (only needed to build the installer)

```bat
scripts\build.bat C:\Qt\6.8.1\msvc2022_64
scripts\package.bat C:\Qt\6.8.1\msvc2022_64
```

`build.bat` configures and builds a Release binary at `dist\Bytes.exe`.
`package.bat` runs `windeployqt` next to it and compiles the Inno Setup
installer to `dist\Bytes_Setup.exe`.

To run the automated tests:

```bat
cmake --build build --config Release --target RUN_TESTS
ctest --test-dir build -C Release --output-on-failure
```

## Project layout

```
src/core/       file classifier, rule engine, safe mover, duplicate
                handler, undo manager, ReadDirectoryChangesW watcher,
                and the FileOrganizer orchestrator
src/cleaner/    TEMP cleaning, Recycle Bin, and the cleanup scheduler
src/database/   SQLite schema + a thread-safe Database façade
src/logging/    Markdown activity log + the ActivityLogger that ties
                logging to the database and the GUI
src/windows/    Windows-specific glue: shell utilities, startup
                registration, the tray icon, and folder icons
src/ui/         MainWindow + one page per section (Dashboard, Juggler,
                Rules, Folders, Cleaner, Activity, Settings, About) and
                the first-run wizard / rule dialogs
tests/          QtTest unit tests, one focused executable per subsystem
installer/      Inno Setup script
scripts/        build / clean / package batch scripts
```

## Data & logs

- Database: `%LOCALAPPDATA%\Bytes\bytes.db` (SQLite)
- Logs: `%LOCALAPPDATA%\Bytes\Logs\yyyy-MM-dd.md`

Neither is ever placed inside Downloads, Desktop, or Documents.

## Safety rules Bytes always follows

- Never overwrites a file by default (duplicates are renamed).
- Never deletes a source file before the destination copy is verified.
- Never organizes a file that is still downloading (`.crdownload`, `.part`,
  `.tmp`, `.download`, `.partial`, or one that fails a stability check).
- Never deletes the Downloads, Desktop, or Documents folders themselves.
- Never force-closes another application's file handle.
- Never executes a file it discovers, and never uploads anything anywhere.
- Any failed operation leaves the original file untouched and is logged.

## License

MIT — see [LICENSE](LICENSE).
