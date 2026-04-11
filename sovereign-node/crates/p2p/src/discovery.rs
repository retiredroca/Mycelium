use std::time::Duration;

#[derive(Debug, Clone)]
pub struct DiscoveryConfig {
    pub bootstrap_interval_secs: u64,
    pub query_timeout_secs: u64,
    pub refresh_interval_secs: u64,
    pub parallel_queries: usize,
    pub replication_factor: usize,
    pub advertise_enabled: bool,
}

impl Default for DiscoveryConfig {
    fn default() -> Self {
        Self {
            bootstrap_interval_secs: 30,
            query_timeout_secs: 10,
            refresh_interval_secs: 3600,
            parallel_queries: 3,
            replication_factor: 20,
            advertise_enabled: true,
        }
    }
}

impl DiscoveryConfig {
    pub fn with_bootstrap_interval(mut self, interval_secs: u64) -> Self {
        self.bootstrap_interval_secs = interval_secs;
        self
    }

    pub fn with_query_timeout(mut self, timeout_secs: u64) -> Self {
        self.query_timeout_secs = timeout_secs;
        self
    }

    pub fn with_refresh_interval(mut self, interval_secs: u64) -> Self {
        self.refresh_interval_secs = interval_secs;
        self
    }

    pub fn with_parallel_queries(mut self, count: usize) -> Self {
        self.parallel_queries = count;
        self
    }

    pub fn with_replication_factor(mut self, factor: usize) -> Self {
        self.replication_factor = factor;
        self
    }

    pub fn bootstrap_interval(&self) -> Duration {
        Duration::from_secs(self.bootstrap_interval_secs)
    }

    pub fn query_timeout(&self) -> Duration {
        Duration::from_secs(self.query_timeout_secs)
    }

    pub fn refresh_interval(&self) -> Duration {
        Duration::from_secs(self.refresh_interval_secs)
    }
}

pub struct BootstrapNode {
    pub peer_id: libp2p::PeerId,
    pub address: libp2p::Multiaddr,
}

impl BootstrapNode {
    pub fn new(peer_id: libp2p::PeerId, address: libp2p::Multiaddr) -> Self {
        Self { peer_id, address }
    }
}

pub fn default_bootstrap_nodes() -> Vec<BootstrapNode> {
    vec![
        BootstrapNode::new(
            "QmSoLUzQGJUN3nC5BVVPXdv2XivShH4dCL9fK5fC15DHzVN".parse().unwrap(),
            "/dnsaddr/bootstrap.libp2p.io/tcp/443/wss/p2p/QmQCU2EcMqAqQPR2i9bChDtGNJchTbq5TbXJJ16u19uLTc".parse().unwrap(),
        ),
    ]
}

pub mod capability {
    use super::*;
    use libp2p::PeerId;
    use std::collections::HashMap;

    pub struct CapabilityRegistry {
        advertised: HashMap<PeerId, Vec<super::super::NodeCapability>>,
    }

    impl CapabilityRegistry {
        pub fn new() -> Self {
            Self {
                advertised: HashMap::new(),
            }
        }

        pub fn register(&mut self, peer_id: PeerId, capabilities: Vec<super::super::NodeCapability>) {
            self.advertised.insert(peer_id, capabilities);
        }

        pub fn get(&self, peer_id: &PeerId) -> Option<&Vec<super::super::NodeCapability>> {
            self.advertised.get(peer_id)
        }

        pub fn find_storage_nodes(&self, min_capacity_gb: u64) -> Vec<PeerId> {
            self.advertised.iter()
                .filter(|(_, caps)| {
                    caps.iter().any(|c| {
                        match c {
                            super::super::NodeCapability::Storage { max_gb } => *max_gb >= min_capacity_gb,
                            _ => false,
                        }
                    })
                })
                .map(|(k, _)| *k)
                .collect()
        }

        pub fn find_relay_nodes(&self, min_bandwidth_mbps: u32) -> Vec<PeerId> {
            self.advertised.iter()
                .filter(|(_, caps)| {
                    caps.iter().any(|c| {
                        match c {
                            super::super::NodeCapability::Relay { bandwidth_mbps } => *bandwidth_mbps >= min_bandwidth_mbps,
                            _ => false,
                        }
                    })
                })
                .map(|(k, _)| *k)
                .collect()
        }
    }

    impl Default for CapabilityRegistry {
        fn default() -> Self {
            Self::new()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_config() {
        let config = DiscoveryConfig::default();
        assert_eq!(config.bootstrap_interval_secs, 30);
        assert_eq!(config.query_timeout_secs, 10);
    }

    #[test]
    fn test_config_builder() {
        let config = DiscoveryConfig::default()
            .with_bootstrap_interval(60)
            .with_query_timeout(20);
        
        assert_eq!(config.bootstrap_interval_secs, 60);
        assert_eq!(config.query_timeout_secs, 20);
    }
}
