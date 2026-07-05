# Contributing to MyTube Protocol

Thank you for your interest in contributing to MyTube!

## Code of Conduct

By participating in this project, you agree to maintain a respectful and inclusive
environment for everyone. We do not tolerate harassment or discrimination of any kind.

## Getting Started

### Development Setup

1. **Install Rust**
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   rustup update
   ```

2. **Install System Dependencies**
   ```bash
   # Debian/Ubuntu
   sudo apt-get install build-essential libssl-dev pkg-config
   
   # macOS
   brew install openssl pkg-config
   ```

3. **Clone and Build**
   ```bash
    git clone https://github.com/retiredroca/Mycelium.git
    cd mycelium
   cargo build --release
   ```

### Running Tests

```bash
# Run all tests
cargo test

# Run with verbose output
cargo test -- --nocapture

# Run specific tests
cargo test -p myc-crypto
cargo test -p myc-p2p
cargo test -p myc-post
```

### Code Style

We use `rustfmt` for code formatting and `clippy` for linting:

```bash
# Format code
cargo fmt

# Run linter
cargo clippy --all-targets -- -D warnings
```

## Branching Strategy

- `main` - Stable, production-ready code
- `develop` - Integration branch for features
- `feature/*` - New features
- `fix/*` - Bug fixes
- `security/*` - Security-related changes

## Commit Messages

Please follow [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add quantum-resistant encryption for posts
fix: resolve peer discovery timeout issue
docs: update architecture diagrams
refactor: simplify TTL calculation logic
security: harden key exchange protocol
```

## Pull Request Process

1. **Fork** the repository and create your branch from `develop`
2. **Write tests** for any new functionality
3. **Ensure tests pass**: `cargo test`
4. **Run linter**: `cargo clippy --fix`
5. **Update documentation** if needed
6. **Submit PR** with a clear description of changes

## Areas for Contribution

### High Priority

- [ ] Complete libp2p integration with GossipSub
- [ ] Implement Solana SVM L2 token contracts
- [ ] Add proper Kyber/Dilithium libraries (currently simulated)
- [ ] Build distributed storage with OrbitDB/Helia

### Medium Priority

- [ ] Mobile client application
- [ ] WebAssembly browser integration
- [ ] DAO governance module
- [ ] Cross-chain bridge implementations

### Lower Priority

- [ ] Performance optimization
- [ ] Additional test coverage
- [ ] Documentation improvements
- [ ] Internationalization

## Security Disclosures

If you discover a security vulnerability, please DO NOT open a public issue.
Instead, contact us privately at security@example.com.

## Questions?

- Open an issue for general questions
- Join our community Discord (link in README)
- Check the [documentation](./docs/architecture.md)

---

Thank you for contributing to MyTube!
