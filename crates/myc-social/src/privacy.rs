use serde::{Deserialize, Serialize};
use crate::myc_profile::layout::Visibility;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PrivacySettings {
    pub profile_visibility: Visibility,
    pub guestbook_policy: GuestbookPolicy,
    pub follow_approval: bool,
    pub show_followers: Visibility,
    pub show_following: Visibility,
    pub allow_dm_from: DMPolicy,
    pub show_guestbook_entries: bool,
    pub show_posts: bool,
    pub indexed_by_search: bool,
}

impl Default for PrivacySettings {
    fn default() -> Self {
        Self {
            profile_visibility: Visibility::Public,
            guestbook_policy: GuestbookPolicy::Approval,
            follow_approval: true,
            show_followers: Visibility::Public,
            show_following: Visibility::Public,
            allow_dm_from: DMPolicy::FollowersOnly,
            show_guestbook_entries: true,
            show_posts: true,
            indexed_by_search: true,
        }
    }
}

impl PrivacySettings {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn private() -> Self {
        Self {
            profile_visibility: Visibility::Private,
            guestbook_policy: GuestbookPolicy::Closed,
            follow_approval: true,
            show_followers: Visibility::Private,
            show_following: Visibility::Private,
            allow_dm_from: DMPolicy::Nobody,
            show_guestbook_entries: false,
            show_posts: false,
            indexed_by_search: false,
        }
    }

    pub fn public() -> Self {
        Self {
            profile_visibility: Visibility::Public,
            guestbook_policy: GuestbookPolicy::ApproveOnce,
            follow_approval: false,
            show_followers: Visibility::Public,
            show_following: Visibility::Public,
            allow_dm_from: DMPolicy::Everyone,
            show_guestbook_entries: true,
            show_posts: true,
            indexed_by_search: true,
        }
    }

    pub fn set_profile_visibility(&mut self, visibility: Visibility) {
        self.profile_visibility = visibility;
    }

    pub fn set_guestbook_policy(&mut self, policy: GuestbookPolicy) {
        self.guestbook_policy = policy;
    }

    pub fn set_follow_approval(&mut self, required: bool) {
        self.follow_approval = required;
    }

    pub fn can_view_profile(&self, is_owner: bool, is_follower: bool) -> bool {
        self.profile_visibility.is_visible(is_owner, is_follower)
    }

    pub fn can_sign_guestbook(&self, is_follower: bool) -> bool {
        match self.guestbook_policy {
            GuestbookPolicy::Open => true,
            GuestbookPolicy::ApproveOnce => true,
            GuestbookPolicy::Approval => true,
            GuestbookPolicy::Closed => false,
        }
    }

    pub fn requires_guestbook_approval(&self, is_first_time: bool) -> bool {
        match self.guestbook_policy {
            GuestbookPolicy::Open => false,
            GuestbookPolicy::ApproveOnce => is_first_time,
            GuestbookPolicy::Approval => true,
            GuestbookPolicy::Closed => true,
        }
    }

    pub fn can_send_dm(&self, is_follower: bool) -> bool {
        match self.allow_dm_from {
            DMPolicy::Everyone => true,
            DMPolicy::FollowersOnly => is_follower,
            DMPolicy::Nobody => false,
        }
    }

    pub fn can_view_followers(&self, is_owner: bool, is_follower: bool) -> bool {
        self.show_followers.is_visible(is_owner, is_follower)
    }

    pub fn can_view_following(&self, is_owner: bool, is_follower: bool) -> bool {
        self.show_following.is_visible(is_owner, is_follower)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum GuestbookPolicy {
    Open,
    ApproveOnce,
    Approval,
    Closed,
}

impl Default for GuestbookPolicy {
    fn default() -> Self {
        Self::Approval
    }
}

impl GuestbookPolicy {
    pub fn as_str(&self) -> &'static str {
        match self {
            GuestbookPolicy::Open => "open",
            GuestbookPolicy::ApproveOnce => "approve_once",
            GuestbookPolicy::Approval => "approval",
            GuestbookPolicy::Closed => "closed",
        }
    }

    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "open" => Some(GuestbookPolicy::Open),
            "approve_once" | "once" => Some(GuestbookPolicy::ApproveOnce),
            "approval" | "approve" => Some(GuestbookPolicy::Approval),
            "closed" => Some(GuestbookPolicy::Closed),
            _ => None,
        }
    }

    pub fn display_name(&self) -> &'static str {
        match self {
            GuestbookPolicy::Open => "Open (auto-approve)",
            GuestbookPolicy::ApproveOnce => "Approve Once (first-time visitors)",
            GuestbookPolicy::Approval => "Require Approval",
            GuestbookPolicy::Closed => "Closed",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DMPolicy {
    Everyone,
    FollowersOnly,
    Nobody,
}

impl Default for DMPolicy {
    fn default() -> Self {
        Self::FollowersOnly
    }
}

impl DMPolicy {
    pub fn as_str(&self) -> &'static str {
        match self {
            DMPolicy::Everyone => "everyone",
            DMPolicy::FollowersOnly => "followers_only",
            DMPolicy::Nobody => "nobody",
        }
    }

    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "everyone" | "all" => Some(DMPolicy::Everyone),
            "followers_only" | "followers" => Some(DMPolicy::FollowersOnly),
            "nobody" | "none" => Some(DMPolicy::Nobody),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlockList {
    pub peer_id: Vec<u8>,
    pub blocked_peers: Vec<BlockedEntry>,
}

impl BlockList {
    pub fn new(peer_id: Vec<u8>) -> Self {
        Self {
            peer_id,
            blocked_peers: Vec::new(),
        }
    }

    pub fn add(&mut self, blocked_peer_id: Vec<u8>, reason: Option<&str>) {
        let entry = BlockedEntry::new(blocked_peer_id, reason);
        self.blocked_peers.push(entry);
    }

    pub fn remove(&mut self, blocked_peer_id: &[u8]) -> bool {
        let len = self.blocked_peers.len();
        self.blocked_peers.retain(|e| &e.peer_id != blocked_peer_id);
        self.blocked_peers.len() != len
    }

    pub fn contains(&self, peer_id: &[u8]) -> bool {
        self.blocked_peers.iter().any(|e| &e.peer_id == peer_id)
    }

    pub fn len(&self) -> usize {
        self.blocked_peers.len()
    }

    pub fn is_empty(&self) -> bool {
        self.blocked_peers.is_empty()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BlockedEntry {
    pub peer_id: Vec<u8>,
    pub reason: Option<String>,
    pub blocked_at: i64,
}

impl BlockedEntry {
    pub fn new(peer_id: Vec<u8>, reason: Option<&str>) -> Self {
        Self {
            peer_id,
            reason: reason.map(|s| s.to_string()),
            blocked_at: chrono::Utc::now().timestamp(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_privacy_defaults() {
        let settings = PrivacySettings::default();
        
        assert!(matches!(settings.profile_visibility, Visibility::Public));
        assert!(matches!(settings.guestbook_policy, GuestbookPolicy::Approval));
        assert!(settings.follow_approval);
    }

    #[test]
    fn test_private_profile() {
        let settings = PrivacySettings::private();
        
        assert!(matches!(settings.profile_visibility, Visibility::Private));
        assert!(matches!(settings.guestbook_policy, GuestbookPolicy::Closed));
        assert!(!settings.show_posts);
    }

    #[test]
    fn test_guestbook_policy() {
        assert!(GuestbookPolicy::from_str("open").is_some());
        assert!(GuestbookPolicy::from_str("closed").is_some());
        assert!(GuestbookPolicy::from_str("invalid").is_none());
    }

    #[test]
    fn test_block_list() {
        let mut blocklist = BlockList::new(vec![1, 2, 3]);
        
        blocklist.add(vec![4, 5, 6], Some("spam"));
        assert!(blocklist.contains(&[4, 5, 6]));
        
        blocklist.remove(&[4, 5, 6]);
        assert!(!blocklist.contains(&[4, 5, 6]));
    }
}
