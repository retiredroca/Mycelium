pub mod keys;
pub mod encryption;
pub mod signatures;
pub mod hybrid;
pub mod quantum_resistant;

pub use keys::{KeyPair, PublicKey, SecretKey, KeyMaterial};
pub use encryption::{Encryptor, Decryptor, EncryptedData, EncryptionScheme};
pub use signatures::{Signer, Verifier, Signature, SignedData};
pub use hybrid::{HybridKeyPair, HybridEncryption, HybridSignature};
pub use quantum_resistant::{QuantumResistantKey, PQCrypto};

use thiserror::Error;

#[derive(Error, Debug, Clone, PartialEq, Eq)]
pub enum CryptoError {
    #[error("Encryption failed: {0}")]
    EncryptionFailed(String),
    
    #[error("Decryption failed: {0}")]
    DecryptionFailed(String),
    
    #[error("Invalid key format: {0}")]
    InvalidKey(String),
    
    #[error("Signature verification failed")]
    InvalidSignature,
    
    #[error("Key exchange failed: {0}")]
    KeyExchangeFailed(String),
    
    #[error("Random number generation failed")]
    RandomGenerationFailed,
    
    #[error("Post-quantum crypto unavailable: {0}")]
    PQCryptoUnavailable(String),
}

pub const CURVE25519_KEY_SIZE: usize = 32;
pub const ED25519_SIGNATURE_SIZE: usize = 64;
pub const AES256_KEY_SIZE: usize = 32;
pub const AES256_NONCE_SIZE: usize = 12;
pub const AES256_TAG_SIZE: usize = 16;

#[cfg(feature = "post-quantum")]
pub const KYBER768_KEY_SIZE: usize = 1184;
#[cfg(feature = "post-quantum")]
pub const KYBER768_CIPHERTEXT_SIZE: usize = 1088;
#[cfg(feature = "post-quantum")]
pub const DILITHIUM3_PUBLIC_KEY_SIZE: usize = 1952;
#[cfg(feature = "post-quantum")]
pub const DILITHIUM3_SIGNATURE_SIZE: usize = 3293;

pub const HYBRID_ENCRYPTION_OVERHEAD: usize = 64;
pub const HYBRID_SIGNATURE_OVERHEAD: usize = 128;
