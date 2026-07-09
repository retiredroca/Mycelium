#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <shellapi.h>
#pragma comment(lib, "ws2_32")
#endif

enum {
    IDC_HTTP_PORT  = 1001,
    IDC_LISTEN     = 1002,
    IDC_TOR        = 1003,
    IDC_SOCKS_PORT = 1004,
    IDC_START      = 1005,
    IDC_STOP       = 1006,
    IDC_CREATE_WALLET = 1007,
    IDC_MINE       = 1008,
    IDC_OPEN_WEB   = 1009,
    IDC_STATUS     = 1010,
    IDC_LOG        = 1011,
    IDC_BALANCE_TEXT = 1012,
};

struct GuiData {
    HWND hwnd  = nullptr;
    HWND hwnd_http_port  = nullptr;
    HWND hwnd_listen     = nullptr;
    HWND hwnd_tor        = nullptr;
    HWND hwnd_socks_port = nullptr;
    HWND hwnd_start      = nullptr;
    HWND hwnd_stop       = nullptr;
    HWND hwnd_balance    = nullptr;
    HWND hwnd_status     = nullptr;
    HWND hwnd_log        = nullptr;
    std::thread http_thread;
    std::thread mine_thread;
    std::atomic<bool> mining_active{false};
    bool node_started = false;
};

static GuiData g_gui;

static inline void gui_log(const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HWND h = g_gui.hwnd_log;
    if (!h) return;
    int len = GetWindowTextLength(h);
    SendMessage(h, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(h, EM_REPLACESEL, 0, (LPARAM)buf);
}

static inline void gui_update_status() {
    if (!g_gui.hwnd_status) return;
    char buf[4096];
    int n = 0;
    n += snprintf(buf + n, sizeof(buf) - n,
        "Peer ID:   %s\r\n"
        "Network:   %s\r\n"
        "Peers:     %zu\r\n"
        "Tor:       %s\r\n"
        "Balance:   %llu MYTUBE\r\n"
        "Minted:    %llu / 1B\r\n"
        "Epoch:     %llu\r\n"
        "Reward:    %llu MYTUBE\r\n"
        "Difficulty: %u bits\r\n"
        "Channel:   %s\r\n"
        "Videos:    %s\r\n",
        g_state.node ? g_state.node->local_peer_id().c_str() : "—",
        g_state.node_running ? "Online" : "Offline",
        g_state.node ? g_state.node->peer_count() : (size_t)0,
        (g_state.node && !g_state.node->local_info.onion_address.empty()) ? "Enabled" : "Disabled",
        (unsigned long long)g_state.wallet.balance,
        (unsigned long long)g_state.tokenomics.minted_supply,
        (unsigned long long)g_state.tokenomics.current_epoch,
        (unsigned long long)mining_block_reward(g_state.tokenomics.current_epoch),
        mining_difficulty_bits(g_state.tokenomics.current_epoch),
        g_state.my_profile.peer_id.empty() ? "None" : "Created",
        g_state.current_video.video_id.empty() ? "0" : "1 uploaded"
    );
    if (g_state.node && !g_state.node->local_info.onion_address.empty()) {
        n += snprintf(buf + n, sizeof(buf) - n,
            "Onion:     %s\r\n",
            g_state.node->local_info.onion_address.c_str());
    }
    SetWindowText(g_gui.hwnd_status, buf);
}

static inline void gui_http_server_thread(uint16_t port) {
#ifdef _WIN32
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        gui_log("[HTTP] socket() failed\r\n");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        gui_log("[HTTP] bind() failed (port %u in use?)\r\n", port);
        closesocket(server_fd);
        return;
    }
    listen(server_fd, SOMAXCONN);
    gui_log("[HTTP] Listening on port %u\r\n", port);
    while (g_state.node_running) {
        SOCKET client = accept(server_fd, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        serve_http_page((uintptr_t)client);
    }
    closesocket(server_fd);
    gui_log("[HTTP] Server stopped\r\n");
#endif
}

static inline void gui_mine_thread() {
    if (g_state.wallet.public_key == std::array<uint8_t, 32>{}) {
        std::array<uint8_t, 32> pk;
        random_bytes(pk.data(), 32);
        g_state.wallet = Wallet{};
        memcpy(g_state.wallet.public_key.data(), pk.data(), 32);
    }
    uint64_t epoch = g_state.tokenomics.current_epoch;
    uint64_t reward = mining_block_reward(epoch);
    uint32_t diff = mining_difficulty_bits(epoch);
    gui_log("[MINE] Searching epoch %llu (diff %u bits, reward %llu MYTUBE)...\r\n",
            (unsigned long long)epoch, diff, (unsigned long long)reward);
    uint64_t nonce = mining_search(g_state.wallet.public_key, epoch, kMiningMaxNoncePerAttempt);
    if (nonce == UINT64_MAX) {
        gui_log("[MINE] No block found in %llu attempts\r\n",
                (unsigned long long)kMiningMaxNoncePerAttempt);
    } else if (mint_to_wallet(g_state.tokenomics, g_state.wallet, reward)) {
        g_state.tokenomics.apply_disinflation();
        gui_log("[MINE] \xC2\xB0 Block found! Nonce %llu, +%llu MYTUBE (balance: %llu)\r\n",
                (unsigned long long)nonce, (unsigned long long)reward,
                (unsigned long long)g_state.wallet.balance);
    }
    g_gui.mining_active = false;
    PostMessage(g_gui.hwnd, WM_TIMER, 0, 0);
}

static LRESULT CALLBACK gui_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        HDC dc = GetDC(hwnd);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(hwnd, dc);
        int font_size = -MulDiv(9, dpi, 72);
        HFONT font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT font_bold = CreateFont(font_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        auto mkctrl = [&](const char* cls, const char* text, DWORD style, int x, int y, int w, int h, int id) -> HWND {
            return CreateWindowEx(0, cls, text, style | WS_CHILD | WS_VISIBLE,
                x, y, w, h, hwnd, (HMENU)(INT_PTR)id, GetModuleHandle(0), 0);
        };

        const int G = 10, L = 210, R = L + 10, GW = 220;

        // === LEFT PANEL: Configuration ===
        mkctrl("STATIC", "MyTube Protocol v0.1.0", SS_CENTER | SS_CENTERIMAGE, 0, 0, 660, 28, 0);
        HWND h = mkctrl("STATIC", "", WS_GROUP | BS_GROUPBOX, G, 32, GW, 158, 0);
        SendMessage(h, WM_SETFONT, (WPARAM)font_bold, 0);
        mkctrl("STATIC", "Configuration", WS_CHILD | WS_VISIBLE, G + 8, 32, 100, 14, 0);

        mkctrl("STATIC", "HTTP Port:", WS_CHILD | WS_VISIBLE, G + 12, 52, 70, 20, 0);
        g_gui.hwnd_http_port = mkctrl("EDIT", "8080", WS_BORDER | ES_NUMBER, G + 80, 50, 60, 22, IDC_HTTP_PORT);

        mkctrl("STATIC", "Listen:", WS_CHILD | WS_VISIBLE, G + 12, 78, 50, 20, 0);
        g_gui.hwnd_listen = mkctrl("EDIT", "/ip4/0.0.0.0/tcp/0", WS_BORDER, G + 60, 76, 150, 22, IDC_LISTEN);

        g_gui.hwnd_tor = mkctrl("BUTTON", "Tor", BS_AUTOCHECKBOX | WS_TABSTOP, G + 12, 104, 50, 22, IDC_TOR);

        mkctrl("STATIC", "SOCKS Port:", WS_CHILD | WS_VISIBLE, G + 70, 106, 70, 20, 0);
        g_gui.hwnd_socks_port = mkctrl("EDIT", "9050", WS_BORDER | ES_NUMBER, G + 140, 104, 60, 22, IDC_SOCKS_PORT);

        g_gui.hwnd_start = mkctrl("BUTTON", "START NODE", WS_TABSTOP | BS_PUSHBUTTON,
            G + 12, 136, 95, 30, IDC_START);
        g_gui.hwnd_stop = mkctrl("BUTTON", "STOP NODE", WS_TABSTOP | BS_PUSHBUTTON,
            G + 113, 136, 95, 30, IDC_STOP);
        EnableWindow(g_gui.hwnd_stop, FALSE);

        SendMessage(g_gui.hwnd_http_port, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(g_gui.hwnd_listen, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(g_gui.hwnd_tor, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(g_gui.hwnd_socks_port, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(g_gui.hwnd_start, WM_SETFONT, (WPARAM)font, 0);
        SendMessage(g_gui.hwnd_stop, WM_SETFONT, (WPARAM)font, 0);

        // === RIGHT PANEL: Status ===
        h = mkctrl("STATIC", "", WS_GROUP | BS_GROUPBOX, R, 32, 430, 158, 0);
        SendMessage(h, WM_SETFONT, (WPARAM)font_bold, 0);
        mkctrl("STATIC", "Status", WS_CHILD | WS_VISIBLE, R + 8, 32, 60, 14, 0);

        g_gui.hwnd_status = mkctrl("EDIT", "", WS_BORDER | ES_MULTILINE | ES_READONLY,
            R + 10, 50, 410, 130, IDC_STATUS);
        SendMessage(g_gui.hwnd_status, WM_SETFONT, (WPARAM)font, 0);

        // === BOTTOM ROW: Wallet + Actions ===
        int BY = 200;

        mkctrl("STATIC", "", WS_GROUP | BS_GROUPBOX, G, BY, 640, 48, 0);
        mkctrl("STATIC", "Wallet", WS_CHILD | WS_VISIBLE, G + 8, BY, 50, 14, 0);

        HWND bw = mkctrl("BUTTON", "Create Wallet", WS_TABSTOP | BS_PUSHBUTTON,
            G + 10, BY + 18, 100, 24, IDC_CREATE_WALLET);
        SendMessage(bw, WM_SETFONT, (WPARAM)font, 0);

        bw = mkctrl("BUTTON", "Mine", WS_TABSTOP | BS_PUSHBUTTON,
            G + 115, BY + 18, 70, 24, IDC_MINE);
        SendMessage(bw, WM_SETFONT, (WPARAM)font, 0);

        g_gui.hwnd_balance = mkctrl("STATIC", "Balance: 0 MYTUBE", SS_CENTERIMAGE,
            G + 195, BY + 20, 200, 22, IDC_BALANCE_TEXT);
        SendMessage(g_gui.hwnd_balance, WM_SETFONT, (WPARAM)font_bold, 0);

        bw = mkctrl("BUTTON", "Open Web UI", WS_TABSTOP | BS_PUSHBUTTON,
            540, BY + 18, 95, 24, IDC_OPEN_WEB);
        SendMessage(bw, WM_SETFONT, (WPARAM)font, 0);

        // === LOG ===
        h = mkctrl("STATIC", "", WS_GROUP | BS_GROUPBOX, G, BY + 52, 640, 248, 0);
        SendMessage(h, WM_SETFONT, (WPARAM)font_bold, 0);
        mkctrl("STATIC", "Log", WS_CHILD | WS_VISIBLE, G + 8, BY + 52, 40, 14, 0);

        g_gui.hwnd_log = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            G + 8, BY + 70, 624, 225, hwnd, (HMENU)(INT_PTR)IDC_LOG, GetModuleHandle(0), 0);
        SendMessage(g_gui.hwnd_log, WM_SETFONT, (WPARAM)font, 0);

        gui_log("=== MyTube Protocol v0.1.0 ===\r\n");
        gui_log("GUI started. Configure and press START NODE.\r\n");

        SetTimer(hwnd, 1, 1000, 0);
        break;
    }
    case WM_TIMER: {
        gui_update_status();
        char bal[64];
        snprintf(bal, sizeof(bal), "Balance: %llu MYTUBE",
                 (unsigned long long)g_state.wallet.balance);
        SetWindowText(g_gui.hwnd_balance, bal);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == IDC_START) {
            if (g_state.node_running) break;

            char buf[256];
            GetWindowText(g_gui.hwnd_http_port, buf, sizeof(buf));
            uint16_t http_port = (uint16_t)atoi(buf);

            GetWindowText(g_gui.hwnd_listen, buf, sizeof(buf));
            std::string listen_addr = buf;
            bool enable_tor = (SendMessage(g_gui.hwnd_tor, BM_GETCHECK, 0, 0) == BST_CHECKED);
            uint16_t socks_port = 9050;
            GetWindowText(g_gui.hwnd_socks_port, buf, sizeof(buf));
            socks_port = (uint16_t)atoi(buf);

            P2pConfig cfg;
            cfg.listen_addresses.push_back(listen_addr.empty() ? "/ip4/0.0.0.0/tcp/0" : listen_addr.c_str());
            cfg.capabilities.push_back({kCapFull});
            cfg.enable_tor = enable_tor;
            cfg.tor_socks_port = socks_port;
            cfg.tor_control_port = 9051;

            g_state.node = new MyceliumNode(MyceliumNode::create(cfg));
            g_state.node_running = true;
            g_gui.node_started = true;

            EnableWindow(g_gui.hwnd_start, FALSE);
            EnableWindow(g_gui.hwnd_stop, TRUE);
            gui_log("[NODE] Started (peer: %s)\r\n", g_state.node->local_peer_id().c_str());
            if (enable_tor && !g_state.node->local_info.onion_address.empty()) {
                gui_log("[NODE] Tor enabled, onion: %s\r\n",
                        g_state.node->local_info.onion_address.c_str());
            }

            if (http_port > 0) {
                gui_log("[HTTP] Starting server on port %u...\r\n", http_port);
                g_gui.http_thread = std::thread(gui_http_server_thread, http_port);
                g_gui.http_thread.detach();
            }
            gui_update_status();

        } else if (id == IDC_STOP) {
            if (!g_state.node_running) break;
            g_state.node_running = false;
            if (g_state.node) {
                delete g_state.node;
                g_state.node = nullptr;
            }
            g_gui.node_started = false;
            EnableWindow(g_gui.hwnd_start, TRUE);
            EnableWindow(g_gui.hwnd_stop, FALSE);
            gui_log("[NODE] Stopped\r\n");
            gui_update_status();

        } else if (id == IDC_CREATE_WALLET) {
            std::array<uint8_t, 32> pk;
            random_bytes(pk.data(), 32);
            g_state.wallet = Wallet{};
            memcpy(g_state.wallet.public_key.data(), pk.data(), 32);
            auto addr = base64_encode(pk.data(), 16);
            gui_log("[WALLET] Created: MYT%s\r\n", addr.c_str());
            gui_update_status();

        } else if (id == IDC_MINE) {
            if (g_gui.mining_active) {
                gui_log("[MINE] Already mining...\r\n");
                break;
            }
            g_gui.mining_active = true;
            gui_log("[MINE] Starting...\r\n");
            g_gui.mine_thread = std::thread(gui_mine_thread);
            g_gui.mine_thread.detach();

        } else if (id == IDC_OPEN_WEB) {
            char buf[256];
            GetWindowText(g_gui.hwnd_http_port, buf, sizeof(buf));
            std::string url = "http://localhost:";
            url += buf;
            ShellExecute(0, "open", url.c_str(), 0, 0, SW_SHOWNORMAL);
        }
        break;
    }
    case WM_CLOSE: {
        if (g_state.node_running) {
            g_state.node_running = false;
            if (g_state.node) {
                delete g_state.node;
                g_state.node = nullptr;
            }
        }
        DestroyWindow(hwnd);
        break;
    }
    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hwnd, msg, w, l);
    }
    return 0;
}

static inline int gui_run() {
    HINSTANCE hinst = GetModuleHandle(0);

    WNDCLASS wc = {};
    wc.lpfnWndProc = gui_wndproc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MyTubeGuiWnd";
    if (!RegisterClass(&wc)) return 1;

    RECT r = {0, 0, 660, 500};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);

    g_gui.hwnd = CreateWindowEx(0, "MyTubeGuiWnd", "MyTube Protocol v0.1.0",
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME),
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        0, 0, hinst, 0);

    if (!g_gui.hwnd) return 1;

    ShowWindow(g_gui.hwnd, SW_SHOW);
    UpdateWindow(g_gui.hwnd);

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
