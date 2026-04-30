# Clipboard Aggregator

A lightweight Windows background utility that collects your copied text and pastes all collected items at once.

- Copy normally with **Ctrl + C**
- Paste all collected items with **Ctrl + Shift + V**
- Runs in background (tray app), no console window

---

## Features

- ✅ Automatic clipboard capture (text only)
- ✅ Paste all collected items in one action
- ✅ Max history size limit (default: 12)
- ✅ Auto-expiry (default: 1 hour)
- ✅ Sensitive content filtering (API keys, tokens, passwords, card numbers)
- ✅ Tray controls (Pause, Clear, Auto-start, Exit)
- ✅ Local-only operation (no cloud sync, no telemetry)

---

## Download

Get the latest release from **GitHub Releases**:  
👉 https://github.com/Mdfaijan2003/ClipboardAggregator/releases

### Available packages
- **ClipboardAggregatorSetup.msi** — recommended for most users
- **setup.exe** — alternative installer
- **ClipboardAggregator-v1.0.0.zip** — zip bundle containing both

---

## Quick Start

1. Download and run `ClipboardAggregatorSetup.msi`.
2. The app starts automatically and appears in your system tray.
3. Copy text as usual (`Ctrl + C`).
4. Press `Ctrl + Shift + V` to paste all stored items at once.

---

## Tray Menu

Right-click the tray icon to:
- Paste all now
- Pause/Resume capture
- Clear history
- Enable/Disable auto-start with Windows
- Exit

---

## Privacy

Clipboard Aggregator is designed to be **local-first**:
- No telemetry
- No network sync
- No account required
- Clipboard history is never uploaded

See [PRIVACY.md](PRIVACY.md) for full details.

---

## Security

If you discover a vulnerability, please follow [SECURITY.md](SECURITY.md).

---

## Troubleshooting

### App does not start on boot
- Re-enable auto-start from the tray menu.
- If needed, reinstall using the installer and enable the startup option.

### Hotkey not working
- Another app may already be using `Ctrl + Shift + V`.
- Close the conflicting app or wait for a future settings release.

### Only first line pastes in some apps
- Some inputs are single-line only and ignore line breaks.
- This will be addressed in a future update.

---

## Requirements

- Windows 10 or later (x64)
- No additional dependencies — everything is bundled in the installer

---

## Build (Developer)

Visual Studio 2022/2026 with MSVC:

```bat
cl /EHsc /W4 /DUNICODE /D_UNICODE ClipboardAggregator.cpp user32.lib shell32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS
```
