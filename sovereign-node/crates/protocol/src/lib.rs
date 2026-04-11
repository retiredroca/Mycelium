use serde::{Deserialize, Serialize};
use libp2p::PeerId;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProtocolMessage {
    pub version: String,
    pub msg_type: MessageType,
    pub sender: PeerId,
    pub timestamp: i64,
    pub payload: Vec<u8>,
    pub signature: Vec<u8>,
}

impl ProtocolMessage {
    pub fn new(msg_type: MessageType, payload: Vec<u8>) -> Self {
        Self {
            version: "1.0.0".to_string(),
            msg_type,
            sender: PeerId::random(),
            timestamp: chrono::Utc::now().timestamp(),
            payload,
            signature: Vec::new(),
        }
    }

    pub fn serialize_payload<T: serde::Serialize>(&mut self, data: &T) -> Result<(), serde_json::Error> {
        self.payload = serde_json::to_vec(data)?;
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MessageType {
    Handshake,
    PostCreate,
    PostUpdate,
    PostDelete,
    PostView,
    PostShare,
    PostComment,
    PeerAnnounce,
    PeerDiscover,
    StorageOffer,
    StorageRequest,
    StorageConfirm,
    Heartbeat,
    SyncRequest,
    SyncResponse,
    Error,
}

impl MessageType {
    pub fn as_str(&self) -> &'static str {
        match self {
            MessageType::Handshake => "handshake",
            MessageType::PostCreate => "post_create",
            MessageType::PostUpdate => "post_update",
            MessageType::PostDelete => "post_delete",
            MessageType::PostView => "post_view",
            MessageType::PostShare => "post_share",
            MessageType::PostComment => "post_comment",
            MessageType::PeerAnnounce => "peer_announce",
            MessageType::PeerDiscover => "peer_discover",
            MessageType::StorageOffer => "storage_offer",
            MessageType::StorageRequest => "storage_request",
            MessageType::StorageConfirm => "storage_confirm",
            MessageType::Heartbeat => "heartbeat",
            MessageType::SyncRequest => "sync_request",
            MessageType::SyncResponse => "sync_response",
            MessageType::Error => "error",
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HandshakePayload {
    pub protocol_version: String,
    pub capabilities: Vec<String>,
    pub listen_addresses: Vec<String>,
    pub user_agent: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ErrorPayload {
    pub code: u32,
    pub message: String,
}

impl ErrorPayload {
    pub fn new(code: u32, message: String) -> Self {
        Self { code, message }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_message_creation() {
        let msg = ProtocolMessage::new(MessageType::Handshake, vec![1, 2, 3]);
        assert_eq!(msg.version, "1.0.0");
        assert!(matches!(msg.msg_type, MessageType::Handshake));
    }

    #[test]
    fn test_payload_serialization() {
        let mut msg = ProtocolMessage::new(MessageType::PeerAnnounce, Vec::new());
        let payload = HandshakePayload {
            protocol_version: "1.0.0".to_string(),
            capabilities: vec!["storage".to_string(), "relay".to_string()],
            listen_addresses: vec!["/ip4/0.0.0.0/tcp/8080".to_string()],
            user_agent: "sovereign-node/1.0.0".to_string(),
        };
        
        msg.serialize_payload(&payload).unwrap();
        assert!(!msg.payload.is_empty());
    }
}
