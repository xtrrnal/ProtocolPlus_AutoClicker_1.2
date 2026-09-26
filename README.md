# Protocol+ Auto Clicker v1.2

A lightweight native Windows auto-clicker with an ice-blue glass-inspired interface, CPS control, random CPS variation, and customizable global hotkeys.

## Features

- Main CPS: 1–1000
- Random CPS Offset: 0–999
- Per-click randomized CPS
- Left, Right, and Middle mouse buttons
- F6 global toggle by default
- Custom global hotkey binding
- Ice Blue Glass-inspired UI
- Protocol+ application icon
- Native C++ Win32 application
- Windows installer with Start Menu and optional Desktop shortcut
- GitHub Actions automated Windows build

## Random CPS

The offset is applied around the Main CPS on every click.

Example:

- Main CPS: `13`
- Random Offset: `12`

The clicker randomly chooses between approximately `1` and `25` CPS for each click interval, clamped to the valid 1–1000 CPS range.

Set Random Offset to `0` for consistent CPS.

## Build

The repository includes a GitHub Actions workflow that:

1. Sets up MSVC
2. Compiles the application resource/icon
3. Builds the native Windows executable
4. Builds the Inno Setup installer
5. Uploads the installer as an Actions artifact

## License

MIT
