#!/bin/bash

# FocusLog Uninstall Script
# This script removes the FocusLog application from the system.

# Exit immediately if a command fails
set -e

APP_NAME="focuslog"
FOCUSLOG_DATA_DIR="$HOME/.config/focuslog"

# --- Helper Functions ---

# Prints an info message
log_info() {
    echo "INFO: $1"
}

# Prints an error message and exits the script
log_error() {
    echo "ERROR: $1" >&2
    exit 1
}

# --- Main Uninstall Logic ---

log_info "Starting FocusLog uninstallation..."

# Detect operating system
OS_ID=$(grep -E '^ID=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')

UNINSTALL_SUCCESS=false

if [[ "$OS_ID" == "arch" ]]; then
    log_info "Detected Arch Linux. Checking if FocusLog is installed as a pacman package..."
    if pacman -Qs "$APP_NAME" &> /dev/null; then
        log_info "FocusLog pacman package found. Attempting to uninstall via pacman..."
        if sudo pacman -R --noconfirm "$APP_NAME"; then # Use --noconfirm for automated uninstallation
            log_info "FocusLog pacman package uninstalled successfully."
            UNINSTALL_SUCCESS=true
        else
            log_error "Failed to uninstall FocusLog pacman package. You may need to uninstall manually: 'sudo pacman -R $APP_NAME'"
        fi
    else
        log_info "FocusLog pacman package not found. Checking for /usr/local/bin installation..."
        if [ -f "/usr/local/bin/$APP_NAME" ]; then
            log_info "FocusLog found in /usr/local/bin. Attempting to remove..."
            if sudo rm "/usr/local/bin/$APP_NAME"; then
                log_info "FocusLog removed from /usr/local/bin successfully."
                UNINSTALL_SUCCESS=true
            else
                log_error "Failed to remove FocusLog from /usr/local/bin. You may need to remove manually: 'sudo rm /usr/local/bin/$APP_NAME'"
            fi
        else
            log_info "FocusLog not found in /usr/local/bin."
        fi
    fi
else # For Debian, Fedora, and other generic Linux distributions
    log_info "Checking for /usr/local/bin installation..."
    if [ -f "/usr/local/bin/$APP_NAME" ]; then
        log_info "FocusLog found in /usr/local/bin. Attempting to remove..."
        if sudo rm "/usr/local/bin/$APP_NAME"; then
            log_info "FocusLog removed from /usr/local/bin successfully."
            UNINSTALL_SUCCESS=true
        else
            log_error "Failed to remove FocusLog from /usr/local/bin. You may need to remove manually: 'sudo rm /usr/local/bin/$APP_NAME'"
        fi
    else
        log_info "FocusLog not found in /usr/local/bin."
    fi
fi

# Remove application data directory
if [ -d "$FOCUSLOG_DATA_DIR" ]; then
    log_info "Removing FocusLog data directory: $FOCUSLOG_DATA_DIR"
    if rm -rf "$FOCUSLOG_DATA_DIR"; then
        log_info "FocusLog data directory removed successfully."
        UNINSTALL_SUCCESS=true # Mark as success if data dir was removed
    else
        log_error "Failed to remove FocusLog data directory. You may need to remove manually: 'rm -rf $FOCUSLOG_DATA_DIR'"
    fi
else
    log_info "FocusLog data directory not found: $FOCUSLOG_DATA_DIR"
fi

if "$UNINSTALL_SUCCESS"; then
    log_info "FocusLog uninstallation completed successfully!"
else
    log_error "FocusLog uninstallation completed with errors or application was not found."
fi

exit 0
