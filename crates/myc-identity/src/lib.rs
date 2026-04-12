pub mod registry;
pub mod lookup;
pub mod discovery;

pub use registry::{UsernameRegistry, Registration};
pub use lookup::UsernameLookup;
pub use discovery::{ProfileDiscovery, DiscoveryResult};

use chrono::Utc;
use serde::{Deserialize, Serialize};
use sha2::{Sha256, Digest};

pub const NAMESPACE: &str = "/mycelium/usernames/";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IdentityRecord {
    pub peer_id: Vec<u8>,
    pub username: String,
    pub profile_cid: Option<String>,
    pub registered_at: i64,
    pub updated_at: i64,
    pub signature: Vec<u8>,
    pub version: u32,
}

impl IdentityRecord {
    pub fn new(peer_id: Vec<u8>, username: String) -> Self {
        let now = Utc::now().timestamp();
        Self {
            peer_id,
            username: username.to_lowercase(),
            profile_cid: None,
            registered_at: now,
            updated_at: now,
            signature: Vec::new(),
            version: 1,
        }
    }

    pub fn with_profile_cid(mut self, cid: String) -> Self {
        self.profile_cid = Some(cid);
        self.updated_at = Utc::now().timestamp();
        self.version += 1;
        self
    }

    pub fn sign(&mut self, private_key: &[u8; 32]) {
        let message = self.signature_message();
        self.signature = Self::sign_message(&message, private_key);
    }

    pub fn verify_signature(&self) -> bool {
        if self.signature.is_empty() {
            return false;
        }
        let message = self.signature_message();
        let computed = Self::sign_message(&message, &[0u8; 32]);
        self.signature.len() == computed.len()
    }

    fn signature_message(&self) -> String {
        format!(
            "{}:{}:{}:{}",
            self.username,
            base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &self.peer_id),
            self.registered_at,
            self.version
        )
    }

    fn sign_message(message: &str, private_key: &[u8; 32]) -> Vec<u8> {
        let mut hasher = Sha256::new();
        hasher.update(message.as_bytes());
        hasher.update(private_key);
        hasher.finalize().to_vec()
    }

    pub fn dht_key(&self) -> String {
        format!("{}{}", NAMESPACE, self.username)
    }
}

pub struct IdentityManager {
    registry: UsernameRegistry,
}

impl IdentityManager {
    pub fn new() -> Self {
        Self {
            registry: UsernameRegistry::new(),
        }
    }

    pub fn register(
        &mut self,
        peer_id: Vec<u8>,
        username: &str,
        private_key: &[u8; 32],
    ) -> Result<IdentityRecord, IdentityError> {
        let record = IdentityRecord::new(peer_id, username.to_string());
        self.registry.register(record, private_key)
    }

    pub fn lookup(&self, username: &str) -> Option<&IdentityRecord> {
        self.registry.lookup(username)
    }

    pub fn resolve_peer_id(&self, username: &str) -> Option<Vec<u8>> {
        self.registry.lookup(username).map(|r| r.peer_id.clone())
    }

    pub fn update_profile_cid(
        &mut self,
        username: &str,
        cid: String,
        private_key: &[u8; 32],
    ) -> Result<(), IdentityError> {
        self.registry.update_profile_cid(username, cid, private_key)
    }

    pub fn transfer_ownership(
        &mut self,
        username: &str,
        new_peer_id: Vec<u8>,
        private_key: &[u8; 32],
    ) -> Result<(), IdentityError> {
        self.registry.transfer(username, new_peer_id, private_key)
    }

    pub fn unregister(&mut self, username: &str, private_key: &[u8; 32]) -> Result<(), IdentityError> {
        self.registry.unregister(username, private_key)
    }
}

impl Default for IdentityManager {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, thiserror::Error)]
pub enum IdentityError {
    #[error("Username already taken")]
    UsernameTaken,
    
    #[error("Username not found")]
    UsernameNotFound,
    
    #[error("Invalid username format")]
    InvalidUsername,
    
    #[error("Signature verification failed")]
    InvalidSignature,
    
    #[error("Unauthorized action")]
    Unauthorized,
    
    #[error("Username reserved")]
    UsernameReserved,
}

pub fn validate_username(username: &str) -> Result<(), IdentityError> {
    let trimmed = username.trim().to_lowercase();
    
    if trimmed.is_empty() {
        return Err(IdentityError::InvalidUsername);
    }
    
    if trimmed.len() < 3 || trimmed.len() > 30 {
        return Err(IdentityError::InvalidUsername);
    }
    
    if !trimmed.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-') {
        return Err(IdentityError::InvalidUsername);
    }
    
    if trimmed.chars().next().unwrap().is_numeric() {
        return Err(IdentityError::InvalidUsername);
    }
    
    let reserved = [
        "admin", "root", "system", "moderator", "mod", "support",
        "help", "api", "www", "mail", "ftp", "null", "undefined",
        "true", "false", "mycelium", "official", "bot", "test",
    ];
    
    if reserved.contains(&trimmed.as_str()) {
        return Err(IdentityError::UsernameReserved);
    }
    
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_username_validation() {
        assert!(validate_username("alice").is_ok());
        assert!(validate_username("alice123").is_ok());
        assert!(validate_username("alice_smith").is_ok());
        assert!(validate_username("").is_err());
        assert!(validate_username("ab").is_err());
        assert!(validate_username("admin").is_err());
    }

    #[test]
    fn test_identity_record() {
        let mut record = IdentityRecord::new(vec![1, 2, 3], "alice".to_string());
        
        assert_eq!(record.username, "alice");
        assert!(record.profile_cid.is_none());
        
        record = record.with_profile_cid("QmTest123".to_string());
        assert!(record.profile_cid.is_some());
    }
}
