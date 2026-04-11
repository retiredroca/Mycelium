use serde::{Deserialize, Serialize};
use libp2p::PeerId;
use chrono::{DateTime, Utc};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GossipMessage {
    pub id: String,
    pub msg_type: super::myc_protocol::MsgType,
    pub sender: PeerId,
    pub timestamp: i64,
    pub payload: GossipPayload,
    pub ttl: u8,
    pub signature: Vec<u8>,
}

impl GossipMessage {
    pub fn new(msg_type: super::myc_protocol::MsgType, payload: GossipPayload) -> Self {
        Self {
            id: uuid::Uuid::new_v4().to_string(),
            msg_type,
            sender: PeerId::random(),
            timestamp: Utc::now().timestamp(),
            payload,
            ttl: 3,
            signature: Vec::new(),
        }
    }

    pub fn with_ttl(mut self, ttl: u8) -> Self {
        self.ttl = ttl;
        self
    }

    pub fn decrement_ttl(&mut self) -> bool {
        if self.ttl > 0 {
            self.ttl -= 1;
            true
        } else {
            false
        }
    }

    pub fn should_duplicate(&self) -> bool {
        self.ttl > 0
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum GossipPayload {
    #[serde(rename = "post")]
    Post {
        post: super::myc_post::Post,
    },
    #[serde(rename = "post_announcement")]
    PostAnnouncement {
        post_id: String,
        author: String,
        timestamp: i64,
    },
    #[serde(rename = "peer_announce")]
    PeerAnnounce {
        peer_id: String,
        capabilities: Vec<String>,
        endpoint: Option<String>,
    },
    #[serde(rename = "sync_request")]
    SyncRequest {
        since: i64,
        limit: usize,
    },
    #[serde(rename = "sync_response")]
    SyncResponse {
        posts: Vec<super::myc_post::Post>,
        more_available: bool,
    },
}

pub struct GossipConfig {
    pub max_payload_size: usize,
    pub default_ttl: u8,
    pub fanout_count: usize,
    pub prune_interval_secs: u64,
}

impl Default for GossipConfig {
    fn default() -> Self {
        Self {
            max_payload_size: 1024 * 1024, // 1MB
            default_ttl: 3,
            fanout_count: 6,
            prune_interval_secs: 300,
        }
    }
}

pub struct GossipManager {
    seen_messages: std::collections::HashSet<String>,
    config: GossipConfig,
}

impl GossipManager {
    pub fn new(config: GossipConfig) -> Self {
        Self {
            seen_messages: std::collections::HashSet::new(),
            config,
        }
    }

    pub fn is_duplicate(&self, msg_id: &str) -> bool {
        self.seen_messages.contains(msg_id)
    }

    pub fn mark_seen(&mut self, msg_id: String) {
        self.seen_messages.insert(msg_id);
    }

    pub fn prune_old_messages(&mut self, max_age_secs: i64) {
        let now = Utc::now().timestamp();
        let cutoff = now - max_age_secs;
        self.seen_messages.retain(|id| {
            if let Some(ts) = extract_timestamp(id) {
                ts > cutoff
            } else {
                true
            }
        });
    }

    pub fn fanout_targets(&self, peer_count: usize) -> Vec<usize> {
        let targets = self.config.fanout_count.min(peer_count);
        (0..targets).collect()
    }
}

fn extract_timestamp(msg_id: &str) -> Option<i64> {
    let parts: Vec<&str> = msg_id.split('-').collect();
    if parts.len() >= 2 {
        parts[1].parse().ok()
    } else {
        None
    }
}

pub mod epidemic {
    use super::*;

    pub struct EpidemicBroadcast {
        pending: std::collections::HashMap<String, Vec<PeerId>>,
        delivered: std::collections::HashSet<String>,
    }

    impl EpidemicBroadcast {
        pub fn new() -> Self {
            Self {
                pending: std::collections::HashMap::new(),
                delivered: std::collections::HashSet::new(),
            }
        }

        pub fn push(&mut self, msg_id: String, peers: Vec<PeerId>) {
            if !self.delivered.contains(&msg_id) {
                self.pending.insert(msg_id.clone(), peers);
            }
        }

        pub fn mark_delivered(&mut self, msg_id: &str, peer: PeerId) {
            self.delivered.insert(msg_id.to_string());
            if let Some(vec) = self.pending.get_mut(msg_id) {
                vec.retain(|p| *p != peer);
                if vec.is_empty() {
                    self.pending.remove(msg_id);
                }
            }
        }

        pub fn has_pending(&self, msg_id: &str) -> bool {
            self.pending.contains_key(msg_id)
        }

        pub fn pending_count(&self) -> usize {
            self.pending.len()
        }
    }

    impl Default for EpidemicBroadcast {
        fn default() -> Self {
            Self::new()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_gossip_message_ttl() {
        let mut msg = GossipMessage::new(
            super::super::myc_protocol::MsgType::PostCreated,
            GossipPayload::PostAnnouncement {
                post_id: "test".to_string(),
                author: "alice".to_string(),
                timestamp: Utc::now().timestamp(),
            },
        );
        
        assert_eq!(msg.ttl, 3);
        
        assert!(msg.decrement_ttl());
        assert_eq!(msg.ttl, 2);
        
        assert!(msg.decrement_ttl());
        assert_eq!(msg.ttl, 1);
        
        assert!(msg.decrement_ttl());
        assert_eq!(msg.ttl, 0);
        
        assert!(!msg.decrement_ttl());
    }

    #[test]
    fn test_duplicate_detection() {
        let mut manager = GossipManager::new(GossipConfig::default());
        let msg_id = "test-123".to_string();
        
        assert!(!manager.is_duplicate(&msg_id));
        manager.mark_seen(msg_id.clone());
        assert!(manager.is_duplicate(&msg_id));
    }
}
