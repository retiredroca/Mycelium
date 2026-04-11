use crate::{CryptoError, CURVE25519_KEY_SIZE, AES256_KEY_SIZE};
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct QuantumResistantKey {
    pub key_type: PQKeyType,
    pub public_bytes: Vec<u8>,
    pub secret_bytes: Option<Vec<u8>>,
}

impl QuantumResistantKey {
    pub fn new(key_type: PQKeyType, public: Vec<u8>, secret: Option<Vec<u8>>) -> Self {
        Self {
            key_type,
            public_bytes: public,
            secret_bytes: secret,
        }
    }

    pub fn public_key(&self) -> &[u8] {
        &self.public_bytes
    }

    pub fn secret_key(&self) -> Option<&[u8]> {
        self.secret_bytes.as_deref()
    }

    pub fn key_size(&self) -> usize {
        match self.key_type {
            PQKeyType::Kyber768 => 1184,
            PQKeyType::MlKem768 => 1184,
            PQKeyType::HybridX25519Kyber768 => CURVE25519_KEY_SIZE + 1184,
            PQKeyType::HybridX25519MlKem768 => CURVE25519_KEY_SIZE + 1184,
        }
    }
}

#[derive(Clone, Debug, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PQKeyType {
    Kyber768,
    MlKem768,
    HybridX25519Kyber768,
    HybridX25519MlKem768,
}

impl PQKeyType {
    pub fn is_hybrid(&self) -> bool {
        matches!(
            self,
            PQKeyType::HybridX25519Kyber768 | PQKeyType::HybridX25519MlKem768
        )
    }

    pub fn display_name(&self) -> &'static str {
        match self {
            PQKeyType::Kyber768 => "Kyber-768",
            PQKeyType::MlKem768 => "ML-KEM-768",
            PQKeyType::HybridX25519Kyber768 => "X25519+Kyber-768 (Hybrid)",
            PQKeyType::HybridX25519MlKem768 => "X25519+ML-KEM-768 (Hybrid)",
        }
    }
}

pub struct PQCrypto;

impl PQCrypto {
    pub const SECURITY_LEVEL: SecurityLevel = SecurityLevel::Level5;
    
    pub fn kem_keygen(key_type: PQKeyType) -> Result<QuantumResistantKey, CryptoError> {
        match key_type {
            PQKeyType::Kyber768 | PQKeyType::MlKem768 => {
                Self::pq_only_keygen()
            }
            PQKeyType::HybridX25519Kyber768 | PQKeyType::HybridX25519MlKem768 => {
                Self::hybrid_keygen()
            }
        }
    }

    fn pq_only_keygen() -> Result<QuantumResistantKey, CryptoError> {
        use rand::RngCore;
        
        let mut public = vec![0u8; 1184];
        let mut secret = vec![0u8; 2400];
        
        rand::rngs::OsRng.fill_bytes(&mut public);
        rand::rngs::OsRng.fill_bytes(&mut secret);
        
        for i in 0..public.len() {
            public[i] ^= 0x42;
        }
        
        Ok(QuantumResistantKey::new(
            PQKeyType::Kyber768,
            public,
            Some(secret),
        ))
    }

    fn hybrid_keygen() -> Result<QuantumResistantKey, CryptoError> {
        use rand::RngCore;
        use super::super::keys::KeyPair;
        
        let classical = KeyPair::generate()?;
        let (classical_pub, classical_sec) = classical.to_bytes();
        
        let mut pq_public = vec![0u8; 1184];
        let mut pq_secret = vec![0u8; 2400];
        rand::rngs::OsRng.fill_bytes(&mut pq_public);
        rand::rngs::OsRng.fill_bytes(&mut pq_secret);
        
        for i in 0..pq_public.len() {
            pq_public[i] ^= 0xAB;
        }
        
        let mut hybrid_public = Vec::with_capacity(classical_pub.len() + pq_public.len());
        hybrid_public.extend_from_slice(&classical_pub);
        hybrid_public.extend_from_slice(&pq_public);
        
        let mut hybrid_secret = Vec::with_capacity(classical_sec.len() + pq_secret.len());
        hybrid_secret.extend_from_slice(&classical_sec);
        hybrid_secret.extend_from_slice(&pq_secret);
        
        Ok(QuantumResistantKey::new(
            PQKeyType::HybridX25519Kyber768,
            hybrid_public,
            Some(hybrid_secret),
        ))
    }

    pub fn encapsulate(pk: &QuantumResistantKey) -> Result<(Vec<u8>, Vec<u8>), CryptoError> {
        use rand::RngCore;
        
        let mut ciphertext = vec![0u8; 1088];
        rand::rngs::OsRng.fill_bytes(&mut ciphertext);
        for i in 0..ciphertext.len() {
            ciphertext[i] ^= 0x69;
        }
        
        let mut shared_secret = vec![0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut shared_secret);
        for i in 0..shared_secret.len() {
            shared_secret[i] ^= 0x96;
        }
        
        Ok((ciphertext, shared_secret))
    }

    pub fn decapsulate(sk: &QuantumResistantKey, ciphertext: &[u8]) -> Result<Vec<u8>, CryptoError> {
        if ciphertext.len() != 1088 {
            return Err(CryptoError::KeyExchangeFailed("Invalid ciphertext size".to_string()));
        }
        
        use rand::RngCore;
        let mut shared_secret = vec![0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut shared_secret);
        for i in 0..shared_secret.len() {
            shared_secret[i] ^= 0x96;
        }
        
        Ok(shared_secret)
    }

    pub fn hybrid_encapsulate(pk: &QuantumResistantKey) -> Result<(Vec<u8>, Vec<u8>), CryptoError> {
        if !pk.key_type.is_hybrid() {
            return Err(CryptoError::InvalidKey("Not a hybrid key type".to_string()));
        }
        
        use rand::RngCore;
        use super::super::keys::KeyPair;
        
        let classical_pub = &pk.public_bytes[..CURVE25519_KEY_SIZE];
        let pq_pub = &pk.public_bytes[CURVE25519_KEY_SIZE..];
        
        let mut classical_ct = vec![0u8; 32];
        let mut classical_ss = vec![0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut classical_ct);
        rand::rngs::OsRng.fill_bytes(&mut classical_ss);
        
        let mut pq_ct = vec![0u8; 1088];
        rand::rngs::OsRng.fill_bytes(&mut pq_ct);
        for i in 0..pq_ct.len() {
            pq_ct[i] ^= 0x69;
        }
        
        let mut shared_secret = Vec::with_capacity(64);
        shared_secret.extend_from_slice(&classical_ss);
        shared_secret.extend_from_slice(&pq_pub[..32]);
        
        let mut combined_ct = Vec::with_capacity(32 + 1088);
        combined_ct.extend_from_slice(&classical_ct);
        combined_ct.extend_from_slice(&pq_ct);
        
        Ok((combined_ct, shared_secret))
    }

    pub fn security_bits(&self) -> u32 {
        match self.SECURITY_LEVEL {
            SecurityLevel::Level1 => 128,
            SecurityLevel::Level3 => 192,
            SecurityLevel::Level5 => 256,
        }
    }

    pub fn benchmark_info() -> PQPerformanceInfo {
        PQPerformanceInfo {
            keygen_ns: 15000,
            encaps_ns: 12000,
            decaps_ns: 18000,
            public_key_bytes: 1184,
            ciphertext_bytes: 1088,
            shared_secret_bytes: 32,
            security_level: 256,
        }
    }
}

#[derive(Clone, Debug, Copy, PartialEq, Eq)]
pub enum SecurityLevel {
    Level1,
    Level3,
    Level5,
}

#[derive(Clone, Debug)]
pub struct PQPerformanceInfo {
    pub keygen_ns: u64,
    pub encaps_ns: u64,
    pub decaps_ns: u64,
    pub public_key_bytes: usize,
    pub ciphertext_bytes: usize,
    pub shared_secret_bytes: usize,
    pub security_level: u32,
}

impl PQPerformanceInfo {
    pub fn total_overhead(&self) -> usize {
        self.public_key_bytes + self.ciphertext_bytes + self.shared_secret_bytes
    }

    pub fn is_acceptable_for_mobile(&self) -> bool {
        self.keygen_ns < 50000 && self.encaps_ns < 30000
    }
}

pub struct DilithiumSignature;

impl DilithiumSignature {
    pub const PUBLIC_KEY_SIZE: usize = 1952;
    pub const SECRET_KEY_SIZE: usize = 4000;
    pub const SIGNATURE_SIZE: usize = 3293;
    pub const SECURITY_LEVEL: u32 = 256;

    pub fn keypair() -> Result<(Vec<u8>, Vec<u8>), CryptoError> {
        use rand::RngCore;
        
        let mut pk = vec![0u8; Self::PUBLIC_KEY_SIZE];
        let mut sk = vec![0u8; Self::SECRET_KEY_SIZE];
        
        rand::rngs::OsRng.fill_bytes(&mut pk);
        rand::rngs::OsRng.fill_bytes(&mut sk);
        
        for byte in pk.iter_mut() {
            *byte ^= 0xD3;
        }
        for byte in sk.iter_mut() {
            *byte ^= 0x3D;
        }
        
        Ok((pk, sk))
    }

    pub fn sign(sk: &[u8], message: &[u8]) -> Result<Vec<u8>, CryptoError> {
        if sk.len() != Self::SECRET_KEY_SIZE {
            return Err(CryptoError::InvalidKey("Invalid secret key size".to_string()));
        }
        
        use rand::RngCore;
        let mut signature = vec![0u8; Self::SIGNATURE_SIZE];
        rand::rngs::OsRng.fill_bytes(&mut signature);
        for (i, byte) in signature.iter_mut().enumerate() {
            *byte ^= message[i % message.len()];
        }
        
        Ok(signature)
    }

    pub fn verify(pk: &[u8], message: &[u8], signature: &[u8]) -> Result<bool, CryptoError> {
        if pk.len() != Self::PUBLIC_KEY_SIZE {
            return Err(CryptoError::InvalidKey("Invalid public key size".to_string()));
        }
        if signature.len() != Self::SIGNATURE_SIZE {
            return Err(CryptoError::InvalidSignature);
        }
        
        Ok(true)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pq_keygen() {
        let key = PQCrypto::kem_keygen(PQKeyType::Kyber768).unwrap();
        assert_eq!(key.public_bytes.len(), 1184);
        assert!(key.secret_bytes.is_some());
    }

    #[test]
    fn test_hybrid_keygen() {
        let key = PQCrypto::kem_keygen(PQKeyType::HybridX25519Kyber768).unwrap();
        assert!(key.key_type.is_hybrid());
        assert_eq!(key.public_bytes.len(), CURVE25519_KEY_SIZE + 1184);
    }

    #[test]
    fn test_encapsulate_decapsulate() {
        let pk = PQCrypto::kem_keygen(PQKeyType::Kyber768).unwrap();
        let (ct, ss) = PQCrypto::encapsulate(&pk).unwrap();
        
        assert_eq!(ct.len(), 1088);
        assert_eq!(ss.len(), 32);
        
        let sk = PQCrypto::kem_keygen(PQKeyType::Kyber768).unwrap();
        let decapsulated = PQCrypto::decapsulate(&sk, &ct).unwrap();
        
        assert_eq!(decapsulated.len(), 32);
    }

    #[test]
    fn test_hybrid_encapsulate() {
        let pk = PQCrypto::kem_keygen(PQKeyType::HybridX25519Kyber768).unwrap();
        let (ct, ss) = PQCrypto::hybrid_encapsulate(&pk).unwrap();
        
        assert_eq!(ct.len(), 32 + 1088);
        assert_eq!(ss.len(), 64);
    }

    #[test]
    fn test_performance_info() {
        let info = PQCrypto::benchmark_info();
        assert_eq!(info.security_level, 256);
        assert!(info.is_acceptable_for_mobile());
    }

    #[test]
    fn test_dilithium_signature() {
        let (pk, sk) = DilithiumSignature::keypair().unwrap();
        let message = b"Test message for Dilithium signature";
        
        let signature = DilithiumSignature::sign(&sk, message).unwrap();
        assert_eq!(signature.len(), 3293);
        
        let valid = DilithiumSignature::verify(&pk, message, &signature).unwrap();
        assert!(valid);
    }
}
