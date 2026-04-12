---
name: Compilation Error
about: Fix compilation errors preventing build
title: "[Compilation Error] "
labels: bug, compilation
assignees: ''
---

## Description

The codebase fails to compile with multiple errors across several crates.

## Error Summary

### 1. Missing `serde::Serialize/Deserialize` for `PeerId`
**Affected files:** `myc-social`, `myc-guestbook`, `myc-identity`
```
error[E0277]: the trait bound `PeerId: serde::Serialize` is not satisfied
error[E0277]: the trait bound `PeerId: serde::Deserialize<'de>` is not satisfied
```

### 2. Missing crate path references
**Affected files:** `myc-protocol`, `myc-sync`
```
error[E0433]: failed to resolve: could not find `myc_social` in the crate root
error[E0433]: failed to resolve: could not find `myc_profile` in the crate root
```

### 3. `Visibility` trait vs type confusion
**Affected files:** `myc-social`
```
error[E0782]: expected a type, found a trait
error[E0603]: unresolved item import `Visibility` is private
```

### 4. Missing `ProfileBuilder::set_username` method
**Affected files:** `myc-profile`
```
error[E0599]: no method named `set_username` found for struct `ProfileBuilder`
```

### 5. `PeerId::from_base58` not found
**Affected files:** `myc-profile`
```
error[E0599]: no function or associated item named `from_base58` found for struct `PeerId`
```

### 6. Duplicate `DiscoverySource` definition
**Affected files:** `myc-sync`
```
error[E0428]: the name `DiscoverySource` is defined multiple times
```

### 7. Missing `libp2p::pubsub` import
**Affected files:** `myc-sync`
```
error[E0432]: unresolved import `libp2p::pubsub`
```

### 8. `SocialError` conversion issues
**Affected files:** `myc-social`
```
error[E0277]: `?` couldn't convert the error to `SocialError`
```

## Steps to Reproduce

```bash
cargo check
```

## Expected Behavior

The codebase should compile without errors.

## Actual Behavior

Compilation fails with ~30+ errors.

## Additional Context

Run `cargo check 2>&1 | grep "error\[E" | head -50` to see all errors.
