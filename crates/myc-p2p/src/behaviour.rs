use libp2p::{
    core::endpoint::Libp2pConfiguration,
    gossipsub::{self, MessageAuthenticity, GossipsubConfig, GossipsubMessageAuthenticity},
    kad::{KadProtocolConfig, QueryId, RecordKey, PeerRecord, Quorum, store::RecordStore},
    mdns, ping,
    identify::{Config as IdentifyConfig, Behaviour as Identify},
    relay::v2:: Behaviour as Relay,
    swarm::{NetworkBehaviour, ConnectionEstablished, ConnectionClosed, DialError},
    PeerId, Multiaddr,
};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::{warn, info, error};

use super::{DiscoveryConfig, NodeCapability, PROTOCOL_NAME, GOSSIPSUB_TOPIC_POSTS, GOSSIPSUB_TOPIC_ANNOUNCEMENTS};
use super::peer_table::{PeerTable, PeerInfo};
use super::storage_handoff::StorageOffer;
use myc_post::Post;

#[derive(NetworkBehaviour)]
#[behaviour(out_event = "Event", poll_method = "async_trait")]
pub struct MyceliumBehaviour {
    pub gossipsub: gossipsub::Behaviour,
    pub kad: kad::Behaviour,
    pub mdns: mdns::tokio::Behaviour,
    pub ping: ping::Behaviour,
    pub identify: Identify,
    pub relay: Relay,
}

pub enum Event {
    Gossipsub(gossipsub::Event),
    Kad(kad::Event),
    Mdns(mdns::Event),
    Ping(ping::Event),
    Identify(identify::Event),
    Relay(relay::v2::Event),
}

pub struct Config {
    pub keypair: libp2p::identity::Keypair,
    pub listen_addresses: Vec<Multiaddr>,
    pub capabilities: Vec<NodeCapability>,
    pub discovery: DiscoveryConfig,
    pub bootstrap_nodes: Vec<(PeerId, Multiaddr)>,
    pub enable_mdns: bool,
    pub enable_relay: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            keypair: libp2p::identity::Keypair::generate_ed25519(),
            listen_addresses: vec!["/ip4/0.0.0.0/tcp/0".parse().unwrap()],
            capabilities: vec![NodeCapability::Full],
            discovery: DiscoveryConfig::default(),
            bootstrap_nodes: Vec::new(),
            enable_mdns: true,
            enable_relay: true,
        }
    }
}

impl MyceliumBehaviour {
    pub fn new(
        local_key: &libp2p::identity::Keypair,
        config: Config,
        peer_table: Arc<RwLock<PeerTable>>,
    ) -> Result<Self, libp2p::TransportError<std::io::Error>> {
        let peer_id = PeerId::from(local_key.public());

        let gossipsub = Self::build_gossipsub(local_key)?;
        let kad = Self::build_kad(peer_id, &config.discovery);
        let mdns = Self::build_mdns()?;
        let ping = Self::build_ping();
        let identify = Self::build_identify(local_key);
        let relay = Self::build_relay();

        Ok(Self {
            gossipsub,
            kad,
            mdns,
            ping,
            identify,
            relay,
        })
    }

    fn build_gossipsub(
        local_key: &libp2p::identity::Keypair,
    ) -> Result<gossipsub::Behaviour, libp2p::TransportError<std::io::Error>> {
        let gossipsub_config = GossipsubConfig::default()
            .heartbeat_interval(std::time::Duration::from_secs(5))
            .validation_mode(gossipsub::ValidationConfig::Strict)
            .max_transmit_size(1024 * 1024); // 1MB max message

        let gossipsub = gossipsub::Behaviour::new(
            MessageAuthenticity::Signed(local_key.clone()),
            gossipsub_config,
        )?;

        Ok(gossipsub)
    }

    fn build_kad(
        peer_id: PeerId,
        discovery: &DiscoveryConfig,
    ) -> kad::Behaviour {
        let mut kad_config = kad::Config::default();
        kad_config.set_query_timeout(std::time::Duration::from_secs(discovery.query_timeout_secs));
        
        kad::Behaviour::new(peer_id, kad_config)
    }

    fn build_mdns() -> Result<mdns::tokio::Behaviour, std::io::Error> {
        Ok(mdns::tokio::Behaviour::new(
            mdns::Config::default(),
        )?)
    }

    fn build_ping() -> ping::Behaviour {
        ping::Behaviour::new(ping::Config::default())
    }

    fn build_identify(local_key: &libp2p::identity::Keypair) -> Identify {
        Identify::new(IdentifyConfig::new(
            PROTOCOL_NAME.to_string(),
            local_key.public(),
        ))
    }

    fn build_relay() -> Relay {
        Relay::new(libp2p::relay::v2::Config::default())
    }

    pub fn subscribe_to_posts(&mut self) -> Result<(), libp2p::gossipsub::error::SubscribeError> {
        self.gossipsub.subscribe(&libp2p::gossipsub::IdentTopic::new(GOSSIPSUB_TOPIC_POSTS))
    }

    pub fn subscribe_to_announcements(&mut self) -> Result<(), libp2p::gossipsub::error::SubscribeError> {
        self.gossipsub.subscribe(&libp2p::gossipsub::IdentTopic::new(GOSSIPSUB_TOPIC_ANNOUNCEMENTS))
    }

    pub fn publish_post(&mut self, post: &Post) -> Result<(), libp2p::gossipsub::error::PublishError> {
        let data = serde_json::to_vec(post).expect("Post should serialize");
        self.gossipsub.publish(
            libp2p::gossipsub::IdentTopic::new(GOSSIPSUB_TOPIC_POSTS),
            data,
        )
    }

    pub fn broadcast_announcement(&mut self, announcement: &[u8]) -> Result<(), libp2p::gossipsub::error::PublishError> {
        self.gossipsub.publish(
            libp2p::gossipsub::IdentTopic::new(GOSSIPSUB_TOPIC_ANNOUNCEMENTS),
            announcement,
        )
    }

    pub fn get_closest_peers(&mut self, key: RecordKey) -> QueryId {
        self.kad.get_closest_peers(key)
    }

    pub fn put_record(&mut self, record: PeerRecord) -> Result<QueryId, kad::RecordPutError> {
        self.kad.put_record(record, Quorum::Majority)
    }

    pub fn get_record(&mut self, key: RecordKey) -> QueryId {
        self.kad.get_record(key)
    }
}

pub mod kad {
    pub use libp2p::kad::*;
}
