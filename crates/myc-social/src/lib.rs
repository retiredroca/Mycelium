pub mod graph;
pub mod privacy;

pub use graph::{SocialGraph, FollowState, Relationship, FollowRequest};
pub use privacy::{PrivacySettings, Visibility, GuestbookPolicy, DMPolicy};

use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;

pub struct SocialManager {
    graphs: Arc<RwLock<HashMap<String, SocialGraph>>>,
    privacy: Arc<RwLock<HashMap<String, PrivacySettings>>>,
}

impl SocialManager {
    pub fn new() -> Self {
        Self {
            graphs: Arc::new(RwLock::new(HashMap::new())),
            privacy: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    pub async fn get_graph(&self, peer_id: &[u8]) -> SocialGraph {
        let key = Self::key_to_string(peer_id);
        let mut graphs = self.graphs.write().await;
        
        graphs.entry(key).or_insert_with(|| SocialGraph::new(peer_id.to_vec())).clone()
    }

    pub async fn follow(
        &self,
        follower_id: &[u8],
        following_id: &[u8],
        requires_approval: bool,
    ) -> Result<(), SocialError> {
        let mut graph = self.get_graph(follower_id).await;
        
        if requires_approval {
            graph.send_follow_request(following_id)?;
        } else {
            graph.follow(following_id)?;
        }
        
        self.sync_graph(follower_id, &graph).await;
        Ok(())
    }

    pub async fn approve_follow(
        &self,
        owner_id: &[u8],
        follower_id: &[u8],
    ) -> Result<(), SocialError> {
        let mut graph = self.get_graph(owner_id).await;
        graph.approve_follower(follower_id)?;
        self.sync_graph(owner_id, &graph).await;
        Ok(())
    }

    pub async fn reject_follow(
        &self,
        owner_id: &[u8],
        follower_id: &[u8],
    ) -> Result<(), SocialError> {
        let mut graph = self.get_graph(owner_id).await;
        graph.reject_follower(follower_id)?;
        self.sync_graph(owner_id, &graph).await;
        Ok(())
    }

    pub async fn unfollow(
        &self,
        follower_id: &[u8],
        following_id: &[u8],
    ) -> Result<(), SocialError> {
        let mut graph = self.get_graph(follower_id).await;
        graph.unfollow(following_id)?;
        self.sync_graph(follower_id, &graph).await;
        Ok(())
    }

    pub async fn block(&self, blocker_id: &[u8], blocked_id: &[u8]) -> Result<(), SocialError> {
        let mut graph = self.get_graph(blocker_id).await;
        graph.block(blocked_id)?;
        self.sync_graph(blocker_id, &graph).await;
        Ok(())
    }

    pub async fn unblock(&self, blocker_id: &[u8], blocked_id: &[u8]) -> Result<(), SocialError> {
        let mut graph = self.get_graph(blocker_id).await;
        graph.unblock(blocked_id)?;
        self.sync_graph(blocker_id, &graph).await;
        Ok(())
    }

    pub async fn is_following(&self, follower_id: &[u8], following_id: &[u8]) -> bool {
        let graph = self.get_graph(follower_id).await;
        graph.is_following(following_id)
    }

    pub async fn is_blocked(&self, blocker_id: &[u8], blocked_id: &[u8]) -> bool {
        let graph = self.get_graph(blocker_id).await;
        graph.is_blocked(blocked_id)
    }

    pub async fn get_privacy(&self, peer_id: &[u8]) -> PrivacySettings {
        let key = Self::key_to_string(peer_id);
        let privacy = self.privacy.read().await;
        privacy.get(&key).cloned().unwrap_or_default()
    }

    pub async fn set_privacy(&self, peer_id: &[u8], settings: PrivacySettings) {
        let key = Self::key_to_string(peer_id);
        let mut privacy = self.privacy.write().await;
        privacy.insert(key, settings);
    }

    async fn sync_graph(&self, peer_id: &[u8], graph: &SocialGraph) {
        let key = Self::key_to_string(peer_id);
        let mut graphs = self.graphs.write().await;
        graphs.insert(key, graph.clone());
    }

    fn key_to_string(key: &[u8]) -> String {
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, key)
    }
}

impl Default for SocialManager {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, thiserror::Error)]
pub enum SocialError {
    #[error("Already following")]
    AlreadyFollowing,
    
    #[error("Not following")]
    NotFollowing,
    
    #[error("Already blocked")]
    AlreadyBlocked,
    
    #[error("Cannot follow yourself")]
    SelfFollow,
    
    #[error("User is blocked")]
    UserBlocked,
    
    #[error("Follow request not found")]
    RequestNotFound,
    
    #[error("Invalid peer ID")]
    InvalidPeerId,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_follow_flow() {
        let manager = SocialManager::new();
        
        let alice = vec![1, 2, 3];
        let bob = vec![4, 5, 6];
        
        manager.follow(&alice, &bob, false).await.unwrap();
        
        assert!(manager.is_following(&alice, &bob).await);
    }

    #[tokio::test]
    async fn test_block() {
        let manager = SocialManager::new();
        
        let alice = vec![1, 2, 3];
        let bob = vec![4, 5, 6];
        
        manager.block(&alice, &bob).await.unwrap();
        
        assert!(manager.is_blocked(&alice, &bob).await);
    }
}
