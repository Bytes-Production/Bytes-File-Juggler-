# Bytes — Requirements Reference

This document summarizes the functional requirements Bytes implements, as
a quick reference alongside the source tree. It mirrors the original
product specification; see the module doc-comments in `src/` for
implementation detail.

## Safety (non-negotiable)

Bytes never: overwrites a file by default, deletes a source before a move
is verified, moves a file that is still downloading, deletes Downloads /
Desktop / Documents, terminates other applications, force-deletes locked
files, executes discovered files, or uploads anything anywhere. A failed
operation always leaves the original untouched and is logged.

## File Juggler

- Native `ReadDirectoryChangesW` monitoring per folder (no polling), on a
  dedicated worker thread per watched folder.
- Configurable grace period (Immediately … 10 minutes, default 1 minute)
  and multi-pass file-stability check (size + modified time + lock check)
  before anything is touched.
- Built-in classifier covering Images, Videos, Music, Documents (Word,
  PDF, Spreadsheets, Presentations), Archives, Installers, Disk Images,
  Development, 3D & CAD, Fonts, Ebooks, Database, Backups, Torrents,
  Shortcuts, Data, and Other, plus optional file-signature sniffing when
  the extension is missing or unrecognized.
- User-defined rules (extension / file type / filename contains / starts
  with / ends with / wildcard / size / source folder / date) with
  priority ordering, manual reordering, and one-click auto-optimization.
- Custom categories and custom destinations (including absolute paths on
  other drives), with automatic destination-folder creation.
- Duplicate handling: rename (default), skip, ask, or replace — "replace"
  is implemented as a safe rename-aside, never a silent overwrite.
- Dry-run / Preview Mode.
- Undo (last / selected / multiple), restoring to the original location
  and never overwriting anything there.
- Bytes-branded folder icons on folders Bytes itself creates, via
  `desktop.ini`, toggleable and reversible.

## Cleaner

- User Temp, Windows Temp, and Recycle Bin: size preview, manual cleanup
  (Clean Selected / Clean Everything), and a Daily / Weekly / Monthly
  schedule (default: Daily, 02:00) registered with the Windows Task
  Scheduler. Recycle Bin auto-cleaning is off by default and always
  confirmed explicitly.
- Locked files are skipped and logged, never force-closed or force-deleted.

## Logging & data

- SQLite database at `%LOCALAPPDATA%\Bytes\bytes.db` (settings,
  monitored_folders, categories, rules, file_operations,
  cleanup_operations, errors, statistics), accessed only through prepared
  statements.
- Daily Markdown activity log at `%LOCALAPPDATA%\Bytes\Logs\yyyy-MM-dd.md`
  for every move, skip, error, folder creation, cleanup, undo, rule
  change, and watcher event.
- Configurable log retention (30/90/180/365 days, or forever).

## Application behavior

- System tray icon with the full Bytes menu (Monitoring status, Open
  Bytes, Pause/Resume Juggler, Run Cleanup, Open Activity, Settings,
  Exit); closing the window minimizes to tray by default.
- Optional "Start Bytes with Windows" via the per-user registry Run key
  (no elevation required).
- Single-instance enforcement; relaunching focuses the existing window.
- First-run wizard (folders, organization mode, grace period, cleaner
  defaults, startup) followed by a safe existing-files prompt that
  defaults to "only organize new files from now on".
- Config export/import as JSON.
- Rule Tester dialog.
- Light / Dark / System appearance.

## Non-functional

- C++20, Qt 6 Widgets + Sql, CMake, MSVC, Inno Setup installer.
- All filesystem operations run off the GUI thread (QThreadPool / worker
  threads); the UI never blocks during scans, moves, or cleanup.
- Unicode throughout (`QString`, `std::wstring`, the `...W` Win32 APIs),
  with long-path (`\\?\`) support for file moves.
- No cloud connectivity, telemetry, or file uploads of any kind.
