use crate::{CryptoError, CURVE25519_KEY_SIZE};
use ed25519_dalek::{Signer, SigningKey, Verifier, VerifyingKey};
use rand::RngCore;
use serde::{Deserialize, Serialize};
use zeroize::{Zeroize, ZeroizeOnDrop};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct KeyMaterial {
    pub key_type: KeyType,
    pub public_bytes: Vec<u8>,
    #[serde(skip)]
    secret_bytes: Option<ZeroizedSecret>,
}

impl KeyMaterial {
    pub fn new_public(public: Vec<u8>) -> Self {
        Self {
            key_type: KeyType::Curve25519,
            public_bytes: public,
            secret_bytes: None,
        }
    }

    pub fn new(public: Vec<u8>, secret: Vec<u8>) -> Self {
        Self {
            key_type: KeyType::Curve25519,
            public_bytes: public,
            secret_bytes: Some(ZeroizedSecret::new(secret)),
        }
    }

    pub fn public_key(&self) -> Option<&[u8]> {
        if self.secret_bytes.is_some() {
            Some(&self.public_bytes)
        } else {
            None
        }
    }

    pub fn secret_key(&self) -> Option<&ZeroizedSecret> {
        self.secret_bytes.as_ref()
    }

    pub fn has_secret(&self) -> bool {
        self.secret_bytes.is_some()
    }

    pub fn to_base64(&self) -> String {
        base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            &self.public_bytes,
        )
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum KeyType {
    Curve25519,
    #[cfg(feature = "post-quantum")]
    Kyber768,
    Hybrid,
}

impl Default for KeyType {
    fn default() -> Self {
        Self::Curve25519
    }
}

#[derive(Clone, Debug, Zeroize, ZeroizeOnDrop)]
pub struct ZeroizedSecret(Vec<u8>);

impl ZeroizedSecret {
    pub fn new(data: Vec<u8>) -> Self {
        Self(data)
    }

    pub fn as_bytes(&self) -> &[u8] {
        &self.0
    }

    pub fn len(&self) -> usize {
        self.0.len()
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }

    pub fn to_vec(&self) -> Vec<u8> {
        self.0.clone()
    }
}

#[derive(Clone, Debug)]
pub struct SecretKey {
    scalar: [u8; CURVE25519_KEY_SIZE],
}

impl SecretKey {
    pub fn generate() -> Result<Self, CryptoError> {
        let mut scalar = [0u8; CURVE25519_KEY_SIZE];
        rand::rngs::OsRng.fill_bytes(&mut scalar);
        
        scalar[0] &= 248;
        scalar[31] &= 127;
        scalar[31] |= 64;
        
        Ok(Self { scalar })
    }

    pub fn from_bytes(bytes: &[u8]) -> Result<Self, CryptoError> {
        if bytes.len() != CURVE25519_KEY_SIZE {
            return Err(CryptoError::InvalidKey(format!(
                "Expected {} bytes, got {}",
                CURVE25519_KEY_SIZE,
                bytes.len()
            )));
        }
        
        let mut scalar = [0u8; CURVE25519_KEY_SIZE];
        scalar.copy_from_slice(bytes);
        
        scalar[0] &= 248;
        scalar[31] &= 127;
        scalar[31] |= 64;
        
        Ok(Self { scalar })
    }

    pub fn as_bytes(&self) -> &[u8; CURVE25519_KEY_SIZE] {
        &self.scalar
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        self.scalar.to_vec()
    }
}

#[derive(Clone, Debug)]
pub struct PublicKey {
    point: [u8; CURVE25519_KEY_SIZE],
}

impl PublicKey {
    pub fn from_secret(secret: &SecretKey) -> Self {
        use curve25519_dalek::constants::ED25519_BASEPOINT_TABLE;
        use curve25519_dalek::scalar::Scalar;
        
        let scalar = Scalar::from_bytes_mod_order(*secret.as_bytes());
        let point = &scalar * ED25519_BASEPOINT_TABLE;
        
        let mut point_bytes = [0u8; CURVE25519_KEY_SIZE];
        point_bytes.copy_from_slice(point.compress().as_bytes());
        
        Self { point: point_bytes }
    }

    pub fn from_bytes(bytes: &[u8]) -> Result<Self, CryptoError> {
        if bytes.len() != CURVE25519_KEY_SIZE {
            return Err(CryptoError::InvalidKey(format!(
                "Expected {} bytes, got {}",
                CURVE25519_KEY_SIZE,
                bytes.len()
            )));
        }
        
        let mut point = [0u8; CURVE25519_KEY_SIZE];
        point.copy_from_slice(bytes);
        
        Ok(Self { point })
    }

    pub fn as_bytes(&self) -> &[u8; CURVE25519_KEY_SIZE] {
        &self.point
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        self.point.to_vec()
    }

    pub fn to_base64(&self) -> String {
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &self.point)
    }
}

#[derive(Clone, Debug)]
pub struct KeyPair {
    secret: SecretKey,
    public: PublicKey,
}

impl KeyPair {
    pub fn generate() -> Result<Self, CryptoError> {
        let secret = SecretKey::generate()?;
        let public = PublicKey::from_secret(&secret);
        Ok(Self { secret, public })
    }

    pub fn from_secret(secret: SecretKey) -> Self {
        let public = PublicKey::from_secret(&secret);
        Self { secret, public }
    }

    pub fn public_key(&self) -> &PublicKey {
        &self.public
    }

    pub fn secret_key(&self) -> &SecretKey {
        &self.secret
    }

    pub fn to_bytes(&self) -> (Vec<u8>, Vec<u8>) {
        (self.secret.to_bytes(), self.public.to_bytes())
    }

    pub fn to_base64(&self) -> (String, String) {
        (
            base64::Engine::encode(&base64::engine::general_purpose::STANDARD, self.secret.to_bytes()),
            base64::Engine::encode(&base64::engine::general_purpose::STANDARD, self.public.to_bytes()),
        )
    }

    pub fn sign(&self, message: &[u8]) -> Vec<u8> {
        let signing_key = SigningKey::from_bytes(self.secret.as_bytes());
        let signature = signing_key.sign(message);
        signature.to_bytes().to_vec()
    }

    pub fn verify(&self, message: &[u8], signature: &[u8]) -> bool {
        if signature.len() != 64 {
            return false;
        }
        
        let verifying_key = VerifyingKey::from_bytes(self.public.as_bytes());
        if verifying_key.is_err() {
            return false;
        }
        
        let signature_bytes: [u8; 64] = match signature.try_into() {
            Ok(s) => s,
            Err(_) => return false,
        };
        
        let ed25519_sig = ed25519_dalek::Signature::from_bytes(&signature_bytes);
        verifying_key.unwrap().verify(message, &ed25519_sig).is_ok()
    }
}

#[derive(Clone, Debug, Zeroize, ZeroizeOnDrop)]
pub struct Ed25519KeyPair {
    signing_key: SigningKey,
}

impl Ed25519KeyPair {
    pub fn generate() -> Self {
        let signing_key = SigningKey::generate(&mut rand::rngs::OsRng);
        Self { signing_key }
    }

    pub fn from_seed(seed: &[u8; 32]) -> Self {
        let signing_key = SigningKey::from_bytes(seed);
        Self { signing_key }
    }

    pub fn sign(&self, message: &[u8]) -> [u8; 64] {
        self.signing_key.sign(message).to_bytes()
    }

    pub fn verifying_key(&self) -> [u8; 32] {
        self.signing_key.verifying_key().to_bytes()
    }
}

pub fn derive_shared_secret(their_public: &PublicKey, my_secret: &SecretKey) -> Vec<u8> {
    use curve25519_dalek::montgomery::MontgomeryPoint;
    
    let mut shared = [0u8; CURVE25519_KEY_SIZE];
    let point = MontgomeryPoint(*their_public.as_bytes());
    
    let shared_point = point.multiply_scalar(my_secret.as_bytes());
    
    shared.copy_from_slice(shared_point.as_bytes());
    shared[0] &= 248;
    shared[31] &= 127;
    shared[31] |= 64;
    
    shared.to_vec()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_key_generation() {
        let keypair = KeyPair::generate().unwrap();
        assert_eq!(keypair.public_key().as_bytes().len(), 32);
    }

    #[test]
    fn test_sign_verify() {
        let keypair = KeyPair::generate().unwrap();
        let message = b"Hello, Quantum-Resistant World!";
        
        let signature = keypair.sign(message);
        assert!(keypair.verify(message, &signature));
    }

    #[test]
    fn test_shared_secret() {
        let alice = KeyPair::generate().unwrap();
        let bob = KeyPair::generate().unwrap();
        
        let shared_alice = derive_shared_secret(bob.public_key(), alice.secret_key());
        let shared_bob = derive_shared_secret(alice.public_key(), bob.secret_key());
        
        assert_eq!(shared_alice, shared_bob);
    }

    #[test]
    fn test_invalid_signature() {
        let keypair = KeyPair::generate().unwrap();
        let message = b"Test message";
        let signature = keypair.sign(message);
        
        let tampered = b"Different message";
        assert!(!keypair.verify(tampered, &signature));
    }

    #[test]
    fn test_keypair_serialization() {
        let keypair = KeyPair::generate().unwrap();
        let (pub_b64, priv_b64) = keypair.to_base64();
        
        assert!(pub_b64.starts_with("SoV"));
        assert!(priv_b64.starts_with("SoV"));
    }
}
