use super::entry::{GuestbookEntry, EntryStatus};
use std::collections::HashMap;
use chrono::{DateTime, Utc};

#[derive(Debug, Clone)]
pub struct ApprovalRecord {
    pub entry_id: String,
    pub action: ApprovalAction,
    pub decided_by: Vec<u8>,
    pub decided_at: i64,
    pub reason: Option<String>,
}

impl ApprovalRecord {
    pub fn new(
        entry_id: String,
        action: ApprovalAction,
        decided_by: Vec<u8>,
    ) -> Self {
        Self {
            entry_id,
            action,
            decided_by,
            decided_at: Utc::now().timestamp(),
            reason: None,
        }
    }

    pub fn with_reason(mut self, reason: &str) -> Self {
        self.reason = Some(reason.to_string());
        self
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ApprovalAction {
    Approved,
    Rejected,
    Blocked,
    Reported,
}

impl ApprovalAction {
    pub fn as_str(&self) -> &'static str {
        match self {
            ApprovalAction::Approved => "approved",
            ApprovalAction::Rejected => "rejected",
            ApprovalAction::Blocked => "blocked",
            ApprovalAction::Reported => "reported",
        }
    }
}

pub struct ApprovalManager {
    history: Vec<ApprovalRecord>,
    author_actions: HashMap<String, Vec<ApprovalAction>>,
    blocked_authors: HashMap<String, i64>,
}

impl ApprovalManager {
    pub fn new() -> Self {
        Self {
            history: Vec::new(),
            author_actions: HashMap::new(),
            blocked_authors: HashMap::new(),
        }
    }

    pub fn record_approval(&mut self, entry: &GuestbookEntry, decided_by: Vec<u8>) {
        let record = ApprovalRecord::new(
            entry.id.clone(),
            ApprovalAction::Approved,
            decided_by,
        );
        
        self.history.push(record);
        
        let author_key = Self::key_to_string(&entry.author_key);
        self.author_actions
            .entry(author_key)
            .or_default()
            .push(ApprovalAction::Approved);
    }

    pub fn record_rejection(
        &mut self,
        entry: &GuestbookEntry,
        decided_by: Vec<u8>,
        reason: Option<&str>,
    ) {
        let mut record = ApprovalRecord::new(
            entry.id.clone(),
            ApprovalAction::Rejected,
            decided_by,
        );
        
        if let Some(r) = reason {
            record = record.with_reason(r);
        }
        
        self.history.push(record);
        
        let author_key = Self::key_to_string(&entry.author_key);
        self.author_actions
            .entry(author_key)
            .or_default()
            .push(ApprovalAction::Rejected);
    }

    pub fn block_author(&mut self, author_key: &[u8], blocked_by: Vec<u8>) {
        let author_key_str = Self::key_to_string(author_key);
        self.blocked_authors.insert(author_key_str, Utc::now().timestamp());
        
        let record = ApprovalRecord::new(
            "BLOCK".to_string(),
            ApprovalAction::Blocked,
            blocked_by,
        );
        self.history.push(record);
    }

    pub fn unblock_author(&mut self, author_key: &[u8]) {
        let author_key_str = Self::key_to_string(author_key);
        self.blocked_authors.remove(&author_key_str);
    }

    pub fn is_author_blocked(&self, author_key: &[u8]) -> bool {
        let author_key_str = Self::key_to_string(author_key);
        self.blocked_authors.contains_key(&author_key_str)
    }

    pub fn get_blocked_authors(&self) -> Vec<String> {
        self.blocked_authors.keys().cloned().collect()
    }

    pub fn get_history(&self) -> &[ApprovalRecord] {
        &self.history
    }

    pub fn get_author_actions(&self, author_key: &[u8]) -> Vec<ApprovalAction> {
        let author_key_str = Self::key_to_string(author_key);
        self.author_actions
            .get(&author_key_str)
            .cloned()
            .unwrap_or_default()
    }

    pub fn is_first_time_visitor(&self, author_key: &[u8]) -> bool {
        let author_key_str = Self::key_to_string(author_key);
        !self.author_actions.contains_key(&author_key_str)
    }

    pub fn approval_rate(&self, author_key: &[u8]) -> f64 {
        let actions = self.get_author_actions(author_key);
        if actions.is_empty() {
            return 1.0;
        }
        
        let approved = actions.iter().filter(|a| **a == ApprovalAction::Approved).count();
        approved as f64 / actions.len() as f64
    }

    pub fn should_auto_approve(&self, author_key: &[u8], policy: &AutoApprovePolicy) -> bool {
        if self.is_author_blocked(author_key) {
            return false;
        }
        
        match policy {
            AutoApprovePolicy::Never => false,
            AutoApprovePolicy::Always => true,
            AutoApprovePolicy::TrustedOnly => {
                self.approval_rate(author_key) >= 0.8
            }
            AutoApprovePolicy::TrustedWithThreshold(threshold) => {
                let entries = self.get_author_actions(author_key).len();
                entries >= *threshold && self.approval_rate(author_key) >= 0.8
            }
        }
    }

    fn key_to_string(key: &[u8]) -> String {
        base64::Engine::encode(&base64::engine::general_purpose::STANDARD, key)
    }
}

impl Default for ApprovalManager {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AutoApprovePolicy {
    Never,
    Always,
    TrustedOnly,
    TrustedWithThreshold(usize),
}

impl AutoApprovePolicy {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "never" => Some(AutoApprovePolicy::Never),
            "always" => Some(AutoApprovePolicy::Always),
            "trusted" | "trusted_only" => Some(AutoApprovePolicy::TrustedOnly),
            s if s.starts_with("threshold:") => {
                let num: usize = s.trim_start_matches("threshold:")
                    .trim()
                    .parse()
                    .ok()?;
                Some(AutoApprovePolicy::TrustedWithThreshold(num))
            }
            _ => None,
        }
    }

    pub fn as_str(&self) -> &'static str {
        match self {
            AutoApprovePolicy::Never => "never",
            AutoApprovePolicy::Always => "always",
            AutoApprovePolicy::TrustedOnly => "trusted_only",
            AutoApprovePolicy::TrustedWithThreshold(n) => {
                if *n == 0 { "always" } else { "trusted_only" }
            }
        }
    }
}

pub struct ApprovalStats {
    pub total_entries: usize,
    pub approved: usize,
    pub rejected: usize,
    pub blocked: usize,
    pub approval_rate: f64,
}

impl ApprovalManager {
    pub fn get_stats(&self) -> ApprovalStats {
        let mut approved = 0;
        let mut rejected = 0;
        let mut blocked = 0;
        
        for record in &self.history {
            match record.action {
                ApprovalAction::Approved => approved += 1,
                ApprovalAction::Rejected => rejected += 1,
                ApprovalAction::Blocked => blocked += 1,
                ApprovalAction::Reported => {}
            }
        }
        
        let total = approved + rejected;
        let approval_rate = if total > 0 {
            approved as f64 / total as f64
        } else {
            1.0
        };
        
        ApprovalStats {
            total_entries: self.history.len(),
            approved,
            rejected,
            blocked,
            approval_rate,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_approval_recording() {
        let mut manager = ApprovalManager::new();
        
        let entry = GuestbookEntry::new("Test".to_string(), "Content".to_string());
        let owner_key = vec![1, 2, 3];
        
        manager.record_approval(&entry, owner_key.clone());
        
        assert_eq!(manager.get_stats().approved, 1);
    }

    #[test]
    fn test_author_blocking() {
        let mut manager = ApprovalManager::new();
        
        let author_key = vec![4, 5, 6];
        
        assert!(!manager.is_author_blocked(&author_key));
        
        manager.block_author(&author_key, vec![1, 2, 3]);
        
        assert!(manager.is_author_blocked(&author_key));
        
        manager.unblock_author(&author_key);
        
        assert!(!manager.is_author_blocked(&author_key));
    }

    #[test]
    fn test_auto_approve_policy() {
        let policy = AutoApprovePolicy::from_str("threshold:5").unwrap();
        assert!(matches!(policy, AutoApprovePolicy::TrustedWithThreshold(5)));
    }
}
