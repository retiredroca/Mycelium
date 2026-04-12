use chrono::Utc;
use myc_crypto::CryptoError;
use serde::{Deserialize, Serialize};
use sha2::{Sha256, Digest};

pub const MAX_ENTRY_LENGTH: usize = 1000;
pub const MAX_NAME_LENGTH: usize = 50;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GuestbookEntry {
    pub id: String,
    pub author_key: Vec<u8>,
    pub author_name: String,
    pub content: String,
    pub timestamp: i64,
    pub status: EntryStatus,
    pub signature: Option<Vec<u8>>,
    pub reply_to: Option<String>,
}

impl GuestbookEntry {
    pub fn new(author_name: String, content: String) -> Self {
        Self {
            id: uuid::Uuid::new_v4().to_string(),
            author_key: Vec::new(),
            author_name,
            content,
            timestamp: Utc::now().timestamp(),
            status: EntryStatus::Pending,
            signature: None,
            reply_to: None,
        }
    }

    pub fn builder() -> EntryBuilder {
        EntryBuilder::new()
    }

    pub fn with_author_key(mut self, key: Vec<u8>) -> Self {
        self.author_key = key;
        self
    }

    pub fn with_signature(mut self, signature: Vec<u8>) -> Self {
        self.signature = Some(signature);
        self
    }

    pub fn sign(&mut self, private_key: &[u8; 32]) {
        let message = self.signature_message();
        let signature = Self::sign_message(&message, private_key);
        self.signature = Some(signature);
    }

    pub fn verify_signature(&self) -> bool {
        if let Some(ref sig) = self.signature {
            if self.author_key.is_empty() {
                return false;
            }
            
            let message = self.signature_message();
            
            let computed = Self::sign_message(&message, &[0u8; 32]);
            
            sig.len() == computed.len()
        } else {
            false
        }
    }

    fn signature_message(&self) -> String {
        format!(
            "{}:{}:{}:{}",
            self.author_key.len(),
            self.author_name,
            self.content,
            self.timestamp
        )
    }

    fn sign_message(message: &str, private_key: &[u8; 32]) -> Vec<u8> {
        let mut hasher = Sha256::new();
        hasher.update(message.as_bytes());
        hasher.update(private_key);
        hasher.finalize().to_vec()
    }

    pub fn approve(&mut self) {
        self.status = EntryStatus::Approved;
    }

    pub fn reject(&mut self) {
        self.status = EntryStatus::Rejected;
    }

    pub fn is_pending(&self) -> bool {
        matches!(self.status, EntryStatus::Pending)
    }

    pub fn is_approved(&self) -> bool {
        matches!(self.status, EntryStatus::Approved)
    }

    pub fn content_hash(&self) -> String {
        let data = serde_json::to_string(self).unwrap();
        let mut hasher = Sha256::new();
        hasher.update(data.as_bytes());
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, hasher.finalize())
    }

    pub fn public_view(&self) -> PublicGuestbookEntry {
        PublicGuestbookEntry {
            id: self.id.clone(),
            author_name: self.author_name.clone(),
            content: self.content.clone(),
            timestamp: self.timestamp,
            reply_to: self.reply_to.clone(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum EntryStatus {
    Pending,
    Approved,
    Rejected,
}

impl Default for EntryStatus {
    fn default() -> Self {
        Self::Pending
    }
}

pub struct EntryBuilder {
    entry: GuestbookEntry,
}

impl EntryBuilder {
    pub fn new() -> Self {
        Self {
            entry: GuestbookEntry::new(String::new(), String::new()),
        }
    }

    pub fn author_name(mut self, name: &str) -> Result<Self, EntryError> {
        if name.len() > MAX_NAME_LENGTH {
            return Err(EntryError::NameTooLong);
        }
        self.entry.author_name = name.to_string();
        Ok(self)
    }

    pub fn content(mut self, content: &str) -> Result<Self, EntryError> {
        if content.len() > MAX_ENTRY_LENGTH {
            return Err(EntryError::ContentTooLong);
        }
        if content.trim().is_empty() {
            return Err(EntryError::EmptyContent);
        }
        self.entry.content = content.to_string();
        Ok(self)
    }

    pub fn author_key(mut self, key: Vec<u8>) -> Self {
        self.entry.author_key = key;
        self
    }

    pub fn reply_to(mut self, parent_id: &str) -> Self {
        self.entry.reply_to = Some(parent_id.to_string());
        self
    }

    pub fn build(self) -> GuestbookEntry {
        self.entry
    }
}

impl Default for EntryBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PublicGuestbookEntry {
    pub id: String,
    pub author_name: String,
    pub content: String,
    pub timestamp: i64,
    pub reply_to: Option<String>,
}

#[derive(Debug, thiserror::Error)]
pub enum EntryError {
    #[error("Name too long (max {0} characters)")]
    NameTooLong,
    
    #[error("Content too long (max {0} characters)")]
    ContentTooLong,
    
    #[error("Content cannot be empty")]
    EmptyContent,
    
    #[error("Invalid author key")]
    InvalidAuthorKey,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_entry_creation() {
        let entry = GuestbookEntry::new(
            "Alice".to_string(),
            "Hello, world!".to_string(),
        );
        
        assert!(entry.is_pending());
        assert_eq!(entry.author_name, "Alice");
        assert_eq!(entry.content, "Hello, world!");
    }

    #[test]
    fn test_entry_builder() {
        let entry = EntryBuilder::new()
            .author_name("Bob").unwrap()
            .content("Great profile!").unwrap()
            .build();
        
        assert_eq!(entry.author_name, "Bob");
        assert_eq!(entry.content, "Great profile!");
    }

    #[test]
    fn test_content_length_limit() {
        let result = EntryBuilder::new()
            .content(&"a".repeat(MAX_ENTRY_LENGTH + 1));
        
        assert!(result.is_err());
    }

    #[test]
    fn test_approval() {
        let mut entry = GuestbookEntry::new("Test".to_string(), "Test".to_string());
        
        assert!(entry.is_pending());
        
        entry.approve();
        assert!(entry.is_approved());
    }
}
