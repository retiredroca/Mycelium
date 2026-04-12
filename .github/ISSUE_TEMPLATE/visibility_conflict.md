---
name: Visibility Trait/Type Conflict
about: Resolve Visibility naming conflict in myc-social
title: "[Refactor] Visibility trait/type naming conflict in myc-social"
labels: refactor, api-design
assignees: ''
---

## Problem

There's a naming conflict where `Visibility` is both a trait and a type in the same module.

## Error

```rust
error[E0782]: expected a type, found a trait
error[E0603]: unresolved item import `Visibility` is private
```

## Location

`crates/myc-social/src/privacy.rs` (or similar)

## Issue

The `Visibility` enum/type conflicts with a `Visibility` trait that may be used elsewhere.

## Suggested Solutions

### Option 1: Rename the enum
```rust
pub enum VisibilityLevel {
    Public,
    Followers,
    Private,
}
```

### Option 2: Use a module prefix
```rust
pub use privacy_types::Visibility as VisibilityLevel;
```

### Option 3: Make the trait more specific
```rust
pub trait ContentVisibility {
    fn visibility(&self) -> Visibility;
}
```

## Affected Code

Look for `Visibility` usage in:
- `myc-social/src/privacy.rs`
- `myc-social/src/graph.rs`
- Any public exports in `myc-social/src/lib.rs`

## Acceptance Criteria

- [ ] Resolve the naming conflict
- [ ] Update all usages
- [ ] Ensure no breaking changes to public API
- [ ] Add tests for visibility settings
