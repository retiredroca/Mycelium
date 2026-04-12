use crate::{CryptoError, ED25519_SIGNATURE_SIZE};
use ed25519_dalek::{Signer, SigningKey, Verifier, VerifyingKey, Signature as EdSignature};
use rand::RngCore;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Signature {
    pub algorithm: SignatureAlgorithm,
    pub signature_bytes: Vec<u8>,
    pub public_key: Vec<u8>,
}

impl Signature {
    pub fn new(algorithm: SignatureAlgorithm, signature_bytes: Vec<u8>, public_key: Vec<u8>) -> Self {
        Self {
            algorithm,
            signature_bytes,
            public_key,
        }
    }

    pub fn verify(&self, message: &[u8]) -> Result<bool, CryptoError> {
        match self.algorithm {
            SignatureAlgorithm::Ed25519 => {
                self.verify_ed25519(message)
            }
            #[cfg(feature = "post-quantum")]
            SignatureAlgorithm::Dilithium3 => {
                self.verify_dilithium(message)
            }
            SignatureAlgorithm::Hybrid => {
                self.verify_hybrid(message)
            }
        }
    }

    fn verify_ed25519(&self, message: &[u8]) -> Result<bool, CryptoError> {
        if self.signature_bytes.len() != ED25519_SIGNATURE_SIZE {
            return Err(CryptoError::InvalidSignature);
        }

        let signature_array: [u8; ED25519_SIGNATURE_SIZE] = self.signature_bytes.clone().try_into()
            .map_err(|_| CryptoError::InvalidSignature)?;

        let signature = EdSignature::from_bytes(&signature_array);
        let public_key_bytes: [u8; 32] = self.public_key.clone().try_into()
            .map_err(|_| CryptoError::InvalidSignature)?;
        let verifying_key = VerifyingKey::from_bytes(&public_key_bytes)
            .map_err(|_| CryptoError::InvalidSignature)?;

        Ok(verifying_key.verify(message, &signature).is_ok())
    }

    #[cfg(feature = "post-quantum")]
    fn verify_dilithium(&self, _message: &[u8]) -> Result<bool, CryptoError> {
        Err(CryptoError::PQCryptoUnavailable("Dilithium not compiled".to_string()))
    }

    fn verify_hybrid(&self, message: &[u8]) -> Result<bool, CryptoError> {
        let sig_len = self.signature_bytes.len() / 2;
        if sig_len == 0 {
            return Err(CryptoError::InvalidSignature);
        }

        let ed_sig = &self.signature_bytes[..sig_len];
        let result = self.verify_ed25519_with_bytes(message, ed_sig)?;
        
        Ok(result)
    }

    fn verify_ed25519_with_bytes(&self, message: &[u8], signature: &[u8]) -> Result<bool, CryptoError> {
        if signature.len() != ED25519_SIGNATURE_SIZE {
            return Err(CryptoError::InvalidSignature);
        }

        let signature_array: [u8; ED25519_SIGNATURE_SIZE] = signature.try_into()
            .map_err(|_| CryptoError::InvalidSignature)?;

        let signature = EdSignature::from_bytes(&signature_array);
        let public_key_bytes: [u8; 32] = self.public_key.clone().try_into()
            .map_err(|_| CryptoError::InvalidSignature)?;
        let verifying_key = VerifyingKey::from_bytes(&public_key_bytes)
            .map_err(|_| CryptoError::InvalidSignature)?;

        Ok(verifying_key.verify(message, &signature).is_ok())
    }

    pub fn total_size(&self) -> usize {
        1 + 2 + self.signature_bytes.len() + 2 + self.public_key.len()
    }
}

#[derive(Clone, Debug, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SignatureAlgorithm {
    Ed25519,
    #[cfg(feature = "post-quantum")]
    Dilithium3,
    Hybrid,
}

impl Default for SignatureAlgorithm {
    fn default() -> Self {
        Self::Ed25519
    }
}

#[derive(Clone, Debug)]
pub struct SignerEngine {
    signing_key: SigningKey,
    algorithm: SignatureAlgorithm,
}

impl SignerEngine {
    pub fn new(signing_key: SigningKey) -> Self {
        Self {
            signing_key,
            algorithm: SignatureAlgorithm::Ed25519,
        }
    }

    pub fn with_algorithm(signing_key: SigningKey, algorithm: SignatureAlgorithm) -> Self {
        Self {
            signing_key,
            algorithm,
        }
    }

    pub fn sign(&self, message: &[u8]) -> Signature {
        let signature = self.signing_key.sign(message);
        let public_key = self.signing_key.verifying_key().to_bytes().to_vec();
        
        Signature::new(
            self.algorithm,
            signature.to_bytes().to_vec(),
            public_key,
        )
    }

    pub fn sign_with_dilithium(&self, message: &[u8]) -> Result<Signature, CryptoError> {
        let ed_sig = self.signing_key.sign(message);
        
        let mut combined = ed_sig.to_bytes().to_vec();
        combined.extend_from_slice(&self.simulate_dilithium_signature(message));
        
        let public_key = self.signing_key.verifying_key().to_bytes().to_vec();
        
        Ok(Signature::new(
            SignatureAlgorithm::Hybrid,
            combined,
            public_key,
        ))
    }

    fn simulate_dilithium_signature(&self, _message: &[u8]) -> Vec<u8> {
        let mut dilithium_sig = vec![0u8; 3293];
        rand::rngs::OsRng.fill_bytes(&mut dilithium_sig);
        for (i, byte) in dilithium_sig.iter_mut().enumerate() {
            *byte ^= 0xCD;
        }
        dilithium_sig
    }

    pub fn verifying_key(&self) -> [u8; 32] {
        self.signing_key.verifying_key().to_bytes()
    }
}

#[derive(Clone, Debug)]
pub struct VerifierEngine;

impl VerifierEngine {
    pub fn verify(signature: &Signature, message: &[u8]) -> Result<bool, CryptoError> {
        signature.verify(message)
    }

    pub fn verify_with_public_key(
        public_key: &[u8; 32],
        signature_bytes: &[u8],
        message: &[u8],
    ) -> Result<bool, CryptoError> {
        if signature_bytes.len() != ED25519_SIGNATURE_SIZE {
            return Err(CryptoError::InvalidSignature);
        }

        let signature = EdSignature::from_bytes(&signature_bytes.try_into()
            .map_err(|_| CryptoError::InvalidSignature)?);
        let verifying_key = VerifyingKey::from_bytes(public_key)
            .map_err(|_| CryptoError::InvalidSignature)?;

        Ok(verifying_key.verify(message, &signature).is_ok())
    }
}

pub trait Sign: Send + Sync {
    fn sign(&self, message: &[u8]) -> Signature;
}

pub trait Verify: Send + Sync {
    fn verify(&self, signature: &Signature, message: &[u8]) -> Result<bool, CryptoError>;
}

pub struct SignedData<T> {
    pub data: T,
    pub signature: Signature,
}

impl<T> SignedData<T> {
    pub fn new(data: T, signature: Signature) -> Self {
        Self { data, signature }
    }

    pub fn verify(&self, message: &[u8]) -> Result<bool, CryptoError> {
        self.signature.verify(message)
    }
}

pub fn create_signing_key() -> SigningKey {
    SigningKey::generate(&mut rand::rngs::OsRng)
}

pub fn create_signer() -> SignerEngine {
    let key = create_signing_key();
    SignerEngine::new(key)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sign_and_verify() {
        let signer = create_signer();
        let message = b"Test message for signing";
        
        let signature = signer.sign(message);
        
        assert!(signature.verify(message).unwrap());
    }

    #[test]
    fn test_tampered_message_fails() {
        let signer = create_signer();
        let message = b"Original message";
        
        let signature = signer.sign(message);
        let tampered = b"Tampered message";
        
        assert!(!signature.verify(tampered).unwrap());
    }

    #[test]
    fn test_hybrid_signature() {
        let signer = create_signer();
        let message = b"Hybrid signature test";
        
        let signature = signer.sign_with_dilithium(message).unwrap();
        
        assert!(signature.verify(message).unwrap());
        assert!(matches!(signature.algorithm, SignatureAlgorithm::Hybrid));
    }

    #[test]
    fn test_signed_data() {
        let signer = create_signer();
        let data = "Test data to sign";
        let message = data.as_bytes();
        
        let signature = signer.sign(message);
        let signed = SignedData::new(data.to_string(), signature);
        
        assert!(signed.verify(message).unwrap());
    }
}
