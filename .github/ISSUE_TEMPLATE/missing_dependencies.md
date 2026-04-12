---
name: Missing Crate Dependencies
about: Add missing inter-crate dependencies
title: "[Bug] Missing crate dependencies in Cargo.toml files"
labels: bug, dependencies
assignees: ''
---

## Problem

Several crates reference other crates in the workspace but don't have them listed as dependencies.

## Missing Dependencies

### myc-protocol/Cargo.toml
```toml
# Missing:
libp2p = { workspace = true }
```

### mycelium-node/Cargo.toml  
```toml
# Missing:
myc-crypto = { path = "../../crates/myc-crypto" }
hex = { workspace = true }
```

## Error

```rust
error[E0433]: failed to resolve: could not find `myc_social` in the crate root
error[E0432]: unresolved import `libp2p::pubsub`
```

## Fix

Update the respective `Cargo.toml` files to include the missing dependencies.

## Verification

Run `cargo check` after fixing to ensure no dependency errors remain.
