use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use chrono::{DateTime, Utc};

pub const TOTAL_SUPPLY: u64 = 1_000_000_000; // 1 billion tokens
pub const DECIMALS: u8 = 6;
pub const MIN_STAKE_AMOUNT: u64 = 100;
pub const PERMANENT_STAKE_MIN: u64 = 100;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Tokenomics {
    pub total_supply: u64,
    pub circulating_supply: u64,
    pub staked_supply: u64,
    pub burned_supply: u64,
    pub annual_inflation_rate: f64,
    pub current_epoch: u64,
}

impl Tokenomics {
    pub fn new() -> Self {
        Self {
            total_supply: TOTAL_SUPPLY,
            circulating_supply: TOTAL_SUPPLY,
            staked_supply: 0,
            burned_supply: 0,
            annual_inflation_rate: 0.08, // 8% initial
            current_epoch: 0,
        }
    }

    pub fn circulating_supply(&self) -> u64 {
        self.total_supply - self.staked_supply - self.burned_supply
    }

    pub fn calculate_epoch_reward(&self, validator_stake: u64, total_stake: u64) -> u64 {
        if total_stake == 0 {
            return 0;
        }
        let annual_reward = (self.total_supply as f64 * self.annual_inflation_rate) / 365.0;
        let epoch_fraction = 1.0 / 365.0;
        let validator_share = validator_stake as f64 / total_stake as f64;
        (annual_reward * epoch_fraction * validator_share) as u64
    }

    pub fn apply_disinflation(&mut self) {
        self.current_epoch += 1;
        if self.annual_inflation_rate > 0.015 {
            self.annual_inflation_rate *= 0.85; // 15% reduction per year
            if self.annual_inflation_rate < 0.015 {
                self.annual_inflation_rate = 0.015;
            }
        }
    }

    pub fn burn(&mut self, amount: u64) {
        self.burned_supply += amount;
    }
}

impl Default for Tokenomics {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone)]
pub struct Wallet {
    pub public_key: [u8; 32],
    pub balance: u64,
    pub staked_balance: u64,
    pub locked_balance: u64,
    pub nonce: u64,
}

impl Wallet {
    pub fn new(public_key: [u8; 32]) -> Self {
        Self {
            public_key,
            balance: 0,
            staked_balance: 0,
            locked_balance: 0,
            nonce: 0,
        }
    }

    pub fn available_balance(&self) -> u64 {
        self.balance.saturating_sub(self.locked_balance)
    }

    pub fn stake(&mut self, amount: u64) -> Result<(), TokenError> {
        if amount < MIN_STAKE_AMOUNT {
            return Err(TokenError::InsufficientStake);
        }
        if self.available_balance() < amount {
            return Err(TokenError::InsufficientBalance);
        }
        self.balance -= amount;
        self.staked_balance += amount;
        Ok(())
    }

    pub fn unstake(&mut self, amount: u64) -> Result<(), TokenError> {
        if self.staked_balance < amount {
            return Err(TokenError::InsufficientStakedBalance);
        }
        self.staked_balance -= amount;
        self.balance += amount;
        Ok(())
    }

    pub fn receive(&mut self, amount: u64) {
        self.balance += amount;
    }

    pub fn send(&mut self, amount: u64) -> Result<(), TokenError> {
        if self.available_balance() < amount {
            return Err(TokenError::InsufficientBalance);
        }
        self.balance -= amount;
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TokenError {
    InsufficientBalance,
    InsufficientStakedBalance,
    InsufficientStake,
    InvalidSignature,
    TransferFailed,
    StakeLocked,
}

impl std::fmt::Display for TokenError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TokenError::InsufficientBalance => write!(f, "Insufficient balance"),
            TokenError::InsufficientStakedBalance => write!(f, "Insufficient staked balance"),
            TokenError::InsufficientStake => write!(f, "Amount below minimum stake"),
            TokenError::InvalidSignature => write!(f, "Invalid transaction signature"),
            TokenError::TransferFailed => write!(f, "Transfer failed"),
            TokenError::StakeLocked => write!(f, "Stake is locked during unbonding"),
        }
    }
}

pub struct StakePosition {
    pub validator_id: String,
    pub amount: u64,
    pub start_epoch: u64,
    pub unbond_start_epoch: Option<u64>,
    pub rewards_accrued: u64,
}

impl StakePosition {
    pub fn new(validator_id: String, amount: u64, start_epoch: u64) -> Self {
        Self {
            validator_id,
            amount,
            start_epoch,
            unbond_start_epoch: None,
            rewards_accrued: 0,
        }
    }

    pub fn initiate_unbond(&mut self, current_epoch: u64) {
        if self.unbond_start_epoch.is_none() {
            self.unbond_start_epoch = Some(current_epoch);
        }
    }

    pub fn can_withdraw(&self, current_epoch: u64) -> bool {
        if let Some(unbond_start) = self.unbond_start_epoch {
            current_epoch - unbond_start >= 7 // 7 epoch unbonding period
        } else {
            false
        }
    }

    pub fn lock_period_remaining(&self, current_epoch: u64) -> Option<u64> {
        if let Some(unbond_start) = self.unbond_start_epoch {
            let remaining = 7 - (current_epoch - unbond_start);
            if remaining > 0 { Some(remaining) } else { None }
        } else {
            None
        }
    }
}

pub struct RewardPool {
    pub relay_rewards: u64,
    pub hosting_rewards: u64,
    pub creation_rewards: u64,
    pub engagement_rewards: u64,
    pub total_distributed: u64,
}

impl RewardPool {
    pub fn new(initial_supply: u64) -> Self {
        let annual_allocation = initial_supply / 100; // 1% annual for rewards
        let per_category = annual_allocation / 4;
        
        Self {
            relay_rewards: per_category,
            hosting_rewards: per_category,
            creation_rewards: per_category,
            engagement_rewards: per_category,
            total_distributed: 0,
        }
    }

    pub fn claim_relay_reward(&mut self, bytes_relayed: u64, total_bytes: u64) -> u64 {
        if total_bytes == 0 { return 0; }
        let share = bytes_relayed as f64 / total_bytes as f64;
        let reward = (self.relay_rewards as f64 * share) as u64;
        self.relay_rewards -= reward;
        self.total_distributed += reward;
        reward
    }

    pub fn claim_hosting_reward(&mut self, storage_gb: u64, total_gb: u64, uptime: f64) -> u64 {
        if total_gb == 0 { return 0; }
        let share = storage_gb as f64 / total_gb as f64;
        let reward = (self.hosting_rewards as f64 * share * uptime) as u64;
        self.hosting_rewards -= reward;
        self.total_distributed += reward;
        reward
    }

    pub fn claim_creation_reward(&mut self, engagement_score: f64, is_permanent: bool) -> u64 {
        let mut reward = 0u64;
        
        if engagement_score >= 100.0 {
            reward += 10;
        }
        if engagement_score >= 1000.0 {
            reward += 50;
        }
        if is_permanent {
            reward += 200;
        }
        
        self.creation_rewards = self.creation_rewards.saturating_sub(reward);
        self.total_distributed += reward;
        reward
    }
}

pub struct Transaction {
    pub tx_id: String,
    pub from: [u8; 32],
    pub to: [u8; 32],
    pub amount: u64,
    pub fee: u64,
    pub timestamp: i64,
    pub signature: Vec<u8>,
    pub status: TransactionStatus,
}

impl Transaction {
    pub fn new(from: [u8; 32], to: [u8; 32], amount: u64) -> Self {
        Self {
            tx_id: uuid::Uuid::new_v4().to_string(),
            from,
            to,
            amount,
            fee: 5000, // 0.005 SOVEREIGN
            timestamp: Utc::now().timestamp(),
            signature: Vec::new(),
            status: TransactionStatus::Pending,
        }
    }

    pub fn sign(&mut self, private_key: &[u8; 32]) {
        let message = format!("{}:{}:{}:{}", self.from, self.to, self.amount, self.timestamp);
        let signature = sign_message(&message, private_key);
        self.signature = signature;
    }

    pub fn verify_signature(&self) -> bool {
        !self.signature.is_empty()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransactionStatus {
    Pending,
    Confirmed,
    Finalized,
    Failed,
}

fn sign_message(message: &str, private_key: &[u8; 32]) -> Vec<u8> {
    use sha2::{Sha256, Digest};
    let mut hasher = Sha256::new();
    hasher.update(message.as_bytes());
    hasher.update(private_key);
    hasher.finalize().to_vec()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_tokenomics_circulating() {
        let mut tokenomics = Tokenomics::new();
        tokenomics.staked_supply = 100_000_000;
        tokenomics.burned_supply = 10_000_000;
        
        assert_eq!(tokenomics.circulating_supply(), 890_000_000);
    }

    #[test]
    fn test_wallet_operations() {
        let mut wallet = Wallet::new([0u8; 32]);
        wallet.receive(1000);
        
        assert_eq!(wallet.balance, 1000);
        
        wallet.stake(100).unwrap();
        assert_eq!(wallet.balance, 900);
        assert_eq!(wallet.staked_balance, 100);
        
        wallet.unstake(50).unwrap();
        assert_eq!(wallet.balance, 950);
        assert_eq!(wallet.staked_balance, 50);
    }

    #[test]
    fn test_stake_unbonding() {
        let mut stake = StakePosition::new("validator1".to_string(), 1000, 0);
        
        assert!(!stake.can_withdraw(1));
        
        stake.initiate_unbond(5);
        assert!(!stake.can_withdraw(6));
        assert!(stake.can_withdraw(12));
    }
}
