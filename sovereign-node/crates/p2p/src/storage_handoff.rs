use serde::{Deserialize, Serialize};
use libp2p::PeerId;
use chrono::{DateTime, Utc};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StorageOffer {
    pub offer_id: String,
    pub host_peer_id: PeerId,
    pub available_gb: u64,
    pub bandwidth_mbps: u32,
    pub price_per_gb_month: u64,
    pub uptime_score: f64,
    pub sla_tier: SLATier,
    pub valid_until: i64,
    pub signature: Vec<u8>,
}

impl StorageOffer {
    pub fn new(
        host_peer_id: PeerId,
        available_gb: u64,
        bandwidth_mbps: u32,
        price_per_gb_month: u64,
        uptime_score: f64,
    ) -> Self {
        Self {
            offer_id: uuid::Uuid::new_v4().to_string(),
            host_peer_id,
            available_gb,
            bandwidth_mbps,
            price_per_gb_month,
            uptime_score,
            sla_tier: SLATier::Standard,
            valid_until: Utc::now().timestamp() + 3600, // 1 hour
            signature: Vec::new(),
        }
    }

    pub fn is_valid(&self) -> bool {
        Utc::now().timestamp() < self.valid_until
    }

    pub fn meets_requirements(&self, required_gb: u64, required_mbps: u32) -> bool {
        self.available_gb >= required_gb && self.bandwidth_mbps >= required_mbps
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SLATier {
    Basic,
    Standard,
    Premium,
    Enterprise,
}

impl SLATier {
    pub fn min_uptime(&self) -> f64 {
        match self {
            SLATier::Basic => 0.90,
            SLATier::Standard => 0.95,
            SLATier::Premium => 0.99,
            SLATier::Enterprise => 0.999,
        }
    }

    pub fn penalty_rate(&self) -> f64 {
        match self {
            SLATier::Basic => 0.10,
            SLATier::Standard => 0.05,
            SLATier::Premium => 0.01,
            SLATier::Enterprise => 0.001,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StorageAgreement {
    pub agreement_id: String,
    pub host_peer_id: PeerId,
    pub client_peer_id: PeerId,
    pub storage_gb: u64,
    pub monthly_price: u64,
    pub stake_amount: u64,
    pub sla_tier: SLATier,
    pub started_at: i64,
    pub expires_at: i64,
    pub status: AgreementStatus,
    pub on_chain_ref: Option<String>,
}

impl StorageAgreement {
    pub fn new(
        host_peer_id: PeerId,
        client_peer_id: PeerId,
        storage_gb: u64,
        monthly_price: u64,
        stake_amount: u64,
        sla_tier: SLATier,
        duration_days: u32,
    ) -> Self {
        let now = Utc::now().timestamp();
        Self {
            agreement_id: uuid::Uuid::new_v4().to_string(),
            host_peer_id,
            client_peer_id,
            storage_gb,
            monthly_price,
            stake_amount,
            sla_tier,
            started_at: now,
            expires_at: now + (duration_days as i64 * 86400),
            status: AgreementStatus::Pending,
            on_chain_ref: None,
        }
    }

    pub fn activate(&mut self, on_chain_ref: String) {
        self.status = AgreementStatus::Active;
        self.on_chain_ref = Some(on_chain_ref);
    }

    pub fn is_active(&self) -> bool {
        self.status == AgreementStatus::Active && Utc::now().timestamp() < self.expires_at
    }

    pub fn remaining_days(&self) -> i64 {
        let remaining = self.expires_at - Utc::now().timestamp();
        remaining.max(0) / 86400
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum AgreementStatus {
    Pending,
    Active,
    Suspended,
    Terminated,
    Expired,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataHandoff {
    pub handoff_id: String,
    pub agreement_id: String,
    pub encrypted_key: Vec<u8>,
    pub content_cid: String,
    pub content_size_bytes: u64,
    pub merkle_root: String,
    pub replication_factor: u8,
    pub created_at: i64,
    pub verified_hosts: Vec<PeerId>,
}

impl DataHandoff {
    pub fn new(
        agreement_id: String,
        encrypted_key: Vec<u8>,
        content_cid: String,
        content_size_bytes: u64,
        merkle_root: String,
        replication_factor: u8,
    ) -> Self {
        Self {
            handoff_id: uuid::Uuid::new_v4().to_string(),
            agreement_id,
            encrypted_key,
            content_cid,
            content_size_bytes,
            merkle_root,
            replication_factor,
            created_at: Utc::now().timestamp(),
            verified_hosts: Vec::new(),
        }
    }

    pub fn add_verified_host(&mut self, peer_id: PeerId) {
        if !self.verified_hosts.contains(&peer_id) {
            self.verified_hosts.push(peer_id);
        }
    }

    pub fn is_fully_replicated(&self) -> bool {
        self.verified_hosts.len() >= self.replication_factor as usize
    }
}

pub struct HandshakeState {
    pub phase: HandshakePhase,
    pub offer: Option<StorageOffer>,
    pub agreement: Option<StorageAgreement>,
    pub handoff: Option<DataHandoff>,
    pub created_at: i64,
}

impl HandshakeState {
    pub fn new() -> Self {
        Self {
            phase: HandshakePhase::Discovery,
            offer: None,
            agreement: None,
            handoff: None,
            created_at: Utc::now().timestamp(),
        }
    }

    pub fn move_to_offer(&mut self, offer: StorageOffer) {
        self.phase = HandshakePhase::OfferReceived;
        self.offer = Some(offer);
    }

    pub fn move_to_agreement(&mut self, agreement: StorageAgreement) {
        self.phase = HandshakePhase::AgreementCreated;
        self.agreement = Some(agreement);
    }

    pub fn move_to_handoff(&mut self, handoff: DataHandoff) {
        self.phase = HandshakePhase::DataTransfer;
        self.handoff = Some(handoff);
    }

    pub fn complete(&mut self) {
        self.phase = HandshakePhase::Complete;
    }

    pub fn is_expired(&self, max_duration_secs: i64) -> bool {
        Utc::now().timestamp() - self.created_at > max_duration_secs
    }
}

impl Default for HandshakeState {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HandshakePhase {
    Discovery,
    OfferReceived,
    AgreementCreated,
    DataTransfer,
    Complete,
    Failed,
}

pub const MIN_REPLICATION_FACTOR: u8 = 3;
pub const MAX_HANDSHAKE_DURATION_SECS: i64 = 300;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sla_tier_uptime() {
        assert!(SLATier::Premium.min_uptime() > SLATier::Standard.min_uptime());
    }

    #[test]
    fn test_agreement_lifecycle() {
        let mut agreement = StorageAgreement::new(
            PeerId::random(),
            PeerId::random(),
            10,
            100,
            500,
            SLATier::Standard,
            30,
        );
        
        assert_eq!(agreement.status, AgreementStatus::Pending);
        agreement.activate("tx123".to_string());
        assert!(agreement.is_active());
    }

    #[test]
    fn test_data_handoff_replication() {
        let mut handoff = DataHandoff::new(
            "agreement1".to_string(),
            vec![1, 2, 3],
            "QmABC123".to_string(),
            1024,
            "root123".to_string(),
            3,
        );
        
        assert!(!handoff.is_fully_replicated());
        
        handoff.add_verified_host(PeerId::random());
        assert!(!handoff.is_fully_replicated());
        
        handoff.add_verified_host(PeerId::random());
        assert!(!handoff.is_fully_replicated());
        
        handoff.add_verified_host(PeerId::random());
        assert!(handoff.is_fully_replicated());
    }
}
