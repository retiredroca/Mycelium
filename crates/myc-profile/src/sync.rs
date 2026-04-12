use crate::{Profile, PublicProfile, compute_profile_cid};
use anyhow::Result;
use libp2p::{PeerId, kad, pubsub, identity};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

pub const PROFILE_TOPIC: &str = "/mycelium/profiles/1.0.0";
pub const PROFILE_KAD_NAMESPACE: &str = "/mycelium/profile";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProfileMessage {
    pub msg_type: ProfileMessageType,
    pub peer_id: Vec<u8>,
    pub profile: Option<Profile>,
    pub profile_cid: Option<String>,
    pub username: Option<String>,
    pub timestamp: i64,
    pub signature: Vec<u8>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum ProfileMessageType {
    ProfileUpdate,
    ProfileDelete,
    UsernameRegister,
    UsernameUpdate,
    UsernameDelete,
    ProfileRequest,
}

impl ProfileMessage {
    pub fn new_update(profile: Profile) -> Self {
        let peer_id = profile.peer_id.clone();
        let profile_cid = compute_profile_cid(&profile);
        let username = profile.username.clone();
        
        Self {
            msg_type: ProfileMessageType::ProfileUpdate,
            peer_id,
            profile: Some(profile),
            profile_cid: Some(profile_cid.clone()),
            username,
            timestamp: chrono::Utc::now().timestamp(),
            signature: Vec::new(),
        }
    }
    
    pub fn new_delete(peer_id: PeerId) -> Self {
        Self {
            msg_type: ProfileMessageType::ProfileDelete,
            peer_id: peer_id.to_bytes(),
            profile: None,
            profile_cid: None,
            username: None,
            timestamp: chrono::Utc::now().timestamp(),
            signature: Vec::new(),
        }
    }
    
    pub fn new_username_register(peer_id: PeerId, username: &str, profile_cid: &str) -> Self {
        Self {
            msg_type: ProfileMessageType::UsernameRegister,
            peer_id: peer_id.to_bytes(),
            profile: None,
            profile_cid: Some(profile_cid.to_string()),
            username: Some(username.to_lowercase()),
            timestamp: chrono::Utc::now().timestamp(),
            signature: Vec::new(),
        }
    }
    
    pub fn new_request(peer_id: PeerId, target_peer_id: &PeerId) -> Self {
        Self {
            msg_type: ProfileMessageType::ProfileRequest,
            peer_id: peer_id.to_bytes(),
            profile: None,
            profile_cid: None,
            username: None,
            timestamp: chrono::Utc::now().timestamp(),
            signature: target_peer_id.to_bytes(),
        }
    }
    
    pub fn target_peer_id(&self) -> Option<PeerId> {
        if self.msg_type == ProfileMessageType::ProfileRequest {
            PeerId::from_bytes(&self.signature).ok()
        } else {
            PeerId::from_bytes(&self.peer_id).ok()
        }
    }
    
    pub fn author_peer_id(&self) -> Option<PeerId> {
        PeerId::from_bytes(&self.peer_id).ok()
    }
    
    pub fn to_bytes(&self) -> Result<Vec<u8>> {
        Ok(serde_json::to_vec(self)?)
    }
    
    pub fn from_bytes(bytes: &[u8]) -> Result<Self> {
        Ok(serde_json::from_slice(bytes)?)
    }
}

#[derive(Debug, Clone)]
pub struct ProfileSyncService {
    peer_id: PeerId,
    cached_profiles: HashMap<String, CachedProfile>,
    username_index: HashMap<String, String>,
}

#[derive(Debug, Clone)]
struct CachedProfile {
    profile: Profile,
    profile_cid: String,
    received_at: i64,
    peer_id_str: String,
}

impl ProfileSyncService {
    pub fn new(peer_id: PeerId) -> Self {
        Self {
            peer_id,
            cached_profiles: HashMap::new(),
            username_index: HashMap::new(),
        }
    }
    
    pub fn handle_message(&mut self, message: ProfileMessage) -> Result<Option<ProfileMessage>> {
        let author_peer_id = match message.author_peer_id() {
            Some(id) => id,
            None => return Ok(None),
        };
        
        if author_peer_id == self.peer_id {
            return Ok(None);
        }
        
        match message.msg_type {
            ProfileMessageType::ProfileUpdate => {
                if let Some(profile) = message.profile {
                    let profile_cid = compute_profile_cid(&profile);
                    
                    if let Some(username) = &profile.username {
                        self.username_index.insert(username.clone(), profile_cid.clone());
                    }
                    
                    self.cached_profiles.insert(
                        profile_cid.clone(),
                        CachedProfile {
                            profile,
                            profile_cid: profile_cid.clone(),
                            received_at: chrono::Utc::now().timestamp(),
                            peer_id_str: author_peer_id.to_base58(),
                        },
                    );
                }
                Ok(None)
            }
            
            ProfileMessageType::ProfileDelete => {
                let peer_id_str = author_peer_id.to_base58();
                let to_remove: Vec<String> = self.cached_profiles
                    .iter()
                    .filter(|(_, p)| p.peer_id_str == peer_id_str)
                    .map(|(k, _)| k.clone())
                    .collect();
                
                for key in to_remove {
                    self.cached_profiles.remove(&key);
                }
                
                let username_to_remove: Vec<String> = self.username_index
                    .iter()
                    .filter(|(_, v)| self.cached_profiles.contains_key(*v))
                    .filter_map(|(k, v)| {
                        self.cached_profiles.get(v).filter(|p| p.peer_id_str == peer_id_str).map(|_| k.clone())
                    })
                    .collect();
                
                for username in username_to_remove {
                    self.username_index.remove(&username);
                }
                
                Ok(None)
            }
            
            ProfileMessageType::UsernameRegister | ProfileMessageType::UsernameUpdate => {
                if let (Some(username), Some(profile_cid)) = (&message.username, &message.profile_cid) {
                    self.username_index.insert(username.to_lowercase(), profile_cid.clone());
                }
                Ok(None)
            }
            
            ProfileMessageType::UsernameDelete => {
                if let Some(username) = &message.username {
                    self.username_index.remove(&username.to_lowercase());
                }
                Ok(None)
            }
            
            ProfileMessageType::ProfileRequest => {
                let target = message.target_peer_id();
                if target == Some(self.peer_id) || target.is_none() {
                    if let Some(profile) = self.get_profile_by_peer_id(&author_peer_id.to_base58()) {
                        return Ok(Some(ProfileMessage::new_update(profile)));
                    }
                }
                Ok(None)
            }
        }
    }
    
    pub fn get_profile(&self, profile_cid: &str) -> Option<Profile> {
        self.cached_profiles.get(profile_cid).map(|c| c.profile.clone())
    }
    
    pub fn get_profile_by_peer_id(&self, peer_id_str: &str) -> Option<Profile> {
        self.cached_profiles
            .values()
            .find(|c| c.peer_id_str == peer_id_str)
            .map(|c| c.profile.clone())
    }
    
    pub fn get_profile_by_username(&self, username: &str) -> Option<Profile> {
        self.username_index
            .get(&username.to_lowercase())
            .and_then(|cid| self.get_profile(cid))
    }
    
    pub fn get_cid_by_username(&self, username: &str) -> Option<String> {
        self.username_index.get(&username.to_lowercase()).cloned()
    }
    
    pub fn get_all_cached_profiles(&self) -> Vec<(String, Profile)> {
        self.cached_profiles
            .iter()
            .map(|(k, v)| (k.clone(), v.profile.clone()))
            .collect()
    }
    
    pub fn cleanup_stale_cache(&mut self, max_age_seconds: i64) {
        let now = chrono::Utc::now().timestamp();
        
        let stale_keys: Vec<String> = self.cached_profiles
            .iter()
            .filter(|(_, c)| now - c.received_at > max_age_seconds)
            .map(|(k, _)| k.clone())
            .collect();
        
        for key in stale_keys {
            self.cached_profiles.remove(&key);
        }
    }
    
    pub fn cache_size(&self) -> usize {
        self.cached_profiles.len()
    }
}

pub struct ProfileBroadcast {
    keypair: identity::Keypair,
}

impl ProfileBroadcast {
    pub fn new(keypair: identity::Keypair) -> Self {
        Self { keypair }
    }
    
    pub fn sign_message(&self, message: &mut ProfileMessage) {
        let bytes = message.to_bytes().unwrap_or_default();
        let sig = self.keypair.sign(&bytes);
        message.signature = sig;
    }
    
    pub fn verify_message(&self, message: &ProfileMessage) -> bool {
        if message.signature.is_empty() {
            return true;
        }
        
        if let Some(author) = message.author_peer_id() {
            author.verify(&message.to_bytes().unwrap_or_default(), &message.signature).is_ok()
        } else {
            false
        }
    }
    
    pub fn create_update_message(&self, profile: Profile) -> ProfileMessage {
        let mut msg = ProfileMessage::new_update(profile);
        self.sign_message(&mut msg);
        msg
    }
    
    pub fn create_delete_message(&self) -> ProfileMessage {
        let mut msg = ProfileMessage::new_delete(self.keypair.public().to_peer_id());
        self.sign_message(&mut msg);
        msg
    }
    
    pub fn create_username_register(&self, username: &str, profile_cid: &str) -> ProfileMessage {
        let mut msg = ProfileMessage::new_username_register(
            self.keypair.public().to_peer_id(),
            username,
            profile_cid,
        );
        self.sign_message(&mut msg);
        msg
    }
    
    pub fn create_request_message(&self, target: Option<&PeerId>) -> ProfileMessage {
        let target_peer = target.unwrap_or(&self.keypair.public().to_peer_id());
        let mut msg = ProfileMessage::new_request(self.keypair.public().to_peer_id(), target_peer);
        self.sign_message(&mut msg);
        msg
    }
}

pub trait ProfileSyncTransport: Send + Sync {
    fn publish_profile(&self, message: ProfileMessage) -> impl std::future::Future<Output = Result<()>> + Send;
    fn subscribe(&self) -> impl std::future::Future<Output = Result<tokio::sync::mpsc::Receiver<ProfileMessage>>> + Send;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_profile_message_creation() {
        let peer_id = PeerId::random();
        let profile = Profile::new(peer_id);
        let msg = ProfileMessage::new_update(profile.clone());
        
        assert_eq!(msg.msg_type, ProfileMessageType::ProfileUpdate);
        assert!(msg.author_peer_id().is_some());
    }

    #[test]
    fn test_username_index() {
        let peer_id = PeerId::random();
        let mut sync = ProfileSyncService::new(peer_id);
        
        let mut profile = Profile::new(peer_id);
        profile.set_username("alice").unwrap();
        
        let msg = ProfileMessage::new_update(profile);
        sync.handle_message(msg).unwrap();
        
        assert!(sync.get_profile_by_username("alice").is_some());
        assert_eq!(sync.get_cid_by_username("ALICE"), sync.get_cid_by_username("alice"));
    }

    #[test]
    fn test_cache_cleanup() {
        let peer_id = PeerId::random();
        let mut sync = ProfileSyncService::new(peer_id);
        
        let profile = Profile::new(peer_id);
        let msg = ProfileMessage::new_update(profile);
        sync.handle_message(msg).unwrap();
        
        assert_eq!(sync.cache_size(), 1);
        
        sync.cleanup_stale_cache(0);
        
        assert_eq!(sync.cache_size(), 0);
    }
}
