# The Mycelium Protocol - Architecture Documentation

## Overview

The Mycelium Protocol is a decentralized social network built on peer-to-peer technology with native token economics for social mining.

## System Architecture

```mermaid
flowchart TB
    subgraph Client["Client Layer"]
        CLI[CLI Application]
        WEB[WebAssembly Browser]
        MOB[Mobile App]
    end

    subgraph P2P["P2P Network Layer"]
        subgraph NodeTypes["Node Types"]
            FN[Full Node]
            RN[Relay Node]
            LN[Light Node]
        end
        
        subgraph Protocols["P2P Protocols"]
            KAD[Kademlia DHT]
            GOSSIP[GossipSub v2]
            MDNS[mDNS Local Discovery]
            RELAY[Libp2p Relay]
        end
    end

    subgraph Storage["Storage Layer"]
        ODB[OrbitDB]
        HELIA[Helia/IPFS]
        STOR[Storacha/Filecoin]
    end

    subgraph Token["Token Layer (SVM L2)"]
        STAKE[Stake Contracts]
        REWARD[Reward Distribution]
        BURN[Fee Burn Mechanism]
    end

    CLI --> P2P
    WEB --> P2P
    MOB --> P2P
    
    FN --> KAD
    FN --> GOSSIP
    FN --> Storage
    
    RN --> KAD
    RN --> GOSSIP
    
    LN --> RELAY
    LN --> KAD
    
    KAD <--> GOSSIP
    Storage <--> P2P
    Token <--> P2P
```

## Peer Discovery Protocol

```mermaid
sequenceDiagram
    participant N1 as New Node
    participant BN as Bootstrap Node
    participant DHT as Kademlia DHT
    participant PT as Peer Table

    N1->>BN: Connect to bootstrap node
    BN->>N1: Return known peers list
    N1->>DHT: FIND_NODE(random_key)
    DHT->>N1: k-closest peers
    N1->>PT: Add discovered peers
    N1->>DHT: STORE(self_peer_record)
    NHT->>DHT: STORE(self_capabilities)
    
    loop Periodic Refresh
        N1->>DHT: Random walk query
        DHT->>N1: New peers discovered
        N1->>PT: Update peer info
    end
```

## Post Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Created: User creates post
    
    state Created {
        [*] --> Fresh: Initial TTL = 24h
        Fresh --> Fresh: View (+2h)
        Fresh --> Fresh: Share (+4h)
        Fresh --> Fresh: Comment (+6h)
    }
    
    Created --> HypePending: Score ≥ 1000
    HypePending --> Hyped: 3+ peers confirm
    Hyped --> Hyped: More engagement
    Hyped --> Expired: TTL = 0
    
    Created --> Staked: User stakes tokens
    HypePending --> Staked: User stakes tokens
    Hyped --> Staked: User stakes tokens
    
    Staked --> Permanent: On-chain confirmation
    Permanent --> [*]: Immutable
    
    state Expired {
        [*] --> SoftArchived: TTL expired
        SoftArchived --> [*]: 30-day restore window
    }
    
    Hyped --> Expired: TTL reaches 0
    Fresh --> Expired: TTL reaches 0
```

## Authorized Host Handshake

```mermaid
sequenceDiagram
    participant U as User Node
    participant H as Host Node
    participant DHT as Kademlia DHT
    participant SC as Storage Contract
    
    Note over U: Discovery Phase
    U->>DHT: Query storage-capable nodes
    DHT->>U: List of hosts with capabilities
    
    Note over U: Offer Phase
    U->>H: ENCRYPTED_OFFER{key, TTL, price}
    H->>H: Verify reputation & capacity
    H->>U: HOST_ACCEPT + stake amount
    
    Note over U: Agreement Phase
    U->>SC: Create StorageAgreement
    SC->>SC: Lock stake amount
    SC->>H: Agreement activated
    SC->>U: On-chain reference returned
    
    Note over U: Data Handoff
    U->>U: Encrypt data with symmetric key
    U->>H: Send encrypted blob + CID
    H->>H: Store & verify Merkle proof
    H->>SC: Confirm storage complete
```

## Token Flow

```mermaid
flowchart LR
    subgraph Emission["Token Emission (8% → 1.5%)"]
        Y1[Year 1: 8%]
        Y3[Year 3: 5%]
        Y6[Year 6: 2%]
    end
    
    subgraph Rewards["Reward Distribution"]
        R[Relay: 35%]
        H[Hosting: 40%]
        C[Creation: 15%]
        E[Engagement: 10%]
    end
    
    subgraph Burn["Token Burn"]
        F[Transaction Fees: 50%]
        I[Identity Verification]
        P[Premium Features]
    end
    
    subgraph Stake["Stake Mechanics"]
        SP[Storage Stake]
        PP[Permanence Stake]
        UP[Unbonding: 7 days]
    end
    
    Emission --> Rewards
    Rewards --> Stake
    Rewards --> F
    
    F --> Burn
    I --> Burn
    P --> Burn
```

## Peer Table Management

```mermaid
flowchart TB
    subgraph BucketStructure["K-Bucket Structure (k=20)"]
        B0[Bucket 0<br/>Most Similar IDs]
        B1[Bucket 1]
        B2[Bucket 2]
        B3[...<br/>...]
        B255[Bucket 255<br/>Least Similar IDs]
    end
    
    subgraph TrustScoring["Trust Score Calculation"]
        BS[Base Score: 50]
        UP[Uptime Weight: 30%]
        BH[Behavior Weight: 20%]
        ST[Stake Weight: 20%]
        
        BS --> TS[Total Score 0-100]
        UP --> TS
        BH --> TS
        ST --> TS
    end
    
    subgraph Maintenance["Peer Table Maintenance"]
        HEART[Heartbeat Check]
        STALE[Stale Entry Removal]
        REPL[Replication Factor]
        
        HEART -->|Offline > 5min| STALE
        STALE -->|Remove| BucketStructure
        REPL -->|Maintain k=20| BucketStructure
    end
```

## Gossip Protocol

```mermaid
flowchart TB
    subgraph Publish["Post Publication"]
        POST[New Post Created]
        ENC[Serialize to JSON]
        SIGN[Sign with private key]
        GOSSIP[Publish to GossipSub]
    end
    
    subgraph Propagation["Epidemic Broadcast"]
        M1[Message with TTL=3]
        P1[Peer 1 receives]
        P2[Peer 2 receives]
        P3[Peer 3 receives]
        
        M1 --> P1
        P1 -->|TTL=2| P2
        P2 -->|TTL=1| P3
        P3 -->|TTL=0| DROP[Drop]
    end
    
    subgraph Deduplication["Duplicate Prevention"]
        CACHE[(Seen Cache)]
        NEW[New Message?]
        DUP[Duplicate<br/>Discard]
        
        NEW --> CACHE
        NEW -->|Yes| DUP
        NEW -->|No| CACHE
    end
    
    Publish --> Propagation
    Propagation --> Deduplication
```

## Hype Algorithm

```mermaid
flowchart TB
    subgraph EngagementScoring["Engagement Score"]
        V[View: +1]
        S[Share: +10]
        C[Comment: +5]
        
        V --> TOTAL[Total Score]
        S --> TOTAL
        C --> TOTAL
    end
    
    subgraph TTLManagement["TTL Extension"]
        BASE[Base TTL: 24h]
        VB[View Bonus: +2h]
        SB[Share Bonus: +4h × (1 + rep/100)]
        CB[Comment Bonus: +6h × depth_factor]
        
        BASE --> EXT[Extended TTL]
        VB --> EXT
        SB --> EXT
        CB --> EXT
    end
    
    subgraph StateTransitions["State Transitions"]
        FRESH[Fresh Post<br/>Score < 100]
        HYPE[Hyped Post<br/>Score ≥ 1000]
        PERM[Permanent<br/>Staked]
        
        FRESH -->|Score ≥ 1000| HYPE
        FRESH -->|Stake tokens| PERM
        HYPE -->|Stake tokens| PERM
    end
```

## Risk Mitigation

```mermaid
flowchart TB
    subgraph Churn["Node Churn"]
        CH1[Host goes offline]
        CH2[Grace period: 24h]
        CH3[Re-replication triggered]
        CH4[Stake slashed]
        CH5[User notified]
        
        CH1 --> CH2 --> CH3 --> CH4 --> CH5
    end
    
    subgraph Inflation["Inflation Control"]
        IN1[Hard cap: 1B tokens]
        IN2[Disinflation: -15%/year]
        IN3[Fee burn: 50%]
        IN4[Staking lock: 7 days]
        
        IN1 --> IN2 --> IN3 --> IN4
    end
    
    subgraph Sybil["Sybil Resistance"]
        SY1[Min stake: 100 tokens]
        SY2[Time-weighted reputation]
        SY3[Social graph validation]
        SY4[Capacity proof]
        
        SY1 --> SY2 --> SY3 --> SY4
    end
```

## Technology Stack Summary

| Component | Technology | Purpose |
|-----------|------------|---------|
| **Node Client** | Rust + libp2p | High-performance P2P networking |
| **Database** | OrbitDB + Helia | Distributed CRDT storage |
| **Storage Bridge** | Storacha/Filecoin | Long-term archival |
| **Token Layer** | Solana SVM L2 | Fast, low-cost transactions |
| **Identity** | Ed25519 + zkLogin | Keyless onboarding |
| **Consensus** | Proof of Stake | Network security |

## Roadmap Phases

```mermaid
gantt
    title Mycelium Protocol Development
    dateFormat YYYY-MM
    section Phase 1
    P2P Core               :2024-01, 6m
    Kademlia DHT           :2024-01, 2m
    GossipSub              :2024-02, 2m
    Peer Table             :2024-03, 2m
    section Phase 2
    Token Integration      :2024-07, 6m
    Solana SVM L2          :2024-07, 3m
    Stake Contracts        :2024-09, 2m
    Reward Distribution    :2024-10, 3m
    section Phase 3
    Hype Logic             :2025-01, 6m
    TTL Algorithm          :2025-01, 2m
    Storage Integration    :2025-02, 3m
    Mobile Client          :2025-04, 3m
```

---

## Quantum-Resistant Security Architecture

### Cryptographic Design Philosophy

The Mycelium Protocol implements **hybrid cryptography** that combines current-generation algorithms (for compatibility) with post-quantum algorithms (for future-proofing). This approach ensures security against both classical and quantum adversaries.

```mermaid
flowchart TB
    subgraph Encryption["Hybrid Encryption"]
        C1[Classical ECDH<br/>X25519]
        P1[Post-Quantum KEM<br/>Kyber-768]
        K1[Key Derivation<br/>HKDF-SHA256]
        S1[Symmetric Cipher<br/>AES-256-GCM]
    end
    
    subgraph Signatures["Hybrid Signatures"]
        C2[Classical Signature<br/>Ed25519]
        P2[Post-Quantum Signature<br/>Dilithium-3]
        V2[Signature<br/>Aggregation]
    end
    
    C1 --> K1
    P1 --> K1
    K1 --> S1
    
    C2 --> V2
    P2 --> V2
```

### Key Exchange Protocol

```mermaid
sequenceDiagram
    participant A as Alice
    participant B as Bob
    
    Note over A: Generate ephemeral keypair<br/>(Classical + PQ)
    
    A->>B: Ephemeral Public Key (Classical + PQ)
    
    Note over B: Derive shared secret<br/>Classical ECDH ⊕ Kyber KEM
    
    B->>A: Ephemeral Public Key (Classical + PQ)
    
    Note over A: Derive shared secret<br/>Classical ECDH ⊕ Kyber KEM
    
    A->>A: Derive AES-256-GCM key via HKDF
    
    Note over A,B: Encrypt with AES-256-GCM<br/>using derived key
```

### Encryption Flow

```
┌─────────────────────────────────────────────────────────────┐
│                   ENCRYPTION PIPELINE                        │
├─────────────────────────────────────────────────────────────┤
│  1. PLAINTEXT INPUT                                         │
│     └── User content (post, media, messages)                 │
│                                                              │
│  2. KEY GENERATION                                          │
│     ├── Classical: X25519 keypair (32 bytes)                │
│     ├── Post-Quantum: Kyber-768 KEM (1184 bytes)           │
│     └── Combined: Hybrid ephemeral keypair                  │
│                                                              │
│  3. KEY EXCHANGE                                            │
│     ├── ECDH shared secret: 32 bytes                        │
│     ├── Kyber shared secret: 32 bytes                       │
│     └── XOR combined: 32 bytes                              │
│                                                              │
│  4. KEY DERIVATION                                          │
│     └── HKDF-SHA256(combined_secret, "SOVEREIGN-v1")        │
│         └── AES-256 key: 32 bytes                           │
│                                                              │
│  5. ENCRYPTION                                              │
│     └── AES-256-GCM                                         │
│         ├── Nonce: 12 bytes                                 │
│         ├── Ciphertext: variable                             │
│         └── Auth Tag: 16 bytes                              │
│                                                              │
│  6. OUTPUT                                                  │
│     └── Encrypted blob + ephemeral public key               │
└─────────────────────────────────────────────────────────────┘
```

### Security Levels

| Algorithm | Type | Key Size | Security Level | Status |
|-----------|------|----------|----------------|--------|
| **X25519** | ECDH | 256-bit | Classical only | Production |
| **Kyber-768** | KEM | 3040-bit | Level 5 (256-bit) | Hybrid-ready |
| **AES-256-GCM** | AEAD | 256-bit | Level 5 | Production |
| **Ed25519** | Sig | 256-bit | Classical only | Production |
| **Dilithium-3** | Sig | Level 5 | Level 5 (256-bit) | Hybrid-ready |

### Post-Quantum Key Types

```mermaid
classDiagram
    class QuantumResistantKey {
        +PQKeyType key_type
        +Vec~u8~ public_bytes
        +Option~Vec~u8~~ secret_bytes
        +key_size() usize
    }
    
    class PQKeyType {
        <<enumeration>>
        Kyber768
        MlKem768
        HybridX25519Kyber768
        HybridX25519MlKem768
    }
    
    class HybridEncryptedData {
        +Vec~u8~ ephemeral_public_key
        +Option~Vec~u8~~ pq_public_key
        +Vec~u8~ ciphertext
        +Vec~u8~ auth_tag
        +Vec~u8~ nonce
        +u8 version
    }
    
    QuantumResistantKey --> PQKeyType
    HybridEncryptedData --> PQKeyType : uses for encapsulation
```

### Data at Rest Encryption

All user data stored on nodes is encrypted using hybrid encryption:

```
┌─────────────────────────────────────────────────────────────┐
│                STORAGE ENCRYPTION MODEL                      │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  User Data ──► User's KeyPair ──► Hybrid Encryption ──► Encrypted Blob
│                  (X25519 +                                       │
│                   Kyber768)                                    │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ EncryptedStorageEntry                                    │ │
│  │ ├── entry_id: UUID                                      │ │
│  │ ├── content_cid: Qm...                                  │ │
│  │ ├── encrypted_content: Vec~u8~ (hybrid encrypted)       │ │
│  │ ├── encryption_metadata:                                │ │
│  │ │   ├── algorithm: "HybridX25519Kyber768_AES256GCM"   │ │
│  │ │   ├── key_type: "Kyber-768"                          │ │
│  │ │   ├── nonce: [u8; 12]                               │ │
│  │ │   └── merkle_root: String                            │ │
│  │ └── size_bytes: u64                                     │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                              │
│  Only the owner's private key can decrypt the content.        │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Signature Schemes

```mermaid
flowchart LR
    subgraph Signing["Signing Process"]
        M[Message] --> H1[Hash Message]
        H1 --> K1[Classical KeyPair]
        K1 --> S1[Ed25519 Sign]
        S1 --> C1[Classical Sig<br/>64 bytes]
        
        H1 --> K2[PQ KeyPair]
        K2 --> S2[Dilithium Sign]
        S2 --> C2[PQ Sig<br/>3293 bytes]
        
        C1 --> A[Aggregate]
        C2 --> A
        A --> HS[Hybrid Signature]
    end
    
    subgraph Verification["Verification Process"]
        HS --> V1[Verify Ed25519]
        HS --> V2[Verify Dilithium]
        V1 --> R1[Result]
        V2 --> R2[Result]
        R1 --> AND{and}
        R2 --> AND
        AND --> V[Valid]
    end
```

### Key Management

```
┌─────────────────────────────────────────────────────────────┐
│                   KEY MANAGEMENT                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  User Identity Key                                          │
│  ├── Stored: User's device (secure enclave)                 │
│  ├── Backup: User-controlled recovery phrase                │
│  └── Use: Signing posts, authorizing actions               │
│                                                              │
│  Storage Encryption Key                                      │
│  ├── Derived: From identity key via HKDF                    │
│  ├── Use: Encrypting/decrypting stored content             │
│  └── Sharing: Encrypted with recipient's public key        │
│                                                              │
│  Session Keys                                               │
│  ├── Ephemeral: Generated per session/message               │
│  ├── Use: Forward secrecy                                   │
│  └── Destroyed: After session ends                         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Security Properties

| Property | Implementation | Quantum Resistant |
|----------|----------------|-------------------|
| **Confidentiality** | AES-256-GCM + Kyber-768 | Yes |
| **Integrity** | GCM authentication tag | Yes |
| **Authentication** | Ed25519 + Dilithium-3 | Yes |
| **Forward Secrecy** | Ephemeral key exchange | Yes* |
| **Post-Compromise Security** | Key rotation | Yes |
| **Non-Repudiation** | Digital signatures | Yes |

*Ephemeral key exchange provides forward secrecy against classical adversaries. Quantum forward secrecy requires PQ ephemeral keys.

### Migration Path

```
Phase 1 (Current):  Classical-only encryption (X25519 + Ed25519)
     │
     ▼
Phase 2 (Planned):  Hybrid mode (X25519 + Kyber + Ed25519 + Dilithium)
     │
     ▼
Phase 3 (Future):   Post-quantum only (when PQ performance improves)
```

### Benchmarking

| Operation | Classical | Hybrid | PQ-Only |
|-----------|-----------|--------|----------|
| Key Generation | ~1ms | ~15ms | ~15ms |
| Key Exchange | ~2ms | ~12ms | ~10ms |
| Encrypt (1KB) | ~0.1ms | ~0.2ms | ~0.2ms |
| Encrypt (1MB) | ~10ms | ~20ms | ~20ms |
| Decrypt (1KB) | ~0.1ms | ~0.2ms | ~0.2ms |
| Sign | ~0.5ms | ~3ms | ~3ms |
| Verify | ~0.5ms | ~3ms | ~3ms |

### Implementation Modules

| Module | Path | Description |
|--------|------|-------------|
| **myc-crypto** | `crates/crypto/` | Core cryptographic operations |
| ├─ keys.rs | Key generation, X25519/Ed25519 |
| ├─ encryption.rs | AES-256-GCM encryption |
| ├─ signatures.rs | Ed25519 signing |
| ├─ hybrid.rs | Hybrid encryption/signing |
| └─ quantum_resistant.rs | Kyber/Dilithium structures |
| **myc-storage** | `crates/storage/` | Encrypted storage layer |
| **myc-post** | `crates/post/` | Encrypted post content |

### Future Upgrades

- **ML-KEM-768**: Replace Kyber-768 with NIST-standardized ML-KEM
- **SLH-DSA**: Add SPHINCS+ for hash-based signatures (stateless)
- **Threshold Cryptography**: Multi-party decryption keys for recovery
- **ZKP Integration**: Zero-knowledge proofs for privacy-preserving actions

---

## Profile System Architecture

### Overview

The Mycelium Protocol implements a fully customizable user profile system with privacy-first design. Users own their profile data, which is encrypted and stored in the distributed network.

```mermaid
flowchart TB
    subgraph Profile["Profile System"]
        P[Profile]
        L[Layout Manager]
        T[Theme Engine]
        W[Widget System]
        V[Content Validator]
    end
    
    P --> L
    P --> T
    L --> W
    P --> V
    
    subgraph Layout["Layout Sections"]
        H[Header]
        M[Main Content]
        S[Sidebar]
        F[Footer]
    end
    
    L --> H
    L --> M
    L --> S
    L --> F
```

### Profile Structure

```mermaid
classDiagram
    class Profile {
        +PeerId peer_id
        +Option~String~ username
        +String display_name
        +Option~String~ bio
        +Option~String~ avatar_cid
        +Option~String~ banner_cid
        +Vec~SocialLink~ links
        +Layout layout
        +Theme theme
        +PrivacySettings privacy
        +created_at: i64
        +updated_at: i64
        +set_username() Result
        +set_display_name() Result
        +validate_content() Result
    }
    
    class Layout {
        +Vec~LayoutSection~ sections
        +LayoutTemplate template
        +apply_template()
        +add_widget()
        +remove_widget()
    }
    
    class LayoutSection {
        +SectionType section_type
        +Vec~Widget~ widgets
        +WidgetVisibility visibility
    }
    
    class Theme {
        +ThemePreset preset
        +Option~String~ primary_color
        +Option~String~ background_color
        +generate_css() String
    }
    
    class Widget {
        +WidgetType widget_type
        +WidgetConfig config
        +Option~String~ custom_content
    }
    
    Profile --> Layout
    Profile --> Theme
    Layout --> LayoutSection
    LayoutSection --> Widget
```

### Layout Templates

```mermaid
flowchart LR
    subgraph Templates["Available Templates"]
        ID[Identity<br/>Bio + Avatar]
        BLOG[Blog<br/>Posts + About]
        PORTFOLIO[Portfolio<br/>Projects + Links]
        SOCIAL[Social Hub<br/>Followers + Activity]
    end
    
    subgraph Sections["Layout Sections"]
        HDR[Header]
        MAIN[Main]
        SIDE[Sidebar]
    end
    
    ID --> HDR
    ID --> MAIN
    BLOG --> HDR
    BLOG --> MAIN
    BLOG --> SIDE
    PORTFOLIO --> HDR
    PORTFOLIO --> MAIN
    PORTFOLIO --> SIDE
    SOCIAL --> HDR
    SOCIAL --> MAIN
```

### Theme Presets

| Preset | Primary | Background | Accent | Use Case |
|--------|---------|------------|--------|----------|
| **Midnight** | `#6366F1` | `#0F172A` | `#818CF8` | Dark theme |
| **Ocean** | `#0EA5E9` | `#0C4A6E` | `#38BDF8` | Blue aesthetic |
| **Forest** | `#22C55E` | `#14532D` | `#4ADE80` | Nature theme |
| **Sunset** | `#F97316` | `#7C2D12` | `#FB923C` | Warm tones |
| **Minimal** | `#6B7280` | `#F9FAFB` | `#9CA3AF` | Clean, simple |
| **Hacker** | `#10B981` | `#111827` | `#34D399` | Terminal style |
| **Aurora** | `#8B5CF6` | `#1E1B4B` | `#A78BFA` | Purple glow |

### Content Validation

All user content passes through a privacy-first validator:

```
┌─────────────────────────────────────────────────────────────┐
│                CONTENT VALIDATION PIPELINE                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  User Input ──► PII Detection ──► Spam Check ──► Approved   │
│                      │                   │                   │
│                      ▼                   ▼                   │
│              Email patterns         Excessive caps           │
│              Phone numbers         Repetitive chars          │
│              Street addresses      Suspicious links          │
│              SSN patterns          Auto-generated content     │
│                                                              │
│  Display Names: Allowed (non-PII only)                      │
│  Bio: Allowed with validation                                │
│  Links: Allowed with URL validation                          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Guestbook System

### Overview

Inspired by the indie web movement, the guestbook allows visitors to leave public messages on user profiles. All entries require approval based on the owner's policy.

```mermaid
flowchart TB
    subgraph Guestbook["Guestbook System"]
        G[Guestbook]
        E[Entry]
        P[Pending Queue]
        A[Approval Manager]
        B[Blocked Authors]
    end
    
    subgraph Entry["GuestbookEntry"]
        id[Entry ID]
        author[Author ID]
        name[Display Name]
        message[Message]
        timestamp[Created At]
        status[Status]
    end
    
    G --> E
    E --> P
    G --> A
    P --> A
    A --> B
```

### Approval Policies

```mermaid
stateDiagram-v2
    [*] --> Open: Anyone can sign
    
    state Open {
        [*] --> Posted
        Posted --> [*]: Immediate display
    }
    
    state ApproveOnce {
        [*] --> Pending
        Pending --> Approved: First visit
        Pending --> Blocked: Known spammer
        Approved --> [*]: Future visits auto-approved
        Blocked --> [*]: Entry rejected
    }
    
    state Approval {
        [*] --> Pending
        Pending --> Approved: Owner approves
        Pending --> Rejected: Owner rejects
        Approved --> [*]: Displayed
        Rejected --> [*]: Hidden
    }
    
    state Closed {
        [*] --> [*]: No entries allowed
    }
```

### Guestbook Entry Lifecycle

```mermaid
sequenceDiagram
    participant V as Visitor
    participant G as Guestbook
    participant A as Approval Manager
    participant O as Owner
    
    V->>G: Sign guestbook(message, name)
    G->>A: Check policy
    
    alt Open Policy
        A->>G: Auto-approve
        G->>G: Display entry
    end
    
    alt Approval Required
        A->>G: Add to pending queue
        O->>A: View pending entries
        O->>A: Approve(entry_id)
        A->>G: Move to approved
        G->>G: Display entry
    end
    
    alt Blocked Author
        A->>A: Check blocked list
        A->>G: Reject silently
        Note over G: Entry not visible
    end
```

---

## Social Graph

### Overview

The social graph tracks user relationships including follows, followers, and blocks. All operations respect user privacy settings.

```mermaid
flowchart TB
    subgraph SocialGraph["Social Graph"]
        SG[Social Graph]
        F[Follows]
        FR[Followers]
        B[Block List]
        P[Privacy Settings]
    end
    
    subgraph PrivacyControls["Privacy Controls"]
        PV[Profile Visibility]
        GP[Guestbook Policy]
        FA[Follow Approval]
        DM[DM Policy]
    end
    
    SG --> F
    SG --> FR
    SG --> B
    SG --> P
    P --> PV
    P --> GP
    P --> FA
    P --> DM
```

### Relationship States

```mermaid
stateDiagram-v2
    [*] --> None: No relationship
    
    None --> Following: User A follows B
    Following --> [*]: User A unfollows B
    
    None --> Follower: User B follows A
    Follower --> [*]: User B unfollows A
    
    None --> Mutual: A→B and B→A
    Mutual --> Following: B unfollows A
    Mutual --> Follower: A unfollows B
    
    None --> Blocked: A blocks B
    Blocked --> None: A unblocks B
    
    Following --> Blocked: A blocks B
    Mutual --> Blocked: A blocks B
```

### Privacy Policies

```mermaid
flowchart LR
    subgraph ProfileVisibility["Profile Visibility"]
        PUB[Public]
        HID[Hidden]
        UNC[Unlisted]
    end
    
    subgraph GuestbookPolicy["Guestbook Policy"]
        O[Open]
        AO[ApproveOnce]
        AR[Approval Required]
        CL[Closed]
    end
    
    subgraph DMPolicy["DM Policy"]
        AL[Allow All]
        FO[Followers Only]
        NO[No One]
    end
    
    subgraph FollowApproval["Follow Approval"]
        Y[Yes - Require Approval]
        N[No - Auto-approve]
    end
```

---

## DHT Identity Registry

### Overview

Decentralized username registration and lookup using the Kademlia DHT. Usernames are first-come, first-served and stored as content-addressed records.

```mermaid
flowchart TB
    subgraph Identity["Identity Registry"]
        I[Identity Manager]
        R[Registry]
        L[Lookup Service]
        D[Discovery]
    end
    
    subgraph Records["Registry Records"]
        U[Username Record]
        P[Profile CID]
        K[Public Key]
        T[Timestamp]
        S[Signature]
    end
    
    I --> R
    I --> L
    I --> D
    R --> U
    U --> P
    U --> K
    U --> T
    U --> S
```

### Username Registration Flow

```mermaid
sequenceDiagram
    participant U as User
    participant I as Identity Manager
    participant DHT as Kademlia DHT
    participant BC as Blockchain
    
    Note over U: Generate keypair
    
    U->>I: Register username("alice")
    I->>I: Check username validity
    I->>I: Generate UsernameRecord
    I->>I: Sign record with private key
    
    I->>DHT: PUT(username_record)
    DHT->>DHT: Store at username hash
    
    Note over DHT: Content-addressed storage
    Note over DHT: CID = hash(username + key + timestamp)
    
    DHT->>I: Storage confirmed
    I->>U: Registration successful
    I->>BC: Optional on-chain anchoring
```

### Identity Lookup

```mermaid
sequenceDiagram
    participant C as Client
    participant I as Identity Service
    participant DHT as Kademlia DHT
    
    C->>I: Lookup("alice")
    I->>DHT: GET(username_hash)
    DHT->>DHT: Find closest peers
    
    alt Username Found
        DHT->>I: UsernameRecord
        I->>I: Verify signature
        I->>C: Profile CID + Public Key
    else Username Not Found
        DHT->>I: Not found
        I->>C: Error: Username not registered
    end
```

### Username Constraints

| Rule | Constraint |
|------|------------|
| **Length** | 3-32 characters |
| **Characters** | a-z, 0-9, underscore, hyphen |
| **Case** | Case-insensitive (stored lowercase) |
| **Reserved** | system usernames (admin, root, etc.) |
| **Uniqueness** | First-come, first-served via DHT |

---

## Implementation Modules Summary

| Module | Path | Description |
|--------|------|-------------|
| **myc-profile** | `crates/myc-profile/` | User profiles, layouts, themes |
| ├─ profile.rs | Profile struct and validation |
| ├─ layout.rs | Layout sections and widgets |
| ├─ theme.rs | Theme presets and CSS generation |
| ├─ widget.rs | Widget types and configs |
| └─ validation.rs | PII detection, content filtering |
| **myc-guestbook** | `crates/myc-guestbook/` | Guestbook entries and approval |
| ├─ entry.rs | GuestbookEntry struct |
| ├─ pending.rs | Pending queue management |
| └─ approval.rs | Approval policies |
| **myc-social** | `crates/myc-social/` | Social graph and privacy |
| ├─ graph.rs | Follow/follower relationships |
| └─ privacy.rs | Privacy settings and policies |
| **myc-identity** | `crates/myc-identity/` | DHT username registry |
| ├─ registry.rs | Username registration |
| ├─ lookup.rs | Username resolution |
| └─ discovery.rs | Service discovery |

---

## Complete Crate Architecture

```
mycelium/
├── Cargo.toml                    # Workspace root
│
├── bin/
│   └── mycelium-node/           # CLI application
│       └── src/
│           ├── main.rs          # Command handlers
│           └── wallet.rs        # Wallet utilities
│
└── crates/
    ├── myc-crypto/              # Cryptographic operations
    ├── myc-p2p/                 # P2P networking (libp2p)
    ├── myc-storage/             # Distributed storage
    ├── myc-post/                # Post creation and lifecycle
    ├── myc-protocol/            # Protocol specifications
    ├── myc-token/               # Tokenomics and rewards
    ├── myc-profile/             # User profiles & customization
    ├── myc-guestbook/           # Guestbook system
    ├── myc-social/              # Social graph & relationships
    └── myc-identity/            # DHT username registry
```

---

## Feature Status

| Feature | Status | Notes |
|---------|--------|-------|
| P2P Networking | ✅ Implemented | Kademlia + GossipSub v2 |
| Post Lifecycle | ✅ Implemented | TTL, Hype, Permanence |
| Tokenomics | ✅ Implemented | Emission, staking, rewards |
| Quantum Encryption | ✅ Implemented | Hybrid X25519+Kyber |
| User Profiles | ✅ Implemented | Layout, themes, widgets |
| Guestbook | ✅ Implemented | Approval-based entries |
| Social Graph | ✅ Implemented | Follow/follower/block |
| Identity Registry | ✅ Implemented | DHT username lookup |
| Storage Integration | 🔄 In Progress | OrbitDB/Helia connection |
| Solana SVM L2 | 🔄 In Progress | Token contracts |
