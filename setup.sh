#!/bin/bash

# FocusLog Kurulum Betiği
# Bu betik, FocusLog uygulamasını derler ve sisteme entegre eder.

set -e # Bir komut başarısız olursa hemen çık

APP_NAME="focuslog"
APP_VERSION="1.0.0" # Uygulama sürümü
APP_DESCRIPTION="Terminal tabanlı bir odaklanma zamanlayıcısı ve istatistik uygulaması."
APP_LICENSE="MIT" # Lisans bilgisi

# --- Yardımcı Fonksiyonlar ---

log_info() {
    echo "INFO: $1"
}

log_error() {
    echo "ERROR: $1" >&2
    exit 1
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "$1 komutu bulunamadı. Lütfen '$1' uygulamasını yükleyin."
    fi
}

install_dependencies_apt() {
    log_info "Debian/Ubuntu için bağımlılıklar yükleniyor..."
    sudo apt install -y build-essential libncurses-dev || log_error "Debian/Ubuntu bağımlılıkları yüklenirken hata oluştu."
}

install_dependencies_dnf() {
    log_info "Fedora için bağımlılıklar yükleniyor..."
    sudo dnf install -y gcc make ncurses-devel || log_error "Fedora bağımlılıkları yüklenirken hata oluştu."
}

install_dependencies_pacman() {
    log_info "Arch Linux için bağımlılıklar yükleniyor..."
    sudo pacman -S --noconfirm gcc make ncurses || log_error "Arch Linux bağımlılıkları yüklenirken hata oluştu."
}

# --- Ana Kurulum Mantığı ---

log_info "FocusLog kurulumu başlatılıyor..."

check_command "gcc"
check_command "make"

OS_ID=$(grep -E '^ID=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')
OS_ID_LIKE=$(grep -E '^ID_LIKE=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')

INSTALL_METHOD="generic"

if [[ "$OS_ID" == "arch" ]]; then
    log_info "Algılanan işletim sistemi: Arch Linux."
    install_dependencies_pacman
    INSTALL_METHOD="arch"
elif [[ "$OS_ID" == "debian" || "$OS_ID_LIKE" == *"debian"* ]]; then
    log_info "Algılanan işletim sistemi: Debian/Ubuntu tabanlı."
    install_dependencies_apt
    INSTALL_METHOD="debian"
elif [[ "$OS_ID" == "fedora" || "$OS_ID_LIKE" == *"fedora"* ]]; then
    log_info "Algılanan işletim sistemi: Fedora tabanlı."
    install_dependencies_dnf
    INSTALL_METHOD="fedora"
else
    log_info "Algılanan işletim sistemi: Diğer Linux dağıtımı (ID: $OS_ID, ID_LIKE: $OS_ID_LIKE)."
    log_info "Genel kurulum deneniyor. Gerekirse bağımlılıkları manuel yükleyin."
fi

log_info "FocusLog derleniyor..."
make clean || true
make focuslog || log_error "Derleme başarısız oldu."

if [ ! -f "focuslog" ]; then
    log_error "Derlenmiş 'focuslog' ikili dosyası bulunamadı."
fi

log_info "FocusLog kuruluyor..."

if [ "$INSTALL_METHOD" == "arch" ]; then
    log_info "PKGBUILD dosyası /tmp dizininde oluşturuldu."
    PKG_BUILD_DIR="/tmp/${APP_NAME}_pkg"
    mkdir -p "$PKG_BUILD_DIR" || log_error "PKGBUILD dizini oluşturulamadı."

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

    log_info "FocusLog'u paketten kurmak isterseniz:"
    log_info "  cd $PKG_BUILD_DIR && makepkg -si"
    log_info "Kaldırmak için: sudo pacman -R $APP_NAME"

    sudo install -Dm755 focuslog "/usr/local/bin/${APP_NAME}" || log_error "Kurulum başarısız oldu."
    log_info "FocusLog kuruldu: /usr/local/bin/$APP_NAME"
    log_info "Kaldırmak için: sudo rm /usr/local/bin/$APP_NAME"
else
    sudo install -Dm755 focuslog "/usr/local/bin/${APP_NAME}" || log_error "Kurulum başarısız oldu."
    log_info "FocusLog kuruldu: /usr/local/bin/$APP_NAME"
    log_info "Kaldırmak için: sudo rm /usr/local/bin/$APP_NAME"
fi

log_info "FocusLog kurulumu tamamlandı."
make clean || true
exit 0
