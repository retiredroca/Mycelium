# The Sovereign Node Protocol

**A Decentralized Social Network with Quantum-Resistant Security and Social Mining**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Rust](https://img.shields.io/badge/Rust-1.75+-orange.svg)](https://www.rust-lang.org)
[![Security: Quantum Resistant](https://img.shields.io/badge/Security-Hybrid%20PQ-brightgreen.svg)]()

## Overview

The Sovereign Node Protocol is a decentralized social network where users own their data, host content on peer nodes for fees, and earn through "Social Mining" - a native token reward system for network participation.

### Key Features

- **User-Owned Data**: Posts and media are encrypted with user-controlled keys
- **Peer Hosting**: Friends and trusted nodes host your data for token rewards
- **Social Mining**: Earn tokens by relaying data, hosting content, and creating engagement
- **Quantum-Resistant**: Hybrid cryptography (X25519 + Kyber-768 + AES-256-GCM)
- **Proof of Engagement**: Content visibility extends based on network interaction

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      CLIENT LAYER                           │
│         Rust CLI  │  WebAssembly  │  Mobile App             │
└───────────────────────────┬─────────────────────────────────┘
                            │ libp2p
┌───────────────────────────▼─────────────────────────────────┐
│                       P2P NETWORK                           │
│  Kademlia DHT  │  GossipSub v2  │  mDNS  │  Relay         │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                    QUANTUM-RESISTANT CRYPTO                 │
│  Hybrid Encryption  │  Hybrid Signatures  │  Key Management  │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                     TOKEN LAYER (SVM L2)                    │
│        Staking  │  Rewards  │  Fee Burn  │  Governance      │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

- Rust 1.75+
- OpenSSL (for native TLS)
- Cargo

### Build

```bash
# Clone the repository
git clone https://github.com/your-org/sovereign-node.git
cd sovereign-node

# Build the project
cargo build --release

# Run the node
./target/release/sovereign-node status
```

### Basic Commands

```bash
# Start a new node
./target/release/sovereign-node start --listen /ip4/0.0.0.0/tcp/4001

# Create a post
./target/release/sovereign-node post --content "Hello, Sovereign Network!"

# Check wallet balance
./target/release/sovereign-node wallet balance

# View network status
./target/release/sovereign-node network info
```

## Post Lifecycle

Posts in the Sovereign Node Protocol have a Time-To-Live (TTL) that can be extended through engagement:

| State | TTL | Extension Mechanism |
|-------|-----|-------------------|
| Fresh | 24h | +2h per view, +4h per share |
| Hyped | 48h+ | +6h per comment, +12h per share |
| Permanent | ∞ | Stake tokens to make permanent |

```
┌──────────────────────────────────────────────────────────────┐
│                     POST LIFECYCLE                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│   Created ──► Fresh (24h TTL)                              │
│                   │                                          │
│                   ├── View ──► TTL +2h                      │
│                   ├── Share ──► TTL +4h                     │
│                   └── Comment ──► TTL +6h                   │
│                          │                                  │
│                          ▼                                  │
│                   Hype (Score ≥ 1000)                        │
│                          │                                  │
│                          ├── More engagement                 │
│                          │                                  │
│                          └── Stake tokens ──► Permanent     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Tokenomics

### Reward Distribution (Annual Emission: 8% → 1.5%)

| Category | Allocation | Description |
|----------|------------|-------------|
| Relay Rewards | 35% | Data forwarding, gossip propagation |
| Hosting Rewards | 40% | Authorized peer storage |
| Creation Rewards | 15% | Hype milestones |
| Engagement Rewards | 10% | Viewers, sharers, commenters |

### Utility Actions

| Action | Mechanism | Amount |
|--------|-----------|--------|
| Post Permanence | Stake (returnable) | 100-10,000 tokens |
| High-Bandwidth Hosting | Stake (returnable) | 500-50,000 tokens |
| Verified Identity | Burn (permanent) | 50 tokens |
| Network Fees | Burn (permanent) | Variable |

## Security

### Quantum-Resistant Cryptography

The protocol uses **hybrid cryptography** combining:
- **X25519**: Classical ECDH key exchange
- **Kyber-768**: Post-quantum KEM (NIST Level 5)
- **AES-256-GCM**: Symmetric encryption
- **Ed25519**: Classical digital signatures
- **Dilithium-3**: Post-quantum signatures

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

- **User Identity Keys**: Stored in device secure enclave
- **Storage Keys**: Derived via HKDF from identity keys
- **Session Keys**: Ephemeral, generated per message

## Project Structure

```
sovereign-node/
├── Cargo.toml                      # Workspace configuration
├── bin/sovereign-node/
│   └── src/
│       ├── main.rs                 # CLI application
│       └── wallet.rs               # Wallet management
├── crates/
│   ├── crypto/                     # Quantum-resistant crypto
│   │   ├── src/
│   │   │   ├── lib.rs             # Module exports
│   │   │   ├── keys.rs            # Key generation (X25519/Ed25519)
│   │   │   ├── encryption.rs      # AES-256-GCM
│   │   │   ├── signatures.rs      # Digital signatures
│   │   │   ├── hybrid.rs          # Hybrid encryption
│   │   │   └── quantum_resistant.rs # Kyber/Dilithium
│   │
│   ├── p2p/                        # P2P networking
│   │   └── src/
│   │       ├── lib.rs             # Node implementation
│   │       ├── behaviour.rs       # libp2p behaviours
│   │       ├── peer_table.rs      # K-bucket peer management
│   │       ├── discovery.rs       # Kademlia DHT config
│   │       ├── gossip.rs          # Epidemic broadcast
│   │       └── storage_handoff.rs # Host handshake
│   │
│   ├── post/                       # Post lifecycle & TTL
│   ├── storage/                     # Encrypted storage
│   ├── token/                       # Tokenomics & rewards
│   └── protocol/                    # Message types
│
├── docs/
│   └── architecture.md             # Architecture diagrams
│
├── README.md                        # This file
└── LICENSE                         # MIT License
```

## Development

### Running Tests

```bash
# Run all tests
cargo test

# Run with logging
RUST_LOG=debug cargo test

# Run specific crate tests
cargo test -p sov-crypto
cargo test -p sov-post
```

### Code Linting

```bash
cargo clippy --all-targets
```

## Roadmap

| Phase | Timeline | Goals |
|-------|----------|-------|
| **Phase 1** | Q1-Q2 2024 | P2P core, Kademlia DHT, GossipSub |
| **Phase 2** | Q3-Q4 2024 | Token integration, Solana SVM L2, staking |
| **Phase 3** | Q1-Q2 2025 | Hype logic, encrypted storage, mobile |
| **Phase 4** | Q3-Q4 2025 | DAO governance, cross-chain bridges |

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [libp2p](https://github.com/libp2p/rust-libp2p) - P2P networking library
- [Kademlia](https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf) - DHT paper
- [NIST PQC](https://csrc.nist.gov/projects/post-quantum-cryptography) - Post-quantum standards
- [Kyber](https://pq-crystals.org/kyber/) - Learning With Errors KEM
- [Dilithium](https://pq-crystals.org/dilithium/) - Lattice-based signatures
