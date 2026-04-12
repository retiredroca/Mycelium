use chrono::Utc;
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SocialGraph {
    pub peer_id: Vec<u8>,
    pub followers: HashMap<String, FollowState>,
    pub following: HashSet<String>,
    pub blocked: HashSet<String>,
    pub pending_requests: HashMap<String, FollowRequest>,
    pub created_at: i64,
    pub updated_at: i64,
}

impl SocialGraph {
    pub fn new(peer_id: Vec<u8>) -> Self {
        let now = Utc::now().timestamp();
        Self {
            peer_id,
            followers: HashMap::new(),
            following: HashSet::new(),
            blocked: HashSet::new(),
            pending_requests: HashMap::new(),
            created_at: now,
            updated_at: now,
        }
    }

    pub fn follow(&mut self, target_id: &[u8]) -> Result<(), FollowError> {
        let target = Self::key_to_string(target_id);
        
        if self.is_self(&target) {
            return Err(FollowError::SelfFollow);
        }
        
        if self.is_blocked(&target) {
            return Err(FollowError::UserBlocked);
        }
        
        if self.following.contains(&target) {
            return Err(FollowError::AlreadyFollowing);
        }
        
        self.following.insert(target);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn unfollow(&mut self, target_id: &[u8]) -> Result<(), FollowError> {
        let target = Self::key_to_string(target_id);
        
        if !self.following.contains(&target) {
            return Err(FollowError::NotFollowing);
        }
        
        self.following.remove(&target);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn send_follow_request(&mut self, target_id: &[u8]) -> Result<(), FollowError> {
        let target = Self::key_to_string(target_id);
        
        if self.is_self(&target) {
            return Err(FollowError::SelfFollow);
        }
        
        if self.is_blocked(&target) {
            return Err(FollowError::UserBlocked);
        }
        
        if self.following.contains(&target) {
            return Err(FollowError::AlreadyFollowing);
        }
        
        let request = FollowRequest::new(self.peer_id.clone(), target_id.to_vec());
        self.pending_requests.insert(request.id.clone(), request);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn add_follower(&mut self, follower_id: &[u8], approved: bool) -> Result<(), FollowError> {
        let follower = Self::key_to_string(follower_id);
        
        if self.is_self(&follower) {
            return Err(FollowError::SelfFollow);
        }
        
        let state = FollowState::new(
            follower_id.to_vec(),
            approved,
        );
        
        self.followers.insert(follower, state);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn approve_follower(&mut self, follower_id: &[u8]) -> Result<(), FollowError> {
        let follower = Self::key_to_string(follower_id);
        
        let state = self.followers.get_mut(&follower)
            .ok_or(FollowError::FollowerNotFound)?;
        
        state.approve();
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn reject_follower(&mut self, follower_id: &[u8]) -> Result<(), FollowError> {
        let follower = Self::key_to_string(follower_id);
        self.followers.remove(&follower);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn remove_follower(&mut self, follower_id: &[u8]) -> Result<(), FollowError> {
        let follower = Self::key_to_string(follower_id);
        
        if !self.followers.contains_key(&follower) {
            return Err(FollowError::FollowerNotFound);
        }
        
        self.followers.remove(&follower);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn block(&mut self, target_id: &[u8]) -> Result<(), FollowError> {
        let target = Self::key_to_string(target_id);
        
        if self.is_self(&target) {
            return Err(FollowError::SelfFollow);
        }
        
        if self.blocked.contains(&target) {
            return Err(FollowError::AlreadyBlocked);
        }
        
        self.following.remove(&target);
        self.followers.remove(&target);
        self.blocked.insert(target);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn unblock(&mut self, target_id: &[u8]) -> Result<(), FollowError> {
        let target = Self::key_to_string(target_id);
        
        if !self.blocked.contains(&target) {
            return Err(FollowError::NotBlocked);
        }
        
        self.blocked.remove(&target);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn is_following(&self, target_id: &[u8]) -> bool {
        let target = Self::key_to_string(target_id);
        self.following.contains(&target)
    }

    pub fn is_follower(&self, peer_id: &[u8]) -> bool {
        let peer = Self::key_to_string(peer_id);
        self.followers.contains_key(&peer)
    }

    pub fn is_mutual(&self, peer_id: &[u8]) -> bool {
        self.is_following(peer_id) && self.is_follower(peer_id)
    }

    pub fn is_blocked(&self, target_id: &[u8]) -> bool {
        let target = Self::key_to_string(target_id);
        self.blocked.contains(&target)
    }

    pub fn followers_count(&self) -> usize {
        self.followers.values()
            .filter(|s| s.approved)
            .count()
    }

    pub fn following_count(&self) -> usize {
        self.following.len()
    }

    pub fn get_followers(&self, approved_only: bool) -> Vec<&FollowState> {
        self.followers.values()
            .filter(|s| !approved_only || s.approved)
            .collect()
    }

    pub fn get_following(&self) -> Vec<String> {
        self.following.iter().cloned().collect()
    }

    pub fn get_pending_requests(&self) -> Vec<&FollowRequest> {
        self.pending_requests.values().collect()
    }

    pub fn get_follower_requests(&self) -> Vec<&FollowState> {
        self.followers.values()
            .filter(|s| !s.approved)
            .collect()
    }

    fn is_self(&self, peer_key: &str) -> bool {
        let self_key = Self::key_to_string(&self.peer_id);
        self_key == peer_key
    }

    fn key_to_string(key: &[u8]) -> String {
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, key)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FollowState {
    pub peer_id: Vec<u8>,
    pub approved: bool,
    pub followed_at: i64,
    pub approved_at: Option<i64>,
}

impl FollowState {
    pub fn new(peer_id: Vec<u8>, approved: bool) -> Self {
        let now = Utc::now().timestamp();
        Self {
            peer_id,
            approved,
            followed_at: now,
            approved_at: if approved { Some(now) } else { None },
        }
    }

    pub fn approve(&mut self) {
        self.approved = true;
        self.approved_at = Some(Utc::now().timestamp());
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FollowRequest {
    pub id: String,
    pub from_peer_id: Vec<u8>,
    pub to_peer_id: Vec<u8>,
    pub created_at: i64,
    pub message: Option<String>,
}

impl FollowRequest {
    pub fn new(from_peer_id: Vec<u8>, to_peer_id: Vec<u8>) -> Self {
        Self {
            id: Uuid::new_v4().to_string(),
            from_peer_id,
            to_peer_id,
            created_at: Utc::now().timestamp(),
            message: None,
        }
    }

    pub fn with_message(mut self, message: &str) -> Self {
        self.message = Some(message.to_string());
        self
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Relationship {
    None,
    Following,
    Follower,
    Mutual,
    Blocked,
}

impl Relationship {
    pub fn from_graph(graph: &SocialGraph, peer_id: &[u8]) -> Self {
        if graph.is_blocked(peer_id) {
            return Relationship::Blocked;
        }
        if graph.is_mutual(peer_id) {
            return Relationship::Mutual;
        }
        if graph.is_following(peer_id) {
            return Relationship::Following;
        }
        if graph.is_follower(peer_id) {
            return Relationship::Follower;
        }
        Relationship::None
    }
}

#[derive(Debug, thiserror::Error)]
pub enum FollowError {
    #[error("Cannot follow yourself")]
    SelfFollow,
    
    #[error("Already following")]
    AlreadyFollowing,
    
    #[error("Not following")]
    NotFollowing,
    
    #[error("Follower not found")]
    FollowerNotFound,
    
    #[error("User is blocked")]
    UserBlocked,
    
    #[error("Already blocked")]
    AlreadyBlocked,
    
    #[error("Not blocked")]
    NotBlocked,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_follow() {
        let alice = vec![1, 2, 3];
        let bob = vec![4, 5, 6];
        
        let mut alice_graph = SocialGraph::new(alice);
        let mut bob_graph = SocialGraph::new(bob.clone());
        
        alice_graph.follow(&bob).unwrap();
        bob_graph.add_follower(&alice, true).unwrap();
        
        assert!(alice_graph.is_following(&bob));
        assert!(bob_graph.is_follower(&alice));
        assert!(bob_graph.is_mutual(&alice));
    }

    #[test]
    fn test_block_removes_follow() {
        let alice = vec![1, 2, 3];
        let bob = vec![4, 5, 6];
        
        let mut graph = SocialGraph::new(alice);
        
        graph.follow(&bob).unwrap();
        assert!(graph.is_following(&bob));
        
        graph.block(&bob).unwrap();
        assert!(!graph.is_following(&bob));
        assert!(graph.is_blocked(&bob));
    }

    #[test]
    fn test_follow_counts() {
        let mut graph = SocialGraph::new(vec![1, 2, 3]);
        
        graph.add_follower(&[4, 5, 6], true).unwrap();
        graph.add_follower(&[7, 8, 9], false).unwrap();
        
        assert_eq!(graph.followers_count(), 1);
    }
}
