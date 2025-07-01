#!/bin/bash
1
# FocusLog Kurulum Betiği
# Bu betik, FocusLog uygulamasını GitHub deposundan indirir
# ve ardından sisteminize kurmak için setup.sh'ı çalıştırır.

REPO_URL="https://github.com/hakangoksu/FocusLog.git"
INSTALL_DIR="/tmp/focuslog_install" # Geçici indirme dizini

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

# Bir komutun sistemde yüklü olup olmadığını kontrol eder
check_command() {
    if ! command -v "$1" &> /dev/null
    then
        log_error "$1 komutu bulunamadı. Lütfen devam etmek için '$1' uygulamasını yükleyin."
    fi
}

# --- Ana İşlem ---

log_info "FocusLog kurulumuna başlanıyor..."

# Git'in yüklü olup olmadığını kontrol et
check_command "git"

# Geçici kurulum dizinini oluştur
log_info "Geçici kurulum dizini oluşturuluyor: $INSTALL_DIR"
mkdir -p "$INSTALL_DIR" || log_error "Geçici dizin oluşturulamadı: $INSTALL_DIR"

# Depoyu klonla
log_info "FocusLog deposu klonlanıyor: $REPO_URL"
git clone "$REPO_URL" "$INSTALL_DIR" || log_error "Depo klonlanırken hata oluştu."

# Klonlanan dizine geç
cd "$INSTALL_DIR" || log_error "Klonlanan dizine geçilemedi: $INSTALL_DIR"

# setup.sh betiğini çalıştır
if [ -f "setup.sh" ]; then
    log_info "setup.sh betiği çalıştırılıyor..."
    chmod +x setup.sh || log_error "setup.sh betiği çalıştırılabilir yapılamadı."
    ./setup.sh
    SETUP_EXIT_CODE=$?
else
    log_error "setup.sh betiği bulunamadı. Depo düzgün klonlanmamış olabilir."
fi

# Geçici dosyaları temizle
log_info "Geçici kurulum dosyaları temizleniyor..."
rm -rf "$INSTALL_DIR"

if [ $SETUP_EXIT_CODE -eq 0 ]; then
    log_info "FocusLog kurulumu başarıyla tamamlandı!"
    log_info "Artık terminalinizden 'focuslog' komutunu çalıştırabilirsiniz."
else
    log_error "FocusLog kurulumu başarısız oldu."
fi

exit $SETUP_EXIT_CODE
