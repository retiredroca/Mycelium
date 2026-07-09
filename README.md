# MyTube Protocol

**A YouTube-Inspired P2P Video Network with Quantum-Resistant Security and Social Mining**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Security: Quantum Resistant](https://img.shields.io/badge/Security-Hybrid%20PQ-brightgreen.svg)]()

## Overview

MyTube is a decentralized peer-to-peer video network — like YouTube, but without the central server. Users own their data, host video chunks on peer nodes for fees, and earn through "Social Mining" — a native token reward system for network participation.

### Design Philosophy

Zero external dependencies, no virtual dispatch, no async runtime, no garbage collection. All crypto primitives are self-contained — SHA-256, HMAC, HKDF, X25519, Ed25519, and AES-256-GCM (via Win32 BCrypt) — with the same post-quantum hybrid interfaces as the original Rust spec, including Kyber-768 and Dilithium-3 stub placeholders.

Every module is a single `static inline` header. The entire project builds in seconds to a ~128 KB static binary, including the embedded web UI.

### Key Features

- **User-Owned Data**: Posts and media are encrypted with user-controlled keys
- **Peer Hosting**: Friends and trusted nodes host your data for token rewards
- **Video Hosting**: Encrypted chunked video with bandwidth-weighted peer selection and streaming slot reservations
- **Embedded Web UI**: YouTube-style landing page served via built-in HTTP listener (`--http-port`)
- **Tor / Onion Routing**: Run nodes over Tor hidden services with auto-derived `.onion` addresses via Ed25519 key pairs
- **Proof-of-Work Mining**: Mine MYTUBE tokens via SHA-256 hash brute-force (placeholder until Social Mining via P2P transport arrives)
- **Social Mining**: Earn tokens by relaying data, hosting content, and creating engagement
- **Quantum-Resistant**: Hybrid cryptography (X25519 + Kyber-768 + AES-256-GCM)
- **Proof of Engagement**: Content visibility extends based on network interaction
- **Zero Runtime Overhead**: No async, no GC, no virtual tables, no external heap frameworks

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      CLIENT LAYER                           │
│    C++ CLI  │  Embedded Web UI (--http-port)                │
│    (future: WASM / Native)                                  │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                    VIDEO / MEDIA LAYER                      │
│  Chunked Encryption  │  Streaming Slots  │  Bandwidth Mgmt  │
│  Codec Metadata  │  Per-Chunk CIDs  │  Thumbnail CIDs      │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                       P2P NETWORK                           │
│  Kademlia DHT  │  Gossip  │  Peer Table  │  TCP Framing    │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                    QUANTUM-RESISTANT CRYPTO                  │
│  Hybrid Encryption  │  Hybrid Signatures  │  Key Management │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                     TOKEN LAYER                              │
│        Staking  │  Rewards  │  Fee Burn  │  Governance      │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

- C++17 compiler (MSVC 2022+, GCC 9+, Clang 10+)
- CMake 3.20+

### Build

```bash
# Clone the repository
git clone https://github.com/retiredroca/Mycelium.git
cd mytube

# Build the project
cmake -B build
cmake --build build --config Release

# Run the node
./build/Release/mycelium status
```

### Versioning

Version format: `0.1.{YY}.{DOW}.{WN}` — where `YY` is the 2-digit year,
`DOW` is the day of week (0=Sun..6=Sat), and `WN` is the ISO week number.
Set via `-DMYCELIUM_BUILD_VERSION=...` at configure time.

- **Local builds**: default version `0.01`
- **CI builds**: auto-computed from the current date

### CI / Build Automation

Builds run on push/PR to `main` **only when** the commit message (or PR title/body)
contains a trigger keyword:

| Keyword   | Action                        |
|-----------|-------------------------------|
| `buildRC` | Build binary with auto-version |
| `buildRelease` | Same as RC (no tagging)   |

Example: `feat: add mining stats; buildRC`

### Basic Commands

```bash
# Start a new node
./build/Release/mycelium start --listen /ip4/0.0.0.0/tcp/4001

# Create a profile
./build/Release/mycelium profile create --display-name "Alice" --username alice

# Create a post
./build/Release/mycelium post --content "Hello, MyTube Network!"

# Create a wallet (balance starts at 0)
./build/Release/mycelium wallet create

# Check wallet balance
./build/Release/mycelium wallet balance

# Mine tokens via proof-of-work (SHA-256, difficulty 16 bits)
./build/Release/mycelium mine

# Upload a video manifest (simulated)
./build/Release/mycelium video upload --video-id "my-video" --duration 120000 --width 1920 --height 1080 --codec 0 --bitrate 8000000 --chunks 4

# View video manifest details
./build/Release/mycelium video manifest

# View network status
./build/Release/mycelium status

# Start a node with embedded web UI (http://localhost:8080)
./build/Release/mycelium start --http-port 8080

# Start a node with Tor hidden service (requires Tor daemon)
./build/Release/mycelium start --listen /ip4/0.0.0.0/tcp/4001 --tor --tor-socks-port 9050 --tor-control-port 9051

# Start with both web UI and Tor
./build/Release/mycelium start --http-port 8080 --tor
```

## Video & Post Lifecycle

Videos and posts on MyTube have a Time-To-Live (TTL) that is extended through viewer engagement — just like trending algorithms on YouTube:

| State | TTL | Extension Mechanism |
|-------|-----|-------------------|
| Fresh | 24h | +2h per view, +4h per share |
| Trending | 48h+ | +6h per comment, +12h per share |
| Permanent | ∞ | Stake tokens to make permanent |

```
┌──────────────────────────────────────────────────────────────┐
│                  VIDEO / POST LIFECYCLE                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   Uploaded ──► Fresh (24h TTL)                              │
│                   │                                          │
│                   ├── View ──► TTL +2h                      │
│                   ├── Share ──► TTL +4h                     │
│                   └── Comment ──► TTL +6h                   │
│                          │                                  │
│                          ▼                                  │
│                Trending (Score ≥ 1000)                       │
│                          │                                  │
│                          ├── More engagement                 │
│                          │                                  │
│                          └── Stake tokens ──► Permanent     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Tokenomics

### Token Supply & Mining

MYTUBE tokens enter circulation through **proof-of-work mining** — there is no pre-mine, no ICO, and no instant grant. Every token is mined into existence.

> **Note:** This PoW is a placeholder genesis mechanism for the simulation. In the real MyTube protocol, the primary distribution will come from **Social Mining** — earning tokens by relaying data, hosting storage, creating content, and driving engagement through the reward pools (see Reward Distribution below). The PoW miner is pure CPU hash brute-force with no relation to network services; it exists only to gate token minting until the P2P transport layer is implemented.

| Parameter | Value |
|-----------|-------|
| Total Supply | 1,000,000,000 MYTUBE (hard cap) |
| Block Reward (epoch 0) | 50 MYTUBE |
| Halving Interval | Every 210,000 epochs |
| Initial Difficulty | 16 leading zero bits |
| Difficulty Adjustment | +1 bit every 10,000 epochs (cap: 28 bits) |
| Mining Algorithm | SHA-256(pubkey ‖ nonce ‖ epoch) |
| Disinflation | Annual inflation rate drops 15% per epoch (floor 1.5%) |

### Reward Distribution (Annual Emission: 8% → 1.5%)

The mined supply is distributed through Social Mining pools using the reward pool system:

| Category | Allocation | Description |
|----------|------------|-------------|
| Relay Rewards | 35% | Data forwarding, gossip propagation |
| Hosting Rewards | 40% | Authorized peer storage + video hosting bandwidth multiplier |
| Creation Rewards | 15% | Hype milestones |
| Engagement Rewards | 10% | Viewers, sharers, commenters |

### Utility Actions

| Action | Mechanism | Amount |
|--------|-----------|--------|
| Post Permanence | Stake (returnable) | 100-10,000 tokens |
| High-Bandwidth Hosting | Stake (returnable) | 500-50,000 tokens |
| Video Streaming Slot | Stake (returnable) | 1,000-100,000 tokens |
| Verified Identity | Burn (permanent) | 50 tokens |
| Network Fees | Burn (permanent) | Variable |

## Web UI

MyTube ships with a **YouTube-style embedded web interface** served directly from the binary. No external HTTP server, no static files — the entire HTML/CSS landing page is compiled into `myc_web.hpp` as a raw string literal and served via a built-in synchronous TCP listener.

```bash
# Start with web UI on port 8080
./build/Release/mycelium start --http-port 8080
# Open http://localhost:8080 in your browser
```

When combined with `--tor`, the node also displays the `.onion` web URL for privacy-preserving remote access.

## Security

### Quantum-Resistant Cryptography

The protocol uses **hybrid cryptography** combining:
- **X25519**: Classical ECDH key exchange (self-contained implementation)
- **Kyber-768**: Post-quantum KEM (NIST Level 5, stub ready for liboqs)
- **AES-256-GCM**: Symmetric encryption (Win32 BCrypt)
- **Ed25519**: Classical digital signatures (self-contained implementation)
- **Dilithium-3**: Post-quantum signatures (stub ready for liboqs)

```
┌─────────────────────────────────────────────────────────────┐
│               HYBRID ENCRYPTION PIPELINE                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Ephemeral Key Gen                                          │
│  ├── Classical: X25519 (32 bytes)                          │
│  └── Post-Quantum: Kyber-768 (1184 bytes)                 │
│           │                                                 │
│           ▼                                                 │
│  Key Exchange ──► Shared Secret                             │
│           │                                                 │
│           ▼                                                 │
│  Key Derivation (HKDF-SHA256) ──► AES-256 Key             │
│           │                                                 │
│           ▼                                                 │
│  AES-256-GCM Encrypt ──► Encrypted Blob                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Key Management

- **User Identity Keys**: Derived from random seeds via SHA-256
- **Storage Keys**: Derived via HKDF from shared secrets
- **Session Keys**: Ephemeral, generated per message via OS RNG

## Project Structure

```
mytube/
├── mycelium-cpp/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp                          # CLI application
│       ├── crypto/
│       │   └── myc_crypto.hpp                # All crypto: SHA-256, HMAC, HKDF,
│       │                                       X25519, Ed25519, AES-256-GCM,
│       │                                       hybrid encrypt/decrypt, PQ stubs
│       ├── protocol/
│       │   └── myc_protocol.hpp              # Wire format messages
│       ├── post/
│       │   └── myc_post.hpp                  # Post struct & lifecycle
│       ├── storage/
│       │   └── myc_storage.hpp               # Encrypted local storage
│       ├── token/
│       │   └── myc_token.hpp                 # Tokenomics, wallet, staking
│       ├── social/
│       │   └── myc_social_graph.hpp          # Follow/follow/block graph
│       ├── profile/
│       │   ├── myc_profile.hpp               # Profile, ProfileBuilder
│       │   ├── myc_profile_theme.hpp          # Theme, presets, colors
│       │   ├── myc_profile_layout.hpp         # Layout sections, widgets
│       │   └── myc_profile_validation.hpp     # Username/bio/URL validation
│       ├── guestbook/
│       │   └── myc_guestbook.hpp             # Guestbook entries, approval
│       ├── media/
│       │   └── myc_video.hpp                 # VideoMetadata, chunking, codecs
│       ├── web/
│       │   └── myc_web.hpp                   # Embedded MyTube HTML landing page + HTTP server
│       ├── identity/
│       │   └── myc_identity.hpp              # Username registry, lookup
│       └── p2p/
│           └── myc_p2p.hpp                   # Peer table, gossip, node
│
├── docs/
│   └── architecture.md                       # Architecture diagrams
│
├── README.md                                  # This file
├── LICENSE                                    # MIT License
└── CONTRIBUTING.md                            # Contribution guidelines
```

## Design Decisions

### Why C++ with `static inline` headers?

| Concern | Approach |
|---------|----------|
| **Zero dependencies** | All crypto (SHA-256, X25519, Ed25519) implemented from scratch; AES-256-GCM uses Win32 BCrypt |
| **No virtual dispatch** | Free functions and structs only; templates used sparingly |
| **No async** | Synchronous I/O with simple select/poll for networking |
| **Fixed buffers** | `std::array` preferred over `std::vector` wherever sizes are known at compile time |
| **Error handling** | Integer error codes with `strerror`-style lookup tables; no exceptions |
| **Build time** | Unity build (single translation unit) compiles in seconds |
| **Binary size** | ~128 KB release build, statically linked (includes embedded web UI) |

### Post-Quantum Readiness

The PQ interfaces (Kyber-768 encapsulate/decapsulate, Dilithium fork/merge) use the same wire format and key sizes as the original Rust protocol. The implementations are stubbed with random-byte XOR placeholders — identical to the original Rust simulation — and can be swapped for real `liboqs` calls by replacing the body of `pq_keygen`, `pq_encapsulate`, and `pq_decapsulate` with no structural changes.

## Development

### Running the CLI

```bash
# Build and run
cd mytube
cmake -B build && cmake --build build --config Release

# Run any command
./build/Release/mycelium help
./build/Release/mycelium status
./build/Release/mycelium wallet create
./build/Release/mycelium wallet balance
./build/Release/mycelium mine
./build/Release/mycelium profile create --display-name "Alice" --username alice
```

## Roadmap

| Phase | Timeline | Goals |
|-------|----------|-------|
| **Phase 1** | Current | C++ port, core crypto, CLI, peer table, profile system, video hosting, embedded web UI, PoW mining |
| **Phase 2** | Next | Real TCP/UDP transport, Kademlia DHT, gossip protocol |
| **Phase 3** | Future | Real liboqs integration (ML-KEM, SLH-DSA), encrypted storage |
| **Phase 4** | Future | Token integration, staking, governance |

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [NIST PQC](https://csrc.nist.gov/projects/post-quantum-cryptography) - Post-quantum standards
- [Kyber](https://pq-crystals.org/kyber/) - Learning With Errors KEM
- [Dilithium](https://pq-crystals.org/dilithium/) - Lattice-based signatures
- [liboqs](https://github.com/open-quantum-safe/liboqs) - Open Quantum Safe library (for future real PQ)
