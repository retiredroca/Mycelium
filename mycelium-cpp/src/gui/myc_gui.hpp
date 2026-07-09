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
#include <commctrl.h>
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "comctl32")
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
    IDC_TAB_OVERVIEW = 1013,
    IDC_TAB_MINE     = 1014,
    IDC_TAB_CONSOLE  = 1015,
    IDC_ACTIVITY_LIST  = 1016,
    IDC_HEADER_TEXT    = 1017,
    IDC_BALANCE_MAIN   = 1018,
    IDC_BALANCE_SUB    = 1019,
    IDC_MINE_INFO      = 1020,
    IDC_MINE_STATUS    = 1021,
};

struct GuiData {
    HWND hwnd  = nullptr;
    int current_tab = 0;
    HWND hpanels[3] = {};

    HWND hwnd_http_port  = nullptr;
    HWND hwnd_listen     = nullptr;
    HWND hwnd_tor        = nullptr;
    HWND hwnd_socks_port = nullptr;
    HWND hwnd_start      = nullptr;
    HWND hwnd_stop       = nullptr;
    HWND hwnd_balance    = nullptr;
    HWND hwnd_status     = nullptr;
    HWND hwnd_log        = nullptr;
    HWND hactivity_list  = nullptr;
    HWND hbalance_main   = nullptr;
    HWND hbalance_sub    = nullptr;
    HWND hmine_info      = nullptr;
    HWND hmine_status    = nullptr;
    HWND hstatusbar      = nullptr;

    std::thread http_thread;
    std::thread mine_thread;
    std::atomic<bool> mining_active{false};
    bool node_started = false;
};

static GuiData g_gui;

static HFONT g_font      = nullptr;
static HFONT g_font_bold = nullptr;
static HFONT g_font_lg   = nullptr;
static HFONT g_font_md   = nullptr;

// Panel subclass: forwards WM_CTLCOLOR* to the main window
static LRESULT CALLBACK gui_panel_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLOREDIT ||
        msg == WM_CTLCOLORBTN || msg == WM_CTLCOLORLISTBOX) {
        return SendMessage(GetParent(hwnd), msg, w, l);
    }
    return DefWindowProc(hwnd, msg, w, l);
}

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
    SendMessage(h, EM_SCROLLCARET, 0, 0);
}

static inline void gui_add_activity(const char* time, const char* event,
                                     const char* amount, const char* status) {
    if (!g_gui.hactivity_list) return;
    LVITEM item = {};
    item.mask = LVIF_TEXT;
    item.iItem = 0;
    item.pszText = (char*)time;
    ListView_InsertItem(g_gui.hactivity_list, &item);
    ListView_SetItemText(g_gui.hactivity_list, 0, 1, (char*)event);
    ListView_SetItemText(g_gui.hactivity_list, 0, 2, (char*)amount);
    ListView_SetItemText(g_gui.hactivity_list, 0, 3, (char*)status);
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
        g_state.node ? g_state.node->local_peer_id().c_str() : "\xE2\x80\x94",
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

    char bal[64];
    snprintf(bal, sizeof(bal), "Balance: %llu MYTUBE",
             (unsigned long long)g_state.wallet.balance);
    SetWindowText(g_gui.hwnd_balance, bal);

    char bal_main[64];
    snprintf(bal_main, sizeof(bal_main), "%llu.%02llu MYTUBE",
             (unsigned long long)(g_state.wallet.balance / 1000000),
             (unsigned long long)((g_state.wallet.balance / 10000) % 100));
    SetWindowText(g_gui.hbalance_main, bal_main);

    char bal_sub[128];
    snprintf(bal_sub, sizeof(bal_sub), "Available: %llu.%02llu  |  Staked: %llu.%02llu",
             (unsigned long long)(g_state.wallet.available() / 1000000),
             (unsigned long long)((g_state.wallet.available() / 10000) % 100),
             (unsigned long long)(g_state.wallet.staked_balance / 1000000),
             (unsigned long long)((g_state.wallet.staked_balance / 10000) % 100));
    SetWindowText(g_gui.hbalance_sub, bal_sub);

    char header[128];
    const char* net = g_state.node_running ? "\xE2\x97\x8F Online" : "\xE2\x97\x8B Offline";
    snprintf(header, sizeof(header), "  MYTUBE PROTOCOL v%s    %s    Epoch %llu",
             MYCELIUM_VERSION, net, (unsigned long long)g_state.tokenomics.current_epoch);
    SetWindowText(GetDlgItem(g_gui.hwnd, IDC_HEADER_TEXT), header);

    char peers[64];
    snprintf(peers, sizeof(peers), "  %s  %zu peers",
             g_state.node_running ? "\xE2\x97\x8F" : "\xE2\x97\x8B",
             g_state.node ? g_state.node->peer_count() : (size_t)0);
    SendMessage(g_gui.hstatusbar, SB_SETTEXT, 0, (LPARAM)peers);

    const char* mining_st = g_gui.mining_active ? "Mining: active" : "Mining: idle";
    SendMessage(g_gui.hstatusbar, SB_SETTEXT, 1, (LPARAM)mining_st);

    char epoch_s[64];
    snprintf(epoch_s, sizeof(epoch_s), "Epoch %llu",
             (unsigned long long)g_state.tokenomics.current_epoch);
    SendMessage(g_gui.hstatusbar, SB_SETTEXT, 2, (LPARAM)epoch_s);

    if (g_gui.hmine_info) {
        char mi[512];
        uint64_t ep = g_state.tokenomics.current_epoch;
        snprintf(mi, sizeof(mi),
            "Current Epoch:  %llu\r\n"
            "Block Reward:   %llu MYTUBE\r\n"
            "Difficulty:     %u bits\r\n"
            "Total Minted:   %llu / %llu\r\n"
            "Balance:        %llu MYTUBE",
            (unsigned long long)ep,
            (unsigned long long)mining_block_reward(ep),
            mining_difficulty_bits(ep),
            (unsigned long long)g_state.tokenomics.minted_supply,
            (unsigned long long)kTotalSupply,
            (unsigned long long)g_state.wallet.balance);
        SetWindowText(g_gui.hmine_info, mi);
    }
}

static inline void gui_switch_tab(HWND hwnd, int tab) {
    g_gui.current_tab = tab;
    for (int i = 0; i < 3; i++) {
        if (g_gui.hpanels[i])
            ShowWindow(g_gui.hpanels[i], (i == tab) ? SW_SHOW : SW_HIDE);
    }
}

static inline void gui_update_mining_status() {
    if (g_gui.hmine_status) {
        SetWindowText(g_gui.hmine_status,
            g_gui.mining_active ? "Status: Mining in progress..." : "Status: Idle");
    }
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
        char event_buf[128];
        snprintf(event_buf, sizeof(event_buf), "Block #%llu mined",
                 (unsigned long long)(g_state.tokenomics.current_epoch - 1));
        char amt_buf[64];
        snprintf(amt_buf, sizeof(amt_buf), "+%llu MYTUBE", (unsigned long long)reward);
        gui_add_activity("now", event_buf, amt_buf, "Confirmed");
        gui_log("[MINE] \xC2\xB0 Block found! Nonce %llu, +%llu MYTUBE (balance: %llu)\r\n",
                (unsigned long long)nonce, (unsigned long long)reward,
                (unsigned long long)g_state.wallet.balance);
    }
    g_gui.mining_active = false;
    gui_update_mining_status();
    PostMessage(g_gui.hwnd, WM_TIMER, 0, 0);
}

static LRESULT CALLBACK gui_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        HDC dc = GetDC(hwnd);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(hwnd, dc);

        int fs9  = -MulDiv(9, dpi, 72);
        int fs11 = -MulDiv(11, dpi, 72);
        int fs20 = -MulDiv(20, dpi, 72);

        g_font      = CreateFont(fs9, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        g_font_bold = CreateFont(fs9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        g_font_md   = CreateFont(fs11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        g_font_lg   = CreateFont(fs20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        auto mkctrl = [&](HWND parent, const char* cls, const char* text, DWORD style,
                          int x, int y, int w, int h, int id) -> HWND {
            return CreateWindowEx(0, cls, text, style | WS_CHILD | WS_VISIBLE,
                x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandle(0), 0);
        };
        auto mkfont = [](HWND h, HFONT f) { SendMessage(h, WM_SETFONT, (WPARAM)f, 0); };

        HINSTANCE hinst = GetModuleHandle(0);

        // === HEADER ===
        char hdr[128];
        snprintf(hdr, sizeof(hdr), "  MYTUBE PROTOCOL v%s    \xE2\x97\x8F Offline    Epoch 0",
                 MYCELIUM_VERSION);
        HWND hheader = mkctrl(hwnd, "STATIC", hdr, SS_CENTERIMAGE, 0, 0, 660, 34, IDC_HEADER_TEXT);
        mkfont(hheader, g_font_bold);

        // === TAB BUTTONS ===
        auto mktab = [&](const char* text, int x, int id) -> HWND {
            HWND h = CreateWindowEx(0, "BUTTON", text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                x, 36, 76, 24, hwnd, (HMENU)(INT_PTR)id, 0, 0);
            mkfont(h, g_font);
            return h;
        };
        mktab("Overview", 12, IDC_TAB_OVERVIEW);
        mktab("Mine", 92, IDC_TAB_MINE);
        mktab("Console", 172, IDC_TAB_CONSOLE);

        // === TAB PANELS ===
        static const char* panel_cls = "MyTubePanel";
        for (int i = 0; i < 3; i++) {
            g_gui.hpanels[i] = CreateWindowEx(0, panel_cls, "",
                WS_CHILD | (i == 0 ? WS_VISIBLE : 0),
                0, 62, 660, 406, hwnd, 0, hinst, 0);
        }

        // === OVERVIEW PANEL (panel 0) ===
        HWND pov = g_gui.hpanels[0];

        g_gui.hbalance_main = mkctrl(pov, "STATIC", "0.00 MYTUBE",
            SS_CENTER | SS_CENTERIMAGE, 0, 4, 660, 36, IDC_BALANCE_MAIN);
        mkfont(g_gui.hbalance_main, g_font_lg);

        g_gui.hbalance_sub = mkctrl(pov, "STATIC",
            "Available: 0.00  |  Staked: 0.00",
            SS_CENTER | SS_CENTERIMAGE, 0, 40, 660, 16, IDC_BALANCE_SUB);
        mkfont(g_gui.hbalance_sub, g_font);

        // Config group
        HWND hcg = mkctrl(pov, "STATIC", "", WS_GROUP | BS_GROUPBOX, 8, 64, 198, 148, 0);
        mkfont(hcg, g_font_bold);
        mkctrl(pov, "STATIC", "Configuration", 0, 16, 64, 80, 14, 0);

        mkctrl(pov, "STATIC", "HTTP Port:", 0, 18, 82, 56, 14, 0);
        g_gui.hwnd_http_port = mkctrl(pov, "EDIT", "8080",
            WS_BORDER | ES_NUMBER, 76, 80, 50, 22, IDC_HTTP_PORT);
        mkfont(g_gui.hwnd_http_port, g_font);

        mkctrl(pov, "STATIC", "Listen:", 0, 18, 108, 40, 14, 0);
        g_gui.hwnd_listen = mkctrl(pov, "EDIT", "/ip4/0.0.0.0/tcp/0",
            WS_BORDER, 62, 106, 130, 22, IDC_LISTEN);
        mkfont(g_gui.hwnd_listen, g_font);

        g_gui.hwnd_tor = mkctrl(pov, "BUTTON", "Tor",
            BS_AUTOCHECKBOX | WS_TABSTOP, 18, 132, 50, 20, IDC_TOR);
        mkfont(g_gui.hwnd_tor, g_font);

        mkctrl(pov, "STATIC", "SOCKS Port:", 0, 74, 134, 60, 14, 0);
        g_gui.hwnd_socks_port = mkctrl(pov, "EDIT", "9050",
            WS_BORDER | ES_NUMBER, 132, 132, 60, 22, IDC_SOCKS_PORT);
        mkfont(g_gui.hwnd_socks_port, g_font);

        // Status group
        HWND hsg = mkctrl(pov, "STATIC", "", WS_GROUP | BS_GROUPBOX, 214, 64, 438, 148, 0);
        mkfont(hsg, g_font_bold);
        mkctrl(pov, "STATIC", "Status", 0, 222, 64, 60, 14, 0);

        g_gui.hwnd_status = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            222, 82, 422, 122, pov, (HMENU)(INT_PTR)IDC_STATUS, 0, 0);
        mkfont(g_gui.hwnd_status, g_font);

        // Activity group
        HWND hag = mkctrl(pov, "STATIC", "", WS_GROUP | BS_GROUPBOX, 8, 220, 644, 178, 0);
        mkfont(hag, g_font_bold);
        mkctrl(pov, "STATIC", "Activity", 0, 16, 220, 50, 14, 0);

        g_gui.hactivity_list = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOSORTHEADER,
            16, 238, 628, 152, pov, (HMENU)(INT_PTR)IDC_ACTIVITY_LIST, 0, 0);
        mkfont(g_gui.hactivity_list, g_font);

        LVCOLUMN lvc = {};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.pszText = (char*)"Time";   lvc.cx = 80;  lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(g_gui.hactivity_list, 0, &lvc);
        lvc.pszText = (char*)"Event";  lvc.cx = 240;
        ListView_InsertColumn(g_gui.hactivity_list, 1, &lvc);
        lvc.pszText = (char*)"Amount"; lvc.cx = 120; lvc.fmt = LVCFMT_RIGHT;
        ListView_InsertColumn(g_gui.hactivity_list, 2, &lvc);
        lvc.pszText = (char*)"Status"; lvc.cx = 80;  lvc.fmt = LVCFMT_CENTER;
        ListView_InsertColumn(g_gui.hactivity_list, 3, &lvc);

        // === MINE PANEL (panel 1) ===
        HWND pmn = g_gui.hpanels[1];

        HWND hmg = mkctrl(pmn, "STATIC", "", WS_GROUP | BS_GROUPBOX, 8, 8, 310, 200, 0);
        mkfont(hmg, g_font_bold);
        mkctrl(pmn, "STATIC", "Mining Stats", 0, 16, 8, 80, 14, 0);

        g_gui.hmine_info = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            16, 26, 294, 140, pmn, (HMENU)(INT_PTR)IDC_MINE_INFO, 0, 0);
        mkfont(g_gui.hmine_info, g_font);

        g_gui.hmine_status = mkctrl(pmn, "STATIC", "Status: Idle",
            SS_CENTERIMAGE, 16, 174, 294, 26, IDC_MINE_STATUS);
        mkfont(g_gui.hmine_status, g_font_bold);

        // === CONSOLE PANEL (panel 2) ===
        HWND pcn = g_gui.hpanels[2];

        g_gui.hwnd_log = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            8, 8, 644, 390, pcn, (HMENU)(INT_PTR)IDC_LOG, 0, 0);
        mkfont(g_gui.hwnd_log, g_font);

        // === BOTTOM ACTION BAR ===
        HWND hbb = mkctrl(hwnd, "STATIC", "", WS_GROUP | BS_GROUPBOX, 10, 472, 640, 34, 0);
        mkfont(hbb, g_font);

        HWND bw = mkctrl(hwnd, "BUTTON", "Create Wallet",
            WS_TABSTOP | BS_PUSHBUTTON, 18, 478, 100, 24, IDC_CREATE_WALLET);
        mkfont(bw, g_font);

        bw = mkctrl(hwnd, "BUTTON", "Mine",
            WS_TABSTOP | BS_PUSHBUTTON, 125, 478, 70, 24, IDC_MINE);
        mkfont(bw, g_font);

        bw = mkctrl(hwnd, "BUTTON", "Open Web UI",
            WS_TABSTOP | BS_PUSHBUTTON, 202, 478, 95, 24, IDC_OPEN_WEB);
        mkfont(bw, g_font);

        g_gui.hwnd_start = mkctrl(hwnd, "BUTTON", "START",
            WS_TABSTOP | BS_PUSHBUTTON, 306, 478, 70, 24, IDC_START);
        mkfont(g_gui.hwnd_start, g_font_bold);

        g_gui.hwnd_stop = mkctrl(hwnd, "BUTTON", "STOP",
            WS_TABSTOP | BS_PUSHBUTTON, 382, 478, 70, 24, IDC_STOP);
        mkfont(g_gui.hwnd_stop, g_font_bold);
        EnableWindow(g_gui.hwnd_stop, FALSE);

        g_gui.hwnd_balance = mkctrl(hwnd, "STATIC", "Balance: 0 MYTUBE",
            SS_CENTERIMAGE, 460, 476, 180, 26, IDC_BALANCE_TEXT);
        mkfont(g_gui.hwnd_balance, g_font_bold);

        // === STATUS BAR ===
        g_gui.hstatusbar = CreateWindow(STATUSCLASSNAME, "",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, 0, 0, 0);
        int parts[] = {150, 350, -1};
        SendMessage(g_gui.hstatusbar, SB_SETPARTS, 3, (LPARAM)parts);
        SendMessage(g_gui.hstatusbar, SB_SETTEXT, 0, (LPARAM)"  \xE2\x97\x8F 0 peers");
        SendMessage(g_gui.hstatusbar, SB_SETTEXT, 1, (LPARAM)"  Mining: idle");
        SendMessage(g_gui.hstatusbar, SB_SETTEXT, 2, (LPARAM)"  Epoch 0");

        gui_log("=== MyTube Protocol v%s ===\r\n", MYCELIUM_VERSION);
        gui_log("GUI started. Use the bottom bar to control the node.\r\n");

        SetTimer(hwnd, 1, 1000, 0);
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        HWND hctl = (HWND)l;
        if (hctl == GetDlgItem(hwnd, IDC_HEADER_TEXT)) {
            SetTextColor(hdc, GetSysColor(COLOR_CAPTIONTEXT));
            SetBkColor(hdc, GetSysColor(COLOR_ACTIVECAPTION));
            return (LRESULT)GetSysColorBrush(COLOR_ACTIVECAPTION);
        }
        if (hctl == g_gui.hbalance_main || hctl == g_gui.hbalance_sub) {
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
        SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)w;
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_SYSCOLORCHANGE: {
        InvalidateRect(hwnd, 0, TRUE);
        break;
    }

    case WM_TIMER: {
        gui_update_status();
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);

        if (id == IDC_TAB_OVERVIEW) {
            gui_switch_tab(hwnd, 0);
        } else if (id == IDC_TAB_MINE) {
            gui_switch_tab(hwnd, 1);
        } else if (id == IDC_TAB_CONSOLE) {
            gui_switch_tab(hwnd, 2);

        } else if (id == IDC_START) {
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
            gui_add_activity("now", "Wallet created", "0.00 MYTUBE", "Ready");
            gui_update_status();

        } else if (id == IDC_MINE) {
            if (g_gui.mining_active) {
                gui_log("[MINE] Already mining...\r\n");
                break;
            }
            g_gui.mining_active = true;
            gui_update_mining_status();
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
        if (g_font)      DeleteObject(g_font);
        if (g_font_bold) DeleteObject(g_font_bold);
        if (g_font_lg)   DeleteObject(g_font_lg);
        if (g_font_md)   DeleteObject(g_font_md);
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProc(hwnd, msg, w, l);
    }
    return 0;
}

static inline int gui_run() {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    HINSTANCE hinst = GetModuleHandle(0);

    WNDCLASS pc = {};
    pc.lpfnWndProc = gui_panel_proc;
    pc.hInstance = hinst;
    pc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    pc.lpszClassName = "MyTubePanel";
    RegisterClass(&pc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = gui_wndproc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MyTubeGuiWnd";
    if (!RegisterClass(&wc)) return 1;

    RECT r = {0, 0, 660, 560};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME), 0);

    char title[128];
    snprintf(title, sizeof(title), "MyTube Protocol v%s", MYCELIUM_VERSION);
    g_gui.hwnd = CreateWindowEx(0, "MyTubeGuiWnd", title,
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
