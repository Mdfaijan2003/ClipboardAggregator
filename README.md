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
- ✅ Tray controls (Pause, Clear, Auto-start, Exit)
- ✅ Local-only operation (no cloud sync, no telemetry)

---

## Download

Get the latest release from **GitHub Releases**:  
`https://github.com/<your-username>/<your-repo>/releases`

### Available packages
- **Installer** (recommended for non-technical users)
- **Portable ZIP** (recommended for developers/power users)

---

## Quick Start

1. Install or extract app.
2. Launch `ClipboardAggregator.exe`.
3. Copy text as usual (`Ctrl + C`).
4. Press `Ctrl + Shift + V` to paste all stored items.

---

## Tray Menu

Right-click tray icon to:
- Paste all now
- Pause/Resume capture
- Clear history
- Enable/Disable auto-start
- Exit

---

## Privacy

Clipboard Aggregator is designed to be **local-first**:
- No telemetry by default
- No network sync
- No account required
- Clipboard history is not uploaded

See [PRIVACY.md](PRIVACY.md) for full details.

---

## Security

If you discover a vulnerability, please follow [SECURITY.md](SECURITY.md).

---

## Troubleshooting

### App does not start on boot
- Re-enable auto-start from tray menu.
- If needed, reinstall using installer and enable startup option.

### Hotkey not working
- Another app may already use `Ctrl + Shift + V`.
- Close conflicting app or change hotkey in a future settings release.

### Only first line pastes in some apps
- Some target inputs are single-line only and ignore line breaks.
- Use single-line join mode in future updates.

---

## Build (Developer)

Visual Studio / MSVC:

```bat
cl /EHsc /W4 /DUNICODE /D_UNICODE clipboard_aggregator.cpp user32.lib shell32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS