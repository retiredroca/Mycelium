use super::entry::{GuestbookEntry, EntryStatus};
use std::collections::{HashMap, VecDeque};
use chrono::Utc;

pub struct PendingQueue {
    entries: VecDeque<PendingEntry>,
    by_author: HashMap<String, Vec<String>>,
    by_id: HashMap<String, usize>,
}

#[derive(Debug, Clone)]
struct PendingEntry {
    entry: GuestbookEntry,
    received_at: i64,
    notified: bool,
}

impl PendingQueue {
    pub fn new() -> Self {
        Self {
            entries: VecDeque::new(),
            by_author: HashMap::new(),
            by_id: HashMap::new(),
        }
    }

    pub fn push(&mut self, entry: GuestbookEntry) {
        let author_key_b64 = Self::key_to_string(&entry.author_key);
        let id = entry.id.clone();
        
        let pending = PendingEntry {
            entry,
            received_at: Utc::now().timestamp(),
            notified: false,
        };
        
        let index = self.entries.len();
        self.entries.push_back(pending);
        self.by_id.insert(id.clone(), index);
        self.by_author.entry(author_key_b64).or_default().push(id);
    }

    pub fn remove(&mut self, entry_id: &str) -> Option<GuestbookEntry> {
        if let Some(index) = self.by_id.remove(entry_id) {
            if let Some(pending) = self.entries.remove(index) {
                let author_key_b64 = Self::key_to_string(&pending.entry.author_key);
                
                if let Some(ids) = self.by_author.get_mut(&author_key_b64) {
                    ids.retain(|id| id != entry_id);
                    if ids.is_empty() {
                        self.by_author.remove(&author_key_b64);
                    }
                }
                
                for (id, idx) in self.by_id.iter_mut() {
                    if *idx > index {
                        *idx -= 1;
                    }
                }
                
                return Some(pending.entry);
            }
        }
        None
    }

    pub fn find(&self, entry_id: &str) -> Option<&GuestbookEntry> {
        if let Some(&index) = self.by_id.get(entry_id) {
            self.entries.get(index).map(|p| &p.entry)
        } else {
            None
        }
    }

    pub fn find_mut(&mut self, entry_id: &str) -> Option<&mut GuestbookEntry> {
        if let Some(&index) = self.by_id.get(entry_id) {
            self.entries.get_mut(index).map(|p| &mut p.entry)
        } else {
            None
        }
    }

    pub fn get_by_author(&self, author_key: &[u8]) -> Vec<&GuestbookEntry> {
        let key_str = Self::key_to_string(author_key);
        
        if let Some(ids) = self.by_author.get(&key_str) {
            ids.iter()
                .filter_map(|id| self.find(id))
                .collect()
        } else {
            Vec::new()
        }
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    pub fn first(&self) -> Option<&GuestbookEntry> {
        self.entries.front().map(|p| &p.entry)
    }

    pub fn last(&self) -> Option<&GuestbookEntry> {
        self.entries.back().map(|p| &p.entry)
    }

    pub fn iter(&self) -> impl Iterator<Item = &GuestbookEntry> {
        self.entries.iter().map(|p| &p.entry)
    }

    pub fn mark_notified(&mut self, entry_id: &str) {
        if let Some(&index) = self.by_id.get(entry_id) {
            if let Some(pending) = self.entries.get_mut(index) {
                pending.notified = true;
            }
        }
    }

    pub fn get_unnotified(&self) -> Vec<&GuestbookEntry> {
        self.entries.iter()
            .filter(|p| !p.notified)
            .map(|p| &p.entry)
            .collect()
    }

    pub fn get_oldest_pending(&self) -> Option<&GuestbookEntry> {
        self.entries.front().map(|p| &p.entry)
    }

    pub fn clear(&mut self) {
        self.entries.clear();
        self.by_author.clear();
        self.by_id.clear();
    }

    fn key_to_string(key: &[u8]) -> String {
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, key)
    }
}

impl Default for PendingQueue {
    fn default() -> Self {
        Self::new()
    }
}

pub struct PendingIterator<'a> {
    queue: &'a PendingQueue,
    position: usize,
}

impl<'a> Iterator for PendingIterator<'a> {
    type Item = &'a GuestbookEntry;

    fn next(&mut self) -> Option<Self::Item> {
        if self.position < self.queue.entries.len() {
            let entry = &self.queue.entries[self.position].entry;
            self.position += 1;
            Some(entry)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pending_queue() {
        let mut queue = PendingQueue::new();
        
        let entry1 = GuestbookEntry::new("Alice".to_string(), "Hello".to_string());
        let id1 = entry1.id.clone();
        
        let entry2 = GuestbookEntry::new("Bob".to_string(), "Hi".to_string());
        let id2 = entry2.id.clone();
        
        queue.push(entry1);
        queue.push(entry2);
        
        assert_eq!(queue.len(), 2);
        
        let removed = queue.remove(&id1);
        assert!(removed.is_some());
        assert_eq!(queue.len(), 1);
        
        let still_there = queue.find(&id2);
        assert!(still_there.is_some());
    }

    #[test]
    fn test_find_mut() {
        let mut queue = PendingQueue::new();
        
        let entry = GuestbookEntry::new("Test".to_string(), "Content".to_string());
        let id = entry.id.clone();
        
        queue.push(entry);
        
        if let Some(found) = queue.find_mut(&id) {
            found.approve();
        }
        
        assert!(queue.find(&id).unwrap().is_approved());
    }
}
