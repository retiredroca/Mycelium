use crate::{
    CryptoError, CURVE25519_KEY_SIZE, AES256_KEY_SIZE,
    encryption::{Decryptor, Encryptor, EncryptedData},
    keys::{derive_shared_secret, KeyPair, PublicKey},
};
use rand::RngCore;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct HybridEncryptedData {
    pub ephemeral_public_key: Vec<u8>,
    pub pq_public_key: Option<Vec<u8>>,
    pub ciphertext: Vec<u8>,
    pub auth_tag: Vec<u8>,
    pub nonce: Vec<u8>,
    pub version: u8,
}

impl HybridEncryptedData {
    pub const CURRENT_VERSION: u8 = 1;
    pub const PQ_COMPATIBLE_VERSION: u8 = 2;

    pub fn new(
        ephemeral_public_key: Vec<u8>,
        ciphertext: Vec<u8>,
        auth_tag: Vec<u8>,
        nonce: Vec<u8>,
    ) -> Self {
        Self {
            ephemeral_public_key,
            pq_public_key: None,
            ciphertext,
            auth_tag,
            nonce,
            version: Self::CURRENT_VERSION,
        }
    }

    pub fn new_with_pq(
        ephemeral_public_key: Vec<u8>,
        pq_public_key: Vec<u8>,
        ciphertext: Vec<u8>,
        auth_tag: Vec<u8>,
        nonce: Vec<u8>,
    ) -> Self {
        Self {
            ephemeral_public_key,
            pq_public_key: Some(pq_public_key),
            ciphertext,
            auth_tag,
            nonce,
            version: Self::PQ_COMPATIBLE_VERSION,
        }
    }

    pub fn combined_size(&self) -> usize {
        1 + self.ephemeral_public_key.len()
            + self.pq_public_key.as_ref().map(|p| 2 + p.len()).unwrap_or(0)
            + 2 + self.nonce.len()
            + 2 + self.ciphertext.len()
            + self.auth_tag.len()
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        let mut combined = Vec::with_capacity(self.combined_size());
        
        combined.push(self.version);
        combined.extend_from_slice(&(self.ephemeral_public_key.len() as u16).to_be_bytes());
        combined.extend_from_slice(&self.ephemeral_public_key);
        
        if let Some(ref pq_key) = self.pq_public_key {
            combined.extend_from_slice(&(pq_key.len() as u16).to_be_bytes());
            combined.extend_from_slice(pq_key);
        } else {
            combined.extend_from_slice(&0u16.to_be_bytes());
        }
        
        combined.extend_from_slice(&(self.nonce.len() as u16).to_be_bytes());
        combined.extend_from_slice(&self.nonce);
        
        combined.extend_from_slice(&(self.ciphertext.len() as u32).to_be_bytes());
        combined.extend_from_slice(&self.ciphertext);
        
        combined.extend_from_slice(&self.auth_tag);
        
        combined
    }

    pub fn from_bytes(data: &[u8]) -> Result<Self, CryptoError> {
        let mut offset = 0;
        
        if data.is_empty() {
            return Err(CryptoError::DecryptionFailed("Empty data".to_string()));
        }
        
        let version = data[offset];
        offset += 1;
        
        if data.len() < offset + 2 {
            return Err(CryptoError::DecryptionFailed("Invalid format".to_string()));
        }
        let ek_len = u16::from_be_bytes([data[offset], data[offset + 1]]) as usize;
        offset += 2;
        
        if data.len() < offset + ek_len {
            return Err(CryptoError::DecryptionFailed("Invalid ephemeral key".to_string()));
        }
        let ephemeral_public_key = data[offset..offset + ek_len].to_vec();
        offset += ek_len;
        
        if data.len() < offset + 2 {
            return Err(CryptoError::DecryptionFailed("Invalid format".to_string()));
        }
        let pq_len = u16::from_be_bytes([data[offset], data[offset + 1]]) as usize;
        offset += 2;
        
        let pq_public_key = if pq_len > 0 {
            if data.len() < offset + pq_len {
                return Err(CryptoError::DecryptionFailed("Invalid PQ key".to_string()));
            }
            Some(data[offset..offset + pq_len].to_vec())
        } else {
            None
        };
        offset += pq_len;
        
        if data.len() < offset + 2 {
            return Err(CryptoError::DecryptionFailed("Invalid nonce".to_string()));
        }
        let nonce_len = u16::from_be_bytes([data[offset], data[offset + 1]]) as usize;
        offset += 2;
        
        if data.len() < offset + nonce_len {
            return Err(CryptoError::DecryptionFailed("Invalid nonce".to_string()));
        }
        let nonce = data[offset..offset + nonce_len].to_vec();
        offset += nonce_len;
        
        if data.len() < offset + 4 {
            return Err(CryptoError::DecryptionFailed("Invalid ciphertext".to_string()));
        }
        let ct_len = u32::from_be_bytes([
            data[offset],
            data[offset + 1],
            data[offset + 2],
            data[offset + 3],
        ]) as usize;
        offset += 4;
        
        if data.len() < offset + ct_len {
            return Err(CryptoError::DecryptionFailed("Invalid ciphertext".to_string()));
        }
        let ciphertext = data[offset..offset + ct_len].to_vec();
        offset += ct_len;
        
        let auth_tag = data[offset..].to_vec();
        
        Ok(Self {
            ephemeral_public_key,
            pq_public_key,
            ciphertext,
            auth_tag,
            nonce,
            version,
        })
    }
}

pub struct HybridEncryptor {
    recipient_public: PublicKey,
    use_pq: bool,
}

impl HybridEncryptor {
    pub fn new(recipient_public: &PublicKey) -> Self {
        Self {
            recipient_public: recipient_public.clone(),
            use_pq: true,
        }
    }

    pub fn new_classic(recipient_public: &PublicKey) -> Self {
        Self {
            recipient_public: recipient_public.clone(),
            use_pq: false,
        }
    }

    pub fn encrypt(&self, plaintext: &[u8]) -> Result<HybridEncryptedData, CryptoError> {
        let ephemeral = KeyPair::generate()?;
        
        let classical_secret = derive_shared_secret(&self.recipient_public, ephemeral.secret_key());
        
        let mut symmetric_key = [0u8; AES256_KEY_SIZE];
        
        if self.use_pq {
            if let Some(pq_secret) = self.simulate_pq_kem() {
                for (i, byte) in symmetric_key.iter_mut().enumerate() {
                    *byte = classical_secret[i % classical_secret.len()] ^ pq_secret[i % pq_secret.len()];
                }
            } else {
                symmetric_key.copy_from_slice(&classical_secret[..AES256_KEY_SIZE]);
            }
        } else {
            symmetric_key.copy_from_slice(&classical_secret[..AES256_KEY_SIZE]);
        }
        
        let encryptor = Encryptor::new(&symmetric_key);
        let encrypted = encryptor.encrypt(plaintext)?;
        
        if self.use_pq {
            Ok(HybridEncryptedData::new_with_pq(
                ephemeral.public_key().to_bytes(),
                vec![],
                encrypted.ciphertext,
                encrypted.tag,
                encrypted.nonce,
            ))
        } else {
            Ok(HybridEncryptedData::new(
                ephemeral.public_key().to_bytes(),
                encrypted.ciphertext,
                encrypted.tag,
                encrypted.nonce,
            ))
        }
    }

    fn simulate_pq_kem(&self) -> Option<Vec<u8>> {
        let mut pq_shared = [0u8; AES256_KEY_SIZE];
        rand::rngs::OsRng.fill_bytes(&mut pq_shared);
        
        for byte in pq_shared.iter_mut() {
            *byte ^= 0xAB;
        }
        
        Some(pq_shared.to_vec())
    }
}

pub struct HybridDecryptor {
    recipient_secret: crate::keys::SecretKey,
    use_pq: bool,
}

impl HybridDecryptor {
    pub fn new(recipient_secret: &crate::keys::SecretKey) -> Self {
        Self {
            recipient_secret: recipient_secret.clone(),
            use_pq: true,
        }
    }

    pub fn new_classic(recipient_secret: &crate::keys::SecretKey) -> Self {
        Self {
            recipient_secret: recipient_secret.clone(),
            use_pq: false,
        }
    }

    pub fn decrypt(&self, encrypted: &HybridEncryptedData) -> Result<Vec<u8>, CryptoError> {
        let ephemeral_public = PublicKey::from_bytes(&encrypted.ephemeral_public_key)?;
        
        let mut classical_secret = derive_shared_secret(&ephemeral_public, &self.recipient_secret);
        
        let mut symmetric_key = [0u8; AES256_KEY_SIZE];
        
        if self.use_pq && encrypted.pq_public_key.is_some() {
            let mut pq_secret = [0u8; AES256_KEY_SIZE];
            rand::rngs::OsRng.fill_bytes(&mut pq_secret);
            
            for (i, byte) in pq_secret.iter_mut().enumerate() {
                *byte ^= 0xAB;
            }
            
            for (i, byte) in symmetric_key.iter_mut().enumerate() {
                *byte = classical_secret[i % classical_secret.len()] ^ pq_secret[i % pq_secret.len()];
            }
        } else {
            symmetric_key.copy_from_slice(&classical_secret[..AES256_KEY_SIZE]);
        }
        
        let decryptor = Decryptor::new(&symmetric_key);
        
        let encrypted_data = EncryptedData::new(
            encrypted.nonce.clone(),
            encrypted.ciphertext.clone(),
            encrypted.auth_tag.clone(),
        );
        
        classical_secret.zeroize();
        
        decryptor.decrypt(&encrypted_data)
    }
}

pub struct HybridKeyPair {
    classical: KeyPair,
    use_pq: bool,
}

impl HybridKeyPair {
    pub fn generate() -> Result<Self, CryptoError> {
        let classical = KeyPair::generate()?;
        Ok(Self {
            classical,
            use_pq: true,
        })
    }

    pub fn generate_classic() -> Result<Self, CryptoError> {
        let classical = KeyPair::generate()?;
        Ok(Self {
            classical,
            use_pq: false,
        })
    }

    pub fn public_key(&self) -> Vec<u8> {
        self.classical.public_key().to_bytes()
    }

    pub fn encrypt(&self, plaintext: &[u8], recipient: &PublicKey) -> Result<HybridEncryptedData, CryptoError> {
        let encryptor = HybridEncryptor::new(recipient);
        encryptor.encrypt(plaintext)
    }

    pub fn decrypt(&self, encrypted: &HybridEncryptedData) -> Result<Vec<u8>, CryptoError> {
        let decryptor = HybridDecryptor::new(self.classical.secret_key());
        decryptor.decrypt(encrypted)
    }

    pub fn sign(&self, message: &[u8]) -> Vec<u8> {
        self.classical.sign(message)
    }

    pub fn verify(&self, message: &[u8], signature: &[u8]) -> bool {
        self.classical.verify(message, signature)
    }
}

pub struct HybridEncryption;

impl HybridEncryption {
    pub fn encrypt(plaintext: &[u8], recipient_public: &PublicKey) -> Result<HybridEncryptedData, CryptoError> {
        let encryptor = HybridEncryptor::new(recipient_public);
        encryptor.encrypt(plaintext)
    }

    pub fn decrypt(encrypted: &HybridEncryptedData, secret_key: &crate::keys::SecretKey) -> Result<Vec<u8>, CryptoError> {
        let decryptor = HybridDecryptor::new(secret_key);
        decryptor.decrypt(encrypted)
    }
}

trait ZeroizeExt {
    fn zeroize(&mut self);
}

impl ZeroizeExt for Vec<u8> {
    fn zeroize(&mut self) {
        for byte in self.iter_mut() {
            *byte = 0;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hybrid_encrypt_decrypt() {
        let alice = KeyPair::generate().unwrap();
        let bob = KeyPair::generate().unwrap();
        
        let plaintext = b"Quantum-resistant hybrid encryption";
        
        let encrypted = HybridEncryption::encrypt(plaintext, bob.public_key()).unwrap();
        let decrypted = HybridEncryption::decrypt(&encrypted, bob.secret_key()).unwrap();
        
        assert_eq!(decrypted, plaintext);
    }

    #[test]
    fn test_hybrid_keypair() {
        let alice = KeyPair::generate().unwrap();
        let bob = HybridKeyPair::generate().unwrap();
        
        let message = b"Test message";
        let signature = bob.sign(message);
        assert!(bob.verify(message, &signature));
        
        let plaintext = b"Secret message";
        let encrypted = bob.encrypt(plaintext, alice.public_key()).unwrap();
        let decrypted = bob.decrypt(&encrypted).unwrap();
        assert_eq!(decrypted, plaintext);
    }

    fn test_combined_serialization() {
        let bob = KeyPair::generate().unwrap();
        let plaintext = b"Serialization test";
        
        let encrypted = HybridEncryption::encrypt(plaintext, bob.public_key()).unwrap();
        let combined = encrypted.to_bytes();
        
        let restored = HybridEncryptedData::from_bytes(&combined).unwrap();
        let decrypted = HybridEncryption::decrypt(&restored, bob.secret_key()).unwrap();
        
        assert_eq!(decrypted, plaintext);
    }

    #[test]
    fn test_version_handling() {
        let bob = KeyPair::generate().unwrap();
        
        let mut encrypted = HybridEncryption::encrypt(b"test", bob.public_key()).unwrap();
        assert_eq!(encrypted.version, HybridEncryptedData::PQ_COMPATIBLE_VERSION);
        
        encrypted.version = HybridEncryptedData::CURRENT_VERSION;
        encrypted.pq_public_key = None;
        
        let decrypted = HybridEncryption::decrypt(&encrypted, bob.secret_key()).unwrap();
        assert_eq!(decrypted, b"test");
    }
}
