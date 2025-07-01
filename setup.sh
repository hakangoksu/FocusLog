#!/bin/bash

# FocusLog Kurulum Betiği
# Bu betik, FocusLog uygulamasını derler ve sisteme entegre eder.

# Bir komut başarısız olursa hemen çık
set -e

APP_NAME="focuslog"
APP_VERSION="1.0.0" # Uygulama sürümü
APP_DESCRIPTION="Terminal tabanlı bir odaklanma zamanlayıcısı ve istatistik uygulaması."
APP_LICENSE="MIT" # Lisans bilgisi

# --- Yardımcı Fonksiyonlar ---

# Bilgi mesajı yazdırır
log_info() {
    echo "INFO: $1"
}

# Hata mesajı yazdırır ve betiği sonlandırır
log_error() {
    echo "ERROR: $1" >&2
    exit 1
}

# Bir komutun yüklü olup olmadığını kontrol eder
check_command() {
    if ! command -v "$1" &> /dev/null
    then
        log_error "$1 komutu bulunamadı. Lütfen devam etmek için '$1' uygulamasını yükleyin."
    fi
}

# Debian/Ubuntu için bağımlılıkları yükler
install_dependencies_apt() {
    log_info "Debian/Ubuntu için bağımlılıklar yükleniyor..."
    # 'apt update' kaldırıldı, sadece bağımlılıklar yüklenecek
    sudo apt install -y build-essential libncurses-dev || log_error "Debian/Ubuntu bağımlılıkları yüklenirken hata oluştu."
}

# Fedora için bağımlılıkları yükler
install_dependencies_dnf() {
    log_info "Fedora için bağımlılıklar yükleniyor..."
    sudo dnf install -y gcc make ncurses-devel || log_error "Fedora bağımlılıkları yüklenirken hata oluştu."
}

# Arch Linux için bağımlılıkları yükler
install_dependencies_pacman() {
    log_info "Arch Linux için bağımlılıklar yükleniyor..."
    # 'pacman -Syu' yerine sadece 'pacman -S' kullanıldı, sistem güncellemesi yapılmayacak
    sudo pacman -S --noconfirm gcc make ncurses || log_error "Arch Linux bağımlılıkları yüklenirken hata oluştu."
}

# --- Ana Kurulum Mantığı ---

log_info "FocusLog kurulumu başlatılıyor..."

# 1. Gerekli derleme araçlarını kontrol et
check_command "gcc"
check_command "make"

# 2. İşletim sistemini algıla ve bağımlılıkları yükle
OS_ID=$(grep -E '^ID=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')
OS_ID_LIKE=$(grep -E '^ID_LIKE=' /etc/os-release | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')

INSTALL_METHOD="generic" # Varsayılan kurulum metodu

if [[ "$OS_ID" == "arch" ]]; then
    log_info "Algılanan İşletim Sistemi: Arch Linux."
    install_dependencies_pacman
    INSTALL_METHOD="arch"
elif [[ "$OS_ID" == "debian" || "$OS_ID_LIKE" == *"debian"* ]]; then
    log_info "Algılanan İşletim Sistemi: Debian/Ubuntu tabanlı."
    install_dependencies_apt
    INSTALL_METHOD="debian"
elif [[ "$OS_ID" == "fedora" || "$OS_ID_LIKE" == *"fedora"* ]]; then
    log_info "Algılanan İşletim Sistemi: Fedora tabanlı."
    install_dependencies_dnf
    INSTALL_METHOD="fedora"
else
    log_info "Algılanan İşletim Sistemi: Diğer Linux dağıtımı (ID: $OS_ID, ID_LIKE: $OS_ID_LIKE)."
    log_info "Genel kurulum deneniyor. Derleme başarısız olursa 'gcc', 'make' ve 'ncurses-devel' paketlerini manuel olarak yüklemeniz gerekebilir."
    INSTALL_METHOD="generic"
fi

# 3. Uygulamayı derle
log_info "FocusLog derleniyor..."
make clean || true # Önceki derlemeleri temizle (varsa)
make focuslog || log_error "Derleme başarısız oldu. Lütfen sisteminizin derleme ortamını ve ncurses geliştirme kütüphanelerini kontrol edin."

# Derlenmiş ikili dosyanın varlığını kontrol et
if [ ! -f "focuslog" ]; then
    log_error "Derleme sonrası 'focuslog' ikili dosyası bulunamadı."
fi

# 4. Uygulamayı işletim sistemine göre kur
log_info "FocusLog kuruluyor..."

if [ "$INSTALL_METHOD" == "arch" ]; then
    log_info "Arch Linux için bir PKGBUILD dosyası oluşturulmuştur. FocusLog'u bir pacman paketi olarak kurmak isterseniz, bu PKGBUILD'i manuel olarak kullanabilirsiniz."
    log_info "Ancak, hızlı kullanım için derlenmiş ikili dosya doğrudan /usr/local/bin dizinine kopyalanacaktır."

    PKG_BUILD_DIR="/tmp/${APP_NAME}_pkg"
    mkdir -p "$PKG_BUILD_DIR" || log_error "PKGBUILD dizini oluşturulamadı."

    # PKGBUILD içeriğini oluştur
    cat <<EOF > "$PKG_BUILD_DIR/PKGBUILD"
# Maintainer: Hakan Göktaş <hakangoksu@example.com> # Lütfen kendi bilgilerinizi girin
pkgname=$APP_NAME
pkgver=$APP_VERSION
pkgrel=1
pkgdesc="$APP_DESCRIPTION"
arch=('x86_64') # Diğer mimariler için 'any' veya uygun mimariyi ekleyebilirsiniz
url="https://github.com/hakangoksu/FocusLog"
license=('$APP_LICENSE')
depends=('ncurses')
makedepends=('gcc' 'make')
# Kaynak dosyayı doğrudan GitHub'daki bir sürüm etiketinden çekiyoruz
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/hakangoksu/FocusLog/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('SKIP') # ÖNEMLİ: Gerçek bir paket için bu 'SKIP' değerini gerçek checksum ile değiştirin!

build() {
  # makepkg, kaynak tar.gz dosyasını \$srcdir/\${pkgname}-\${pkgver} dizinine çıkarır.
  cd "\$srcdir/${pkgname}-${pkgver}"
  make
}

package() {
  # Derlenmiş ikili dosyayı paketin /usr/bin dizinine kurar
  cd "\$srcdir/${pkgname}-${pkgver}"
  install -Dm755 "${pkgname}" "\$pkgdir/usr/bin/${pkgname}"
  # Uygulamanın veri dizini ~/.config/focuslog olduğu için ek bir kurulum gerekmez.
}
EOF

    log_info "Arch Linux için PKGBUILD dosyası şurada oluşturuldu: $PKG_BUILD_DIR/PKGBUILD"
    log_info "FocusLog'u bir pacman paketi olarak kurmak için:"
    log_info "  cd $PKG_BUILD_DIR"
    log_info "  makepkg -si"
    log_info "Bu komut, paketi derleyecek ve bağımlılıklarıyla birlikte pacman aracılığıyla yükleyecektir."
    log_info "Ardından 'pacman -Qs focuslog' komutuyla paketi görebilirsiniz."
    log_info "Kaldırmak için: 'sudo pacman -R focuslog'"
    log_info ""

    # Hızlı kullanım için ikili dosyayı kopyala
    sudo install -Dm755 focuslog "/usr/local/bin/${APP_NAME}" || log_error "/usr/local/bin dizinine kurulum başarısız oldu."
    log_info "FocusLog şuraya kuruldu: /usr/local/bin/$APP_NAME"
    log_info "Bu yöntemle yüklenen FocusLog'u kaldırmak için: sudo rm /usr/local/bin/$APP_NAME"


else # Debian, Fedora ve diğer genel Linux dağıtımları için
    # /usr/local/bin dizinine kurulum
    sudo install -Dm755 focuslog "/usr/local/bin/${APP_NAME}" || log_error "/usr/local/bin dizinine kurulum başarısız oldu."
    log_info "FocusLog şuraya kuruldu: /usr/local/bin/$APP_NAME"
    log_info "Kaldırmak için: sudo rm /usr/local/bin/$APP_NAME"
fi

log_info "FocusLog kurulumu tamamlandı."

# Derlenmiş ikili dosyayı ve Makefile'ı kaynak dizinden kaldır
make clean || true

exit 0
