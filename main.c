#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h> // bool tipini kullanmak için
#include <ctype.h>   // tolower fonksiyonu için bu satır eklendi

// --- Makrolar ve Sabitler ---
#define COLOR_PAIR_DEFAULT  1
#define COLOR_PAIR_HIGHLIGHT 2
#define COLOR_PAIR_TITLE    3
#define COLOR_PAIR_RED      4 // Yeni: Kırmızı renk çifti
// Yeni renk çiftleri 5'ten başlayacak
#define MIN_CUSTOM_COLOR_PAIR 5
#define MAX_CUSTOM_COLOR_PAIR 255 // Ncurses'ın destekleyebileceği maksimum renk çifti ID'si
#define MAX_COLOR_PAIRS     256   // Renk çiftlerini takip etmek için maksimum ID + 1

// Ana menü seçenekleri
#define MENU_START_WORK     0
#define MENU_VIEW_STATS     1
#define MENU_SETTINGS       2
#define MENU_EXIT           3
#define TOTAL_MAIN_MENU_ITEMS 4

#define MAX_CATEGORY_NAME_LEN 100
#define MAX_FOCUS_NAME_LEN    100
#define MAX_CATEGORIES        50
#define MAX_FOCUSES_PER_CATEGORY 50 // Her kategori için maksimum odak sayısı
#define IDLE_TIMEOUT_SECONDS 5 // Boşta kalma süresi (saniye)

#define STATS_FOCUS_NAME_COL_WIDTH 25 // İstatistikler tablosunda odak adı sütunu genişliği

// --- Yeni Veri Yapıları ---
typedef struct {
    char name[MAX_FOCUS_NAME_LEN];
    int color_pair_id;
} Focus;

typedef struct {
    char name[MAX_CATEGORY_NAME_LEN];
    Focus focuses[MAX_FOCUSES_PER_CATEGORY];
    int num_focuses;
    int color_pair_id;
} Category;

// İstatistikler için yeni veri yapıları
typedef struct {
    char name[MAX_FOCUS_NAME_LEN];
    long total_duration; // Saniye cinsinden toplam süre
    int session_count;   // Oturum sayısı
} StatFocus;

typedef struct {
    char name[MAX_CATEGORY_NAME_LEN];
    StatFocus focuses[MAX_FOCUSES_PER_CATEGORY];
    int num_focuses;
} StatCategory;


// --- Global Değişkenler ---
const char *menu_items_tr[] = {
    "Yeni Çalışma Başlat",
    "İstatistikleri Gör",
    "Ayarlar",
    "Çıkış"
};

const char *menu_items_en[] = {
    "Start New Work",
    "View Statistics",
    "Settings",
    "Exit"
};

Category user_categories[MAX_CATEGORIES];
int num_user_categories = 0;
int next_available_color_pair_id = MIN_CUSTOM_COLOR_PAIR;

char focuslog_data_dir[256];
char categories_file_path[300];
char work_log_file_path[300];

StatCategory stat_categories[MAX_CATEGORIES]; // İstatistik verileri için
int num_stat_categories = 0;

time_t last_input_time; // Son kullanıcı giriş zamanı

// Renk çiftlerinin başlatılıp başlatılmadığını takip etmek için global dizi
bool g_initialized_color_pairs[MAX_COLOR_PAIRS];

// --- Fonksiyon Tanımlamaları ---
// draw_menu_and_get_choice fonksiyonuna yeni bir parametre eklendi: current_lang_menu_items_for_idle
int draw_menu_and_get_choice(const char **options, int num_options, const char *title_msg, int initial_highlight, int *color_ids, const char **current_lang_menu_items_for_idle);
void start_timer_session(const char *category_name, const char *focus_name, int duration_seconds, const char **current_lang_menu_items, int category_color_id, int focus_color_id);
int get_duration_from_user(const char **current_lang_menu_items);
void manage_settings(const char **current_lang_menu_items);

int handle_new_category_creation(const char **current_lang_menu_items); // Return int: index or -1
int handle_new_focus_creation(Category *cat, const char **current_lang_menu_items); // Return int: index or -1

// Yeni silme fonksiyonları
void delete_category(int index, const char **current_lang_menu_items);
void delete_focus(Category *cat, int index, const char **current_lang_menu_items);
void filter_work_log(const char *deleted_category, const char *deleted_focus); // İstatistik loglarını filtrelemek için yeni fonksiyon


void load_data();
void save_data();
void create_data_directory();
void record_work_session(const char *category, const char *focus, time_t start_time, time_t end_time, long duration);
int get_random_color_pair();

void format_duration_string(long total_seconds, char *buffer, size_t buffer_size);

// İstatistik fonksiyonları
void view_statistics(const char **current_lang_menu_items);
void load_statistics();
int get_stat_category_index(const char *category_name);
int get_stat_focus_index(StatCategory *stat_cat, const char *focus_name);

// Yeni yardımcı fonksiyon: Kullanıcıdan string girişi al (ESC ile iptal edilebilir)
int get_string_input(char *buffer, size_t buffer_size, int y, int x, const char *prompt);

// Yeni: Boşta kalma çubuğu fonksiyonu
void draw_idle_bar(const char **current_lang_menu_items);

// Yeni: Onay fonksiyonu
bool get_double_confirmation(const char *message1, const char *message2, const char **current_lang_menu_items);

// Yeni: Tüm yüklü renk çiftlerinin başlatıldığından emin olmak için fonksiyon
void ensure_all_color_pairs_initialized();


// --- Ana Fonksiyon ---
int main() {
    setlocale(LC_ALL, "");
    srandom(time(NULL));

    create_data_directory();

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        // Varsayılan renk çiftlerini başlat
        init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_PAIR_HIGHLIGHT, COLOR_BLACK, COLOR_CYAN);
        init_pair(COLOR_PAIR_TITLE, COLOR_YELLOW, COLOR_BLACK);
        init_pair(COLOR_PAIR_RED, COLOR_RED, COLOR_BLACK); // Kırmızı renk çifti
    }

    load_data();
    ensure_all_color_pairs_initialized(); // Yüklenen tüm renk çiftlerini başlat

    const char *lang_env = getenv("LANG");
    const char **current_main_menu_items;
    const char *main_title_msg;

    if (lang_env != NULL && (strncmp(lang_env, "en", 2) == 0 || strncmp(lang_env, "EN", 2) == 0)) {
        current_main_menu_items = menu_items_en;
        main_title_msg = "FocusLog Main Menu";
    } else {
        current_main_menu_items = menu_items_tr;
        main_title_msg = "FocusLog Ana Menü";
    }

    int main_menu_choice;
    last_input_time = time(NULL); // Uygulama başlangıcında zamanı ayarla

    while (1) {
        // draw_menu_and_get_choice fonksiyonuna current_main_menu_items parametresi eklendi
        main_menu_choice = draw_menu_and_get_choice(current_main_menu_items, TOTAL_MAIN_MENU_ITEMS, main_title_msg, 0, NULL, current_main_menu_items);

        switch (main_menu_choice) {
            case MENU_START_WORK: {
                int selected_category_idx = -1;
                int selected_focus_idx = -1;
                bool return_to_main_menu = false;
                bool return_to_category_selection = false;

                do { // Category selection loop
                    selected_category_idx = -1; // Reset for re-entry
                    return_to_category_selection = false; // Reset

                    char **temp_category_names = (char **)malloc((num_user_categories + 1) * sizeof(char*));
                    int *temp_category_color_ids = (int *)malloc((num_user_categories + 1) * sizeof(int));

                    if (temp_category_names == NULL || temp_category_color_ids == NULL) {
                        endwin(); fprintf(stderr, "Memory allocation error for category names/colors!\n"); return 1;
                    }

                    for (int i = 0; i < num_user_categories; i++) {
                        temp_category_names[i] = user_categories[i].name;
                        temp_category_color_ids[i] = user_categories[i].color_pair_id;
                    }
                    temp_category_names[num_user_categories] = (char*)((current_main_menu_items == menu_items_en) ? "Add New Category" : "Yeni Kategori Ekle");
                    temp_category_color_ids[num_user_categories] = COLOR_PAIR_DEFAULT;

                    int category_choice = draw_menu_and_get_choice((const char**)temp_category_names, num_user_categories + 1,
                                                                   (current_main_menu_items == menu_items_en) ? "Select Category" : "Kategori Seç", 0, temp_category_color_ids, current_main_menu_items);

                    free(temp_category_names);
                    free(temp_category_color_ids);

                    if (category_choice == num_user_categories) { // "Yeni Kategori Ekle" seçildi
                        int new_cat_idx = handle_new_category_creation(current_main_menu_items);
                        if (new_cat_idx != -1) { // Yeni kategori başarıyla eklendi
                            selected_category_idx = new_cat_idx;
                        } else {
                            // ESC from new category creation, stay in category selection loop
                            continue; // Re-enter category selection
                        }
                    } else if (category_choice != -1 && category_choice < num_user_categories) { // Mevcut bir kategori seçildi
                        selected_category_idx = category_choice;
                    } else if (category_choice == -1) { // ESC from category selection
                        return_to_main_menu = true;
                    }

                    if (return_to_main_menu) break; // Exit to main menu

                    if (selected_category_idx != -1) {
                        Category *selected_cat = &user_categories[selected_category_idx];
                        selected_focus_idx = -1; // Reset for re-entry

                        do { // Focus selection loop
                            char **temp_focus_options = (char **)malloc((selected_cat->num_focuses + 1) * sizeof(char*));
                            int *temp_focus_color_ids = (int *)malloc((selected_cat->num_focuses + 1) * sizeof(int));

                            if (temp_focus_options == NULL || temp_focus_color_ids == NULL) {
                                endwin(); fprintf(stderr, "Memory allocation error for focus names/colors!\n"); return 1;
                            }

                            int num_focus_options = selected_cat->num_focuses;
                            for (int i = 0; i < selected_cat->num_focuses; i++) {
                                temp_focus_options[i] = selected_cat->focuses[i].name;
                                temp_focus_color_ids[i] = selected_cat->focuses[i].color_pair_id;
                            }
                            temp_focus_options[num_focus_options] = (char*)((current_main_menu_items == menu_items_en) ? "Add New Focus for this Category" : "Bu Kategoriye Yeni Odak Ekle");
                            temp_focus_color_ids[num_focus_options] = COLOR_PAIR_DEFAULT;
                            num_focus_options++;

                            int focus_choice = draw_menu_and_get_choice((const char**)temp_focus_options, num_focus_options,
                                                                         (current_main_menu_items == menu_items_en) ? "Select Focus" : "Odak Seç", 0, temp_focus_color_ids, current_main_menu_items);

                            free(temp_focus_options);
                            free(temp_focus_color_ids);

                            if (focus_choice == selected_cat->num_focuses) { // "Yeni Odak Ekle" seçildi
                                int new_focus_idx = handle_new_focus_creation(selected_cat, current_main_menu_items);
                                if (new_focus_idx != -1) {
                                    selected_focus_idx = new_focus_idx;
                                } else {
                                    // ESC from new focus creation, stay in focus selection loop
                                    continue; // Re-enter focus selection
                                }
                            } else if (focus_choice != -1 && focus_choice < selected_cat->num_focuses) { // Mevcut bir odak seçildi
                                selected_focus_idx = focus_choice;
                            } else if (focus_choice == -1) { // ESC from focus selection
                                return_to_category_selection = true;
                            }

                            if (return_to_category_selection) break; // Exit to category selection

                            if (selected_focus_idx != -1) {
                                const char *final_focus_name = selected_cat->focuses[selected_focus_idx].name;
                                int final_focus_color_id = selected_cat->focuses[selected_focus_idx].color_pair_id;

                                int duration = get_duration_from_user(current_main_menu_items);
                                if (duration > 0) { // Duration entered, not cancelled
                                    start_timer_session(selected_cat->name, final_focus_name, duration, current_main_menu_items, selected_cat->color_pair_id, final_focus_color_id);
                                    return_to_main_menu = true; // Session finished, go back to main menu
                                } else if (duration == -1) { // ESC from duration input
                                    selected_focus_idx = -1; // Force re-entry into focus selection loop
                                    continue; // Re-enter focus selection loop
                                }
                            }
                        } while (selected_focus_idx == -1 && !return_to_category_selection); // Keep looping until a focus is selected or back to category
                    }
                } while (selected_category_idx == -1 && !return_to_main_menu); // Keep looping until a category is selected or back to main menu
                break; // Exit MENU_START_WORK block
            }
            case MENU_VIEW_STATS:
                view_statistics(current_main_menu_items);
                break;
            case MENU_SETTINGS:
                manage_settings(current_main_menu_items);
                break;
            case MENU_EXIT:
                goto end_program;
        }
    }

end_program:
    save_data();
    endwin();
    return 0;
}

// --- Fonksiyon Implementasyonları ---

// Yeni yardımcı fonksiyon: Kullanıcıdan string girişi al (ESC ile iptal edilebilir)
// Return: 0 (başarılı), -1 (iptal edildi)
int get_string_input(char *buffer, size_t buffer_size, int y, int x, const char *prompt) {
    int input_char;
    int current_len = 0;
    buffer[0] = '\0'; // Ensure buffer is empty initially

    curs_set(1); // Show cursor
    echo();      // Enable echoing input characters

    mvprintw(y, x, "%s", prompt);
    refresh();

    int input_x = x + strlen(prompt);

    while (1) {
        mvprintw(y, input_x, "%s", buffer); // Display current buffer content
        clrtoeol(); // Clear to end of line (to remove any leftover chars if input shrinks)
        refresh();

        input_char = getch();

        if (input_char == 10 || input_char == KEY_ENTER) { // Enter key
            curs_set(0); // Hide cursor
            noecho();    // Disable echoing
            return 0; // Success
        } else if (input_char == 27) { // ESC key
            curs_set(0);
            noecho();
            return -1; // Canceled
        } else if (input_char == KEY_BACKSPACE || input_char == 127) { // Backspace (127 for some terminals)
            if (current_len > 0) {
                current_len--;
                buffer[current_len] = '\0';
                // No need to clear on screen, mvprintw will overwrite
            }
        } else if (current_len < buffer_size - 1 && isprint(input_char)) { // Printable character
            buffer[current_len++] = input_char;
            buffer[current_len] = '\0';
        }
    }
}


// draw_menu_and_get_choice fonksiyonuna yeni bir parametre eklendi
int draw_menu_and_get_choice(const char **options, int num_options, const char *title_msg, int initial_highlight, int *color_ids, const char **current_lang_menu_items_for_idle) {
    if (num_options == 0) {
        return -1;
    }
    int highlight = initial_highlight;
    int choice = -1;
    int c;

    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);

    int max_item_len = 0;
    for (int i = 0; i < num_options; ++i) {
        if (options[i] != NULL && strlen(options[i]) > max_item_len) {
            max_item_len = strlen(options[i]);
        }
    }
    if (strlen(title_msg) > max_item_len) {
        max_item_len = strlen(title_msg);
    }

    int menu_start_x = (xMax - max_item_len) / 2;
    int menu_start_y = (yMax - (num_options * 2 + 2)) / 2;

    // Menüyü çiz
    clear();
    attron(COLOR_PAIR(COLOR_PAIR_TITLE));
    mvprintw(menu_start_y - 2, (xMax - strlen(title_msg)) / 2, "%s", title_msg);
    attroff(COLOR_PAIR(COLOR_PAIR_TITLE));

    for (int i = 0; i < num_options; ++i) {
        if (i == highlight) {
            attron(COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
            attron(A_BOLD);
        } else {
            if (color_ids != NULL && color_ids[i] != 0 && has_colors()) {
                attron(COLOR_PAIR(color_ids[i]));
            } else {
                attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
            }
        }
        if (options[i] != NULL) {
             mvprintw(menu_start_y + i * 2, menu_start_x, "%s", options[i]);
        }
        attroff(COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
        attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));
        if (color_ids != NULL && color_ids[i] != 0 && i != highlight && has_colors()) {
            attroff(COLOR_PAIR(color_ids[i]));
        }
        attroff(A_BOLD);
    }
    refresh();

    int old_highlight = highlight;

    nodelay(stdscr, TRUE); // getch'i non-blocking yap
    last_input_time = time(NULL); // Menüye girildiğinde zamanı sıfırla

    while (1) {
        c = getch(); // Non-blocking çağrı

        if (c == ERR) { // Tuş basılmadı
            if (time(NULL) - last_input_time >= IDLE_TIMEOUT_SECONDS) {
                // current_lang_menu_items_for_idle parametresi kullanıldı
                draw_idle_bar(current_lang_menu_items_for_idle); // Boşta kalma çubuğunu göster
                last_input_time = time(NULL); // Boşta kalma çubuğu gösterildikten sonra zamanı sıfırla

                // Menüyü yeniden çiz (draw_idle_bar ekranı temizlediği için)
                clear();
                attron(COLOR_PAIR(COLOR_PAIR_TITLE));
                mvprintw(menu_start_y - 2, (xMax - strlen(title_msg)) / 2, "%s", title_msg);
                attroff(COLOR_PAIR(COLOR_PAIR_TITLE));

                for (int i = 0; i < num_options; ++i) {
                    if (i == highlight) {
                        attron(COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
                        attron(A_BOLD);
                    } else {
                        if (color_ids != NULL && color_ids[i] != 0 && has_colors()) {
                            attron(COLOR_PAIR(color_ids[i]));
                        } else {
                            attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
                        }
                    }
                    if (options[i] != NULL) {
                        mvprintw(menu_start_y + i * 2, menu_start_x, "%s", options[i]);
                    }
                    attroff(COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
                    attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));
                    if (color_ids != NULL && color_ids[i] != 0 && has_colors()) {
                        attroff(COLOR_PAIR(color_ids[i]));
                    }
                    attroff(A_BOLD);
                }
            }
            usleep(100000); // CPU kullanımını azaltmak için küçük bir gecikme
            continue; // Tuş basılmadı, döngüye devam et
        } else { // Tuş basıldı
            last_input_time = time(NULL); // Kullanıcı giriş yaptığında zamanı güncelle
            old_highlight = highlight;

            switch (c) {
                case KEY_UP:
                    highlight--;
                    if (highlight < 0) {
                        highlight = num_options - 1;
                    }
                    break;
                case KEY_DOWN:
                    highlight++;
                    if (highlight >= num_options) {
                        highlight = 0;
                    }
                    break;
                case 10:
                case KEY_ENTER:
                    choice = highlight;
                    break;
                case 27:
                    choice = -1;
                    break;
            }

            if (choice != -1) {
                nodelay(stdscr, FALSE); // Blocking moda geri dön
                return choice;
            }

            if (highlight != old_highlight) {
                // Eski vurguyu kaldır
                if (color_ids != NULL && color_ids[old_highlight] != 0 && has_colors()) {
                    attron(COLOR_PAIR(color_ids[old_highlight]));
                } else {
                    attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
                }
                attroff(A_BOLD);
                if (options[old_highlight] != NULL) {
                    mvprintw(menu_start_y + old_highlight * 2, menu_start_x, "%s", options[old_highlight]);
                }
                attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));
                if (color_ids != NULL && color_ids[old_highlight] != 0 && has_colors()) {
                    attroff(COLOR_PAIR(color_ids[old_highlight]));
                }

                // Yeni vurguyu çiz
                attron(COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
                attron(A_BOLD);
                if (options[highlight] != NULL) {
                    mvprintw(menu_start_y + highlight * 2, menu_start_x, "%s", options[highlight]);
                }
                attroff(COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
                attroff(A_BOLD);
            }
            refresh();
        }
    }
}

void format_duration_string(long total_seconds, char *buffer, size_t buffer_size) {
    long days = total_seconds / (24 * 3600);
    long remaining_seconds = total_seconds % (24 * 3600);
    long hours = remaining_seconds / 3600;
    remaining_seconds %= 3600;
    long minutes = remaining_seconds / 60;
    long seconds = remaining_seconds % 60;

    if (days > 0) {
        snprintf(buffer, buffer_size, "%02ld:%02ld:%02ld:%02ld", days, hours, minutes, seconds);
    } else if (hours > 0) {
        snprintf(buffer, buffer_size, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    } else {
        snprintf(buffer, buffer_size, "%02ld:%02ld", minutes, seconds);
    }
}

void start_timer_session(const char *category_name, const char *focus_name, int duration_seconds, const char **current_lang_menu_items, int category_color_id, int focus_color_id) {
    clear();
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);

    time_t start_time_actual = time(NULL);
    time_t current_time_val;
    long elapsed_seconds;
    long remaining_seconds = duration_seconds;

    const char *timer_title = (current_lang_menu_items == menu_items_en) ? "Focusing on:" : "Odaklanılıyor:";
    const char *category_label = (current_lang_menu_items == menu_items_en) ? "Category:" : "Kategori:";
    const char *pause_msg = (current_lang_menu_items == menu_items_en) ? "Press SPACE to pause/resume, ESC to finish" : "Duraklat/Devam Etmek için BOŞLUK, Bitirmek için ESC";
    const char *press_esc_to_return_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return to menu..." : "Menüye dönmek için ESC tuşuna basın...";


    bool paused = false;
    time_t pause_start_time = 0;
    long total_paused_time = 0;

    int input_char;
    char last_time_str[20] = "";
    char current_time_str[20];

    if (category_color_id != 0 && has_colors()) attron(COLOR_PAIR(category_color_id));
    mvprintw(yMax / 2 - 6, (xMax - strlen(category_label) - strlen(category_name) - 2) / 2, "%s %s", category_label, category_name);
    if (category_color_id != 0 && has_colors()) attroff(COLOR_PAIR(category_color_id));

    if (focus_color_id != 0 && has_colors()) attron(COLOR_PAIR(focus_color_id));
    mvprintw(yMax / 2 - 4, (xMax - strlen(timer_title) - strlen(focus_name) - 2) / 2, "%s %s", timer_title, focus_name);
    if (focus_color_id != 0 && has_colors()) attroff(COLOR_PAIR(focus_color_id));

    mvprintw(yMax - 2, (xMax - strlen(pause_msg)) / 2, "%s", pause_msg);
    refresh();

    while (1) {
        if (!paused) {
            current_time_val = time(NULL);
            elapsed_seconds = (long)(current_time_val - start_time_actual - total_paused_time);
            remaining_seconds = duration_seconds - elapsed_seconds;
        }

        if (remaining_seconds <= 0) {
            remaining_seconds = 0;
        }

        format_duration_string(remaining_seconds, current_time_str, sizeof(current_time_str));

        if (strcmp(current_time_str, last_time_str) != 0 || (paused && strlen(last_time_str) > 0) || (!paused && strlen(last_time_str) == 0)) {
            if (strlen(last_time_str) > strlen(current_time_str)) {
                mvprintw(yMax / 2, (xMax - strlen(last_time_str)) / 2, "%*s", (int)strlen(last_time_str), "");
            }

            attron(A_BOLD | COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
            mvprintw(yMax / 2, (xMax - strlen(current_time_str)) / 2, "%s", current_time_str);
            attroff(A_BOLD | COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
            strcpy(last_time_str, current_time_str);
        }

        const char *paused_text = (current_lang_menu_items == menu_items_en) ? "PAUSED" : "DURAKLATILDI";
        const char *blank_text = "            ";
        if (paused) {
            attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
            mvprintw(yMax / 2 + 2, (xMax - strlen(paused_text)) / 2, "%s", paused_text);
            attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));
        } else {
            mvprintw(yMax / 2 + 2, (xMax - strlen(blank_text)) / 2, "%s", blank_text);
        }

        refresh();

        nodelay(stdscr, TRUE);
        input_char = getch();
        nodelay(stdscr, FALSE);

        switch (input_char) {
            case ' ':
                paused = !paused;
                if (paused) {
                    pause_start_time = time(NULL);
                } else {
                    total_paused_time += (time(NULL) - pause_start_time);
                }
                break;
            case 27: {
                time_t end_time = time(NULL);
                record_work_session(category_name, focus_name, start_time_actual, end_time, elapsed_seconds); // elapsed_seconds kaydedildi
                return;
            }
        }

        if (remaining_seconds <= 0) {
            time_t end_time = time(NULL);
            record_work_session(category_name, focus_name, start_time_actual, end_time, duration_seconds); // Tam süre kaydedildi
            clear();
            const char *finished_msg = (current_lang_menu_items == menu_items_en) ? "Time's Up! Session Finished!" : "Süre Doldu! Oturum Bitti!";
            mvprintw(yMax / 2, (xMax - strlen(finished_msg)) / 2, "%s", finished_msg);
            mvprintw(yMax / 2 + 2, (xMax - strlen(press_esc_to_return_msg)) / 2, "%s", press_esc_to_return_msg); // Updated message
            refresh();
            getch();
            return;
        }

        usleep(100000);
    }
}

// get_duration_from_user fonksiyonu (ESC ile iptal edilebilir)
// Return: Süre (saniye cinsinden), -1 (iptal edildi)
int get_duration_from_user(const char **current_lang_menu_items) {
    clear();
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);

    const char *prompt = (current_lang_menu_items == menu_items_en) ? "Enter duration in minutes (e.g., 25): " : "Süreyi dakika olarak girin (örn: 25): ";
    char input_buffer[10];
    int duration_minutes = 0;

    int result = get_string_input(input_buffer, sizeof(input_buffer), yMax / 2, (xMax - strlen(prompt) - 10) / 2, prompt);

    if (result == -1) { // ESC ile iptal edildi
        return -1;
    }

    duration_minutes = atoi(input_buffer);

    if (duration_minutes <= 0) {
        clear();
        const char *invalid_msg = (current_lang_menu_items == menu_items_en) ? "Invalid duration. Using 25 minutes." : "Geçersiz süre. 25 dakika kullanılıyor.";
        mvprintw(yMax / 2, (xMax - strlen(invalid_msg)) / 2, "%s", invalid_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
        return 25 * 60;
    }
    return duration_minutes * 60;
}

// Yeni: İki aşamalı onay fonksiyonu
bool get_double_confirmation(const char *message1, const char *message2, const char **current_lang_menu_items) {
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);
    clear();

    attron(COLOR_PAIR(COLOR_PAIR_RED));
    mvprintw(yMax / 2 - 2, (xMax - strlen(message1)) / 2, "%s", message1);
    attroff(COLOR_PAIR(COLOR_PAIR_RED));
    refresh();
    int ch1 = getch();
    if (tolower(ch1) != 'y' && tolower(ch1) != 'e') {
        return false;
    }

    clear();
    attron(COLOR_PAIR(COLOR_PAIR_RED));
    mvprintw(yMax / 2 - 2, (xMax - strlen(message2)) / 2, "%s", message2);
    attroff(COLOR_PAIR(COLOR_PAIR_RED));
    refresh();
    int ch2 = getch();
    if (tolower(ch2) != 'y' && tolower(ch2) != 'e') {
        return false;
    }
    return true;
}


void manage_settings(const char **current_lang_menu_items) {
    int yMax, xMax;

    char *settings_options[5]; // 2 yeni seçenek eklendi
    settings_options[0] = (char*)((current_lang_menu_items == menu_items_en) ? "Add New Category" : "Yeni Kategori Ekle");
    settings_options[1] = (char*)((current_lang_menu_items == menu_items_en) ? "Manage Existing Categories & Focuses" : "Mevcut Kategorileri ve Odakları Yönet");
    settings_options[2] = (char*)((current_lang_menu_items == menu_items_en) ? "Reset All Statistics" : "Tüm İstatistikleri Sıfırla");
    settings_options[3] = (char*)((current_lang_menu_items == menu_items_en) ? "Delete All Categories & Focuses" : "Tüm Odakları ve Kategorileri Sil");
    settings_options[4] = (char*)((current_lang_menu_items == menu_items_en) ? "Back to Main Menu" : "Ana Menüye Geri Dön");

    int settings_colors[5]; // Renkleri belirle
    settings_colors[0] = COLOR_PAIR_DEFAULT;
    settings_colors[1] = COLOR_PAIR_DEFAULT;
    settings_colors[2] = COLOR_PAIR_RED; // Kırmızı
    settings_colors[3] = COLOR_PAIR_RED; // Kırmızı
    settings_colors[4] = COLOR_PAIR_DEFAULT;

    while (1) {
        // draw_menu_and_get_choice fonksiyonuna current_lang_menu_items parametresi eklendi
        int selected_option = draw_menu_and_get_choice((const char**)settings_options, 5,
                                                       (current_lang_menu_items == menu_items_en) ? "Settings" : "Ayarlar", 0, settings_colors, current_lang_menu_items);
        getmaxyx(stdscr, yMax, xMax);

        if (selected_option == 0) { // "Yeni Kategori Ekle"
            int new_cat_idx = handle_new_category_creation(current_lang_menu_items);
            if (new_cat_idx != -1) { // Yeni kategori başarıyla eklendi
                // Doğrudan bu kategorinin odaklarını yönetmeye geç
                int cat_to_manage_idx = new_cat_idx;
                Category *selected_cat = &user_categories[cat_to_manage_idx];

                while(1) { // Odak veya kategori işlem döngüsü
                    char **temp_options = (char**)malloc((selected_cat->num_focuses + 3) * sizeof(char*)); // Odaklar + Yeni Odak + Kategoriyi Sil + Geri
                    int *temp_color_ids = (int*)malloc((selected_cat->num_focuses + 3) * sizeof(int));
                    if (temp_options == NULL || temp_color_ids == NULL) {
                        clear(); mvprintw(yMax/2, (xMax - strlen("Memory error!"))/2, "Memory error!"); refresh(); getch(); break;
                    }

                    int current_option_idx = 0;
                    for (int i = 0; i < selected_cat->num_focuses; i++) {
                        temp_options[current_option_idx] = selected_cat->focuses[i].name;
                        temp_color_ids[current_option_idx] = selected_cat->focuses[i].color_pair_id;
                        current_option_idx++;
                    }
                    temp_options[current_option_idx] = (char*)((current_lang_menu_items == menu_items_en) ? "Add New Focus" : "Yeni Odak Ekle");
                    temp_color_ids[current_option_idx++] = COLOR_PAIR_DEFAULT;
                    temp_options[current_option_idx] = (char*)((current_lang_menu_items == menu_items_en) ? "Delete This Category" : "Bu Kategoriyi Sil");
                    temp_color_ids[current_option_idx++] = COLOR_PAIR_RED; // Kırmızı
                    temp_options[current_option_idx] = (char*)((current_lang_menu_items == menu_items_en) ? "Back to Categories" : "Kategorilere Geri Dön");
                    temp_color_ids[current_option_idx++] = COLOR_PAIR_DEFAULT;

                    char title_buffer[MAX_CATEGORY_NAME_LEN + 30];
                    snprintf(title_buffer, sizeof(title_buffer), "%s: %s", (current_lang_menu_items == menu_items_en) ? "Manage" : "Yönet", selected_cat->name);

                    // draw_menu_and_get_choice fonksiyonuna current_lang_menu_items parametresi eklendi
                    int sub_choice = draw_menu_and_get_choice((const char**)temp_options, current_option_idx, title_buffer, 0, temp_color_ids, current_lang_menu_items);

                    free(temp_options);
                    free(temp_color_ids);

                    if (sub_choice == current_option_idx - 1) { // "Back to Categories"
                        break; // Kategori seçimine geri dön
                    } else if (sub_choice == current_option_idx - 2) { // "Delete This Category"
                        delete_category(cat_to_manage_idx, current_lang_menu_items);
                        break; // Kategori silindiği için kategori seçimine geri dön
                    } else if (sub_choice == current_option_idx - 3) { // "Add New Focus"
                        handle_new_focus_creation(selected_cat, current_lang_menu_items);
                    } else if (sub_choice != -1 && sub_choice < selected_cat->num_focuses) {
                        // Odak seçildi, şimdi odak için silme seçeneği sun
                        char *focus_options[2];
                        focus_options[0] = (char*)((current_lang_menu_items == menu_items_en) ? "Delete This Focus" : "Bu Odağı Sil");
                        focus_options[1] = (char*)((current_lang_menu_items == menu_items_en) ? "Back to Focus List" : "Odak Listesine Geri Dön");

                        int focus_option_colors[2];
                        focus_option_colors[0] = COLOR_PAIR_RED; // Kırmızı
                        focus_option_colors[1] = COLOR_PAIR_DEFAULT;

                        char focus_title_buffer[MAX_FOCUS_NAME_LEN + 30];
                        snprintf(focus_title_buffer, sizeof(focus_title_buffer), "%s: %s", (current_lang_menu_items == menu_items_en) ? "Manage Focus" : "Odağı Yönet", selected_cat->focuses[sub_choice].name);

                        // draw_menu_and_get_choice fonksiyonuna current_lang_menu_items parametresi eklendi
                        int delete_focus_choice = draw_menu_and_get_choice((const char**)focus_options, 2, focus_title_buffer, 0, focus_option_colors, current_lang_menu_items);
                        if (delete_focus_choice == 0) { // "Delete This Focus"
                            delete_focus(selected_cat, sub_choice, current_lang_menu_items);
                            // Odak silindiği için odak listesine geri dön (döngü devam edecek ve liste yenilenecek)
                        } else if (delete_focus_choice == 1 || delete_focus_choice == -1) {
                            // Geri dön veya ESC, döngü devam edecek
                        }
                    } else if (sub_choice == -1) { // ESC basıldı
                        break; // Kategori seçimine geri dön
                    }
                }
            }
        } else if (selected_option == 1) { // "Mevcut Kategorileri ve Odakları Yönet"
            if (num_user_categories == 0) {
                clear();
                const char *no_cat_msg = (current_lang_menu_items == menu_items_en) ? "No categories to manage yet. Add some first." : "Henüz yönetilecek kategori yok. Önce ekleyin.";
                mvprintw(yMax / 2, (xMax - strlen(no_cat_msg)) / 2, "%s", no_cat_msg);
                const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
                mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
                refresh();
                getch();
            } else {
                while(1) { // Kategori seçim döngüsü
                    char **temp_category_names = (char **)malloc((num_user_categories + 1) * sizeof(char*)); // +1 for "Back" option
                    int *temp_category_color_ids = (int *)malloc((num_user_categories + 1) * sizeof(int));
                    if (temp_category_names == NULL || temp_category_color_ids == NULL) {
                        clear(); mvprintw(yMax/2, (xMax - strlen("Memory error!"))/2, "Memory error!"); refresh(); getch(); break;
                    }

                    for (int i = 0; i < num_user_categories; i++) {
                        temp_category_names[i] = user_categories[i].name;
                        temp_category_color_ids[i] = user_categories[i].color_pair_id;
                    }
                    temp_category_names[num_user_categories] = (char*)((current_lang_menu_items == menu_items_en) ? "Back" : "Geri");
                    temp_category_color_ids[num_user_categories] = COLOR_PAIR_DEFAULT;

                    // draw_menu_and_get_choice fonksiyonuna current_lang_menu_items parametresi eklendi
                    int cat_to_manage_idx = draw_menu_and_get_choice((const char**)temp_category_names, num_user_categories + 1,
                                                                    (current_lang_menu_items == menu_items_en) ? "Select Category to Manage" : "Yönetilecek Kategoriyi Seç", 0, temp_category_color_ids, current_lang_menu_items);
                    free(temp_category_names);
                    free(temp_category_color_ids);

                    if (cat_to_manage_idx == num_user_categories || cat_to_manage_idx == -1) { // "Back" veya ESC
                        break; // Kategori yönetiminden çık
                    } else if (cat_to_manage_idx != -1 && cat_to_manage_idx < num_user_categories) {
                        Category *selected_cat = &user_categories[cat_to_manage_idx];

                        while(1) { // Odak veya kategori işlem döngüsü
                            char **temp_options = (char**)malloc((selected_cat->num_focuses + 3) * sizeof(char*)); // Odaklar + Yeni Odak + Kategoriyi Sil + Geri
                            int *temp_color_ids = (int*)malloc((selected_cat->num_focuses + 3) * sizeof(int));
                            if (temp_options == NULL || temp_color_ids == NULL) {
                                clear(); mvprintw(yMax/2, (xMax - strlen("Memory error!"))/2, "Memory error!"); refresh(); getch(); break;
                            }

                            int current_option_idx = 0;
                            for (int i = 0; i < selected_cat->num_focuses; i++) {
                                temp_options[current_option_idx] = selected_cat->focuses[i].name;
                                temp_color_ids[current_option_idx] = selected_cat->focuses[i].color_pair_id;
                                current_option_idx++;
                            }
                            temp_options[current_option_idx] = (char*)((current_lang_menu_items == menu_items_en) ? "Add New Focus" : "Yeni Odak Ekle");
                            temp_color_ids[current_option_idx++] = COLOR_PAIR_DEFAULT;
                            temp_options[current_option_idx] = (char*)((current_lang_menu_items == menu_items_en) ? "Delete This Category" : "Bu Kategoriyi Sil");
                            temp_color_ids[current_option_idx++] = COLOR_PAIR_RED; // Kırmızı
                            temp_options[current_option_idx] = (char*)((current_lang_menu_items == menu_items_en) ? "Back to Categories" : "Kategorilere Geri Dön");
                            temp_color_ids[current_option_idx++] = COLOR_PAIR_DEFAULT;

                            char title_buffer[MAX_CATEGORY_NAME_LEN + 30];
                            snprintf(title_buffer, sizeof(title_buffer), "%s: %s", (current_lang_menu_items == menu_items_en) ? "Manage" : "Yönet", selected_cat->name);

                            // draw_menu_and_get_choice fonksiyonuna current_lang_menu_items parametresi eklendi
                            int sub_choice = draw_menu_and_get_choice((const char**)temp_options, current_option_idx, title_buffer, 0, temp_color_ids, current_lang_menu_items);

                            free(temp_options);
                            free(temp_color_ids);

                            if (sub_choice == current_option_idx - 1) { // "Back to Categories"
                                break; // Kategori seçimine geri dön
                            } else if (sub_choice == current_option_idx - 2) { // "Delete This Category"
                                delete_category(cat_to_manage_idx, current_lang_menu_items);
                                break; // Kategori silindiği için kategori seçimine geri dön
                            } else if (sub_choice == current_option_idx - 3) { // "Add New Focus"
                                handle_new_focus_creation(selected_cat, current_lang_menu_items);
                            } else if (sub_choice != -1 && sub_choice < selected_cat->num_focuses) {
                                // Odak seçildi, şimdi odak için silme seçeneği sun
                                char *focus_options[2];
                                focus_options[0] = (char*)((current_lang_menu_items == menu_items_en) ? "Delete This Focus" : "Bu Odağı Sil");
                                focus_options[1] = (char*)((current_lang_menu_items == menu_items_en) ? "Back to Focus List" : "Odak Listesine Geri Dön");

                                int focus_option_colors[2];
                                focus_option_colors[0] = COLOR_PAIR_RED; // Kırmızı
                                focus_option_colors[1] = COLOR_PAIR_DEFAULT;

                                char focus_title_buffer[MAX_FOCUS_NAME_LEN + 30];
                                snprintf(focus_title_buffer, sizeof(focus_title_buffer), "%s: %s", (current_lang_menu_items == menu_items_en) ? "Manage Focus" : "Odağı Yönet", selected_cat->focuses[sub_choice].name);

                                // draw_menu_and_get_choice fonksiyonuna current_lang_menu_items parametresi eklendi
                                int delete_focus_choice = draw_menu_and_get_choice((const char**)focus_options, 2, focus_title_buffer, 0, focus_option_colors, current_lang_menu_items);
                                if (delete_focus_choice == 0) { // "Delete This Focus"
                                    delete_focus(selected_cat, sub_choice, current_lang_menu_items);
                                    // Odak silindiği için odak listesine geri dön (döngü devam edecek ve liste yenilenecek)
                                } else if (delete_focus_choice == 1 || delete_focus_choice == -1) {
                                    // Geri dön veya ESC, döngü devam edecek
                                }
                            } else if (sub_choice == -1) { // ESC basıldı
                                break; // Kategori seçimine geri dön
                            }
                        }
                    }
                }
            }
        } else if (selected_option == 2) { // "Tüm İstatistikleri Sıfırla"
            const char *confirm1 = (current_lang_menu_items == menu_items_en) ? "Are you sure you want to reset ALL statistics? (y/N)" : "TÜM istatistikleri sıfırlamak istediğinizden emin misiniz? (e/H)";
            const char *confirm2 = (current_lang_menu_items == menu_items_en) ? "This action cannot be undone. REALLY sure? (y/N)" : "Bu işlem geri alınamaz. GERÇEKTEN emin misiniz? (e/H)";
            if (get_double_confirmation(confirm1, confirm2, current_lang_menu_items)) {
                FILE *log_file = fopen(work_log_file_path, "w"); // Dosyayı yazma modunda açarak içeriğini sıfırla
                if (log_file != NULL) {
                    // CSV başlığını yeniden yaz
                    fprintf(log_file, "\"Category\",\"Focus\",\"StartTime\",\"EndTime\",\"Duration\"\n");
                    fclose(log_file);
                    clear();
                    const char *success_msg = (current_lang_menu_items == menu_items_en) ? "All statistics reset successfully!" : "Tüm istatistikler başarıyla sıfırlandı!";
                    mvprintw(yMax / 2, (xMax - strlen(success_msg)) / 2, "%s", success_msg);
                } else {
                    clear();
                    const char *error_msg = (current_lang_menu_items == menu_items_en) ? "Error resetting statistics." : "İstatistikler sıfırlanırken hata oluştu.";
                    mvprintw(yMax / 2, (xMax - strlen(error_msg)) / 2, "%s", error_msg);
                }
                const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
                mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
                refresh();
                getch();
            }
        } else if (selected_option == 3) { // "Tüm Odakları ve Kategorileri Sil"
            const char *confirm1 = (current_lang_menu_items == menu_items_en) ? "Are you sure you want to delete ALL categories and focuses? (y/N)" : "TÜM kategori ve odakları silmek istediğinizden emin misiniz? (e/H)";
            const char *confirm2 = (current_lang_menu_items == menu_items_en) ? "This will also reset ALL statistics. REALLY sure? (y/N)" : "Bu işlem tüm istatistikleri de sıfırlayacaktır. GERÇEKTEN emin misiniz? (e/H)";
            if (get_double_confirmation(confirm1, confirm2, current_lang_menu_items)) {
                // Kategori ve odak dosyasını sil
                int cat_del_result = remove(categories_file_path);
                // İstatistik dosyasını sıfırla (içeriğini boşalt ve başlığı yaz)
                FILE *log_file = fopen(work_log_file_path, "w");
                int log_reset_result = -1;
                if (log_file != NULL) {
                    fprintf(log_file, "\"Category\",\"Focus\",\"StartTime\",\"EndTime\",\"Duration\"\n");
                    fclose(log_file);
                    log_reset_result = 0; // Başarılı sıfırlama
                }

                if (cat_del_result == 0 && log_reset_result == 0) {
                    num_user_categories = 0; // Bellekteki veriyi de sıfırla
                    next_available_color_pair_id = MIN_CUSTOM_COLOR_PAIR; // Renk ID'lerini sıfırla
                    clear();
                    const char *success_msg = (current_lang_menu_items == menu_items_en) ? "All categories, focuses, and statistics deleted!" : "Tüm kategori, odak ve istatistikler silindi!";
                    mvprintw(yMax / 2, (xMax - strlen(success_msg)) / 2, "%s", success_msg);
                } else {
                    clear();
                    const char *error_msg = (current_lang_menu_items == menu_items_en) ? "Error deleting categories/focuses." : "Kategori/odaklar silinirken hata oluştu.";
                    mvprintw(yMax / 2, (xMax - strlen(error_msg)) / 2, "%s", error_msg);
                }
                const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
                mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
                refresh();
                getch();
            }
        } else if (selected_option == 4 || selected_option == -1) {
            return;
        }
    }
}

// handle_new_category_creation fonksiyonu (ESC ile iptal edilebilir)
// Return: Yeni kategorinin indeksi (başarılı), -1 (iptal edildi veya hata)
int handle_new_category_creation(const char **current_lang_menu_items) {
    clear();
    int yMax, xMax; getmaxyx(stdscr, yMax, xMax);
    const char *prompt = (current_lang_menu_items == menu_items_en) ? "Enter new category name: " : "Yeni kategori adını girin: ";
    char new_cat_name_buffer[MAX_CATEGORY_NAME_LEN];

    int result = get_string_input(new_cat_name_buffer, sizeof(new_cat_name_buffer), yMax / 2, (xMax - strlen(prompt) - MAX_CATEGORY_NAME_LEN) / 2, prompt);
    if (result == -1) { // ESC ile iptal edildi
        return -1;
    }

    if (strlen(new_cat_name_buffer) == 0) { // Boş isim girildi
        clear();
        const char *empty_name_msg = (current_lang_menu_items == menu_items_en) ? "Category name cannot be empty!" : "Kategori adı boş olamaz!";
        mvprintw(yMax / 2, (xMax - strlen(empty_name_msg)) / 2, "%s", empty_name_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
        return -1;
    }

    // Kategori zaten var mı kontrol et
    for (int i = 0; i < num_user_categories; i++) { // Hata düzeltildi: i < num_user_categories
        if (strcmp(user_categories[i].name, new_cat_name_buffer) == 0) {
            clear();
            const char *exists_msg = (current_lang_menu_items == menu_items_en) ? "Category already exists!" : "Kategori zaten mevcut!";
            mvprintw(yMax / 2, (xMax - strlen(exists_msg)) / 2, "%s", exists_msg);
            const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
            mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
            refresh();
            getch();
            return -1;
        }
    }

    if (num_user_categories < MAX_CATEGORIES) {
        strcpy(user_categories[num_user_categories].name, new_cat_name_buffer);
        user_categories[num_user_categories].num_focuses = 0;
        user_categories[num_user_categories].color_pair_id = get_random_color_pair(); // Bu çağrı renk çiftini başlatır
        num_user_categories++;
        save_data();
        return num_user_categories - 1; // Yeni eklenen kategorinin indeksini döndür
    } else {
        clear();
        mvprintw(yMax/2, (xMax - strlen("Max categories reached!"))/2, "Max categories reached!");
        refresh(); getch();
        return -1;
    }
}

// handle_new_focus_creation fonksiyonu (ESC ile iptal edilebilir)
// Return: Yeni odağın indeksi (başarılı), -1 (iptal edildi veya hata)
int handle_new_focus_creation(Category *cat, const char **current_lang_menu_items) {
    clear();
    int yMax, xMax; getmaxyx(stdscr, yMax, xMax);
    const char *prompt = (current_lang_menu_items == menu_items_en) ? "Enter new focus name: " : "Yeni odak adını girin: ";
    char new_focus_name_buffer[MAX_FOCUS_NAME_LEN];

    int result = get_string_input(new_focus_name_buffer, sizeof(new_focus_name_buffer), yMax / 2, (xMax - strlen(prompt) - MAX_FOCUS_NAME_LEN) / 2, prompt);
    if (result == -1) { // ESC ile iptal edildi
        return -1;
    }

    if (strlen(new_focus_name_buffer) == 0) { // Boş isim girildi
        clear();
        const char *empty_name_msg = (current_lang_menu_items == menu_items_en) ? "Focus name cannot be empty!" : "Odak adı boş olamaz!";
        mvprintw(yMax / 2, (xMax - strlen(empty_name_msg)) / 2, "%s", empty_name_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
        return -1;
    }

    // Odak zaten var mı kontrol et
    for (int i = 0; i < cat->num_focuses; i++) {
        if (strcmp(cat->focuses[i].name, new_focus_name_buffer) == 0) {
            clear();
            const char *exists_msg = (current_lang_menu_items == menu_items_en) ? "Focus already exists in this category!" : "Bu kategoride odak zaten mevcut!";
            mvprintw(yMax / 2, (xMax - strlen(exists_msg)) / 2, "%s", exists_msg);
            const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
            mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
            refresh();
        }
    }

    if (cat->num_focuses < MAX_FOCUSES_PER_CATEGORY) {
        strcpy(cat->focuses[cat->num_focuses].name, new_focus_name_buffer);
        cat->focuses[cat->num_focuses].color_pair_id = get_random_color_pair(); // Bu çağrı renk çiftini başlatır
        cat->num_focuses++;
        save_data();
        return cat->num_focuses - 1; // Yeni eklenen odağın indeksini döndür
    } else {
        clear();
        mvprintw(yMax/2, (xMax - strlen("Max focuses reached for this category!"))/2, "Max focuses reached for this category!");
        refresh(); getch();
        return -1;
    }
}

// Kategoriyi silme fonksiyonu
void delete_category(int index, const char **current_lang_menu_items) {
    if (index < 0 || index >= num_user_categories) return;

    int yMax, xMax; getmaxyx(stdscr, yMax, xMax);
    clear();
    const char *confirm_msg1 = (current_lang_menu_items == menu_items_en) ?
                              "Are you sure you want to delete category '%s' and all its focuses? (y/N)" :
                              "'%s' kategorisini ve tüm odaklarını silmek istediğinizden emin misiniz? (e/H)";
    const char *confirm_msg2 = (current_lang_menu_items == menu_items_en) ?
                              "This will also delete all associated statistics. REALLY sure? (y/N)" :
                              "Bu işlem ilişkili tüm istatistikleri de silecektir. GERÇEKTEN emin misiniz? (e/H)";
    char buffer1[256];
    snprintf(buffer1, sizeof(buffer1), confirm_msg1, user_categories[index].name);

    if (get_double_confirmation(buffer1, confirm_msg2, current_lang_menu_items)) {
        // İstatistik loglarından ilgili kategoriye ait tüm kayıtları sil
        filter_work_log(user_categories[index].name, NULL);

        // Kategoriyi listeden kaldır
        for (int i = index; i < num_user_categories - 1; i++) {
            user_categories[i] = user_categories[i+1];
        }
        num_user_categories--;
        save_data();

        clear();
        const char *deleted_msg = (current_lang_menu_items == menu_items_en) ? "Category deleted." : "Kategori silindi.";
        mvprintw(yMax / 2, (xMax - strlen(deleted_msg)) / 2, "%s", deleted_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
    } else {
        clear();
        const char *canceled_msg = (current_lang_menu_items == menu_items_en) ? "Deletion canceled." : "Silme işlemi iptal edildi.";
        mvprintw(yMax / 2, (xMax - strlen(canceled_msg)) / 2, "%s", canceled_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
    }
}

// Odağı silme fonksiyonu
void delete_focus(Category *cat, int index, const char **current_lang_menu_items) {
    if (index < 0 || index >= cat->num_focuses) return;

    int yMax, xMax; getmaxyx(stdscr, yMax, xMax);
    clear();
    const char *confirm_msg1 = (current_lang_menu_items == menu_items_en) ?
                              "Are you sure you want to delete focus '%s'? (y/N)" :
                              "'%s' odağını silmek istediğinizden emin misiniz? (e/H)";
    const char *confirm_msg2 = (current_lang_menu_items == menu_items_en) ?
                              "This will also delete all associated statistics. REALLY sure? (y/N)" :
                              "Bu işlem ilişkili tüm istatistikleri de silecektir. GERÇEKTEN emin misiniz? (e/H)";
    char buffer1[256];
    snprintf(buffer1, sizeof(buffer1), confirm_msg1, cat->focuses[index].name);

    if (get_double_confirmation(buffer1, confirm_msg2, current_lang_menu_items)) {
        // İstatistik loglarından ilgili odağa ait tüm kayıtları sil
        filter_work_log(cat->name, cat->focuses[index].name);

        // Odağı listeden kaldır
        for (int i = index; i < cat->num_focuses - 1; i++) {
            cat->focuses[i] = cat->focuses[i+1];
        }
        cat->num_focuses--;
        save_data();

        clear();
        const char *deleted_msg = (current_lang_menu_items == menu_items_en) ? "Focus deleted." : "Odak silindi.";
        mvprintw(yMax / 2, (xMax - strlen(deleted_msg)) / 2, "%s", deleted_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
    } else {
        clear();
        const char *canceled_msg = (current_lang_menu_items == menu_items_en) ? "Deletion canceled." : "Silme işlemi iptal edildi.";
        mvprintw(yMax / 2, (xMax - strlen(canceled_msg)) / 2, "%s", canceled_msg);
        const char *press_key_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return..." : "Geri dönmek için ESC tuşuna basın..."; // Updated message
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_key_msg)) / 2, "%s", press_key_msg);
        refresh();
        getch();
    }
}

// work_log.csv dosyasından belirtilen kategori veya odağa ait kayıtları filtreler
void filter_work_log(const char *deleted_category, const char *deleted_focus) {
    FILE *src_file = fopen(work_log_file_path, "r");
    if (src_file == NULL) {
        // Dosya yoksa veya okunamıyorsa yapacak bir şey yok.
        return;
    }

    char temp_file_path[300];
    snprintf(temp_file_path, sizeof(temp_file_path), "%s/work_log_temp.csv", focuslog_data_dir);
    FILE *temp_file = fopen(temp_file_path, "w");
    if (temp_file == NULL) {
        fprintf(stderr, "Hata: Geçici dosya oluşturulamadı: %s\n", temp_file_path);
        fclose(src_file);
        return;
    }

    char line_buffer[512];

    // Başlık satırını kopyala
    if (fgets(line_buffer, sizeof(line_buffer), src_file) != NULL) {
        fputs(line_buffer, temp_file);
    }

    while (fgets(line_buffer, sizeof(line_buffer), src_file) != NULL) {
        char temp_line[512];
        strncpy(temp_line, line_buffer, sizeof(temp_line) - 1);
        temp_line[sizeof(temp_line) - 1] = '\0';
        temp_line[strcspn(temp_line, "\n")] = 0; // Yeni satır karakterini kaldır

        char category_name[MAX_CATEGORY_NAME_LEN];
        char focus_name[MAX_FOCUS_NAME_LEN];

        char *ptr = temp_line;
        char *start = ptr;
        bool in_quote = false;

        // Kategori adını al
        while (*ptr && (in_quote || *ptr != ',')) {
            if (*ptr == '"') in_quote = !in_quote;
            ptr++;
        }
        strncpy(category_name, start + (*start == '"' ? 1 : 0), (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0));
        category_name[ (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0) ] = '\0';

        // Odak adını al
        ptr++; // Virgülü geç
        start = ptr;
        in_quote = false;
        while (*ptr && (in_quote || *ptr != ',')) {
            if (*ptr == '"') in_quote = !in_quote;
            ptr++;
        }
        strncpy(focus_name, start + (*start == '"' ? 1 : 0), (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0));
        focus_name[ (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0) ] = '\0';

        // Diğer alanları atla (Başlangıç Zamanı, Bitiş Zamanı, Süre)
        for (int i = 0; i < 2; i++) {
            ptr++; // Virgülü geç
            start = ptr;
            in_quote = false;
            while (*ptr && (in_quote || *ptr != ',')) {
                if (*ptr == '"') in_quote = !in_quote;
                ptr++;
            }
        }

        bool should_delete = false;
        if (deleted_category != NULL && strcmp(category_name, deleted_category) == 0) {
            if (deleted_focus == NULL || strcmp(focus_name, deleted_focus) == 0) {
                should_delete = true;
            }
        }

        if (!should_delete) {
            fputs(line_buffer, temp_file); // Orijinal satırı yeni dosyaya yaz
        }
    }

    fclose(src_file);
    fclose(temp_file);

    // Orijinal dosyayı sil ve geçici dosyayı yeniden adlandır
    remove(work_log_file_path);
    rename(temp_file_path, work_log_file_path);
}


void create_data_directory() {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Hata: HOME ortam değişkeni bulunamadı.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(focuslog_data_dir, sizeof(focuslog_data_dir), "%s/.config/focuslog", home_dir);

    if (mkdir(focuslog_data_dir, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Hata: Dizin oluşturulamadı: %s\n", focuslog_data_dir);
        exit(EXIT_FAILURE);
    }

    snprintf(categories_file_path, sizeof(categories_file_path), "%s/categories_and_focuses.txt", focuslog_data_dir);
    snprintf(work_log_file_path, sizeof(work_log_file_path), "%s/work_log.csv", focuslog_data_dir);
}

void load_data() {
    FILE *file = fopen(categories_file_path, "r");
    if (file == NULL) {
        num_user_categories = 0;
        return;
    }

    num_user_categories = 0;
    next_available_color_pair_id = MIN_CUSTOM_COLOR_PAIR;

    char line[MAX_CATEGORY_NAME_LEN + MAX_FOCUS_NAME_LEN + 5 + 10];
    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0) continue;

        if (line[0] == '#') {
            if (num_user_categories < MAX_CATEGORIES) {
                char *temp_line = strdup(line + 1);
                if (temp_line == NULL) { continue; }

                char *name_part = strtok(temp_line, ";");
                char *id_part = strtok(NULL, ";");

                if (name_part != NULL) {
                    strncpy(user_categories[num_user_categories].name, name_part, MAX_CATEGORY_NAME_LEN - 1);
                    user_categories[num_user_categories].name[MAX_CATEGORY_NAME_LEN - 1] = '\0';
                    user_categories[num_user_categories].num_focuses = 0;
                    if (id_part != NULL) {
                        user_categories[num_user_categories].color_pair_id = atoi(id_part);
                        if (user_categories[num_user_categories].color_pair_id >= next_available_color_pair_id) {
                            next_available_color_pair_id = user_categories[num_user_categories].color_pair_id + 1;
                        }
                    } else {
                        // Eğer renk ID'si dosyada yoksa yeni bir tane ata
                        user_categories[num_user_categories].color_pair_id = get_random_color_pair();
                    }
                    // init_pair çağrısı ensure_all_color_pairs_initialized() içinde yapılacak
                    num_user_categories++;
                }
                free(temp_line);
            }
        } else {
            if (num_user_categories > 0) {
                Category *current_cat = &user_categories[num_user_categories - 1];
                if (current_cat->num_focuses < MAX_FOCUSES_PER_CATEGORY) {
                    char *temp_line = strdup(line);
                    if (temp_line == NULL) { continue; }

                    char *name_part = strtok(temp_line, ";");
                    char *id_part = strtok(NULL, ";");

                    if (name_part != NULL) {
                        strncpy(current_cat->focuses[current_cat->num_focuses].name, name_part, MAX_FOCUS_NAME_LEN - 1);
                        current_cat->focuses[current_cat->num_focuses].name[MAX_FOCUS_NAME_LEN - 1] = '\0';
                        if (id_part != NULL) {
                            current_cat->focuses[current_cat->num_focuses].color_pair_id = atoi(id_part);
                            if (current_cat->focuses[current_cat->num_focuses].color_pair_id >= next_available_color_pair_id) {
                                next_available_color_pair_id = current_cat->focuses[current_cat->num_focuses].color_pair_id + 1;
                            }
                        } else {
                            // Eğer renk ID'si dosyada yoksa yeni bir tane ata
                            current_cat->focuses[current_cat->num_focuses].color_pair_id = get_random_color_pair();
                        }
                        // init_pair çağrısı ensure_all_color_pairs_initialized() içinde yapılacak
                        current_cat->num_focuses++;
                    }
                    free(temp_line);
                }
            }
        }
    }
    fclose(file);
}

void save_data() {
    FILE *file = fopen(categories_file_path, "w");
    if (file == NULL) {
        fprintf(stderr, "Hata: Kategori ve odak dosyasına yazılamadı: %s\n", categories_file_path);
        return;
    }

    for (int i = 0; i < num_user_categories; i++) {
        fprintf(file, "#%s;%d\n", user_categories[i].name, user_categories[i].color_pair_id);
        for (int j = 0; j < user_categories[i].num_focuses; j++) {
            fprintf(file, "%s;%d\n", user_categories[i].focuses[j].name, user_categories[i].focuses[j].color_pair_id);
        }
    }
    fclose(file);
}

void record_work_session(const char *category, const char *focus, time_t start_time, time_t end_time, long duration) {
    FILE *file = fopen(work_log_file_path, "a");
    if (file == NULL) {
        fprintf(stderr, "Hata: Çalışma kayıt dosyasına yazılamadı: %s\n", work_log_file_path);
        return;
    }

    char start_time_str[20], end_time_str[20];
    strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M:%S", localtime(&start_time));
    strftime(end_time_str, sizeof(end_time_str), "%Y-%m-%d %H:%M:%S", localtime(&end_time));

    fprintf(file, "\"%s\",\"%s\",\"%s\",\"%s\",%ld\n", category, focus, start_time_str, end_time_str, duration);
    fclose(file);
}

int get_random_color_pair() {
    if (next_available_color_pair_id > MAX_CUSTOM_COLOR_PAIR) {
        return COLOR_PAIR_DEFAULT;
    }
    if (!has_colors()) {
        return COLOR_PAIR_DEFAULT;
    }

    int fg_color;
    // COLOR_BLACK (0) ve COLOR_WHITE (7) dışında rastgele bir renk seç
    // Daha belirgin renkler için 1-6 arasındaki renkleri tercih et
    do {
        fg_color = (rand() % 6) + 1; // 1-6 arası (Kırmızı, Yeşil, Sarı, Mavi, Magenta, Cyan)
    } while (fg_color == COLOR_BLACK || fg_color == COLOR_WHITE);

    // Yeni renk çiftini başlat ve başlatıldığını işaretle.
    // Arka planı siyah tutarak, ACS_BLOCK ile ön plan rengini kullanarak görünürlük sağlayacağız.
    init_pair(next_available_color_pair_id, fg_color, COLOR_BLACK);
    g_initialized_color_pairs[next_available_color_pair_id] = true;

    return next_available_color_pair_id++;
}

// Tüm yüklü renk çiftlerinin başlatıldığından emin olmak için fonksiyon
void ensure_all_color_pairs_initialized() {
    if (!has_colors()) return;

    // Başlatma dizisini sıfırla
    for (int i = 0; i < MAX_COLOR_PAIRS; i++) {
        g_initialized_color_pairs[i] = false;
    }

    // Varsayılan renk çiftlerini işaretle (main'de zaten başlatıldılar)
    g_initialized_color_pairs[COLOR_PAIR_DEFAULT] = true;
    g_initialized_color_pairs[COLOR_PAIR_HIGHLIGHT] = true;
    g_initialized_color_pairs[COLOR_PAIR_TITLE] = true;
    g_initialized_color_pairs[COLOR_PAIR_RED] = true;

    for (int i = 0; i < num_user_categories; i++) {
        int cat_id = user_categories[i].color_pair_id;
        if (cat_id >= MIN_CUSTOM_COLOR_PAIR && cat_id < MAX_COLOR_PAIRS && !g_initialized_color_pairs[cat_id]) {
            int fg_color = ((cat_id - MIN_CUSTOM_COLOR_PAIR) % 6) + 1; // Deterministic color based on ID
            if (fg_color < 1 || fg_color > 6) fg_color = COLOR_WHITE; // Fallback for safety
            init_pair(cat_id, fg_color, COLOR_BLACK);
            g_initialized_color_pairs[cat_id] = true;
        }

        for (int j = 0; j < user_categories[i].num_focuses; j++) {
            int focus_id = user_categories[i].focuses[j].color_pair_id;
            if (focus_id >= MIN_CUSTOM_COLOR_PAIR && focus_id < MAX_COLOR_PAIRS && !g_initialized_color_pairs[focus_id]) {
                int fg_color = ((focus_id - MIN_CUSTOM_COLOR_PAIR) % 6) + 1; // Deterministic color based on ID
                if (fg_color < 1 || fg_color > 6) fg_color = COLOR_WHITE; // Fallback for safety
                init_pair(focus_id, fg_color, COLOR_BLACK);
                g_initialized_color_pairs[focus_id] = true;
            }
        }
    }
}


// İstatistik yükleme fonksiyonu
void load_statistics() {
    num_stat_categories = 0; // İstatistikleri sıfırla

    FILE *file = fopen(work_log_file_path, "r");
    if (file == NULL) {
        return;
    }

    char line_buffer[512];

    // CSV başlığını atla
    if (fgets(line_buffer, sizeof(line_buffer), file) == NULL) {
        fclose(file);
        return;
    }

    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        line_buffer[strcspn(line_buffer, "\n")] = 0;

        char category_name[MAX_CATEGORY_NAME_LEN];
        char focus_name[MAX_FOCUS_NAME_LEN];
        char duration_str[20];
        long duration_s = 0;

        char *ptr = line_buffer;
        char *start = ptr;
        bool in_quote = false;

        // Kategori adını al
        while (*ptr && (in_quote || *ptr != ',')) {
            if (*ptr == '"') in_quote = !in_quote;
            ptr++;
        }
        // Tırnakları kaldırarak kopyala
        strncpy(category_name, start + (*start == '"' ? 1 : 0), (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0));
        category_name[ (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0) ] = '\0';

        // Odak adını al
        ptr++; // Virgülü geç
        start = ptr;
        in_quote = false;
        while (*ptr && (in_quote || *ptr != ',')) {
            if (*ptr == '"') in_quote = !in_quote;
            ptr++;
        }
        // Tırnakları kaldırarak kopyala
        strncpy(focus_name, start + (*start == '"' ? 1 : 0), (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0));
        focus_name[ (ptr - start) - (*start == '"' ? 1 : 0) - (*(ptr-1) == '"' ? 1 : 0) ] = '\0';

        // Diğer alanları atla (Başlangıç Zamanı, Bitiş Zamanı)
        for (int i = 0; i < 2; i++) {
            ptr++; // Virgülü geç
            start = ptr;
            in_quote = false;
            while (*ptr && (in_quote || *ptr != ',')) {
                if (*ptr == '"') in_quote = !in_quote;
                ptr++;
            }
        }

        // Süre adını al (son alan)
        ptr++; // Virgülü geç
        start = ptr;
        strncpy(duration_str, start, sizeof(duration_str) - 1);
        duration_str[sizeof(duration_str) - 1] = '\0';


        duration_s = atol(duration_str);

        // İstatistiklere ekle
        int cat_idx = get_stat_category_index(category_name);
        if (cat_idx == -1) { // Yeni kategori
            if (num_stat_categories < MAX_CATEGORIES) {
                cat_idx = num_stat_categories++;
                strncpy(stat_categories[cat_idx].name, category_name, MAX_CATEGORY_NAME_LEN - 1);
                stat_categories[cat_idx].name[MAX_CATEGORY_NAME_LEN - 1] = '\0';
                stat_categories[cat_idx].num_focuses = 0;
            } else {
                continue; // Maksimum kategori sayısına ulaşıldı
            }
        }

        int focus_idx = get_stat_focus_index(&stat_categories[cat_idx], focus_name);
        if (focus_idx == -1) { // Yeni odak
            if (stat_categories[cat_idx].num_focuses < MAX_FOCUSES_PER_CATEGORY) {
                focus_idx = stat_categories[cat_idx].num_focuses++;
                strncpy(stat_categories[cat_idx].focuses[focus_idx].name, focus_name, MAX_FOCUS_NAME_LEN - 1);
                stat_categories[cat_idx].focuses[focus_idx].name[MAX_FOCUS_NAME_LEN - 1] = '\0';
                stat_categories[cat_idx].focuses[focus_idx].total_duration = 0;
                stat_categories[cat_idx].focuses[focus_idx].session_count = 0;
            } else {
                continue;
            }
        }
        stat_categories[cat_idx].focuses[focus_idx].total_duration += duration_s;
        stat_categories[cat_idx].focuses[focus_idx].session_count++;
    }
    fclose(file);
}

int get_stat_category_index(const char *category_name) {
    for (int i = 0; i < num_stat_categories; i++) {
        if (strcmp(stat_categories[i].name, category_name) == 0) {
            return i;
        }
    }
    return -1;
}

int get_stat_focus_index(StatCategory *stat_cat, const char *focus_name) {
    for (int i = 0; i < stat_cat->num_focuses; i++) {
        if (strcmp(stat_cat->focuses[i].name, focus_name) == 0) {
            return i;
        }
    }
    return -1;
}


void view_statistics(const char **current_lang_menu_items) {
    clear();
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);

    const char *title = (current_lang_menu_items == menu_items_en) ? "Statistics" : "İstatistikler";
    const char *no_data_msg = (current_lang_menu_items == menu_items_en) ? "No work log data found." : "Çalışma kaydı bulunamadı.";
    const char *press_esc_to_return_msg = (current_lang_menu_items == menu_items_en) ? "Press ESC to return to menu..." : "Menüye dönmek için ESC tuşuna basın..."; // Updated message

    attron(COLOR_PAIR(COLOR_PAIR_TITLE));
    mvprintw(0, (xMax - strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(COLOR_PAIR_TITLE));
    refresh();

    load_statistics(); // İstatistik verilerini yükle

    if (num_stat_categories == 0) {
        mvprintw(yMax / 2, (xMax - strlen(no_data_msg)) / 2, "%s", no_data_msg);
        mvprintw(yMax / 2 + 2, (xMax - strlen(press_esc_to_return_msg)) / 2, "%s", press_esc_to_return_msg); // Updated message
        refresh();
        getch();
        return;
    }

    int current_y = 2;
    int max_display_rows = yMax - current_y - 4;

    // Calculate max widths for alignment in the statistics table
    int max_cat_name_len_display = strlen((current_lang_menu_items == menu_items_en) ? "Category" : "Kategori");
    int max_focus_name_len_display = strlen((current_lang_menu_items == menu_items_en) ? "Focus" : "Odak");
    char temp_duration_buffer[20];
    format_duration_string(9999999, temp_duration_buffer, sizeof(temp_duration_buffer)); // Max possible duration string
    int max_duration_str_len_display = strlen((current_lang_menu_items == menu_items_en) ? "Duration" : "Süre");
    if (strlen(temp_duration_buffer) > max_duration_str_len_display) {
        max_duration_str_len_display = strlen(temp_duration_buffer);
    }


    for (int i = 0; i < num_user_categories; i++) {
        if (strlen(user_categories[i].name) > max_cat_name_len_display) {
            max_cat_name_len_display = strlen(user_categories[i].name);
        }
        for (int j = 0; j < user_categories[i].num_focuses; j++) {
            if (strlen(user_categories[i].focuses[j].name) > max_focus_name_len_display) {
                max_focus_name_len_display = strlen(user_categories[i].focuses[j].name);
            }
        }
    }

    // Add some padding to column widths
    max_cat_name_len_display += 2;
    max_focus_name_len_display += 2;
    max_duration_str_len_display += 2;

    // Calculate total line width for the table (Category + " | " + Focus + " | " + Duration)
    int total_table_line_width = max_cat_name_len_display + strlen(" | ") + max_focus_name_len_display + strlen(" | ") + max_duration_str_len_display;

    // Calculate start_x to center the entire table
    int table_start_x = (xMax - total_table_line_width) / 2;
    if (table_start_x < 0) table_start_x = 0;


    const char *cat_header = (current_lang_menu_items == menu_items_en) ? "Category" : "Kategori";
    const char *focus_header = (current_lang_menu_items == menu_items_en) ? "Focus" : "Odak";
    const char *duration_header = (current_lang_menu_items == menu_items_en) ? "Duration" : "Süre";

    // Print headers
    attron(A_BOLD | COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
    mvprintw(current_y, table_start_x, "%-*s | %-*s | %*s",
             max_cat_name_len_display, cat_header,
             max_focus_name_len_display, focus_header,
             max_duration_str_len_display, duration_header);
    attroff(A_BOLD | COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
    current_y++;

    // Print separator line
    mvhline(current_y, table_start_x, '-', total_table_line_width);
    current_y++;


    int ch;

    while (1) {
        clear();
        attron(COLOR_PAIR(COLOR_PAIR_TITLE));
        mvprintw(0, (xMax - strlen(title)) / 2, "%s", title);
        attroff(COLOR_PAIR(COLOR_PAIR_TITLE));

        // Yeniden başlıkları ve ayırıcıyı çiz
        attron(A_BOLD | COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
        mvprintw(2, table_start_x, "%-*s | %-*s | %*s",
                 max_cat_name_len_display, cat_header,
                 max_focus_name_len_display, focus_header,
                 max_duration_str_len_display, duration_header);
        attroff(A_BOLD | COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
        mvhline(3, table_start_x, '-', total_table_line_width);

        int display_row = 4; // Start drawing content from row 4 (after headers and separator)
        int items_displayed = 0;

        // Kullanıcı tanımlı kategorileri döngüye al
        for (int i = 0; i < num_user_categories; i++) {
            if (display_row + items_displayed >= yMax - 2) break; // Check if enough space for category header

            const char *current_category_name = user_categories[i].name;
            int category_color_id = user_categories[i].color_pair_id;

            attron(COLOR_PAIR(category_color_id));
            attron(A_BOLD);
            mvprintw(display_row + items_displayed, table_start_x, "%s", current_category_name);
            attroff(COLOR_PAIR(category_color_id));
            attroff(A_BOLD);
            items_displayed++;

            // Her odağın detayları
            for (int j = 0; j < user_categories[i].num_focuses; j++) {
                if (display_row + items_displayed >= yMax - 2) break; // Check if enough space for focus line

                const char *current_focus_name = user_categories[i].focuses[j].name;
                int focus_color_id = user_categories[i].focuses[j].color_pair_id;
                long focus_total_duration = 0;

                int stat_cat_idx = get_stat_category_index(user_categories[i].name);
                if (stat_cat_idx != -1) { // Kategori istatistiklerde varsa
                    int stat_focus_idx = get_stat_focus_index(&stat_categories[stat_cat_idx], current_focus_name);
                    if (stat_focus_idx != -1) { // Odak istatistiklerde varsa
                        focus_total_duration = stat_categories[stat_cat_idx].focuses[stat_focus_idx].total_duration;
                    }
                }

                char focus_total_duration_str[20];
                format_duration_string(focus_total_duration, focus_total_duration_str, sizeof(focus_total_duration_str));

                // Print category name (empty for subsequent focuses in same category)
                mvprintw(display_row + items_displayed, table_start_x, "%-*s", max_cat_name_len_display, "");

                // Print separator
                mvprintw(display_row + items_displayed, table_start_x + max_cat_name_len_display, " | ");

                // Print focus name with its color
                attron(COLOR_PAIR(focus_color_id));
                mvprintw(display_row + items_displayed, table_start_x + max_cat_name_len_display + strlen(" | "), "%-*s", max_focus_name_len_display, current_focus_name);
                attroff(COLOR_PAIR(focus_color_id));

                // Print separator
                mvprintw(display_row + items_displayed, table_start_x + max_cat_name_len_display + strlen(" | ") + max_focus_name_len_display, " | ");

                // Print duration (right-aligned within its column)
                mvprintw(display_row + items_displayed, table_start_x + max_cat_name_len_display + strlen(" | ") + max_focus_name_len_display + strlen(" | "), "%*s", max_duration_str_len_display, focus_total_duration_str);

                items_displayed++;
            }
            if (items_displayed < max_display_rows) { // Add an empty line between categories
                mvprintw(display_row + items_displayed, table_start_x, "");
                items_displayed++;
            }
        }

        mvprintw(yMax - 2, (xMax - strlen(press_esc_to_return_msg)) / 2, "%s", press_esc_to_return_msg); // Updated message
        refresh();
        ch = getch();

        if (ch == 27) { // ESC
            break;
        }
    }
}

// Boşta kalma çubuğunu çizen fonksiyon
void draw_idle_bar(const char **current_lang_menu_items) {
    clear();
    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);

    load_statistics(); // En güncel istatistikleri yükle

    // Toplam süreyi ve odakların listesini hazırla
    long total_overall_duration = 0;
    // Tüm odakları tek bir listede toplamak için dinamik dizi
    StatFocus *all_focuses = (StatFocus *)malloc(MAX_CATEGORIES * MAX_FOCUSES_PER_CATEGORY * sizeof(StatFocus));
    if (all_focuses == NULL) {
        // Bellek hatası
        return;
    }
    int num_all_focuses = 0;

    for (int i = 0; i < num_stat_categories; i++) {
        for (int j = 0; j < stat_categories[i].num_focuses; j++) {
            total_overall_duration += stat_categories[i].focuses[j].total_duration;
            if (num_all_focuses < MAX_CATEGORIES * MAX_FOCUSES_PER_CATEGORY) {
                all_focuses[num_all_focuses++] = stat_categories[i].focuses[j];
            }
        }
    }

    // Odakları süreye göre azalan sırada sırala (kabarcık sıralaması basitlik için)
    for (int i = 0; i < num_all_focuses - 1; i++) {
        for (int j = 0; j < num_all_focuses - i - 1; j++) {
            if (all_focuses[j].total_duration < all_focuses[j+1].total_duration) {
                StatFocus temp = all_focuses[j];
                all_focuses[j] = all_focuses[j+1];
                all_focuses[j+1] = temp;
            }
        }
    }

    // Başlık
    const char *idle_title = (current_lang_menu_items == menu_items_en) ? "Current Focus Distribution" : "Mevcut Odak Dağılımı";
    attron(COLOR_PAIR(COLOR_PAIR_TITLE));
    mvprintw(yMax / 2 - 10, (xMax - strlen(idle_title)) / 2, "%s", idle_title);
    attroff(COLOR_PAIR(COLOR_PAIR_TITLE));

    // Saat ve Dakika
    time_t rawtime;
    struct tm *info;
    char time_buffer[80];
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M", info);
    attron(A_BOLD);
    mvprintw(yMax / 2 - 8, (xMax - strlen(time_buffer)) / 2, "%s", time_buffer);
    attroff(A_BOLD);


    // İstatistik Çubuğu
    int bar_width = xMax - 20; // Ekran genişliğinin bir kısmı
    if (bar_width < 10) bar_width = 10; // Minimum bar genişliği
    int bar_start_x = (xMax - bar_width) / 2;
    int bar_y = yMax / 2 - 5;

    // Barın çerçevesini çiz (önce çerçeve)
    mvhline(bar_y - 1, bar_start_x - 1, '-', bar_width + 2);
    mvvline(bar_y, bar_start_x - 1, '|', 1);
    mvvline(bar_y, bar_start_x + bar_width, '|', 1);
    mvhline(bar_y + 1, bar_start_x - 1, '-', bar_width + 2);
    mvaddch(bar_y - 1, bar_start_x - 1, ACS_ULCORNER);
    mvaddch(bar_y - 1, bar_start_x + bar_width, ACS_URCORNER);
    mvaddch(bar_y + 1, bar_start_x - 1, ACS_LLCORNER);
    mvaddch(bar_y + 1, bar_start_x + bar_width, ACS_LRCORNER);

    // Barın içini varsayılan arka plan rengiyle doldur
    attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
    mvhline(bar_y, bar_start_x, ' ', bar_width);
    attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));

    if (total_overall_duration > 0) {
        int current_bar_x = bar_start_x;
        for (int i = 0; i < num_all_focuses; i++) {
            double percentage = (double)all_focuses[i].total_duration / total_overall_duration;
            int segment_width = (int)(bar_width * percentage);

            // Minimum segment genişliği: Eğer yüzde > 0 ise ve hesaplanan genişlik 0 ise, 1 piksel yap
            if (percentage > 0 && segment_width == 0) {
                segment_width = 1;
            }

            // Barın dışına taşmasını engelle
            if (current_bar_x + segment_width > bar_start_x + bar_width) {
                segment_width = (bar_start_x + bar_width) - current_bar_x;
            }
            if (segment_width <= 0) continue; // Geçersiz segment genişliği

            // Odağın rengini bul
            int focus_color_id = COLOR_PAIR_DEFAULT;
            for (int k = 0; k < num_user_categories; k++) {
                for (int l = 0; l < user_categories[k].num_focuses; l++) {
                    if (strcmp(user_categories[k].focuses[l].name, all_focuses[i].name) == 0) {
                        focus_color_id = user_categories[k].focuses[l].color_pair_id;
                        break;
                    }
                }
                if (focus_color_id != COLOR_PAIR_DEFAULT) break;
            }

            // Bar segmentini çiz (ACS_BLOCK ile)
            attron(COLOR_PAIR(focus_color_id));
            for (int x_pos = 0; x_pos < segment_width; x_pos++) {
                mvaddch(bar_y, current_bar_x + x_pos, ACS_BLOCK); // Boşluk yerine ACS_BLOCK kullanıldı
            }
            attroff(COLOR_PAIR(focus_color_id));
            current_bar_x += segment_width;
        }
        // Kalan kısmı varsayılan renkle doldur (yuvarlama hatalarını önlemek için)
        if (current_bar_x < bar_start_x + bar_width) {
            attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
            for (int x_pos = current_bar_x; x_pos < bar_start_x + bar_width; x_pos++) {
                mvaddch(bar_y, x_pos, ACS_BLOCK); // Boşluk yerine ACS_BLOCK kullanıldı
            }
            attroff(COLOR_PAIR(COLOR_PAIR_DEFAULT));
        }
    } else {
        const char *no_stats_msg = (current_lang_menu_items == menu_items_en) ? "No statistics yet to display." : "Henüz görüntülenecek istatistik yok.";
        mvprintw(bar_y, (xMax - strlen(no_stats_msg)) / 2, "%s", no_stats_msg);
    }

    // En çok odaklanılan 3 odak bölümü
    mvprintw(bar_y + 3, (xMax - strlen((current_lang_menu_items == menu_items_en) ? "Top 3 Focuses:" : "En Çok Odaklanılan 3 Odak:")) / 2, "%s", (current_lang_menu_items == menu_items_en) ? "Top 3 Focuses:" : "En Çok Odaklanılan 3 Odak:");

    // Calculate max widths for alignment in Top 3 Focuses section
    int max_top_cat_len = 0;
    int max_top_focus_len = 0;
    char temp_duration_buffer[20]; // For duration string length

    for (int i = 0; i < num_all_focuses && i < 3; i++) {
        // Find category name for this focus
        char current_category_name[MAX_CATEGORY_NAME_LEN] = "";
        for (int k = 0; k < num_user_categories; k++) {
            for (int l = 0; l < user_categories[k].num_focuses; l++) {
                if (strcmp(user_categories[k].focuses[l].name, all_focuses[i].name) == 0) {
                    strncpy(current_category_name, user_categories[k].name, sizeof(current_category_name) - 1);
                    current_category_name[sizeof(current_category_name) - 1] = '\0';
                    break;
                }
            }
            if (strlen(current_category_name) > 0) break;
        }
        if (strlen(current_category_name) > max_top_cat_len) {
            max_top_cat_len = strlen(current_category_name);
        }
        if (strlen(all_focuses[i].name) > max_top_focus_len) {
            max_top_focus_len = strlen(all_focuses[i].name);
        }
    }

    // Ensure minimum column widths for readability
    if (max_top_cat_len < 10) max_top_cat_len = 10;
    if (max_top_focus_len < 15) max_top_focus_len = 15;

    // Adjust for padding and separators
    int col1_width = max_top_cat_len;
    int col2_width = max_top_focus_len;
    format_duration_string(9999999, temp_duration_buffer, sizeof(temp_duration_buffer)); // Max possible duration string
    int col3_width = strlen(temp_duration_buffer); // Duration width

    // Calculate total line width for the table (Category + " - " + Focus + ": " + Duration)
    int total_top_focus_line_width = col1_width + strlen(" - ") + col2_width + strlen(": ") + col3_width;

    // Calculate start_x to center the entire block
    int top_focus_block_start_x = (xMax - total_top_focus_line_width) / 2;
    if (top_focus_block_start_x < 0) top_focus_block_start_x = 0;


    const char *cat_header = (current_lang_menu_items == menu_items_en) ? "Category" : "Kategori";
    const char *focus_header = (current_lang_menu_items == menu_items_en) ? "Focus" : "Odak";
    const char *duration_header = (current_lang_menu_items == menu_items_en) ? "Duration" : "Süre";

    // Print headers
    mvprintw(bar_y + 5, top_focus_block_start_x, "%-*s - %-*s : %*s",
             col1_width, cat_header,
             col2_width, focus_header,
             col3_width, duration_header);

    mvhline(bar_y + 6, top_focus_block_start_x, '-', total_top_focus_line_width);


    int display_y = bar_y + 7;
    for (int i = 0; i < num_all_focuses && i < 3; i++) {
        char total_time_str[20];
        format_duration_string(all_focuses[i].total_duration, total_time_str, sizeof(total_time_str));

        char category_for_focus[MAX_CATEGORY_NAME_LEN] = "";
        int category_color_id = COLOR_PAIR_DEFAULT;
        int focus_color_id = COLOR_PAIR_DEFAULT;

        // Find category and focus colors
        for (int k = 0; k < num_user_categories; k++) {
            for (int l = 0; l < user_categories[k].num_focuses; l++) {
                if (strcmp(user_categories[k].focuses[l].name, all_focuses[i].name) == 0) {
                    strncpy(category_for_focus, user_categories[k].name, sizeof(category_for_focus) - 1);
                    category_for_focus[sizeof(category_for_focus) - 1] = '\0';
                    category_color_id = user_categories[k].color_pair_id;
                    focus_color_id = user_categories[k].focuses[l].color_pair_id;
                    break;
                }
            }
            if (strlen(category_for_focus) > 0) break;
        }
        if (strlen(category_for_focus) == 0) {
            strncpy(category_for_focus, (current_lang_menu_items == menu_items_en) ? "Unknown Category" : "Bilinmeyen Kategori", sizeof(category_for_focus) - 1);
            category_for_focus[sizeof(category_for_focus) - 1] = '\0';
        }

        // Print category name with its color
        attron(COLOR_PAIR(category_color_id));
        mvprintw(display_y + i, top_focus_block_start_x, "%-*s", col1_width, category_for_focus);
        attroff(COLOR_PAIR(category_color_id));

        // Print separator
        mvprintw(display_y + i, top_focus_block_start_x + col1_width, " - ");

        // Print focus name with its color
        attron(COLOR_PAIR(focus_color_id));
        mvprintw(display_y + i, top_focus_block_start_x + col1_width + strlen(" - "), "%-*s", col2_width, all_focuses[i].name);
        attroff(COLOR_PAIR(focus_color_id));

        // Print separator
        mvprintw(display_y + i, top_focus_block_start_x + col1_width + strlen(" - ") + col2_width, " : ");

        // Print duration (right-aligned within its column)
        mvprintw(display_y + i, top_focus_block_start_x + col1_width + strlen(" - ") + col2_width + strlen(" : "), "%*s", col3_width, total_time_str);
    }

    refresh();
    nodelay(stdscr, FALSE); // Bloğa girene kadar beklet
    getch(); // Herhangi bir tuşa basılmasını bekle
    nodelay(stdscr, TRUE); // Geri döndüğünde non-blocking moda geç

    free(all_focuses); // Belleği serbest bırak
}
1
