use super::{IdentityRecord, IdentityError};
use std::collections::HashMap;

pub struct UsernameLookup {
    cache: HashMap<String, CachedResult>,
    dht_query_fn: Option<Box<dyn Fn(&str) -> Option<Vec<u8>> + Send + Sync>>,
}

#[derive(Debug, Clone)]
struct CachedResult {
    record: IdentityRecord,
    cached_at: i64,
    ttl: i64,
}

impl CachedResult {
    fn is_expired(&self) -> bool {
        let now = chrono::Utc::now().timestamp();
        now - self.cached_at > self.ttl
    }
}

impl UsernameLookup {
    pub fn new() -> Self {
        Self {
            cache: HashMap::new(),
            dht_query_fn: None,
        }
    }

    pub fn with_dht<F>(mut self, query_fn: F) -> Self
    where
        F: Fn(&str) -> Option<Vec<u8>> + Send + Sync + 'static,
    {
        self.dht_query_fn = Some(Box::new(query_fn));
        self
    }

    pub fn lookup(&mut self, username: &str) -> Option<&IdentityRecord> {
        let username = username.to_lowercase();
        
        if let Some(cached) = self.cache.get(&username) {
            if !cached.is_expired() {
                return Some(&cached.record);
            }
            self.cache.remove(&username);
        }
        
        if let Some(query_fn) = &self.dht_query_fn {
            let key = format!("/mycelium/usernames/{}", username);
            if let Some(data) = query_fn(&key) {
                if let Ok(record) = serde_json::from_slice::<IdentityRecord>(&data) {
                    let cached = CachedResult {
                        record: record.clone(),
                        cached_at: chrono::Utc::now().timestamp(),
                        ttl: 3600,
                    };
                    self.cache.insert(username, cached);
                    return Some(&self.cache.get(&username)?.record);
                }
            }
        }
        
        None
    }

    pub fn prefetch(&mut self, usernames: &[&str]) {
        for username in usernames {
            self.lookup(username);
        }
    }

    pub fn invalidate(&mut self, username: &str) {
        let username = username.to_lowercase();
        self.cache.remove(&username);
    }

    pub fn invalidate_all(&mut self) {
        self.cache.clear();
    }

    pub fn cache_size(&self) -> usize {
        self.cache.len()
    }

    pub fn cleanup_expired(&mut self) {
        self.cache.retain(|_, cached| !cached.is_expired());
    }
}

impl Default for UsernameLookup {
    fn default() -> Self {
        Self::new()
    }
}

pub struct ReverseLookup {
    peer_to_username: HashMap<String, String>,
}

impl ReverseLookup {
    pub fn new() -> Self {
        Self {
            peer_to_username: HashMap::new(),
        }
    }

    pub fn add(&mut self, peer_id: &[u8], username: &str) {
        let peer_key = base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            peer_id,
        );
        self.peer_to_username.insert(peer_key, username.to_lowercase());
    }

    pub fn get_username(&self, peer_id: &[u8]) -> Option<String> {
        let peer_key = base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            peer_id,
        );
        self.peer_to_username.get(&peer_key).cloned()
    }

    pub fn remove(&mut self, peer_id: &[u8]) -> Option<String> {
        let peer_key = base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            peer_id,
        );
        self.peer_to_username.remove(&peer_key)
    }

    pub fn len(&self) -> usize {
        self.peer_to_username.len()
    }
}

impl Default for ReverseLookup {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache() {
        let mut lookup = UsernameLookup::new();
        
        let record = IdentityRecord::new(vec![1, 2, 3], "alice".to_string());
        
        let cached = CachedResult {
            record,
            cached_at: chrono::Utc::now().timestamp(),
            ttl: 3600,
        };
        
        lookup.cache.insert("alice".to_string(), cached);
        
        let result = lookup.lookup("alice");
        assert!(result.is_some());
    }

    #[test]
    fn test_reverse_lookup() {
        let mut reverse = ReverseLookup::new();
        
        reverse.add(&[1, 2, 3], "alice");
        
        let username = reverse.get_username(&[1, 2, 3]);
        assert_eq!(username, Some("alice".to_string()));
    }
}
