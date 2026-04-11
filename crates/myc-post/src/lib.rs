use myc_crypto::{
    CryptoError, HybridEncryptedData, KeyPair, PublicKey,
    quantum_resistant::PQKeyType,
    signatures::{SignerEngine, SignatureAlgorithm},
};
use serde::{Deserialize, Serialize};
use sha2::{Sha256, Digest};

pub const HYPED_THRESHOLD: f64 = 1000.0;
pub const PERMANENT_STAKE_MIN: u64 = 100;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Post {
    pub id: String,
    pub author: String,
    pub author_public_key: Vec<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<PostContent>,
    pub content_reference: Option<ContentReference>,
    pub created_at: i64,
    pub ttl_seconds: u64,
    pub engagement_score: f64,
    pub view_count: u64,
    pub share_count: u64,
    pub comment_count: u64,
    pub is_permanent: bool,
    pub staked_tokens: Option<u64>,
    pub signature: Option<PostSignature>,
    pub encryption: EncryptionInfo,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PostContent {
    pub text: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub media_cids: Option<Vec<String>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mentions: Option<Vec<String>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContentReference {
    pub storage_type: StorageType,
    pub cid: String,
    pub encrypted: bool,
    pub encryption_key_cid: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum StorageType {
    Local,
    Distributed,
    Permanent,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PostSignature {
    pub algorithm: String,
    pub signature_bytes: Vec<u8>,
    pub public_key: Vec<u8>,
}

impl PostSignature {
    pub fn new(algorithm: &str, signature_bytes: Vec<u8>, public_key: Vec<u8>) -> Self {
        Self {
            algorithm: algorithm.to_string(),
            signature_bytes,
            public_key,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EncryptionInfo {
    pub enabled: bool,
    pub algorithm: Option<String>,
    pub key_type: Option<String>,
    pub encrypted_at: Option<i64>,
}

impl Default for EncryptionInfo {
    fn default() -> Self {
        Self {
            enabled: false,
            algorithm: None,
            key_type: None,
            encrypted_at: None,
        }
    }
}

impl EncryptionInfo {
    pub fn quantum_resistant() -> Self {
        Self {
            enabled: true,
            algorithm: Some("HybridX25519Kyber768_AES256GCM".to_string()),
            key_type: Some(PQKeyType::HybridX25519Kyber768.display_name().to_string()),
            encrypted_at: Some(chrono::Utc::now().timestamp()),
        }
    }
}

impl Post {
    pub fn new(author: String, content: String, key_pair: &KeyPair) -> Result<Self, CryptoError> {
        let id = uuid::Uuid::new_v4().to_string();
        let created_at = chrono::Utc::now().timestamp();
        let author_public_key = key_pair.public_key().to_bytes();

        let post_content = PostContent {
            text: content,
            media_cids: None,
            mentions: None,
        };

        let mut post = Self {
            id,
            author,
            author_public_key: author_public_key.clone(),
            content: Some(post_content),
            content_reference: None,
            created_at,
            ttl_seconds: 86400,
            engagement_score: 0.0,
            view_count: 0,
            share_count: 0,
            comment_count: 0,
            is_permanent: false,
            staked_tokens: None,
            signature: None,
            encryption: EncryptionInfo::default(),
        };

        let sig = post.sign(key_pair)?;
        post.signature = Some(sig);

        Ok(post)
    }

    pub fn new_encrypted(author: String, content: String, key_pair: &KeyPair) -> Result<Self, CryptoError> {
        let id = uuid::Uuid::new_v4().to_string();
        let created_at = chrono::Utc::now().timestamp();
        let author_public_key = key_pair.public_key().to_bytes();

        let post_content = PostContent {
            text: content.clone(),
            media_cids: None,
            mentions: None,
        };

        let encrypted_content = Self::encrypt_content(&content, key_pair)?;

        let mut post = Self {
            id,
            author,
            author_public_key: author_public_key.clone(),
            content: Some(post_content),
            content_reference: Some(ContentReference {
                storage_type: StorageType::Distributed,
                cid: compute_cid(&encrypted_content),
                encrypted: true,
                encryption_key_cid: None,
            }),
            created_at,
            ttl_seconds: 86400,
            engagement_score: 0.0,
            view_count: 0,
            share_count: 0,
            comment_count: 0,
            is_permanent: false,
            staked_tokens: None,
            signature: None,
            encryption: EncryptionInfo::quantum_resistant(),
        };

        let sig = post.sign(key_pair)?;
        post.signature = Some(sig);

        Ok(post)
    }

    fn encrypt_content(content: &str, key_pair: &KeyPair) -> Result<Vec<u8>, CryptoError> {
        use myc_crypto::HybridEncryption;
        let encrypted = HybridEncryption::encrypt(content.as_bytes(), key_pair.public_key())?;
        Ok(encrypted.to_bytes())
    }

    pub fn decrypt_content(&self, key_pair: &KeyPair) -> Result<String, CryptoError> {
        let content = self.content.as_ref().ok_or_else(|| {
            CryptoError::DecryptionFailed("No content to decrypt".to_string())
        })?;

        if !self.encryption.enabled {
            return Ok(content.text.clone());
        }

        if let Some(ref reference) = self.content_reference {
            return Err(CryptoError::DecryptionFailed(format!(
                "Content stored externally at CID: {} (external decryption not implemented)",
                reference.cid
            )));
        }

        Ok(content.text.clone())
    }

    pub fn sign(&mut self, key_pair: &KeyPair) -> Result<PostSignature, CryptoError> {
        let signing_key = ed25519_dalek::SigningKey::from_bytes(
            key_pair.secret_key().as_bytes().try_into()
                .map_err(|_| CryptoError::InvalidKey("Invalid secret key".to_string()))?
        );

        let signer = SignerEngine::with_algorithm(signing_key, SignatureAlgorithm::Ed25519);
        let signature = signer.sign(&self.canonical_form());

        Ok(PostSignature::new(
            "Ed25519",
            signature.signature_bytes,
            signature.public_key,
        ))
    }

    pub fn verify_signature(&self) -> bool {
        let signature = match &self.signature {
            Some(s) => s,
            None => return false,
        };

        let canonical = self.canonical_form();
        let sig_bytes: [u8; 64] = match signature.signature_bytes.clone().try_into() {
            Ok(b) => b,
            Err(_) => return false,
        };
        let pk_bytes: [u8; 32] = match signature.public_key.clone().try_into() {
            Ok(b) => b,
            Err(_) => return false,
        };

        let verifying_key = match ed25519_dalek::VerifyingKey::from_bytes(&pk_bytes) {
            Ok(vk) => vk,
            Err(_) => return false,
        };

        let sig = ed25519_dalek::Signature::from_bytes(&sig_bytes);
        verifying_key.verify(canonical.as_bytes(), &sig).is_ok()
    }

    fn canonical_form(&self) -> String {
        format!(
            "{}:{}:{}:{}",
            self.id,
            self.author,
            self.created_at,
            self.content.as_ref().map(|c| &c.text).unwrap_or(&"".to_string())
        )
    }

    pub fn record_view(&mut self) {
        self.view_count += 1;
        self.engagement_score += 1.0;
        self.ttl_seconds = self.ttl_seconds.saturating_add(7200);
    }

    pub fn record_share(&mut self, author_reputation: f64) {
        self.share_count += 1;
        let base_extension = 14400.0 * (1.0 + author_reputation / 100.0);
        self.engagement_score += 10.0;
        self.ttl_seconds = self.ttl_seconds.saturating_add(base_extension as u64);
    }

    pub fn record_comment(&mut self, thread_depth: u32) {
        self.comment_count += 1;
        let depth_bonus = 1.0 + (thread_depth as f64 * 0.1);
        self.engagement_score += 5.0;
        self.ttl_seconds = self.ttl_seconds.saturating_add((21600.0 * depth_bonus) as u64);
    }

    pub fn is_expired(&self) -> bool {
        !self.is_permanent && self.ttl_seconds == 0
    }

    pub fn is_hyped(&self) -> bool {
        self.engagement_score >= HYPED_THRESHOLD
    }

    pub fn remaining_ttl(&self, current_time: i64) -> i64 {
        let elapsed = current_time - self.created_at;
        (self.ttl_seconds as i64 - elapsed).max(0)
    }

    pub fn stake_for_permanence(&mut self, amount: u64) -> Result<(), PostError> {
        if self.is_permanent {
            return Err(PostError::AlreadyPermanent);
        }
        if amount < PERMANENT_STAKE_MIN {
            return Err(PostError::InsufficientStake);
        }
        self.is_permanent = true;
        self.staked_tokens = Some(amount);
        self.content_reference = Some(ContentReference {
            storage_type: StorageType::Permanent,
            cid: compute_cid(self.content_hash().as_bytes()),
            encrypted: self.encryption.enabled,
            encryption_key_cid: None,
        });
        Ok(())
    }

    pub fn content_hash(&self) -> String {
        let data = serde_json::to_string(self).unwrap();
        let mut hasher = Sha256::new();
        hasher.update(data.as_bytes());
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, hasher.finalize())
    }

    pub fn author_key(&self) -> Option<PublicKey> {
        PublicKey::from_bytes(&self.author_public_key).ok()
    }
}

fn compute_cid(data: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(data);
    let result = hasher.finalize();
    format!("Qm{}", &base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &result[..16])[..44])
}

#[derive(Debug, thiserror::Error)]
pub enum PostError {
    #[error("Post is already permanent")]
    AlreadyPermanent,
    #[error("Insufficient stake amount")]
    InsufficientStake,
    #[error("Post has expired")]
    Expired,
    #[error("Encryption failed: {0}")]
    EncryptionFailed(String),
    #[error("Decryption failed: {0}")]
    DecryptionFailed(String),
}

#[derive(Debug, Clone)]
pub enum Interaction {
    View,
    Share { author_reputation: f64 },
    Comment { thread_depth: u32 },
}

pub fn calculate_ttl_extension(post: &Post, interaction: &Interaction) -> u64 {
    match interaction {
        Interaction::View => 7200,
        Interaction::Share { author_reputation } => {
            (14400.0 * (1.0 + author_reputation / 100.0)) as u64
        }
        Interaction::Comment { thread_depth } => {
            let depth_bonus = 1.0 + (*thread_depth as f64 * 0.1);
            (21600.0 * depth_bonus) as u64
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_post_creation() {
        let key_pair = KeyPair::generate().unwrap();
        let post = Post::new(
            "alice".to_string(),
            "Hello, Mycelium Network!".to_string(),
            &key_pair,
        ).unwrap();
        
        assert_eq!(post.ttl_seconds, 86400);
        assert_eq!(post.engagement_score, 0.0);
        assert!(!post.is_hyped());
        assert!(post.verify_signature());
    }

    #[test]
    fn test_encrypted_post() {
        let key_pair = KeyPair::generate().unwrap();
        let post = Post::new_encrypted(
            "alice".to_string(),
            "Secret message".to_string(),
            &key_pair,
        ).unwrap();
        
        assert!(post.encryption.enabled);
        assert!(post.content_reference.is_some());
        
        let decrypted = post.decrypt_content(&key_pair).unwrap();
        assert_eq!(decrypted, "Secret message");
    }

    #[test]
    fn test_view_extends_ttl() {
        let key_pair = KeyPair::generate().unwrap();
        let mut post = Post::new("alice".to_string(), "Test".to_string(), &key_pair).unwrap();
        let initial_ttl = post.ttl_seconds;
        post.record_view();
        assert!(post.ttl_seconds > initial_ttl);
    }

    #[test]
    fn test_hype_threshold() {
        let key_pair = KeyPair::generate().unwrap();
        let mut post = Post::new("alice".to_string(), "Test".to_string(), &key_pair).unwrap();
        
        for _ in 0..100 {
            post.record_share(50.0);
        }
        assert!(post.is_hyped());
    }

    #[test]
    fn test_permanence_stake() {
        let key_pair = KeyPair::generate().unwrap();
        let mut post = Post::new("alice".to_string(), "Test".to_string(), &key_pair).unwrap();
        post.stake_for_permanence(200).unwrap();
        
        assert!(post.is_permanent);
        assert_eq!(post.staked_tokens, Some(200));
        assert!(matches!(
            post.content_reference.as_ref().unwrap().storage_type,
            StorageType::Permanent
        ));
    }

    #[test]
    fn test_signature_verification() {
        let key_pair = KeyPair::generate().unwrap();
        let post = Post::new(
            "alice".to_string(),
            "Signed post".to_string(),
            &key_pair,
        ).unwrap();
        
        assert!(post.verify_signature());
    }
}
