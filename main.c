#include <ncurses.h> // Ncurses kütüphanesini dahil ediyoruz

int main() {
    // 1. Ncurses ortamını başlat
    initscr();

    // 2. Kullanıcının tuş basımlarını ekranda gösterme (eko yapma)
    noecho();

    // 3. Tuş basımlarını anında alma (Enter'a basmaya gerek kalmadan)
    cbreak();

    // 4. Özel tuşları (F1, ok tuşları vb.) etkinleştir
    keypad(stdscr, TRUE);

    // 5. Ekranı temizle
    clear();

    // 6. Bir metin yazdırma
    // mvprintw(y, x, "metin");
    // Ekranın ortasına yakın bir yere yazmak için basit bir hesaplama yapabiliriz.
    // getmaxy(stdscr) -> ekranın yüksekliği
    // getmaxx(stdscr) -> ekranın genişliği
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax); // Ekranın maksimum y ve x koordinatlarını al

    // Mesajı ekranın ortasına yaklaşık olarak yerleştir
    mvprintw(yMax / 2, (xMax - (sizeof("TermTrack'e Hoş Geldiniz!") - 1)) / 2, "TermTrack'e Hoş Geldiniz!");
    mvprintw(yMax / 2 + 2, (xMax - (sizeof("Devam etmek için herhangi bir tuşa basın...") - 1)) / 2, "Devam etmek için herhangi bir tuşa basın...");

    // 7. Ekranda yapılan değişiklikleri güncelle ve göster
    refresh();

    // 8. Kullanıcının bir tuşa basmasını bekle
    getch();

    // 9. Ncurses ortamını düzgünce kapat ve terminali eski haline döndür
    endwin();

    return 0;
}
