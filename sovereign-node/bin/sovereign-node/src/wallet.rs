use ed25519_dalek::{Signer, SigningKey};
use sov_token::{Wallet, TokenError};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;

pub fn create_wallet() -> Result<Wallet, WalletError> {
    let signing_key = SigningKey::generate(&mut rand::rngs::OsRng);
    let public_key = signing_key.verifying_key();
    
    let mut wallet = Wallet::new(public_key.to_bytes());
    wallet.receive(1_000_000); // Initial faucet for testing
    
    Ok(wallet)
}

pub fn public_key_to_string(key: &[u8; 32]) -> String {
    use sha2::{Sha256, Digest};
    let mut hasher = Sha256::new();
    hasher.update(key);
    let result = hasher.finalize();
    format!("SOVEX{}", &base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &result[..16]))
}

pub struct WalletManager {
    wallets: Arc<RwLock<HashMap<String, Wallet>>>,
    active_wallet: Arc<RwLock<Option<String>>>,
}

impl WalletManager {
    pub fn new() -> Self {
        Self {
            wallets: Arc::new(RwLock::new(HashMap::new())),
            active_wallet: Arc::new(RwLock::new(None)),
        }
    }

    pub async fn create_wallet(&self, name: String) -> Result<Wallet, WalletError> {
        let wallet = create_wallet()?;
        let mut wallets = self.wallets.write().await;
        wallets.insert(name.clone(), wallet.clone());
        
        let mut active = self.active_wallet.write().await;
        *active = Some(name);
        
        Ok(wallet)
    }

    pub async fn get_active_wallet(&self) -> Option<Wallet> {
        let active = self.active_wallet.read().await;
        let wallets = self.wallets.read().await;
        active.as_ref().and_then(|name| wallets.get(name).cloned())
    }

    pub async fn list_wallets(&self) -> Vec<String> {
        let wallets = self.wallets.read().await;
        wallets.keys().cloned().collect()
    }
}

impl Default for WalletManager {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug)]
pub enum WalletError {
    InsufficientBalance,
    WalletNotFound,
    InvalidAddress,
}

impl std::fmt::Display for WalletError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            WalletError::InsufficientBalance => write!(f, "Insufficient balance"),
            WalletError::WalletNotFound => write!(f, "Wallet not found"),
            WalletError::InvalidAddress => write!(f, "Invalid address"),
        }
    }
}

impl std::error::Error for WalletError {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_wallet_creation() {
        let wallet = create_wallet().unwrap();
        assert_eq!(wallet.balance, 1_000_000);
        assert_eq!(wallet.staked_balance, 0);
    }

    #[test]
    fn test_public_key_conversion() {
        let key = [0u8; 32];
        let formatted = public_key_to_string(&key);
        assert!(formatted.starts_with("SOVEX"));
    }
}
