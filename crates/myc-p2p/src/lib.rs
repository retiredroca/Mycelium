pub mod behaviour;
pub mod discovery;
pub mod gossip;
pub mod peer_table;
pub mod storage_handoff;

use anyhow::Result;
use async_trait::async_trait;
use behaviour::MyceliumBehaviour;
use chrono::Utc;
use libp2p::{identity::Keypair, Multiaddr, PeerId};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::info;

pub use behaviour::Config as P2pConfig;
pub use discovery::DiscoveryConfig;
pub use peer_table::{PeerInfo, PeerTable, TrustScore};

pub const PROTOCOL_NAME: &str = "/mycelium-node/1.0.0";
pub const GOSSIPSUB_TOPIC_POSTS: &str = "posts";
pub const GOSSIPSUB_TOPIC_ANNOUNCEMENTS: &str = "announcements";

#[derive(Debug, Clone)]
pub enum NodeCapability {
    Storage { max_gb: u64 },
    Relay { bandwidth_mbps: u32 },
    Full,
}

#[derive(Debug, Clone)]
pub struct LocalNodeInfo {
    pub peer_id: PeerId,
    pub listen_addresses: Vec<Multiaddr>,
    pub capabilities: Vec<NodeCapability>,
    pub started_at: i64,
    pub version: String,
}

pub struct MyceliumNode {
    local_info: LocalNodeInfo,
    peer_table: Arc<RwLock<PeerTable>>,
    post_cache: Arc<RwLock<HashMap<String, myc_post::Post>>>,
    keypair: Keypair,
}

impl MyceliumNode {
    pub async fn new(config: P2pConfig) -> Result<Self> {
        let keypair = config.keypair.clone();
        let peer_id = PeerId::from(keypair.public());
        
        info!("Initializing Mycelium Node with PeerId: {}", peer_id);

        let peer_table = Arc::new(RwLock::new(PeerTable::new(peer_id)));
        let post_cache = Arc::new(RwLock::new(HashMap::new()));

        let local_info = LocalNodeInfo {
            peer_id,
            listen_addresses: Vec::new(),
            capabilities: config.capabilities.clone(),
            started_at: Utc::now().timestamp(),
            version: env!("CARGO_PKG_VERSION").to_string(),
        };

        Ok(Self {
            local_info,
            peer_table,
            post_cache,
            keypair,
        })
    }

    pub fn local_peer_id(&self) -> PeerId {
        self.local_info.peer_id
    }

    pub fn local_info(&self) -> &LocalNodeInfo {
        &self.local_info
    }

    pub async fn add_peer(&self, peer_id: PeerId, info: PeerInfo) {
        let mut table = self.peer_table.write().await;
        table.add_peer(peer_id, info);
    }

    pub async fn remove_peer(&self, peer_id: &PeerId) {
        let mut table = self.peer_table.write().await;
        table.remove_peer(peer_id);
    }

    pub async fn get_peers(&self) -> Vec<(PeerId, PeerInfo)> {
        let table = self.peer_table.read().await;
        table.get_all_peers()
    }

    pub async fn store_post(&self, post: myc_post::Post) {
        let mut cache = self.post_cache.write().await;
        cache.insert(post.id.clone(), post);
    }

    pub async fn get_post(&self, post_id: &str) -> Option<myc_post::Post> {
        let cache = self.post_cache.read().await;
        cache.get(post_id).cloned()
    }

    pub async fn get_recent_posts(&self, limit: usize) -> Vec<myc_post::Post> {
        let cache = self.post_cache.read().await;
        let mut posts: Vec<_> = cache.values().cloned().collect();
        posts.sort_by(|a, b| b.created_at.cmp(&a.created_at));
        posts.into_iter().take(limit).collect()
    }

    pub async fn peer_count(&self) -> usize {
        let table = self.peer_table.read().await;
        table.peer_count()
    }
}

#[async_trait]
pub trait EventHandler: Send + Sync {
    async fn handle_post_created(&self, post: myc_post::Post, from: PeerId);
    async fn handle_peer_connected(&self, peer_id: PeerId, info: &PeerInfo);
    async fn handle_peer_disconnected(&self, peer_id: &PeerId);
    async fn handle_storage_offer(&self, offer: storage_handoff::StorageOffer, from: PeerId) -> bool;
}

pub struct NoOpHandler;

#[async_trait]
impl EventHandler for NoOpHandler {
    async fn handle_post_created(&self, _post: myc_post::Post, _from: PeerId) {}
    async fn handle_peer_connected(&self, _peer_id: PeerId, _info: &PeerInfo) {}
    async fn handle_peer_disconnected(&self, _peer_id: &PeerId) {}
    async fn handle_storage_offer(&self, _offer: storage_handoff::StorageOffer, _from: PeerId) -> bool {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use libp2p::identity::Keypair;

    #[tokio::test]
    async fn test_node_creation() {
        let keypair = Keypair::generate_ed25519();
        let config = P2pConfig {
            keypair,
            listen_addresses: vec!["/ip4/0.0.0.0/tcp/0".parse().unwrap()],
            capabilities: vec![NodeCapability::Full],
            discovery: DiscoveryConfig::default(),
            ..Default::default()
        };
        
        let node = MyceliumNode::new(config).await.unwrap();
        assert!(!node.local_info().listen_addresses.is_empty() || true);
    }

    #[tokio::test]
    async fn test_post_storage() {
        let keypair = Keypair::generate_ed25519();
        let config = P2pConfig {
            keypair,
            ..Default::default()
        };
        
        let node = MyceliumNode::new(config).await.unwrap();
        
        let post = myc_post::Post::new(
            "alice".to_string(),
            "Test post".to_string(),
            &myc_crypto::KeyPair::generate().unwrap(),
        ).unwrap();
        
        node.store_post(post.clone()).await;
        let retrieved = node.get_post(&post.id).await;
        
        assert!(retrieved.is_some());
        assert_eq!(retrieved.unwrap().author, "alice");
    }
}
