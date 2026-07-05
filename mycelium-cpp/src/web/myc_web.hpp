#pragma once
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32")
#endif

static inline const char* kMyTubeWebPage = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MyTube — Decentralized P2P Video Network</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
:root {
  --yt-red: #FF0000; --yt-red-dark: #CC0000;
  --bg: #0f0f0f; --bg-secondary: #1a1a1a; --bg-card: #222;
  --text: #fff; --text-secondary: #aaa; --border: #333;
}
body {
  font-family: 'Segoe UI', 'Roboto', Arial, sans-serif;
  background: var(--bg); color: var(--text); min-height: 100vh;
}
.navbar {
  position: fixed; top: 0; left: 0; right: 0; height: 56px;
  background: var(--bg-secondary); border-bottom: 1px solid var(--border);
  display: flex; align-items: center; padding: 0 24px; z-index: 100;
  justify-content: space-between;
}
.navbar .logo {
  display: flex; align-items: center; gap: 8px;
  text-decoration: none; color: var(--text); font-size: 20px; font-weight: 700;
}
.navbar .logo .yt-icon { color: var(--yt-red); font-size: 28px; font-weight: 900; }
.navbar .logo .tube { font-weight: 300; }
.search-bar {
  display: flex; align-items: center; max-width: 600px; flex: 1; margin: 0 40px;
}
.search-bar input {
  flex: 1; padding: 8px 16px; background: var(--bg);
  border: 1px solid var(--border); border-right: none;
  border-radius: 20px 0 0 20px; color: var(--text); font-size: 14px; outline: none;
}
.search-bar button {
  padding: 8px 20px; background: var(--bg-secondary);
  border: 1px solid var(--border); border-radius: 0 20px 20px 0;
  color: var(--text); cursor: pointer;
}
.navbar .nav-icons { display: flex; gap: 20px; color: var(--text-secondary); }
.sidebar {
  position: fixed; top: 56px; left: 0; bottom: 0; width: 240px;
  background: var(--bg-secondary); padding: 12px 0; overflow-y: auto;
}
.sidebar-section { padding: 8px 12px; }
.sidebar-section h3 {
  font-size: 14px; color: var(--text-secondary);
  text-transform: uppercase; padding: 8px 12px; letter-spacing: 0.5px;
}
.sidebar-item {
  display: flex; align-items: center; gap: 20px; padding: 10px 12px;
  border-radius: 10px; cursor: default; color: var(--text);
  font-size: 14px; transition: background 0.15s;
}
.sidebar-item:hover { background: var(--bg-card); }
.sidebar-item .ico { width: 20px; text-align: center; color: var(--text-secondary); }
.sidebar-item .badge {
  background: var(--yt-red); color: #fff; font-size: 11px;
  padding: 1px 6px; border-radius: 10px; margin-left: auto;
}
.sidebar-item.active { background: var(--bg-card); font-weight: 500; }
.main { margin-left: 240px; margin-top: 56px; padding: 24px; }
.banner {
  background: linear-gradient(135deg, var(--yt-red-dark) 0%, var(--yt-red) 100%);
  border-radius: 16px; padding: 32px 40px; margin-bottom: 28px;
  display: flex; justify-content: space-between; align-items: center;
}
.banner h1 { font-size: 28px; margin-bottom: 8px; }
.banner p { opacity: 0.9; font-size: 15px; line-height: 1.5; max-width: 500px; }
.banner .stats { display: flex; gap: 40px; margin-top: 16px; }
.banner .stat { text-align: center; }
.banner .stat .num { font-size: 24px; font-weight: 700; }
.banner .stat .label { font-size: 12px; opacity: 0.8; text-transform: uppercase; letter-spacing: 0.5px; }
.video-grid {
  display: grid; grid-template-columns: repeat(auto-fill, minmax(340px, 1fr)); gap: 20px;
}
.video-card {
  background: var(--bg-secondary); border-radius: 12px; overflow: hidden; transition: transform 0.15s;
}
.video-card:hover { transform: translateY(-2px); }
.video-card .thumb {
  aspect-ratio: 16/9; background: var(--bg-card);
  display: flex; align-items: center; justify-content: center;
  position: relative; overflow: hidden;
}
.video-card .thumb .ico { font-size: 48px; color: var(--text-secondary); opacity: 0.5; }
.video-card .thumb .duration {
  position: absolute; bottom: 8px; right: 8px;
  background: rgba(0,0,0,0.8); color: #fff; font-size: 12px;
  padding: 2px 6px; border-radius: 4px;
}
.video-card .info { padding: 12px; }
.video-card .info h3 { font-size: 14px; margin-bottom: 4px; }
.video-card .info .meta { font-size: 12px; color: var(--text-secondary); }
.token-section {
  margin-top: 40px; padding: 24px; background: var(--bg-secondary); border-radius: 12px;
}
.token-section h2 { font-size: 20px; margin-bottom: 16px; }
.token-section h2 .ico { color: var(--yt-red); margin-right: 8px; }
.token-grid {
  display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px;
}
.token-card {
  background: var(--bg-card); padding: 16px; border-radius: 8px; text-align: center;
}
.token-card .num { font-size: 20px; font-weight: 700; color: var(--yt-red); }
.token-card .label { font-size: 12px; color: var(--text-secondary); margin-top: 4px; }
.footer {
  text-align: center; padding: 40px 0 24px;
  color: var(--text-secondary); font-size: 13px;
}
.footer a { color: var(--text-secondary); text-decoration: none; }
.footer a:hover { color: var(--text); }
@media (max-width: 768px) {
  .sidebar { display: none; }
  .main { margin-left: 0; }
  .search-bar { margin: 0 12px; }
  .banner { flex-direction: column; text-align: center; padding: 24px; }
  .banner .stats { gap: 20px; }
  .video-grid { grid-template-columns: 1fr; }
}
</style>
</head>
<body>
<nav class="navbar">
  <a href="/" class="logo"><span class="yt-icon">&#9654;</span> <span>My<span class="tube">Tube</span></span></a>
  <div class="search-bar"><input type="text" placeholder="Search videos..." readonly><button>&#128269;</button></div>
  <div class="nav-icons">&#127916; &#128276; &#128100;</div>
</nav>
<div class="sidebar">
  <div class="sidebar-section">
    <div class="sidebar-item active"><span class="ico">&#127968;</span> Home</div>
    <div class="sidebar-item"><span class="ico">&#128293;</span> Trending</div>
    <div class="sidebar-item"><span class="ico">&#127916;</span> Videos</div>
    <div class="sidebar-item"><span class="ico">&#128451;</span> Storage</div>
  </div>
  <div class="sidebar-section">
    <h3>Subscriptions</h3>
    <div class="sidebar-item"><span class="ico">&#9679;</span> Alice</div>
    <div class="sidebar-item"><span class="ico">&#9679;</span> Bob</div>
    <div class="sidebar-item"><span class="ico">&#9679;</span> Charlie</div>
  </div>
  <div class="sidebar-section">
    <h3>Network</h3>
    <div class="sidebar-item"><span class="ico">&#10070;</span> Peers <span class="badge">0</span></div>
    <div class="sidebar-item"><span class="ico">&#9754;</span> MYTUBE <span class="badge">1B</span></div>
    <div class="sidebar-item"><span class="ico">&#128737;</span> PQ Secure</div>
  </div>
</div>
<div class="main">
  <div class="banner">
    <div>
      <h1>&#9654; Welcome to MyTube</h1>
      <p>A decentralized peer-to-peer video network. Own your content, host on peer nodes, and earn MYTUBE tokens through social mining.</p>
      <div class="stats">
        <div class="stat"><div class="num">97 KB</div><div class="label">Binary Size</div></div>
        <div class="stat"><div class="num">C++17</div><div class="label">Zero Deps</div></div>
        <div class="stat"><div class="num">PQ</div><div class="label">Quantum Safe</div></div>
      </div>
    </div>
    <div style="font-size:64px;opacity:0.3;">&#9654;</div>
  </div>
  <div class="video-grid">
    <div class="video-card">
      <div class="thumb"><span class="ico">&#128737;</span><span class="duration">2:00</span></div>
      <div class="info"><h3>Quantum-Resistant Hybrid Encryption Pipeline</h3><div class="meta"><span>MyTube Protocol</span><span>12K views &#8226; 3 days ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#9754;</span><span class="duration">1:30</span></div>
      <div class="info"><h3>MYTUBE Token &#8212; Social Mining Rewards Explained</h3><div class="meta"><span>MyTube Protocol</span><span>8.5K views &#8226; 1 week ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#128101;</span><span class="duration">3:15</span></div>
      <div class="info"><h3>Peer Hosting: How to Earn Tokens by Storing Videos</h3><div class="meta"><span>MyTube Protocol</span><span>5.2K views &#8226; 2 weeks ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#9986;</span><span class="duration">4:00</span></div>
      <div class="info"><h3>Chunked Video Encryption &#8212; 4MB Segment Architecture</h3><div class="meta"><span>MyTube Protocol</span><span>3.1K views &#8226; 1 month ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#129309;</span><span class="duration">2:45</span></div>
      <div class="info"><h3>Storage Handshake: Creating P2P Hosting Agreements</h3><div class="meta"><span>MyTube Protocol</span><span>2.8K views &#8226; 1 month ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#128187;</span><span class="duration">5:30</span></div>
      <div class="info"><h3>Building with MyTube &#8212; C++17 static inline headers</h3><div class="meta"><span>MyTube Protocol</span><span>1.9K views &#8226; 2 months ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#127916;</span><span class="duration">1:00</span></div>
      <div class="info"><h3>Video Upload CLI &#8212; Simulating Chunked Uploads</h3><div class="meta"><span>MyTube Protocol</span><span>1.2K views &#8226; 2 months ago</span></div></div>
    </div>
    <div class="video-card">
      <div class="thumb"><span class="ico">&#128200;</span><span class="duration">3:30</span></div>
      <div class="info"><h3>Bandwidth-Weighted Reward Distribution for Hosts</h3><div class="meta"><span>MyTube Protocol</span><span>800 views &#8226; 3 months ago</span></div></div>
    </div>
  </div>
  <div class="token-section">
    <h2><span class="ico">&#9754;</span> MYTUBE Tokenomics</h2>
    <div class="token-grid">
      <div class="token-card"><div class="num">1B</div><div class="label">Total Supply</div></div>
      <div class="token-card"><div class="num">8%</div><div class="label">Annual Emission</div></div>
      <div class="token-card"><div class="num">40%</div><div class="label">Hosting Rewards</div></div>
      <div class="token-card"><div class="num">35%</div><div class="label">Relay Rewards</div></div>
      <div class="token-card"><div class="num">15%</div><div class="label">Creation Rewards</div></div>
      <div class="token-card"><div class="num">10%</div><div class="label">Engagement Rewards</div></div>
    </div>
  </div>
  <div class="footer">
    <p><a href="https://github.com/retiredroca/Mycelium">MyTube Protocol</a> &mdash; MIT License &mdash; Built with C++17, zero external dependencies</p>
  </div>
</div>
</body>
</html>)HTML";

static inline void serve_http_page(uintptr_t client_fd) {
#ifdef _WIN32
    SOCKET s = (SOCKET)client_fd;
#else
    int s = (int)client_fd;
#endif
    char buf[4096];
#ifdef _WIN32
    recv(s, buf, sizeof(buf), 0);
#else
    read(s, buf, sizeof(buf));
#endif

    const char* headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    const char* page = kMyTubeWebPage;
    size_t page_len = strlen(page);

#ifdef _WIN32
    send(s, headers, (int)strlen(headers), 0);
    size_t sent = 0;
    while (sent < page_len) {
        int n = (int)(page_len - sent > 65536 ? 65536 : page_len - sent);
        int r = send(s, page + sent, n, 0);
        if (r <= 0) break;
        sent += (size_t)r;
    }
    shutdown(s, SD_SEND);
    closesocket(s);
#else
    write(s, headers, strlen(headers));
    size_t sent = 0;
    while (sent < page_len) {
        size_t n = page_len - sent > 65536 ? 65536 : page_len - sent;
        ssize_t r = write(s, page + sent, n);
        if (r <= 0) break;
        sent += (size_t)r;
    }
    shutdown(s, SHUT_WR);
    close(s);
#endif
}
