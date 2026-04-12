pub mod entry;
pub mod pending;
pub mod approval;

pub use entry::{GuestbookEntry, EntryStatus, EntryBuilder};
pub use pending::PendingQueue;
pub use approval::{ApprovalManager, ApprovalAction};

use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;

pub struct Guestbook {
    pub owner_peer_id: Vec<u8>,
    pub entries: Vec<GuestbookEntry>,
    pub pending: PendingQueue,
    approval_manager: ApprovalManager,
}

impl Guestbook {
    pub fn new(owner_peer_id: Vec<u8>) -> Self {
        Self {
            owner_peer_id,
            entries: Vec::new(),
            pending: PendingQueue::new(),
            approval_manager: ApprovalManager::new(),
        }
    }

    pub fn add_entry(&mut self, entry: GuestbookEntry) -> bool {
        match entry.status {
            EntryStatus::Pending => {
                self.pending.push(entry);
                true
            }
            EntryStatus::Approved => {
                self.entries.push(entry);
                true
            }
            EntryStatus::Rejected => {
                false
            }
        }
    }

    pub fn approve_entry(&mut self, entry_id: &str, owner_key: &[u8]) -> Result<(), GuestbookError> {
        let entry = self.pending.find_mut(entry_id)
            .ok_or(GuestbookError::EntryNotFound)?;
        
        if !entry.verify_signature() {
            return Err(GuestbookError::InvalidSignature);
        }
        
        entry.approve();
        self.entries.push(entry.clone());
        self.pending.remove(entry_id);
        
        Ok(())
    }

    pub fn reject_entry(&mut self, entry_id: &str) -> Result<(), GuestbookError> {
        let entry = self.pending.find_mut(entry_id)
            .ok_or(GuestbookError::EntryNotFound)?;
        
        entry.reject();
        self.pending.remove(entry_id);
        
        Ok(())
    }

    pub fn get_visible_entries(&self, viewer_key: Option<&[u8]>) -> Vec<&GuestbookEntry> {
        let entries: Vec<_> = self.entries.iter()
            .filter(|e| self.is_entry_visible(e, viewer_key))
            .collect();
        entries
    }

    fn is_entry_visible(&self, entry: &GuestbookEntry, viewer_key: Option<&[u8]>) -> bool {
        match viewer_key {
            Some(key) => {
                if entry.author_key == key {
                    return true;
                }
            }
            None => return true,
        }
        true
    }

    pub fn get_pending_count(&self) -> usize {
        self.pending.len()
    }

    pub fn get_entries_count(&self) -> usize {
        self.entries.len()
    }

    pub fn sign_entry(&self, entry: &mut GuestbookEntry, private_key: &[u8; 32]) {
        entry.sign(private_key);
    }
}

#[derive(Debug, thiserror::Error)]
pub enum GuestbookError {
    #[error("Entry not found")]
    EntryNotFound,
    
    #[error("Invalid signature")]
    InvalidSignature,
    
    #[error("Content too long")]
    ContentTooLong,
    
    #[error("Guestbook is closed")]
    GuestbookClosed,
    
    #[error("Author is blocked")]
    AuthorBlocked,
}

pub struct GuestbookManager {
    guestbooks: Arc<RwLock<HashMap<String, Guestbook>>>,
}

impl GuestbookManager {
    pub fn new() -> Self {
        Self {
            guestbooks: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    pub async fn get_guestbook(&self, owner_id: &str) -> Option<Guestbook> {
        let guestbooks = self.guestbooks.read().await;
        guestbooks.get(owner_id).cloned()
    }

    pub async fn create_guestbook(&self, owner_peer_id: Vec<u8>) -> Guestbook {
        let owner_id = base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            &owner_peer_id,
        );
        
        let guestbook = Guestbook::new(owner_peer_id);
        
        let mut guestbooks = self.guestbooks.write().await;
        guestbooks.insert(owner_id, guestbook.clone());
        
        guestbook
    }

    pub async fn get_or_create(&self, owner_peer_id: Vec<u8>) -> Guestbook {
        let owner_id = base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            &owner_peer_id,
        );
        
        let mut guestbooks = self.guestbooks.write().await;
        
        if let Some(existing) = guestbooks.get(&owner_id) {
            return existing.clone();
        }
        
        let guestbook = Guestbook::new(owner_peer_id);
        guestbooks.insert(owner_id, guestbook.clone());
        guestbook
    }

    pub async fn submit_entry(&self, owner_id: &str, entry: GuestbookEntry) -> Result<(), GuestbookError> {
        let mut guestbooks = self.guestbooks.write().await;
        
        let guestbook = guestbooks.get_mut(owner_id)
            .ok_or(GuestbookError::EntryNotFound)?;
        
        if !guestbook.add_entry(entry) {
            return Err(GuestbookError::GuestbookClosed);
        }
        
        Ok(())
    }
}

impl Default for GuestbookManager {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_guestbook_creation() {
        let peer_id = vec![1, 2, 3, 4];
        let guestbook = Guestbook::new(peer_id);
        
        assert_eq!(guestbook.get_entries_count(), 0);
        assert_eq!(guestbook.get_pending_count(), 0);
    }

    #[test]
    fn test_entry_approval() {
        let mut guestbook = Guestbook::new(vec![1, 2, 3]);
        
        let entry = GuestbookEntry::new(
            "visitor".to_string(),
            "Hello!".to_string(),
        );
        
        guestbook.add_entry(entry);
        assert_eq!(guestbook.get_pending_count(), 1);
    }
}
