use crate::{CryptoError, AES256_KEY_SIZE, AES256_NONCE_SIZE, AES256_TAG_SIZE};
use aes_gcm::{
    aead::{Aead, KeyInit, OsRng},
    Aes256Gcm, Nonce,
};
use rand::RngCore;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct EncryptedData {
    pub nonce: Vec<u8>,
    pub ciphertext: Vec<u8>,
    pub tag: Vec<u8>,
    pub algorithm: EncryptionAlgorithm,
}

impl EncryptedData {
    pub fn new(nonce: Vec<u8>, ciphertext: Vec<u8>, tag: Vec<u8>) -> Self {
        Self {
            nonce,
            ciphertext,
            tag,
            algorithm: EncryptionAlgorithm::Aes256Gcm,
        }
    }

    pub fn combine(&self) -> Vec<u8> {
        let mut combined = Vec::with_capacity(AES256_NONCE_SIZE + self.ciphertext.len() + AES256_TAG_SIZE);
        combined.extend_from_slice(&self.nonce);
        combined.extend_from_slice(&self.ciphertext);
        combined.extend_from_slice(&self.tag);
        combined
    }

    pub fn split(combined: &[u8]) -> Result<Self, CryptoError> {
        if combined.len() < AES256_NONCE_SIZE + AES256_TAG_SIZE {
            return Err(CryptoError::DecryptionFailed("Data too short".to_string()));
        }

        let mut nonce = [0u8; AES256_NONCE_SIZE];
        nonce.copy_from_slice(&combined[..AES256_NONCE_SIZE]);

        let ciphertext_len = combined.len() - AES256_NONCE_SIZE - AES256_TAG_SIZE;
        let ciphertext = combined[AES256_NONCE_SIZE..AES256_NONCE_SIZE + ciphertext_len].to_vec();

        let mut tag = [0u8; AES256_TAG_SIZE];
        tag.copy_from_slice(&combined[AES256_NONCE_SIZE + ciphertext_len..]);

        Ok(Self {
            nonce: nonce.to_vec(),
            ciphertext,
            tag,
            algorithm: EncryptionAlgorithm::Aes256Gcm,
        })
    }
}

#[derive(Clone, Debug, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum EncryptionAlgorithm {
    Aes256Gcm,
    ChaCha20Poly1305,
    Hybrid,
}

impl Default for EncryptionAlgorithm {
    fn default() -> Self {
        Self::Aes256Gcm
    }
}

#[derive(Clone, Debug)]
pub struct Encryptor {
    cipher: Aes256Gcm,
}

impl Encryptor {
    pub fn new(key: &[u8; AES256_KEY_SIZE]) -> Self {
        let cipher = Aes256Gcm::new_from_slice(key).expect("Valid key size");
        Self { cipher }
    }

    pub fn from_shared_secret(shared_secret: &[u8]) -> Result<Self, CryptoError> {
        let key = derive_key(shared_secret)?;
        Ok(Self::new(&key))
    }

    pub fn encrypt(&self, plaintext: &[u8]) -> Result<EncryptedData, CryptoError> {
        let mut nonce_bytes = [0u8; AES256_NONCE_SIZE];
        OsRng.fill_bytes(&mut nonce_bytes);
        let nonce = Nonce::from_slice(&nonce_bytes);

        let ciphertext = self
            .cipher
            .encrypt(nonce, plaintext)
            .map_err(|e| CryptoError::EncryptionFailed(e.to_string()))?;

        let tag = ciphertext[ciphertext.len() - AES256_TAG_SIZE..].to_vec();
        let ciphertext_no_tag = ciphertext[..ciphertext.len() - AES256_TAG_SIZE].to_vec();

        Ok(EncryptedData::new(
            nonce_bytes.to_vec(),
            ciphertext_no_tag,
            tag,
        ))
    }

    pub fn encrypt_to_combined(&self, plaintext: &[u8]) -> Result<Vec<u8>, CryptoError> {
        let encrypted = self.encrypt(plaintext)?;
        Ok(encrypted.combine())
    }
}

#[derive(Clone, Debug)]
pub struct Decryptor {
    cipher: Aes256Gcm,
}

impl Decryptor {
    pub fn new(key: &[u8; AES256_KEY_SIZE]) -> Self {
        let cipher = Aes256Gcm::new_from_slice(key).expect("Valid key size");
        Self { cipher }
    }

    pub fn from_shared_secret(shared_secret: &[u8]) -> Result<Self, CryptoError> {
        let key = derive_key(shared_secret)?;
        Ok(Self::new(&key))
    }

    pub fn decrypt(&self, encrypted: &EncryptedData) -> Result<Vec<u8>, CryptoError> {
        if encrypted.nonce.len() != AES256_NONCE_SIZE {
            return Err(CryptoError::DecryptionFailed("Invalid nonce size".to_string()));
        }

        let nonce = Nonce::from_slice(&encrypted.nonce);

        let mut combined = encrypted.ciphertext.clone();
        combined.extend_from_slice(&encrypted.tag);

        self.cipher
            .decrypt(nonce, combined.as_ref())
            .map_err(|e| CryptoError::DecryptionFailed(e.to_string()))
    }

    pub fn decrypt_combined(&self, combined: &[u8]) -> Result<Vec<u8>, CryptoError> {
        let encrypted = EncryptedData::split(combined)?;
        self.decrypt(&encrypted)
    }
}

fn derive_key(shared_secret: &[u8]) -> Result<[u8; AES256_KEY_SIZE], CryptoError> {
    use sha2::{Sha256, Digest};
    
    let mut hasher = Sha256::new();
    hasher.update(b"SOVEREIGN-KEY-DERIVATION");
    hasher.update(shared_secret);
    hasher.update(b"KYBER-HYBRID-v1");
    
    let result = hasher.finalize();
    
    let mut key = [0u8; AES256_KEY_SIZE];
    key.copy_from_slice(&result[..AES256_KEY_SIZE]);
    
    Ok(key)
}

pub fn generate_symmetric_key() -> [u8; AES256_KEY_SIZE] {
    let mut key = [0u8; AES256_KEY_SIZE];
    OsRng.fill_bytes(&mut key);
    key
}

pub fn symmetric_encrypt(plaintext: &[u8], key: &[u8; AES256_KEY_SIZE]) -> Result<Vec<u8>, CryptoError> {
    let encryptor = Encryptor::new(key);
    encryptor.encrypt_to_combined(plaintext)
}

pub fn symmetric_decrypt(ciphertext: &[u8], key: &[u8; AES256_KEY_SIZE]) -> Result<Vec<u8>, CryptoError> {
    let decryptor = Decryptor::new(key);
    decryptor.decrypt_combined(ciphertext)
}

pub struct EncryptionScheme {
    pub algorithm: EncryptionAlgorithm,
    pub key_size: usize,
    pub nonce_size: usize,
    pub tag_size: usize,
}

impl EncryptionScheme {
    pub fn aes256gcm() -> Self {
        Self {
            algorithm: EncryptionAlgorithm::Aes256Gcm,
            key_size: AES256_KEY_SIZE,
            nonce_size: AES256_NONCE_SIZE,
            tag_size: AES256_TAG_SIZE,
        }
    }

    pub fn overhead(&self) -> usize {
        self.nonce_size + self.tag_size
    }

    pub fn ciphertext_size(&self, plaintext_size: usize) -> usize {
        plaintext_size + self.overhead()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encrypt_decrypt() {
        let keypair = super::super::keys::KeyPair::generate().unwrap();
        let shared = super::super::keys::derive_shared_secret(
            keypair.public_key(),
            keypair.secret_key(),
        );
        
        let encryptor = Encryptor::from_shared_secret(&shared).unwrap();
        let decryptor = Decryptor::from_shared_secret(&shared).unwrap();
        
        let plaintext = b"Quantum-resistant message";
        let encrypted = encryptor.encrypt(plaintext).unwrap();
        
        let decrypted = decryptor.decrypt(&encrypted).unwrap();
        assert_eq!(decrypted, plaintext);
    }

    #[test]
    fn test_combined_format() {
        let key = generate_symmetric_key();
        let plaintext = b"Combined format test";
        
        let combined = symmetric_encrypt(plaintext, &key).unwrap();
        let decrypted = symmetric_decrypt(&combined, &key).unwrap();
        
        assert_eq!(decrypted, plaintext);
    }

    #[test]
    fn test_encryption_scheme() {
        let scheme = EncryptionScheme::aes256gcm();
        let plaintext_size = 1024;
        
        assert_eq!(
            scheme.ciphertext_size(plaintext_size),
            plaintext_size + scheme.overhead()
        );
    }

    #[test]
    fn test_invalid_decryption() {
        let key = generate_symmetric_key();
        let wrong_key = generate_symmetric_key();
        
        let combined = symmetric_encrypt(b"secret", &key).unwrap();
        let result = symmetric_decrypt(&combined, &wrong_key);
        
        assert!(result.is_err());
    }
}
