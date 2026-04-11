use serde::{Deserialize, Serialize};
use myc_crypto::{
    CryptoError, HybridEncryptedData, HybridEncryption, KeyPair, PublicKey,
    quantum_resistant::{PQCrypto, PQKeyType},
    encryption::{self, EncryptedData},
};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;
use zeroize::Zeroize;

pub const ENCRYPTION_ENABLED: bool = true;
pub const DEFAULT_KEY_TYPE: PQKeyType = PQKeyType::HybridX25519Kyber768;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EncryptedStorageEntry {
    pub entry_id: String,
    pub content_cid: String,
    pub encrypted_content: Vec<u8>,
    pub encryption_metadata: EncryptionMetadata,
    pub size_bytes: u64,
    pub created_at: i64,
    pub last_verified: i64,
}

impl EncryptedStorageEntry {
    pub fn new(
        entry_id: String,
        content_cid: String,
        encrypted_content: Vec<u8>,
        metadata: EncryptionMetadata,
        size_bytes: u64,
    ) -> Self {
        Self {
            entry_id,
            content_cid,
            encrypted_content,
            encryption_metadata: metadata,
            size_bytes,
            created_at: chrono::Utc::now().timestamp(),
            last_verified: 0,
        }
    }

    pub fn needs_verification(&self) -> bool {
        let now = chrono::Utc::now().timestamp();
        now - self.last_verified > 86400
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EncryptionMetadata {
    pub algorithm: String,
    pub key_type: String,
    pub nonce: Vec<u8>,
    pub merkle_root: String,
    pub content_hash: String,
}

impl EncryptionMetadata {
    pub fn new(algorithm: &str, key_type: PQKeyType, nonce: Vec<u8>, content_hash: String) -> Self {
        Self {
            algorithm: algorithm.to_string(),
            key_type: key_type.display_name().to_string(),
            nonce,
            merkle_root: String::new(),
            content_hash,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StorageManifest {
    pub manifest_id: String,
    pub owner_id: String,
    pub public_key: Vec<u8>,
    pub entries: Vec<StorageEntrySummary>,
    pub total_size_bytes: u64,
    pub encryption_enabled: bool,
    pub created_at: i64,
    pub updated_at: i64,
}

impl StorageManifest {
    pub fn new(owner_id: String, public_key: Vec<u8>) -> Self {
        let now = chrono::Utc::now().timestamp();
        Self {
            manifest_id: uuid::Uuid::new_v4().to_string(),
            owner_id,
            public_key,
            entries: Vec::new(),
            total_size_bytes: 0,
            encryption_enabled: ENCRYPTION_ENABLED,
            created_at: now,
            updated_at: now,
        }
    }

    pub fn add_entry(&mut self, summary: StorageEntrySummary, size_bytes: u64) {
        self.total_size_bytes += size_bytes;
        self.updated_at = chrono::Utc::now().timestamp();
        self.entries.push(summary);
    }

    pub fn remove_entry(&mut self, entry_id: &str) -> Option<StorageEntrySummary> {
        if let Some(pos) = self.entries.iter().position(|e| e.entry_id == entry_id) {
            let entry = self.entries.remove(pos);
            self.total_size_bytes -= entry.size_bytes;
            self.updated_at = chrono::Utc::now().timestamp();
            Some(entry)
        } else {
            None
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StorageEntrySummary {
    pub entry_id: String,
    pub content_cid: String,
    pub size_bytes: u64,
    pub encrypted: bool,
    pub created_at: i64,
}

pub struct EncryptedLocalStorage {
    manifest: Arc<RwLock<StorageManifest>>,
    entries: Arc<RwLock<HashMap<String, EncryptedStorageEntry>>>,
    key_pair: Arc<KeyPair>,
    key_type: PQKeyType,
}

impl EncryptedLocalStorage {
    pub fn new(owner_id: String) -> Result<Self, CryptoError> {
        let key_pair = KeyPair::generate()?;
        
        let manifest = StorageManifest::new(owner_id, key_pair.public_key().to_bytes());
        
        Ok(Self {
            manifest: Arc::new(RwLock::new(manifest)),
            entries: Arc::new(RwLock::new(HashMap::new())),
            key_pair: Arc::new(key_pair),
            key_type: DEFAULT_KEY_TYPE,
        })
    }

    pub fn with_key_pair(owner_id: String, key_pair: KeyPair) -> Self {
        let manifest = StorageManifest::new(owner_id, key_pair.public_key().to_bytes());
        
        Self {
            manifest: Arc::new(RwLock::new(manifest)),
            entries: Arc::new(RwLock::new(HashMap::new())),
            key_pair: Arc::new(key_pair),
            key_type: DEFAULT_KEY_TYPE,
        }
    }

    pub fn public_key(&self) -> Vec<u8> {
        self.key_pair.public_key().to_bytes()
    }

    pub async fn store_encrypted(&self, data: Vec<u8>, content_type: String) -> Result<EncryptedStorageEntry, CryptoError> {
        let content_hash = compute_hash(&data);
        let cid = compute_cid(&data);
        
        let encrypted = HybridEncryption::encrypt(&data, self.key_pair.public_key())?;
        let encrypted_bytes = encrypted.to_bytes();
        
        let entry_id = uuid::Uuid::new_v4().to_string();
        
        let metadata = EncryptionMetadata::new(
            "HybridX25519Kyber768_AES256GCM",
            self.key_type,
            encrypted.nonce.clone(),
            content_hash.clone(),
        );
        
        let entry = EncryptedStorageEntry::new(
            entry_id.clone(),
            cid.clone(),
            encrypted_bytes,
            metadata,
            data.len() as u64,
        );
        
        let summary = StorageEntrySummary {
            entry_id: entry_id.clone(),
            content_cid: cid,
            size_bytes: data.len() as u64,
            encrypted: true,
            created_at: chrono::Utc::now().timestamp(),
        };
        
        let mut entries = self.entries.write().await;
        entries.insert(entry_id.clone(), entry.clone());
        
        let mut manifest = self.manifest.write().await;
        manifest.add_entry(summary, data.len() as u64);
        
        Ok(entry)
    }

    pub async fn retrieve_decrypted(&self, entry_id: &str) -> Result<Option<Vec<u8>>, CryptoError> {
        let entries = self.entries.read().await;
        
        if let Some(entry) = entries.get(entry_id) {
            let encrypted = HybridEncryptedData::from_bytes(&entry.encrypted_content)?;
            let decrypted = self.key_pair.decrypt(&encrypted)?;
            Ok(Some(decrypted))
        } else {
            Ok(None)
        }
    }

    pub async fn delete(&self, entry_id: &str) -> Option<EncryptedStorageEntry> {
        let mut entries = self.entries.write().await;
        let entry = entries.remove(entry_id)?;
        
        let mut manifest = self.manifest.write().await;
        manifest.remove_entry(entry_id);
        
        Some(entry)
    }

    pub async fn get_manifest(&self) -> StorageManifest {
        let manifest = self.manifest.read().await;
        manifest.clone()
    }

    pub async fn total_size(&self) -> u64 {
        let manifest = self.manifest.read().await;
        manifest.total_size_bytes
    }

    pub fn encrypt_for_recipient(&self, data: &[u8], recipient: &PublicKey) -> Result<Vec<u8>, CryptoError> {
        let encrypted = HybridEncryption::encrypt(data, recipient)?;
        Ok(encrypted.to_bytes())
    }

    pub fn decrypt_from_sender(&self, encrypted_data: &[u8]) -> Result<Vec<u8>, CryptoError> {
        let encrypted = HybridEncryptedData::from_bytes(encrypted_data)?;
        self.key_pair.decrypt(&encrypted)
    }
}

pub struct EncryptedDataHandoff {
    pub handoff_id: String,
    pub content_cid: String,
    pub encrypted_blob: Vec<u8>,
    pub encryption_metadata: EncryptionMetadata,
    pub merkle_proof: MerkleProof,
    pub replication_factor: u8,
    pub verified_hosts: Vec<String>,
    pub created_at: i64,
}

impl EncryptedDataHandoff {
    pub fn new(
        content_cid: String,
        encrypted_blob: Vec<u8>,
        metadata: EncryptionMetadata,
        merkle_root: String,
        replication_factor: u8,
    ) -> Self {
        Self {
            handoff_id: uuid::Uuid::new_v4().to_string(),
            content_cid,
            encrypted_blob,
            encryption_metadata: metadata,
            merkle_proof: MerkleProof { root: merkle_root },
            replication_factor,
            verified_hosts: Vec::new(),
            created_at: chrono::Utc::now().timestamp(),
        }
    }

    pub fn add_verified_host(&mut self, node_id: String) {
        if !self.verified_hosts.contains(&node_id) {
            self.verified_hosts.push(node_id);
        }
    }

    pub fn is_fully_replicated(&self) -> bool {
        self.verified_hosts.len() >= self.replication_factor as usize
    }
}

#[derive(Debug, Clone)]
pub struct MerkleProof {
    pub root: String,
}

impl MerkleProof {
    pub fn verify(&self, data: &[u8]) -> bool {
        let computed = compute_cid(data);
        computed.starts_with(&self.root[..8.min(self.root.len())])
    }
}

fn compute_cid(data: &[u8]) -> String {
    let hash = compute_hash(data);
    format!("Qm{}", &base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &hash[..16])[..44])
}

fn compute_hash(data: &[u8]) -> Vec<u8> {
    use sha2::{Sha256, Digest};
    let mut hasher = Sha256::new();
    hasher.update(data);
    hasher.finalize().to_vec()
}

pub mod integrity {
    use super::*;
    
    pub struct EncryptedMerkleProof {
        pub root: String,
        pub path: Vec<MerkleNode>,
        pub leaf_index: usize,
        pub encrypted: bool,
    }

    #[derive(Debug, Clone)]
    pub enum MerkleNode {
        Left(Vec<u8>),
        Right(Vec<u8>),
    }

    pub fn verify_encrypted_proof(encrypted_data: &[u8], proof: &EncryptedMerkleProof) -> bool {
        if !proof.encrypted {
            return verify_proof(encrypted_data, proof);
        }
        
        let mut current = compute_hash(encrypted_data);
        
        for node in &proof.path {
            current = match node {
                MerkleNode::Left(left) => {
                    let mut combined = Vec::new();
                    combined.extend_from_slice(&current);
                    combined.extend_from_slice(left);
                    compute_hash(&combined)
                }
                MerkleNode::Right(right) => {
                    let mut combined = Vec::new();
                    combined.extend_from_slice(right);
                    combined.extend_from_slice(&current);
                    compute_hash(&combined)
                }
            };
        }
        
        String::from_utf8_lossy(&current) == proof.root
    }

    fn verify_proof(data: &[u8], proof: &EncryptedMerkleProof) -> bool {
        let computed = compute_hash(data);
        String::from_utf8_lossy(&computed) == proof.root
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_encrypted_storage() {
        let storage = EncryptedLocalStorage::new("alice".to_string()).unwrap();
        
        let data = b"Secret message for quantum-resistant storage".to_vec();
        let entry = storage.store_encrypted(data.clone(), "text/plain".to_string()).await.unwrap();
        
        assert!(entry.encrypted);
        assert!(entry.encryption_metadata.algorithm.contains("Hybrid"));
        
        let decrypted = storage.retrieve_decrypted(&entry.entry_id).await.unwrap().unwrap();
        assert_eq!(decrypted, data);
    }

    #[tokio::test]
    async fn test_encrypt_for_recipient() {
        let alice = EncryptedLocalStorage::new("alice".to_string()).unwrap();
        let bob = EncryptedLocalStorage::new("bob".to_string()).unwrap();
        
        let secret = b"Private message from Alice to Bob";
        
        let encrypted = alice.encrypt_for_recipient(secret, bob.key_pair.public_key()).unwrap();
        
        let decrypted = bob.decrypt_from_sender(&encrypted).unwrap();
        assert_eq!(decrypted, secret);
    }

    #[tokio::test]
    async fn test_delete() {
        let storage = EncryptedLocalStorage::new("alice".to_string()).unwrap();
        
        let entry = storage.store_encrypted(b"test".to_vec(), "text/plain".to_string()).await.unwrap();
        
        let deleted = storage.delete(&entry.entry_id).await;
        assert!(deleted.is_some());
        
        let retrieved = storage.retrieve_decrypted(&entry.entry_id).await.unwrap();
        assert!(retrieved.is_none());
    }

    #[test]
    fn test_handoff_encryption() {
        let alice = EncryptedLocalStorage::new("alice".to_string()).unwrap();
        
        let data = b"Data to hand off to host";
        let encrypted = alice.encrypt_for_recipient(data, alice.key_pair.public_key()).unwrap();
        
        let metadata = EncryptionMetadata::new(
            "HybridX25519Kyber768_AES256GCM",
            PQKeyType::HybridX25519Kyber768,
            vec![],
            base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &compute_hash(data)),
        );
        
        let handoff = EncryptedDataHandoff::new(
            compute_cid(data),
            encrypted,
            metadata,
            "merkle_root".to_string(),
            3,
        );
        
        assert_eq!(handoff.replication_factor, 3);
        assert!(!handoff.is_fully_replicated());
        
        handoff.add_verified_host("host1".to_string());
        handoff.add_verified_host("host2".to_string());
        handoff.add_verified_host("host3".to_string());
        
        assert!(handoff.is_fully_replicated());
    }
}
