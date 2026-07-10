#pragma once
#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>
#include <deque>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#endif

// g_state is defined in main.cpp (same translation unit)

// ============================================================
// TUI terminal constants and state
// ============================================================
namespace tui {

static constexpr int kTabOverview = 0;
static constexpr int kTabMine = 1;
static constexpr int kTabConsole = 2;

static constexpr int kTitleLine = 0;
static constexpr int kTabLine = 1;
static constexpr int kSepLine = 2;
static constexpr int kContentStart = 3;

static int g_cols = 80;
static int g_rows = 24;
static int g_active_tab = 0;
static bool g_running = true;
static bool g_mining = false;
static std::deque<std::string> g_log;
static std::thread g_http_thread;
static bool g_http_thread_running = false;

// Log: max lines and a mutex-like toggle for thread safety (single-threaded draw)
static constexpr size_t kMaxLogLines = 500;
static int g_log_scroll = 0;
static bool g_dirty = true;
static bool g_dialog_showing = false;
static int64_t g_last_refresh = 0;
static bool g_cursor_visible = true;

} // namespace tui

// ============================================================
// Terminal abstraction
// ============================================================
static inline void tui_get_term_size(int& cols, int& rows) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        cols = w.ws_col;
        rows = w.ws_row;
    }
#endif
}

static inline void tui_gotoxy(int x, int y) {
    if (x < 0) x = 0; if (y < 0) y = 0;
#ifdef _WIN32
    COORD c = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
#else
    printf("\033[%d;%dH", y + 1, x + 1);
#endif
}

static inline void tui_set_color(int fg, int bg) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(h, (WORD)(fg | (bg << 4)));
#else
    printf("\033[%d;%dm", 30 + fg, 40 + bg);
#endif
}

static inline void tui_reset_color() {
#ifdef _WIN32
    tui_set_color(7, 0); // light gray on black
#else
    printf("\033[0m");
#endif
}

static inline void tui_clear_screen() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(h, &csbi);
    DWORD sz = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written;
    FillConsoleOutputCharacterA(h, ' ', sz, {0, 0}, &written);
    FillConsoleOutputAttribute(h, 7, sz, {0, 0}, &written);
    tui_gotoxy(0, 0);
#else
    printf("\033[2J\033[H");
#endif
}

static inline void tui_clear_line(int y) {
    tui_gotoxy(0, y);
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    FillConsoleOutputCharacterA(h, ' ', tui::g_cols, {0, (SHORT)y}, &written);
    tui_gotoxy(0, y);
#else
    printf("\033[2K");
#endif
}

static inline void tui_cursor_show(bool show) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = show ? TRUE : FALSE;
    SetConsoleCursorInfo(h, &ci);
#else
    printf(show ? "\033[?25h" : "\033[?25l");
#endif
    tui::g_cursor_visible = show;
}

static inline void tui_set_raw_mode(bool enable) {
#ifdef _WIN32
    static DWORD old_mode = 0;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (enable) {
        GetConsoleMode(h, &old_mode);
        SetConsoleMode(h, 0);
    } else {
        SetConsoleMode(h, old_mode);
    }
#else
    static struct termios oldt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        struct termios newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
#endif
}

static inline int tui_getch() {
#ifdef _WIN32
    return _getch();
#else
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return (unsigned char)c;
    return -1;
#endif
}

static inline bool tui_kbhit() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
#endif
}

// ============================================================
// Log
// ============================================================
static inline void tui_log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    tui::g_log.push_back(buf);
    if (tui::g_log.size() > tui::kMaxLogLines)
        tui::g_log.pop_front();
    tui::g_dirty = true;
}

static inline void tui_log_raw(const char* msg) {
    tui::g_log.push_back(msg);
    if (tui::g_log.size() > tui::kMaxLogLines)
        tui::g_log.pop_front();
    tui::g_dirty = true;
}

// ============================================================
// HTTP server thread (extracted from handle_start)
// ============================================================
static inline void tui_http_thread_func(uint16_t port) {
#ifdef _WIN32
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        tui_log("[HTTP] socket() failed\r\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        tui_log("[HTTP] bind() failed (port %u in use?)\r\n", port);
        closesocket(server_fd);
        return;
    }
    listen(server_fd, SOMAXCONN);
    tui_log("[HTTP] Listening on port %u\n", port);
    while (g_state.node_running) {
        SOCKET client = accept(server_fd, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        serve_http_page((uintptr_t)client);
    }
    closesocket(server_fd);
    tui_log("[HTTP] Server stopped\n");
#else
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        tui_log("[HTTP] socket() failed\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        tui_log("[HTTP] bind() failed (port %u in use?)\n", port);
        close(server_fd);
        return;
    }
    listen(server_fd, SOMAXCONN);
    tui_log("[HTTP] Listening on port %u\n", port);
    while (g_state.node_running) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) break;
        serve_http_page((uintptr_t)client);
    }
    close(server_fd);
    tui_log("[HTTP] Server stopped\n");
#endif
}

// ============================================================
// Drawing helpers
// ============================================================
static inline void tui_draw_title() {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), " MyCelium v%s  |  Peer: %s  |  %s  |  Peers: %zu",
                     MYCELIUM_VERSION,
                     g_state.node ? g_state.node->local_peer_id().c_str() : "---",
                     g_state.node_running ? "Online" : "Offline",
                     g_state.node ? g_state.node->peer_count() : 0);
    if (n < 0 || n >= tui::g_cols) n = tui::g_cols - 1;
    tui_set_color(0, 7); // black on white (inverse)
    tui_clear_line(tui::kTitleLine);
    printf("%-*s", tui::g_cols - 1, buf);
    tui_reset_color();
}

static inline void tui_draw_tabs() {
    tui_clear_line(tui::kTabLine);
    const char* names[] = {"Overview", "Mine", "Console"};
    int x = 2;
    for (int i = 0; i < 3; ++i) {
        tui_gotoxy(x, tui::kTabLine);
        if (i == tui::g_active_tab) {
            tui_set_color(0, 7);
            printf(" [%d] %s ", i + 1, names[i]);
            tui_reset_color();
        } else {
            printf(" [%d] %s ", i + 1, names[i]);
        }
        x += (int)strlen(names[i]) + 6;
    }
}

static inline void tui_draw_separator(int y) {
    tui_set_color(8, 0); // dark gray
    tui_clear_line(y);
    for (int i = 0; i < tui::g_cols - 1; ++i) putchar('-');
    tui_reset_color();
}

static inline void tui_write_at(int x, int y, const char* s) {
    tui_gotoxy(x, y);
    printf("%s", s);
}

static inline void tui_write_at_color(int x, int y, int fg, int bg, const char* s) {
    tui_gotoxy(x, y);
    tui_set_color(fg, bg);
    printf("%s", s);
    tui_reset_color();
}

// ============================================================
// Tab content drawing
// ============================================================
static inline void tui_draw_overview(int y, int h) {
    int line = y;
    auto& w = g_state.wallet;
    char buf[128];

    if (w.public_key != std::array<uint8_t, 32>{}) {
        auto addr = base64_encode(w.public_key.data(), 16);
        snprintf(buf, sizeof(buf), " Wallet: MYT%s", addr.c_str());
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
        snprintf(buf, sizeof(buf), " Balance: %llu MYTUBE", (unsigned long long)w.balance);
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
        snprintf(buf, sizeof(buf), " Staked: %llu MYTUBE", (unsigned long long)w.staked_balance);
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
        snprintf(buf, sizeof(buf), " Available: %llu MYTUBE", (unsigned long long)w.available());
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
    } else {
        tui_write_at(2, line++, " No wallet. Press C to create or R to restore.");
        tui_clear_line(line++);
    }

    if (g_state.node) {
        snprintf(buf, sizeof(buf), " Peer ID: %s", g_state.node->local_peer_id().c_str());
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
        tui_write_at(2, line++, " Network: Online");
        tui_clear_line(line);
        snprintf(buf, sizeof(buf), " Peers: %zu", g_state.node->peer_count());
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
        snprintf(buf, sizeof(buf), " Mempool: %zu txs", g_state.node->mempool_size());
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
        snprintf(buf, sizeof(buf), " Chain height: %zu", g_state.node->chain.size());
        tui_write_at(2, line++, buf);
        tui_clear_line(line);
    } else {
        tui_write_at(2, line++, " Node: Not started");
        tui_clear_line(line++);
    }

    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Epoch: %llu", (unsigned long long)g_state.tokenomics.current_epoch);
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Block reward: %llu MYTUBE",
             (unsigned long long)mining_block_reward(g_state.tokenomics.current_epoch));
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Difficulty: %u bits",
             mining_difficulty_bits(g_state.tokenomics.current_epoch));
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Minted supply: %llu / %llu",
             (unsigned long long)g_state.tokenomics.minted_supply,
             (unsigned long long)g_state.tokenomics.total_supply);
    tui_write_at(2, line++, buf);

    // Clear remaining content lines
    while (line < y + h) {
        tui_clear_line(line++);
    }
}

static inline void tui_draw_mine(int y, int h) {
    int line = y;
    char buf[128];

    snprintf(buf, sizeof(buf), " Epoch: %llu", (unsigned long long)g_state.tokenomics.current_epoch);
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Block reward: %llu MYTUBE",
             (unsigned long long)mining_block_reward(g_state.tokenomics.current_epoch));
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Difficulty: %u bits",
             mining_difficulty_bits(g_state.tokenomics.current_epoch));
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Total minted: %llu / %llu",
             (unsigned long long)g_state.tokenomics.minted_supply,
             (unsigned long long)g_state.tokenomics.total_supply);
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Mempool: %zu pending txs",
             g_state.node ? g_state.node->mempool_size() : 0);
    tui_write_at(2, line++, buf);
    tui_clear_line(line);
    snprintf(buf, sizeof(buf), " Balance: %llu MYTUBE",
             (unsigned long long)g_state.wallet.balance);
    tui_write_at(2, line++, buf);
    tui_clear_line(line);

    tui_clear_line(line++);
    if (tui::g_mining) {
        tui_write_at(2, line, " Status: Mining... (Press M to stop)");
        tui_set_color(2, 0); // green
        tui_gotoxy(2, line);
        printf(" Status: Mining... (Press M to stop)");
        tui_reset_color();
    } else {
        tui_write_at(2, line, " Status: Idle (Press M to start mining)");
    }
    tui_clear_line(++line);

    while (++line < y + h) {
        tui_clear_line(line);
    }
}

static inline void tui_draw_console(int y, int h) {
    // Calculate how many lines fit
    int avail = h - 1; // leave one line for scroll hint
    int total = (int)tui::g_log.size();
    int start_idx = total - avail - tui::g_log_scroll;
    if (start_idx < 0) start_idx = 0;
    if (start_idx > total - avail) start_idx = total - avail > 0 ? total - avail : 0;

    int line = y;
    for (int i = 0; i < avail; ++i) {
        tui_clear_line(line);
        int idx = start_idx + i;
        if (idx >= 0 && idx < total) {
            tui_gotoxy(2, line);
            const std::string& msg = tui::g_log[idx];
            // Truncate to fit
            int max_w = tui::g_cols - 3;
            if ((int)msg.size() > max_w) {
                printf("%.*s", max_w, msg.c_str());
            } else {
                printf("%s", msg.c_str());
            }
        }
        ++line;
    }

    // Scroll hint
    char hint[64];
    if (total > avail) {
        snprintf(hint, sizeof(hint), " Lines: %d  (PgUp/PgDn to scroll)", total);
    } else {
        snprintf(hint, sizeof(hint), " Lines: %d", total);
    }
    tui_clear_line(line);
    tui_set_color(8, 0);
    tui_gotoxy(2, line);
    printf("%s", hint);
    tui_reset_color();
}

// ============================================================
// Action bar
// ============================================================
static inline void tui_draw_actions(int y) {
    tui_clear_line(y);
    tui_gotoxy(2, y);
    tui_set_color(0, 7); // inverse
    printf("  [C] Wallet  [R]estore  [M]ine  [S]tart  [T]op  [Q]uit  ");
    tui_reset_color();
}

static inline void tui_draw_status(int y) {
    tui_clear_line(y);
    tui_gotoxy(2, y);
    tui_set_color(8, 0); // dark gray
    char buf[128];
    if (tui::g_mining) {
        snprintf(buf, sizeof(buf), " Mining... | Balance: %llu MYTUBE | Epoch: %llu",
                 (unsigned long long)g_state.wallet.balance,
                 (unsigned long long)g_state.tokenomics.current_epoch);
    } else if (g_state.node_running) {
        snprintf(buf, sizeof(buf), " Online | Peers: %zu | Balance: %llu MYTUBE | Epoch: %llu",
                 g_state.node ? g_state.node->peer_count() : 0,
                 (unsigned long long)g_state.wallet.balance,
                 (unsigned long long)g_state.tokenomics.current_epoch);
    } else {
        snprintf(buf, sizeof(buf), " Offline | Balance: %llu MYTUBE | Press S to start",
                 (unsigned long long)g_state.wallet.balance);
    }
    printf("%-*s", tui::g_cols - 3, buf);
    tui_reset_color();
}

// ============================================================
// Full screen redraw
// ============================================================
const char* s_tui_log_title = "";
struct TuiDialog {
    std::string title;
    std::vector<std::string> lines;
    std::string input_label;
    char input_buf[256];
    int input_len = 0;
    int input_cursor = 0;
    bool input_active = false;
    bool confirm = false;
    bool done = false;
    int result = 0; // 0=cancel, 1=ok
};

static TuiDialog g_dialog;

// ============================================================
// Dialogs
// ============================================================
static inline void tui_show_dialog(const TuiDialog& d) {
    g_dialog = d;
    tui::g_dialog_showing = true;
    tui::g_dirty = true;
}

static inline void tui_close_dialog() {
    tui::g_dialog_showing = false;
    tui::g_dirty = true;
}

static inline void tui_draw_dialog() {
    // Calculate dialog dimensions
    int dlg_w = tui::g_cols - 8 < 60 ? tui::g_cols - 8 : 60;
    int dlg_h = 3 + (int)g_dialog.lines.size() + (g_dialog.input_active ? 2 : 0) + 2;
    int dlg_x = (tui::g_cols - dlg_w) / 2;
    int dlg_y = (tui::g_rows - dlg_h) / 2;
    if (dlg_y < 2) dlg_y = 2;

    // Draw semi-transparent overlay (just blank lines)
    tui_set_color(7, 0);
    for (int i = 0; i < tui::g_rows; ++i) {
        if (i >= dlg_y - 1 && i <= dlg_y + dlg_h + 1) continue;
        tui_clear_line(i);
    }

    // Draw box
    for (int i = 0; i < dlg_h + 2; ++i) {
        int yy = dlg_y + i;
        if (yy < 0 || yy >= tui::g_rows) continue;
        tui_set_color(0, 7); // inverse
        tui_gotoxy(dlg_x, yy);
        printf("%c", i == 0 ? '+' : (i == dlg_h + 1 ? '+' : '|'));
        for (int x = 1; x < dlg_w - 1; ++x)
            printf("%c", i == 0 || i == dlg_h + 1 ? '-' : ' ');
        printf("%c", i == 0 ? '+' : (i == dlg_h + 1 ? '+' : '|'));
        tui_reset_color();
    }

    // Title
    tui_set_color(0, 7);
    tui_gotoxy(dlg_x + 2, dlg_y + 1);
    printf(" %s ", g_dialog.title.c_str());
    tui_reset_color();

    // Content lines
    for (size_t i = 0; i < g_dialog.lines.size(); ++i) {
        int yy = dlg_y + 2 + (int)i;
        if (yy >= tui::g_rows) break;
        tui_gotoxy(dlg_x + 2, yy);
        printf("%-*s", dlg_w - 4, g_dialog.lines[i].c_str());
    }

    // Input field
    if (g_dialog.input_active) {
        int yy = dlg_y + 2 + (int)g_dialog.lines.size();
        tui_gotoxy(dlg_x + 2, yy);
        printf("%s: ", g_dialog.input_label.c_str());
        tui_gotoxy(dlg_x + 2 + (int)g_dialog.input_label.size() + 2, yy);
        // Show input text (masked with *)
        std::string masked(g_dialog.input_len, '*');
        printf("%-*s", dlg_w - 4 - (int)g_dialog.input_label.size() - 2, masked.c_str());
    }

    // Bottom buttons
    tui_set_color(0, 7);
    tui_gotoxy(dlg_x + 2, dlg_y + dlg_h);
    printf(" [Enter] OK   [Esc] Cancel ");
    tui_reset_color();
}

// ============================================================
// Wallet dialogs
// ============================================================
static inline void tui_dialog_create_wallet() {
    // Passphrase input
    {
        TuiDialog d;
        d.title = "Create Wallet";
        d.lines.push_back("Enter a passphrase (>=8 chars, 1 digit, 1 special):");
        d.input_label = "Passphrase";
        d.input_active = true;
        d.input_len = 0;
        memset(d.input_buf, 0, sizeof(d.input_buf));
        tui_show_dialog(d);
    }

    // Wait for dialog input
    while (tui::g_dialog_showing) {
        if (tui_kbhit()) {
            int ch = tui_getch();
            if (ch == 27) { // Escape
                tui_close_dialog();
                tui_log("[TUI] Wallet creation canceled.\n");
                return;
            } else if (ch == 13) { // Enter
                g_dialog.input_buf[g_dialog.input_len] = 0;
                std::string pass = g_dialog.input_buf;

                // Validate passphrase
                if (!validate_passphrase(pass.c_str())) {
                    g_dialog.lines.clear();
                    g_dialog.lines.push_back("Passphrase must be >=8 chars with");
                    g_dialog.lines.push_back("at least 1 digit and 1 special char.");
                    g_dialog.input_len = 0;
                    memset(g_dialog.input_buf, 0, sizeof(g_dialog.input_buf));
                } else {
                    // Confirm
                    TuiDialog d2;
                    d2.title = "Confirm Passphrase";
                    d2.lines.push_back("Re-enter passphrase:");
                    d2.input_label = "Confirm";
                    d2.input_active = true;
                    d2.input_len = 0;
                    memset(d2.input_buf, 0, sizeof(d2.input_buf));
                    tui_show_dialog(d2);

                    bool confirmed = false;
                    while (tui::g_dialog_showing) {
                        if (tui_kbhit()) {
                            int ch2 = tui_getch();
                            if (ch2 == 27) {
                                tui_close_dialog();
                                tui_log("[TUI] Wallet creation canceled.\n");
                                return;
                            } else if (ch2 == 13) {
                                g_dialog.input_buf[g_dialog.input_len] = 0;
                                if (pass == g_dialog.input_buf) {
                                    confirmed = true;
                                    tui_close_dialog();
                                } else {
                                    g_dialog.lines.clear();
                                    g_dialog.lines.push_back("Passphrases do not match.");
                                    g_dialog.input_len = 0;
                                    memset(g_dialog.input_buf, 0, sizeof(g_dialog.input_buf));
                                }
                            } else if (ch2 == 8 || ch2 == 127) {
                                if (g_dialog.input_len > 0) --g_dialog.input_len;
                            } else if (ch2 >= 32 && ch2 < 127 && g_dialog.input_len < 255) {
                                g_dialog.input_buf[g_dialog.input_len++] = (char)ch2;
                            }
                        }
                        tui_draw_dialog();
                        tui_gotoxy(0, tui::g_rows - 1);
                        fflush(stdout);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }

                    if (!confirmed) return;

                    // Create wallet
                    std::vector<std::string> words;
                    std::array<uint8_t, 64> seed;
                    std::array<uint8_t, 32> priv, pub;
                    if (!wallet_create_full(words, seed, priv, pub, pass.c_str())) {
                        tui_log("[TUI] Failed to create wallet.\n");
                        return;
                    }

                    std::string wallet_path = "./data/chain/wallet.dat";
                    wallet_save(wallet_path, priv, words, pass.c_str());

                    g_state.wallet = Wallet{};
                    g_state.wallet.public_key = pub;
                    g_state.private_key = priv;

                    auto addr = base64_encode(pub.data(), 16);
                    tui_log("[TUI] Wallet created: MYT%s\n", addr.c_str());

                    // Show mnemonic dialog
                    TuiDialog md;
                    md.title = "Wallet Mnemonic (24 words)";
                    // Build columns
                    std::string col1, col2, col3;
                    for (int i = 0; i < 24; ++i) {
                        char num[4];
                        snprintf(num, sizeof(num), "%2d.", i + 1);
                        std::string entry = std::string(num) + " " + words[i];
                        if (i < 8) {
                            if (!col1.empty()) col1 += "  ";
                            col1 += entry;
                        } else if (i < 16) {
                            if (!col2.empty()) col2 += "  ";
                            col2 += entry;
                        } else {
                            if (!col3.empty()) col3 += "  ";
                            col3 += entry;
                        }
                    }
                    md.lines.push_back("");
                    md.lines.push_back("  " + col1);
                    md.lines.push_back("  " + col2);
                    md.lines.push_back("  " + col3);
                    md.lines.push_back("");
                    md.lines.push_back("  Store these words safely!");
                    md.lines.push_back("  They are needed to restore your wallet.");
                    md.input_active = false;
                    md.confirm = true;
                    tui_show_dialog(md);

                    // Wait for user to press Enter
                    while (tui::g_dialog_showing) {
                        if (tui_kbhit()) {
                            int ch3 = tui_getch();
                            if (ch3 == 13 || ch3 == 27) {
                                tui_close_dialog();
                            }
                        }
                        tui_draw_dialog();
                        tui_gotoxy(0, tui::g_rows - 1);
                        fflush(stdout);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }

                    memset(pass.data(), 0, pass.size());
                    tui_log("[TUI] Wallet ready.\n");
                    return;
                }
            } else if (ch == 8 || ch == 127) {
                if (g_dialog.input_len > 0) --g_dialog.input_len;
            } else if (ch >= 32 && ch < 127 && g_dialog.input_len < 255) {
                g_dialog.input_buf[g_dialog.input_len++] = (char)ch;
            }
        }
        tui_draw_dialog();
        tui_gotoxy(0, tui::g_rows - 1);
        fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static inline void tui_dialog_restore_wallet() {
    TuiDialog d;
    d.title = "Restore Wallet";
    d.lines.push_back("Enter 24 mnemonic words separated by spaces:");
    d.lines.push_back("");
    d.lines.push_back("Then enter your passphrase when prompted.");
    d.input_label = "Words";
    d.input_active = true;
    d.input_len = 0;
    memset(d.input_buf, 0, sizeof(d.input_buf));
    tui_show_dialog(d);

    std::vector<std::string> words;
    bool got_words = false;

    while (tui::g_dialog_showing) {
        if (tui_kbhit()) {
            int ch = tui_getch();
            if (ch == 27) {
                tui_close_dialog();
                tui_log("[TUI] Wallet restore canceled.\n");
                return;
            } else if (ch == 13) {
                g_dialog.input_buf[g_dialog.input_len] = 0;

                if (!got_words) {
                    // Parse words
                    words.clear();
                    char* p = g_dialog.input_buf;
                    while (*p) {
                        while (*p == ' ') ++p;
                        if (!*p) break;
                        char* start = p;
                        while (*p && *p != ' ') ++p;
                        words.push_back(std::string(start, p));
                    }

                    if (words.size() != 24) {
                        g_dialog.lines.clear();
                        g_dialog.lines.push_back("Need exactly 24 words (got " +
                            std::to_string(words.size()) + ").");
                        g_dialog.lines.push_back("Try again:");
                        g_dialog.input_len = 0;
                        memset(g_dialog.input_buf, 0, sizeof(g_dialog.input_buf));
                        tui::g_dirty = true;
                        continue;
                    }

                    // Validate checksum
                    std::array<uint8_t, 32> entropy;
                    if (!mnemonic_restore(words, entropy)) {
                        g_dialog.lines.clear();
                        g_dialog.lines.push_back("Invalid mnemonic - checksum mismatch.");
                        g_dialog.lines.push_back("Try again:");
                        g_dialog.input_len = 0;
                        memset(g_dialog.input_buf, 0, sizeof(g_dialog.input_buf));
                        tui::g_dirty = true;
                        continue;
                    }

                    got_words = true;
                    g_dialog.lines.clear();
                    g_dialog.lines.push_back("Enter passphrase:");
                    g_dialog.input_label = "Passphrase";
                    g_dialog.input_len = 0;
                    memset(g_dialog.input_buf, 0, sizeof(g_dialog.input_buf));
                    tui::g_dirty = true;
                } else {
                    // Got passphrase
                    g_dialog.input_buf[g_dialog.input_len] = 0;
                    std::string pass = g_dialog.input_buf;

                    // Derive keypair
                    std::array<uint8_t, 64> seed;
                    mnemonic_to_seed(words, pass.c_str(), seed.data());
                    auto hash = sha512(seed.data(), 64);
                    std::array<uint8_t, 32> priv;
                    memcpy(priv.data(), hash.data(), 32);
                    priv[0] &= 248; priv[31] &= 127; priv[31] |= 64;
                    auto pub = ed25519_pubkey(priv);

                    std::string wallet_path = "./data/chain/wallet.dat";
                    wallet_save(wallet_path, priv, words, pass.c_str());

                    g_state.wallet = Wallet{};
                    g_state.wallet.public_key = pub;
                    g_state.private_key = priv;

                    auto addr = base64_encode(pub.data(), 16);
                    tui_log("[TUI] Wallet restored: MYT%s\n", addr.c_str());
                    memset(pass.data(), 0, pass.size());
                    tui_close_dialog();
                    tui_log("[TUI] Wallet ready.\n");
                    return;
                }
            } else if (ch == 8 || ch == 127) {
                if (g_dialog.input_len > 0) --g_dialog.input_len;
            } else if (ch >= 32 && ch < 127 && g_dialog.input_len < 255) {
                g_dialog.input_buf[g_dialog.input_len++] = (char)ch;
            }
        }
        tui_draw_dialog();
        tui_gotoxy(0, tui::g_rows - 1);
        fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// ============================================================
// Input processing
// ============================================================
static inline void tui_handle_key(int ch) {
    if (tui::g_dialog_showing) return; // handled in dialog function

    switch (ch) {
        case '1': tui::g_active_tab = 0; tui::g_log_scroll = 0; tui::g_dirty = true; break;
        case '2': tui::g_active_tab = 1; tui::g_log_scroll = 0; tui::g_dirty = true; break;
        case '3': tui::g_active_tab = 2; tui::g_log_scroll = 0; tui::g_dirty = true; break;
        case '\t': {
            tui::g_active_tab = (tui::g_active_tab + 1) % 3;
            tui::g_log_scroll = 0;
            tui::g_dirty = true;
            break;
        }
        case 'c': case 'C':
            tui_dialog_create_wallet();
            break;
        case 'r': case 'R':
            tui_dialog_restore_wallet();
            break;
        case 'm': case 'M': {
            if (tui::g_mining) {
                tui::g_mining = false;
                tui_log("[TUI] Mining stopped by user.\n");
            } else {
                if (g_state.private_key == std::array<uint8_t, 32>{}) {
                    tui_log("[TUI] No wallet. Create or restore one first.\n");
                } else if (!g_state.node) {
                    tui_log("[TUI] No node running. Press S to start first.\n");
                } else {
                    tui::g_mining = true;
                    tui_log("[TUI] Mining started...\n");
                }
            }
            tui::g_dirty = true;
            break;
        }
        case 's': case 'S': {
            if (g_state.node_running) {
                tui_log("[TUI] Node already running.\n");
                break;
            }
            if (g_state.node) { delete g_state.node; g_state.node = nullptr; }
            {
                P2pConfig p2p = g_state.config.to_p2p_config();
                g_state.node = new MyceliumNode(MyceliumNode::create(p2p));
                g_state.node->init_storage(p2p.storage);
                g_state.node->load_chain(g_state.tokenomics, g_state.wallet);
                g_state.node_running = true;
            }
            tui_log("[TUI] Node started. Peer ID: %s\n",
                    g_state.node->local_peer_id().c_str());
            tui::g_dirty = true;
            break;
        }
        case 't': case 'T': {
            if (!g_state.node_running) {
                tui_log("[TUI] Node not running.\n");
                break;
            }
            g_state.node_running = false;
            tui::g_http_thread_running = false;
            if (tui::g_http_thread.joinable())
                tui::g_http_thread.join();
            if (g_state.node) { delete g_state.node; g_state.node = nullptr; }
            tui_log("[TUI] Node stopped.\n");
            tui::g_dirty = true;
            break;
        }
        case 'q': case 'Q':
        case 27: // Escape
            if (!tui::g_dialog_showing) {
                tui_log("[TUI] Shutting down...\n");
                tui::g_running = false;
            }
            break;
        case 'w': case 'W': {
            // Open web UI URL
            tui_log("[TUI] Web UI: start node with --http-port for web interface.\n");
            break;
        }
        // Console scrolling
        case 0x21: // PgUp
            if (tui::g_active_tab == 2) {
                int avail = tui::g_rows - tui::kContentStart - 4;
                tui::g_log_scroll += avail;
                tui::g_dirty = true;
            }
            break;
        case 0x22: // PgDn
            if (tui::g_active_tab == 2) {
                tui::g_log_scroll -= tui::g_rows - tui::kContentStart - 4;
                if (tui::g_log_scroll < 0) tui::g_log_scroll = 0;
                tui::g_dirty = true;
            }
            break;
    }
}

// ============================================================
// Mining background work (called in event loop when g_mining)
// ============================================================
static inline void tui_do_mining() {
    if (!tui::g_mining) return;
    if (!g_state.node || !g_state.node_running) {
        tui::g_mining = false;
        tui_log("[TUI] Mining stopped: node not running.\n");
        return;
    }

    uint64_t epoch = g_state.tokenomics.current_epoch;
    uint64_t reward = mining_block_reward(epoch);
    uint32_t diff = mining_difficulty_bits(epoch);
    size_t mp_sz = g_state.node->mempool_size();

    tui_log("[MINE] Searching epoch %llu (diff %u bits, reward %llu, mempool %zu txs)...\n",
            (unsigned long long)epoch, diff, (unsigned long long)reward, mp_sz);

    bool found = g_state.node->mine_block(
        g_state.tokenomics, g_state.wallet,
        g_state.private_key, g_state.config.mining_max_nonce);

    if (found) {
        tui_log("[MINE] Block mined! Height: %zu, Reward: %llu\n",
                g_state.node->chain.size() - 1, (unsigned long long)reward);
    } else {
        tui_log("[MINE] No block found in this attempt.\n");
    }
    tui::g_dirty = true;
}

// ============================================================
// Main screen draw
// ============================================================
static inline void tui_draw_screen() {
    int content_h = tui::g_rows - 6; // title(1) + tab(1) + sep(1) + actions(1) + status(1) + 1 gap

    tui_draw_title();
    tui_draw_tabs();
    tui_draw_separator(tui::kSepLine);

    switch (tui::g_active_tab) {
        case 0: tui_draw_overview(tui::kContentStart, content_h); break;
        case 1: tui_draw_mine(tui::kContentStart, content_h); break;
        case 2: tui_draw_console(tui::kContentStart, content_h); break;
    }

    tui_draw_separator(tui::g_rows - 3);
    tui_draw_actions(tui::g_rows - 2);
    tui_draw_status(tui::g_rows - 1);

    if (tui::g_dialog_showing) {
        tui_draw_dialog();
    }
}

// ============================================================
// Main event loop
// ============================================================
static inline int tui_run(int http_port) {
    // Init terminal
    tui_get_term_size(tui::g_cols, tui::g_rows);
    if (tui::g_cols < 60) tui::g_cols = 60;
    if (tui::g_rows < 16) tui::g_rows = 16;

    tui_set_raw_mode(true);
    tui_cursor_show(false);
    tui_clear_screen();

    tui_log("[TUI] Terminal UI started. Terminal: %dx%d\n", tui::g_cols, tui::g_rows);
    tui_log("[TUI] Press 1-2-3 or Tab to switch tabs.\n");
    tui_log("[TUI] C=Create Wallet, R=Restore, M=Mine, S=Start, T=Stop, Q=Quit\n");

    tui::g_running = true;

    // If http_port was provided, start the node + HTTP server immediately
    if (http_port > 0) {
        if (g_state.node) { delete g_state.node; g_state.node = nullptr; }
        P2pConfig p2p = g_state.config.to_p2p_config();
        g_state.node = new MyceliumNode(MyceliumNode::create(p2p));
        g_state.node->init_storage(p2p.storage);
        g_state.node->load_chain(g_state.tokenomics, g_state.wallet);
        g_state.node_running = true;
        tui_log("[TUI] Node started. Peer ID: %s\n",
                g_state.node->local_peer_id().c_str());

        tui::g_http_thread = std::thread(tui_http_thread_func, (uint16_t)http_port);
        tui::g_http_thread_running = true;
    }

    // Event loop
    while (tui::g_running) {
        // Handle input
        while (tui_kbhit()) {
            int ch = tui_getch();
            if (ch == 0 || ch == 0xE0) {
                // Extended key
                ch = tui_getch();
                if (ch == 0x49) ch = 0x21; // PgUp
                else if (ch == 0x51) ch = 0x22; // PgDn
                else continue;
            }
            tui_handle_key(ch);
        }

        // Mining work (one attempt per cycle)
        tui_do_mining();

        // Periodic refresh (every 200ms)
        int64_t now = ProtocolMessage{}.now_sec() * 1000;
        if (tui::g_dirty || now - tui::g_last_refresh > 200) {
            tui_get_term_size(tui::g_cols, tui::g_rows);
            tui_draw_screen();
            tui_gotoxy(0, tui::g_rows - 1);
            fflush(stdout);
            tui::g_last_refresh = now;
            tui::g_dirty = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms sleep to prevent busy-waiting
    }

    // Cleanup
    tui::g_mining = false;
    if (g_state.node_running) {
        g_state.node_running = false;
        tui::g_http_thread_running = false;
        if (tui::g_http_thread.joinable())
            tui::g_http_thread.join();
        if (g_state.node) { delete g_state.node; g_state.node = nullptr; }
    }

    tui_cursor_show(true);
    tui_set_raw_mode(false);
    tui_clear_screen();
    tui_reset_color();
    printf("Terminal UI closed.\n");
    return 0;
}
