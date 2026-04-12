---
name: Missing ProfileBuilder Method
about: Add set_username to ProfileBuilder
title: "[Bug] ProfileBuilder missing set_username method"
labels: bug, api-design
assignees: ''
---

## Problem

`ProfileBuilder` is missing the `set_username` method, but `Profile` has it.

## Error

```rust
error[E0599]: no method named `set_username` found for struct `ProfileBuilder`
```

## Location

`crates/myc-profile/src/profile.rs`

## Current Code

```rust
impl ProfileBuilder {
    pub fn new(peer_id: PeerId) -> Self { ... }
    
    // Missing: pub fn username(mut self, username: &str) -> Result<Self, ProfileError>
}
```

## Expected Implementation

```rust
pub fn username(mut self, username: &str) -> Result<Self, ProfileError> {
    let validator = ContentValidator::new();
    validator.validate_username(username)?;
    self.profile.username = Some(username.to_lowercase());
    Ok(self)
}
```

## Acceptance Criteria

- [ ] Add `username` method to `ProfileBuilder`
- [ ] Add test for username validation in builder
- [ ] Ensure consistency with `Profile::set_username`
