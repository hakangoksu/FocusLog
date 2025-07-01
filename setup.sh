#!/bin/bash

# FocusLog Installation Script
# This script compiles and integrates the FocusLog application into the system.

set -e # Exit immediately if a command fails

APP_NAME="focuslog"
APP_VERSION="1.0.0" # Application version
APP_DESCRIPTION="A terminal-based focus timer and statistics application."
APP_LICENSE="MIT" # License information

# --- Helper Functions ---

log_info() {
    echo "INFO: $1"
}

log_error() {
    echo "ERROR: $1" >&2
    exit 1
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "Command '$1' not found. Please install '$1' to proceed."
    fi
}

install_dependencies_apt() {
    log_info "Installing dependencies for Debian/Ubuntu..."
    # System update removed, only dependencies will be installed
    sudo apt install -y build-essential libncurses-dev || log_error "Error installing Debian/Ubuntu dependencies."
}

install_dependencies_dnf() {
    log_info "Installing dependencies for Fedora..."
    sudo dnf install -y gcc make ncurses-devel || log_error "Error installing Fedora dependencies."
}

install_dependencies_pacman() {
    log_info "Installing dependencies for Arch Linux..."
    # System update removed, only dependencies will be installed
    sudo pacman -S --noconfirm gcc make ncurses || log_error "Error installing Arch Linux dependencies."
}

# --- Main Installation Logic ---

log_info "Starting FocusLog installation..."

check_command "gcc"
check_command "make"

OS_ID=$(grep -E '^ID=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')
OS_ID_LIKE=$(grep -E '^ID_LIKE=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')

INSTALL_METHOD="generic"

if [[ "$OS_ID" == "arch" ]]; then
    log_info "Detected operating system: Arch Linux."
    install_dependencies_pacman
    INSTALL_METHOD="arch"
elif [[ "$OS_ID" == "debian" || "$OS_ID_LIKE" == *"debian"* ]]; then
    log_info "Detected operating system: Debian/Ubuntu based."
    install_dependencies_apt
    INSTALL_METHOD="debian"
elif [[ "$OS_ID" == "fedora" || "$OS_ID_LIKE" == *"fedora"* ]]; then
    log_info "Detected operating system: Fedora based."
    install_dependencies_dnf
    INSTALL_METHOD="fedora"
else
    log_info "Detected operating system: Other Linux distribution (ID: $OS_ID, ID_LIKE: $OS_ID_LIKE)."
    log_info "Attempting generic installation. Please install dependencies manually if necessary."
fi

log_info "Compiling FocusLog..."
make clean || true
make focuslog || log_error "Compilation failed."

if [ ! -f "focuslog" ]; then
    log_error "Compiled 'focuslog' binary not found."
fi

log_info "Installing FocusLog..."

if [ "$INSTALL_METHOD" == "arch" ]; then
    log_info "PKGBUILD file created in /tmp directory."
    PKG_BUILD_DIR="/tmp/${APP_NAME}_pkg"
    mkdir -p "$PKG_BUILD_DIR" || log_error "Could not create PKGBUILD directory."

    cat <<EOF > "$PKG_BUILD_DIR/PKGBUILD"
# Maintainer: Hakan Göksu
pkgname=$APP_NAME
pkgver=$APP_VERSION
pkgrel=1
pkgdesc="$APP_DESCRIPTION"
arch=('x86_64')
url="https://github.com/hakangoksu/FocusLog"
license=('$APP_LICENSE')
depends=('ncurses')
makedepends=('gcc' 'make')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/hakangoksu/FocusLog/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
  cd "\$srcdir/${pkgname}-${pkgver}"
  make
}

package() {
  cd "\$srcdir/${pkgname}-${pkgver}"
  install -Dm755 "${pkgname}" "\$pkgdir/usr/bin/${pkgname}"
}
EOF

    log_info "If you want to install FocusLog as a package:"
    log_info "  cd $PKG_BUILD_DIR && makepkg -si"
    log_info "To uninstall: sudo pacman -R $APP_NAME"

    sudo install -Dm755 focuslog "/usr/local/bin/${APP_NAME}" || log_error "Installation failed."
    log_info "FocusLog installed to: /usr/local/bin/$APP_NAME"
    log_info "To uninstall: sudo rm /usr/local/bin/$APP_NAME"
else
    sudo install -Dm755 focuslog "/usr/local/bin/${APP_NAME}" || log_error "Installation failed."
    log_info "FocusLog installed to: /usr/local/bin/$APP_NAME"
    log_info "To uninstall: sudo rm /usr/local/bin/$APP_NAME"
fi

log_info "FocusLog installation completed."
make clean || true
exit 0
