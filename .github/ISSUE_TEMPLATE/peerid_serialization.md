---
name: PeerId Serialization
about: Implement Serialize/Deserialize for PeerId across crates
title: "[Feature] PeerId serialization support"
labels: enhancement, serialization
assignees: ''
---

## Problem

`PeerId` from `libp2p` doesn't implement `serde::Serialize` or `serde::Deserialize` by default. Multiple crates (`myc-social`, `myc-guestbook`, `myc-identity`) store `PeerId` in structs that need serialization.

## Error

```rust
error[E0277]: the trait bound `PeerId: serde::Serialize` is not satisfied
error[E0277]: the trait bound `PeerId: serde::Deserialize<'de>` is not satisfied
```

## Solution

Implement a wrapper type that serializes `PeerId` as a string (base58 encoding):

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SerializablePeerId(#[serde(skip)] PeerId);

impl Serialize for SerializablePeerId {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where S: Serializer {
        serializer.serialize_str(&self.0.to_base58())
    }
}

impl<'de> Deserialize<'de> for SerializablePeerId {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where D: Deserializer<'de> {
        let s = String::deserialize(deserializer)?;
        let peer_id = PeerId::from_base58(&s)
            .map_err(de::Error::custom)?;
        Ok(Self(peer_id))
    }
}
```

## Affected Crates

- [ ] `myc-social`
- [ ] `myc-guestbook`  
- [ ] `myc-identity`
- [ ] `myc-profile`

## Notes

Consider adding this to a shared `myc-types` crate for reuse across the workspace.
