use libp2p::PeerId;
use std::collections::{HashMap, BTreeMap};
use chrono::{DateTime, Utc};

#[derive(Debug, Clone)]
pub struct PeerInfo {
    pub peer_id: PeerId,
    pub addresses: Vec<libp2p::Multiaddr>,
    pub connected_at: i64,
    pub last_seen: i64,
    pub trust_score: TrustScore,
    pub capabilities: Vec<super::NodeCapability>,
    pub bytes_transferred: u64,
    pub messages_received: u64,
    pub messages_sent: u64,
    pub is_persistent: bool,
}

impl PeerInfo {
    pub fn new(peer_id: PeerId) -> Self {
        let now = Utc::now().timestamp();
        Self {
            peer_id,
            addresses: Vec::new(),
            connected_at: now,
            last_seen: now,
            trust_score: TrustScore::default(),
            capabilities: Vec::new(),
            bytes_transferred: 0,
            messages_received: 0,
            messages_sent: 0,
            is_persistent: false,
        }
    }

    pub fn add_address(&mut self, addr: libp2p::Multiaddr) {
        if !self.addresses.contains(&addr) {
            self.addresses.push(addr);
        }
        self.update_activity();
    }

    pub fn update_activity(&mut self) {
        self.last_seen = Utc::now().timestamp();
    }

    pub fn record_message_sent(&mut self, bytes: u64) {
        self.messages_sent += 1;
        self.bytes_transferred += bytes;
        self.update_activity();
    }

    pub fn record_message_received(&mut self, bytes: u64) {
        self.messages_received += 1;
        self.bytes_transferred += bytes;
        self.update_activity();
    }

    pub fn uptime_seconds(&self) -> i64 {
        Utc::now().timestamp() - self.connected_at
    }

    pub fn is_online(&self) -> bool {
        let now = Utc::now().timestamp();
        now - self.last_seen < 300 // 5 minutes threshold
    }
}

#[derive(Debug, Clone, Default)]
pub struct TrustScore {
    pub base_score: f64,
    pub uptime_weight: f64,
    pub behavior_weight: f64,
    pub stake_weight: f64,
}

impl TrustScore {
    pub fn new(stake_amount: u64) -> Self {
        let stake_weight = (stake_amount as f64).min(10000.0) / 10000.0;
        Self {
            base_score: 50.0, // Start at 50
            uptime_weight: 0.0,
            behavior_weight: 0.0,
            stake_weight,
        }
    }

    pub fn calculate(&self) -> f64 {
        let mut score = self.base_score;
        score += self.uptime_weight * 30.0;
        score += self.behavior_weight * 20.0;
        score += self.stake_weight * 20.0;
        score.clamp(0.0, 100.0)
    }

    pub fn update_uptime(&mut self, uptime_hours: f64) {
        self.uptime_weight = (uptime_hours / 24.0).min(1.0);
    }

    pub fn record_positive_behavior(&mut self) {
        self.behavior_weight = (self.behavior_weight + 0.1).min(1.0);
    }

    pub fn record_negative_behavior(&mut self) {
        self.behavior_weight = (self.behavior_weight - 0.2).max(-1.0);
    }
}

pub struct PeerTable {
    local_peer_id: PeerId,
    peers: HashMap<PeerId, PeerInfo>,
    buckets: BTreeMap<u8, Vec<PeerId>>,
    max_peers_per_bucket: usize,
}

impl PeerTable {
    pub fn new(local_peer_id: PeerId) -> Self {
        Self {
            local_peer_id,
            peers: HashMap::new(),
            buckets: BTreeMap::new(),
            max_peers_per_bucket: 20,
        }
    }

    pub fn add_peer(&mut self, peer_id: PeerId, mut info: PeerInfo) {
        info.add_address = |addr| info.addresses.push(addr);
        
        if !self.peers.contains_key(&peer_id) {
            let bucket_index = self.xor_distance_bucket(&peer_id);
            let bucket = self.buckets.entry(bucket_index).or_default();
            
            if bucket.len() >= self.max_peers_per_bucket {
                if let Some(oldest) = bucket.iter()
                    .filter(|p| *p != &peer_id)
                    .min_by_key(|p| self.peers.get(*p).map(|i| i.last_seen).unwrap_or(i64::MAX))
                {
                    self.peers.remove(oldest);
                    bucket.retain(|p| p != oldest);
                }
            }
            
            bucket.push(peer_id);
        }
        
        info.update_activity();
        self.peers.insert(peer_id, info);
    }

    pub fn remove_peer(&mut self, peer_id: &PeerId) {
        if let Some(info) = self.peers.remove(peer_id) {
            let bucket_index = self.xor_distance_bucket(peer_id);
            if let Some(bucket) = self.buckets.get_mut(&bucket_index) {
                bucket.retain(|p| p != peer_id);
            }
        }
    }

    pub fn get_peer(&self, peer_id: &PeerId) -> Option<&PeerInfo> {
        self.peers.get(peer_id)
    }

    pub fn get_all_peers(&self) -> Vec<(PeerId, PeerInfo)> {
        self.peers.iter()
            .map(|(k, v)| (*k, v.clone()))
            .collect()
    }

    pub fn peer_count(&self) -> usize {
        self.peers.len()
    }

    pub fn get_peers_by_capability(&self, capability: &super::NodeCapability) -> Vec<PeerId> {
        self.peers.iter()
            .filter(|(_, info)| info.capabilities.contains(capability))
            .map(|(k, _)| *k)
            .collect()
    }

    pub fn get_trusted_peers(&self, min_score: f64) -> Vec<PeerId> {
        self.peers.iter()
            .filter(|(_, info)| info.trust_score.calculate() >= min_score)
            .map(|(k, _)| *k)
            .collect()
    }

    pub fn get_online_peers(&self) -> Vec<PeerId> {
        self.peers.iter()
            .filter(|(_, info)| info.is_online())
            .map(|(k, _)| *k)
            .collect()
    }

    pub fn refresh_stale_entries(&mut self, threshold_secs: i64) -> Vec<PeerId> {
        let now = Utc::now().timestamp();
        let mut stale = Vec::new();
        
        for (peer_id, info) in &self.peers {
            if now - info.last_seen > threshold_secs {
                stale.push(*peer_id);
            }
        }
        
        for peer_id in &stale {
            self.remove_peer(peer_id);
        }
        
        stale
    }

    fn xor_distance_bucket(&self, peer_id: &PeerId) -> u8 {
        let local_id_bytes = self.local_peer_id.to_bytes();
        let peer_id_bytes = peer_id.to_bytes();
        
        let distance = local_id_bytes.iter()
            .zip(peer_id_bytes.iter())
            .map(|(a, b)| a ^ b)
            .collect::<Vec<_>>();
        
        for (i, byte) in distance.iter().enumerate() {
            if *byte != 0 {
                return (8 - i) as u8;
            }
        }
        0
    }

    pub fn get_bucket_stats(&self) -> Vec<(u8, usize)> {
        self.buckets.iter()
            .map(|(k, v)| (*k, v.len()))
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_trust_score_calculation() {
        let trust = TrustScore::new(5000);
        assert!(trust.calculate() > 50.0);
    }

    #[test]
    fn test_peer_table_add_remove() {
        let local = PeerId::random();
        let mut table = PeerTable::new(local);
        
        let peer = PeerId::random();
        table.add_peer(peer, PeerInfo::new(peer));
        
        assert_eq!(table.peer_count(), 1);
        
        table.remove_peer(&peer);
        assert_eq!(table.peer_count(), 0);
    }
}
