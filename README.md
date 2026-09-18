> **This project continues as [qfocus](https://github.com/quvyta/focus).** This repository is kept read-only for reference.

# FocusLog

**FocusLog** is a terminal-based, lightweight, and fast focus timer and productivity tracker. Built with C and the `ncurses` library, it offers an aesthetic and efficient experience directly from the terminal.
<p align="center">
  <img src="https://github.com/user-attachments/assets/808ac98a-3f5c-42db-9a20-897d394e8613" width="45%" />
  <img src="https://github.com/user-attachments/assets/b517074f-4413-41b6-a488-e941b506e17b" width="45%" />
</p>

---

## 🚀 Installation

### 📥 Install

```bash
curl -sL https://raw.githubusercontent.com/hakangoksu/FocusLog/main/install.sh | bash
```

This command will download the latest version, install required dependencies, compile the source code, and install FocusLog on your system.

### 🗑️ Uninstall

```bash
curl -sL https://raw.githubusercontent.com/hakangoksu/FocusLog/main/uninstall.sh | bash
```

This will completely remove FocusLog and its configuration files from your system.

---

## ✨ Features

### ✔️ Implemented

* **Flexible Focus Timer**: Start focus sessions with custom durations.
* **Category & Focus Management**: Organize work into categories and define specific tasks.
* **Color Coding**: Categories and focus areas are visually distinguished with randomly assigned colors.
* **Work Log**: Automatically records each session's category, focus name, start/end time, and duration.
* **Statistics Overview**: View total focus durations by category and task.
* **Idle Screen Display**: A dynamic screen showing focus distribution and top-focused areas during inactivity.
* **Data Persistence**: All data is stored in `~/.config/focuslog/`.
* **Multi-language Support**: English and Turkish support with automatic locale detection.
* **Settings Menu**: Add/edit categories and focuses, reset statistics, or delete data.
* **Two-Step Confirmation**: Prevents accidental data loss during sensitive operations.

### ⏳ Coming Soon

* **Advanced Statistics**: Weekly/monthly reports, visual breakdowns (ASCII graphs), and average session metrics.
* **Pomodoro Integration**: Automatic break cycles and Pomodoro timers.
* **Notifications**: Session start/end alerts using `notify-send` or terminal prompts.
* **User-Defined Colors**: Custom color selection for categories and tasks.
* **Tagging System**: Assign multiple tags to focuses.
* **Export Options**: Export logs and statistics to JSON or TXT formats.
* **Undo Functionality**: Revert accidental deletions.
* **Custom Shortcuts**: Personalized keyboard shortcuts for navigation and actions.

---

## 💻 Supported Platforms

FocusLog is designed for Linux systems.

* **Arch Linux**: ✅ Tested & Supported
* **Debian/Ubuntu**: ❓ Supported (Untested)
* **Fedora**: ❓ Supported (Untested)
* **Other Distros**: ❓ Likely supported — just make sure these dependencies are installed:

  * `gcc`
  * `make`
  * `ncurses-devel`

---

## 🎯 Project Goals

FocusLog aims to be:

* Minimal and distraction-free
* Fully functional from the terminal
* Easy to install and maintain
* Open to contributions and customization

---

## 📖 License

FocusLog is released under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html).

---

## ✍️ Contributing

Contributions, feature suggestions, and issue reports are welcome. Feel free to open a PR or issue on [GitHub](https://github.com/hakangoksu/FocusLog)!

---

Crafted with ❤️ by [@hakangoksu](https://github.com/hakangoksu)
