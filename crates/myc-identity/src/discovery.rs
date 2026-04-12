use super::{IdentityRecord, IdentityError};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DiscoveryResult {
    pub peer_id: Vec<u8>,
    pub username: Option<String>,
    pub profile_cid: Option<String>,
    pub source: DiscoverySource,
    pub timestamp: i64,
    pub latency_ms: Option<u64>,
}

impl DiscoveryResult {
    pub fn new(peer_id: Vec<u8>) -> Self {
        Self {
            peer_id,
            username: None,
            profile_cid: None,
            source: DiscoverySource::Unknown,
            timestamp: chrono::Utc::now().timestamp(),
            latency_ms: None,
        }
    }

    pub fn with_username(mut self, username: String) -> Self {
        self.username = Some(username);
        self
    }

    pub fn with_profile_cid(mut self, cid: String) -> Self {
        self.profile_cid = Some(cid);
        self
    }

    pub fn from_record(record: &IdentityRecord, source: DiscoverySource) -> Self {
        Self {
            peer_id: record.peer_id.clone(),
            username: Some(record.username.clone()),
            profile_cid: record.profile_cid.clone(),
            source,
            timestamp: chrono::Utc::now().timestamp(),
            latency_ms: None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DiscoverySource {
    LocalCache,
    DHT,
    BootstrapNodes,
    Relay,
    Manual,
    Unknown,
}

impl DiscoverySource {
    pub fn as_str(&self) -> &'static str {
        match self {
            DiscoverySource::LocalCache => "local_cache",
            DiscoverySource::DHT => "dht",
            DiscoverySource::BootstrapNodes => "bootstrap",
            DiscoverySource::Relay => "relay",
            DiscoverySource::Manual => "manual",
            DiscoverySource::Unknown => "unknown",
        }
    }
}

pub struct ProfileDiscovery {
    sources: Vec<Box<dyn DiscoverySource>>,
}

impl ProfileDiscovery {
    pub fn new() -> Self {
        Self {
            sources: Vec::new(),
        }
    }

    pub fn add_source<S: DiscoverySource + 'static>(&mut self, source: S) {
        self.sources.push(Box::new(source));
    }

    pub async fn discover(&self, query: &DiscoveryQuery) -> Vec<DiscoveryResult> {
        let mut results = Vec::new();
        
        for source in &self.sources {
            if let Some(result) = source.query(query).await {
                results.push(result);
            }
        }
        
        results.sort_by(|a, b| {
            a.source.cmp(&b.source)
        });
        
        results
    }
}

impl Default for ProfileDiscovery {
    fn default() -> Self {
        Self::new()
    }
}

pub trait DiscoverySource: Send + Sync {
    fn query(&self, query: &DiscoveryQuery) -> std::pin::Pin<Box<dyn std::future::Future<Output = Option<DiscoveryResult>> + Send + '_>>;
}

#[derive(Debug, Clone)]
pub struct DiscoveryQuery {
    pub query_type: QueryType,
    pub value: String,
    pub timeout_ms: u64,
}

impl DiscoveryQuery {
    pub fn by_username(username: &str) -> Self {
        Self {
            query_type: QueryType::Username,
            value: username.to_lowercase(),
            timeout_ms: 5000,
        }
    }

    pub fn by_peer_id(peer_id: &[u8]) -> Self {
        Self {
            query_type: QueryType::PeerId,
            value: base64::Engine::encode(&base64::engine::general_purpose::STANDARD, peer_id),
            timeout_ms: 3000,
        }
    }

    pub fn by_profile_cid(cid: &str) -> Self {
        Self {
            query_type: QueryType::ProfileCID,
            value: cid.to_string(),
            timeout_ms: 10000,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum QueryType {
    Username,
    PeerId,
    ProfileCID,
}

pub struct DHTDiscoverySource {
    dht_client: Option<Box<dyn DHTClient>>,
}

impl DHTDiscoverySource {
    pub fn new() -> Self {
        Self { dht_client: None }
    }

    pub fn with_client<C: DHTClient + 'static>(mut self, client: C) -> Self {
        self.dht_client = Some(Box::new(client));
        self
    }
}

impl Default for DHTDiscoverySource {
    fn default() -> Self {
        Self::new()
    }
}

impl DiscoverySource for DHTDiscoverySource {
    fn query(&self, query: &DiscoveryQuery) -> std::pin::Pin<Box<dyn std::future::Future<Output = Option<DiscoveryResult>> + Send + '_>> {
        Box::pin(async move {
            let client = self.dht_client.as_ref()?;
            
            let key = match query.query_type {
                QueryType::Username => format!("/mycelium/usernames/{}", query.value),
                QueryType::PeerId => format!("/mycelium/peer/{}", query.value),
                QueryType::ProfileCID => format!("/mycelium/profile/{}", query.value),
            };
            
            let start = std::time::Instant::now();
            let data = client.get(&key, query.timeout_ms).await.ok()?;
            let latency = start.elapsed().as_millis() as u64;
            
            let record: IdentityRecord = serde_json::from_slice(&data).ok()?;
            
            let mut result = DiscoveryResult::from_record(&record, DiscoverySource::DHT);
            result.latency_ms = Some(latency);
            
            Some(result)
        })
    }
}

pub trait DHTClient: Send + Sync {
    fn get(&self, key: &str, timeout_ms: u64) -> std::pin::Pin<Box<dyn std::future::Future<Output = Result<Vec<u8>, DHTError>> + Send + '_>>;
    fn put(&self, key: &str, value: Vec<u8>, ttl_secs: u64) -> std::pin::Pin<Box<dyn std::future::Future<Output = Result<(), DHTError>> + Send + '_>>;
}

#[derive(Debug, thiserror::Error)]
pub enum DHTError {
    #[error("Key not found")]
    NotFound,
    
    #[error("Timeout")]
    Timeout,
    
    #[error("Network error")]
    NetworkError,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_discovery_query() {
        let query = DiscoveryQuery::by_username("alice");
        assert_eq!(query.value, "alice");
        
        let query = DiscoveryQuery::by_peer_id(&[1, 2, 3]);
        assert!(query.value.contains("AQID"));
    }

    #[test]
    fn test_discovery_result() {
        let record = IdentityRecord::new(vec![1, 2, 3], "alice".to_string());
        
        let result = DiscoveryResult::from_record(&record, DiscoverySource::DHT);
        assert_eq!(result.username, Some("alice".to_string()));
        assert_eq!(result.source, DiscoverySource::DHT);
    }
}
