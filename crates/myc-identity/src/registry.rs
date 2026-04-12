use super::{IdentityRecord, IdentityError, validate_username, NAMESPACE};
use chrono::Utc;
use std::collections::HashMap;
use sha2::{Sha256, Digest};

#[derive(Debug, Clone)]
pub struct Registration {
    pub record: IdentityRecord,
    pub expires_at: Option<i64>,
    pub renewals: u32,
}

pub struct UsernameRegistry {
    records: HashMap<String, Registration>,
    expired_usernames: Vec<String>,
}

impl UsernameRegistry {
    pub fn new() -> Self {
        Self {
            records: HashMap::new(),
            expired_usernames: Vec::new(),
        }
    }

    pub fn register(
        &mut self,
        mut record: IdentityRecord,
        private_key: &[u8; 32],
    ) -> Result<IdentityRecord, IdentityError> {
        validate_username(&record.username)?;
        
        if self.records.contains_key(&record.username) {
            return Err(IdentityError::UsernameTaken);
        }
        
        record.sign(private_key);
        
        let registration = Registration {
            record,
            expires_at: None,
            renewals: 0,
        };
        
        self.records.insert(record.username.clone(), registration);
        
        Ok(record)
    }

    pub fn lookup(&self, username: &str) -> Option<&IdentityRecord> {
        let username = username.to_lowercase();
        self.records.get(&username).map(|r| &r.record)
    }

    pub fn lookup_mut(&mut self, username: &str) -> Option<&mut IdentityRecord> {
        let username = username.to_lowercase();
        self.records.get_mut(&username).map(|r| &mut r.record)
    }

    pub fn update_profile_cid(
        &mut self,
        username: &str,
        cid: String,
        private_key: &[u8; 32],
    ) -> Result<(), IdentityError> {
        let username = username.to_lowercase();
        
        let registration = self.records.get_mut(&username)
            .ok_or(IdentityError::UsernameNotFound)?;
        
        if !registration.record.verify_signature() {
            return Err(IdentityError::InvalidSignature);
        }
        
        registration.record.profile_cid = Some(cid);
        registration.record.updated_at = Utc::now().timestamp();
        registration.record.version += 1;
        registration.record.sign(private_key);
        
        Ok(())
    }

    pub fn transfer(
        &mut self,
        username: &str,
        new_peer_id: Vec<u8>,
        private_key: &[u8; 32],
    ) -> Result<(), IdentityError> {
        let username = username.to_lowercase();
        
        let registration = self.records.get_mut(&username)
            .ok_or(IdentityError::UsernameNotFound)?;
        
        if !registration.record.verify_signature() {
            return Err(IdentityError::InvalidSignature);
        }
        
        registration.record.peer_id = new_peer_id;
        registration.record.updated_at = Utc::now().timestamp();
        registration.record.version += 1;
        registration.record.sign(private_key);
        
        Ok(())
    }

    pub fn unregister(&mut self, username: &str, private_key: &[u8; 32]) -> Result<(), IdentityError> {
        let username = username.to_lowercase();
        
        let registration = self.records.get(&username)
            .ok_or(IdentityError::UsernameNotFound)?;
        
        if !registration.record.verify_signature() {
            return Err(IdentityError::InvalidSignature);
        }
        
        self.records.remove(&username);
        self.expired_usernames.push(username);
        
        Ok(())
    }

    pub fn renew(
        &mut self,
        username: &str,
        private_key: &[u8; 32],
    ) -> Result<(), IdentityError> {
        let username = username.to_lowercase();
        
        let registration = self.records.get_mut(&username)
            .ok_or(IdentityError::UsernameNotFound)?;
        
        if !registration.record.verify_signature() {
            return Err(IdentityError::InvalidSignature);
        }
        
        registration.renewals += 1;
        registration.record.updated_at = Utc::now().timestamp();
        registration.record.sign(private_key);
        
        Ok(())
    }

    pub fn is_available(&self, username: &str) -> bool {
        let username = username.to_lowercase();
        !self.records.contains_key(&username)
    }

    pub fn search(&self, prefix: &str) -> Vec<&IdentityRecord> {
        let prefix = prefix.to_lowercase();
        
        self.records.values()
            .filter(|r| r.record.username.starts_with(&prefix))
            .map(|r| &r.record)
            .collect()
    }

    pub fn all_records(&self) -> Vec<&IdentityRecord> {
        self.records.values().map(|r| &r.record).collect()
    }

    pub fn count(&self) -> usize {
        self.records.len()
    }

    pub fn cleanup_expired(&mut self) {
        let now = Utc::now().timestamp();
        
        self.records.retain(|_, reg| {
            if let Some(expires) = reg.expires_at {
                now < expires
            } else {
                true
            }
        });
    }

    pub fn dht_record(&self, username: &str) -> Option<DHTRecord> {
        let username = username.to_lowercase();
        
        self.records.get(&username).map(|reg| {
            DHTRecord {
                key: format!("{}{}", NAMESPACE, username),
                value: serde_json::to_vec(&reg.record).unwrap_or_default(),
                timestamp: reg.record.updated_at,
                signature: reg.record.signature.clone(),
            }
        })
    }

    pub fn from_dht_record(&mut self, data: &[u8]) -> Result<IdentityRecord, IdentityError> {
        let record: IdentityRecord = serde_json::from_slice(data)
            .map_err(|_| IdentityError::InvalidUsername)?;
        
        if !record.verify_signature() {
            return Err(IdentityError::InvalidSignature);
        }
        
        let username = record.username.clone();
        
        if self.records.contains_key(&username) {
            let existing = self.records.get(&username).unwrap();
            if existing.record.updated_at >= record.updated_at {
                return Err(IdentityError::UsernameTaken);
            }
        }
        
        let registration = Registration {
            record: record.clone(),
            expires_at: None,
            renewals: 0,
        };
        
        self.records.insert(username, registration);
        
        Ok(record)
    }
}

impl Default for UsernameRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct DHTRecord {
    pub key: String,
    pub value: Vec<u8>,
    pub timestamp: i64,
    pub signature: Vec<u8>,
}

impl DHTRecord {
    pub fn verify(&self) -> bool {
        if self.signature.is_empty() {
            return false;
        }
        let message = format!("{}:{}", self.key, self.timestamp);
        let computed = Self::sign_message(&message, &[0u8; 32]);
        self.signature.len() == computed.len()
    }

    fn sign_message(message: &str, private_key: &[u8; 32]) -> Vec<u8> {
        let mut hasher = Sha256::new();
        hasher.update(message.as_bytes());
        hasher.update(private_key);
        hasher.finalize().to_vec()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_register_username() {
        let mut registry = UsernameRegistry::new();
        
        let record = IdentityRecord::new(vec![1, 2, 3], "alice".to_string());
        let result = registry.register(record, &[0u8; 32]);
        
        assert!(result.is_ok());
        assert!(!registry.is_available("alice"));
    }

    #[test]
    fn test_username_taken() {
        let mut registry = UsernameRegistry::new();
        
        let record1 = IdentityRecord::new(vec![1, 2, 3], "alice".to_string());
        registry.register(record1, &[0u8; 32]).unwrap();
        
        let record2 = IdentityRecord::new(vec![4, 5, 6], "alice".to_string());
        let result = registry.register(record2, &[0u8; 32]);
        
        assert!(result.is_err());
    }

    #[test]
    fn test_search() {
        let mut registry = UsernameRegistry::new();
        
        registry.register(IdentityRecord::new(vec![1], "alice".to_string()), &[0u8; 32]).unwrap();
        registry.register(IdentityRecord::new(vec![2], "alex".to_string()), &[0u8; 32]).unwrap();
        registry.register(IdentityRecord::new(vec![3], "bob".to_string()), &[0u8; 32]).unwrap();
        
        let results = registry.search("al");
        assert_eq!(results.len(), 2);
    }
}
