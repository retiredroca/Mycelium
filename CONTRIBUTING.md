# Contributing to MyTube Protocol

Thank you for your interest in contributing to MyTube!

## Code of Conduct

By participating in this project, you agree to maintain a respectful and inclusive
environment for everyone. We do not tolerate harassment or discrimination of any kind.

## Getting Started

### Development Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/retiredroca/Mycelium.git
   cd mycelium
   ```

2. **Prerequisites**
   - C++17 compiler (MSVC 2022+, GCC 9+, Clang 10+)
   - CMake 3.20+

3. **Build**
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```

### Running Tests

There is no separate test suite. The project is validated by building and running the CLI commands:

```bash
# Verify all commands execute without error
./build/Release/mycelium help
./build/Release/mycelium status
./build/Release/mycelium wallet create
./build/Release/mycelium wallet balance
./build/Release/mycelium profile create --display-name "Test" --username test
./build/Release/mycelium post --content "Hello"
./build/Release/mycelium video upload --video-id "v1" --duration 120000 --width 1920 --height 1080
./build/Release/mycelium video manifest

# Start with web UI (visit http://localhost:8080)
./build/Release/mycelium start --http-port 8080
```

### Code Style

- **No exceptions**, **no virtual dispatch**, **no async**
- All modules are single `static inline` headers in `src/`
- Fixed-size buffers (`std::array`) preferred over heap (`std::vector`)
- Integer error codes with `strerror`-style lookup tables
- Snake case for functions and variables
- 4-space indentation
- No `using namespace std;`

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
2. **Build successfully**: `cmake --build build --config Release`
3. **Run all CLI commands** to verify nothing is broken
4. **Update documentation** if needed
5. **Submit PR** with a clear description of changes

## Areas for Contribution

### High Priority

- [ ] Real TCP/UDP transport layer (currently types only, no socket code)
- [ ] Kademlia DHT integration with peer table
- [ ] Gossip protocol message propagation
- [ ] Real liboqs integration (ML-KEM, SLH-DSA) replacing PQ stubs

### Medium Priority

- [ ] Web UI improvements (video playback, dynamic content)
- [ ] Multiple simultaneous connections in HTTP server
- [ ] Wallet persistence across restarts
- [ ] Cross-platform testing (Linux, macOS)

### Lower Priority

- [ ] Performance optimization
- [ ] Additional compile-time verification
- [ ] Documentation improvements
- [ ] Internationalization

## Security Disclosures

If you discover a security vulnerability, please DO NOT open a public issue.
Instead, contact us privately at security@mytube.example.com.

## Questions?

- Open an issue for general questions
- Check the [documentation](./docs/architecture.md)

---

Thank you for contributing to MyTube!
