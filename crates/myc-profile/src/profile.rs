use crate::{Layout, Theme, ContentValidator};
use chrono::{DateTime, Utc};
use libp2p::PeerId;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Profile {
    pub id: String,
    pub peer_id: Vec<u8>,
    pub username: Option<String>,
    pub display_name: String,
    pub bio: Option<String>,
    pub avatar_cid: Option<String>,
    pub banner_cid: Option<String>,
    pub layout: Layout,
    pub theme: Theme,
    pub links: Vec<SocialLink>,
    pub created_at: i64,
    pub updated_at: i64,
    pub signature: Option<Vec<u8>>,
}

impl Default for Profile {
    fn default() -> Self {
        Self {
            id: uuid::Uuid::new_v4().to_string(),
            peer_id: Vec::new(),
            username: None,
            display_name: String::new(),
            bio: None,
            avatar_cid: None,
            banner_cid: None,
            layout: Layout::default(),
            theme: Theme::default(),
            links: Vec::new(),
            created_at: Utc::now().timestamp(),
            updated_at: Utc::now().timestamp(),
            signature: None,
        }
    }
}

impl Profile {
    pub fn new(peer_id: PeerId) -> Self {
        Self {
            peer_id: peer_id.to_bytes(),
            display_name: format!("User_{}", &peer_id.to_base58()[..8]),
            ..Default::default()
        }
    }

    pub fn builder(peer_id: PeerId) -> ProfileBuilder {
        ProfileBuilder::new(peer_id)
    }

    pub fn peer_id(&self) -> Option<PeerId> {
        PeerId::from_bytes(&self.peer_id).ok()
    }

    pub fn set_username(&mut self, username: &str) -> Result<(), ProfileError> {
        let validator = ContentValidator::new();
        validator.validate_username(username)?;
        self.username = Some(username.to_lowercase());
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn set_display_name(&mut self, name: &str) -> Result<(), ProfileError> {
        if name.len() > 100 {
            return Err(ProfileError::DisplayNameTooLong);
        }
        self.display_name = name.to_string();
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn set_bio(&mut self, bio: &str) -> Result<(), ProfileError> {
        let validator = ContentValidator::new();
        validator.validate_content(bio)?;
        
        if bio.len() > 500 {
            return Err(ProfileError::BioTooLong);
        }
        self.bio = Some(bio.to_string());
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn add_link(&mut self, link: SocialLink) -> Result<(), ProfileError> {
        let validator = ContentValidator::new();
        validator.validate_link(&link)?;
        
        if self.links.len() >= 10 {
            return Err(ProfileError::TooManyLinks);
        }
        self.links.push(link);
        self.updated_at = Utc::now().timestamp();
        Ok(())
    }

    pub fn remove_link(&mut self, id: &str) -> bool {
        let len = self.links.len();
        self.links.retain(|l| l.id != id);
        if self.links.len() != len {
            self.updated_at = Utc::now().timestamp();
            true
        } else {
            false
        }
    }

    pub fn set_avatar(&mut self, cid: String) {
        self.avatar_cid = Some(cid);
        self.updated_at = Utc::now().timestamp();
    }

    pub fn set_banner(&mut self, cid: String) {
        self.banner_cid = Some(cid);
        self.updated_at = Utc::now().timestamp();
    }

    pub fn is_complete(&self) -> bool {
        self.username.is_some() 
            && !self.display_name.is_empty()
            && self.bio.is_some()
    }

    pub fn public_view(&self, viewer_is_follower: bool) -> PublicProfile {
        PublicProfile {
            peer_id: self.peer_id.clone(),
            username: self.username.clone(),
            display_name: self.display_name.clone(),
            bio: self.bio.clone(),
            avatar_cid: self.avatar_cid.clone(),
            banner_cid: self.banner_cid.clone(),
            links: if viewer_is_follower {
                self.links.clone()
            } else {
                self.links.iter().filter(|l| !l.is_private).cloned().collect()
            },
            created_at: self.created_at,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ProfileBuilder {
    profile: Profile,
}

impl ProfileBuilder {
    pub fn new(peer_id: PeerId) -> Self {
        Self {
            profile: Profile::new(peer_id),
        }
    }

    pub fn username(mut self, username: &str) -> Result<Self, ProfileError> {
        let validator = ContentValidator::new();
        validator.validate_username(username)?;
        self.profile.username = Some(username.to_lowercase());
        Ok(self)
    }

    pub fn display_name(mut self, name: &str) -> Result<Self, ProfileError> {
        if name.len() > 100 {
            return Err(ProfileError::DisplayNameTooLong);
        }
        self.profile.display_name = name.to_string();
        Ok(self)
    }

    pub fn bio(mut self, bio: &str) -> Result<Self, ProfileError> {
        let validator = ContentValidator::new();
        validator.validate_content(bio)?;
        if bio.len() > 500 {
            return Err(ProfileError::BioTooLong);
        }
        self.profile.bio = Some(bio.to_string());
        Ok(self)
    }

    pub fn avatar(mut self, cid: String) -> Self {
        self.profile.avatar_cid = Some(cid);
        self
    }

    pub fn banner(mut self, cid: String) -> Self {
        self.profile.banner_cid = Some(cid);
        self
    }

    pub fn link(mut self, link: SocialLink) -> Result<Self, ProfileError> {
        let validator = ContentValidator::new();
        validator.validate_link(&link)?;
        if self.profile.links.len() >= 10 {
            return Err(ProfileError::TooManyLinks);
        }
        self.profile.links.push(link);
        Ok(self)
    }

    pub fn theme(mut self, theme: Theme) -> Self {
        self.profile.theme = theme;
        self
    }

    pub fn layout(mut self, layout: Layout) -> Self {
        self.profile.layout = layout;
        self
    }

    pub fn build(self) -> Profile {
        self.profile
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SocialLink {
    pub id: String,
    pub title: String,
    pub url: String,
    pub icon: Option<String>,
    pub is_private: bool,
}

impl SocialLink {
    pub fn new(title: &str, url: &str) -> Self {
        Self {
            id: uuid::Uuid::new_v4().to_string(),
            title: title.to_string(),
            url: url.to_string(),
            icon: None,
            is_private: false,
        }
    }

    pub fn with_icon(mut self, icon: &str) -> Self {
        self.icon = Some(icon.to_string());
        self
    }

    pub fn private(mut self) -> Self {
        self.is_private = true;
        self
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PublicProfile {
    pub peer_id: Vec<u8>,
    pub username: Option<String>,
    pub display_name: String,
    pub bio: Option<String>,
    pub avatar_cid: Option<String>,
    pub banner_cid: Option<String>,
    pub links: Vec<SocialLink>,
    pub created_at: i64,
}

impl PublicProfile {
    pub fn peer_id(&self) -> Option<PeerId> {
        PeerId::from_bytes(&self.peer_id).ok()
    }
}

#[derive(Debug, thiserror::Error, Clone)]
pub enum ProfileError {
    #[error("Username already taken")]
    UsernameTaken,
    
    #[error("Invalid username format")]
    InvalidUsername,
    
    #[error("Username too long (max 30 characters)")]
    UsernameTooLong,
    
    #[error("Username contains invalid characters")]
    InvalidCharacters,
    
    #[error("Display name too long (max 100 characters)")]
    DisplayNameTooLong,
    
    #[error("Bio too long (max 500 characters)")]
    BioTooLong,
    
    #[error("Too many links (max 10)")]
    TooManyLinks,
    
    #[error("Content contains prohibited information")]
    ProhibitedContent,
    
    #[error("Invalid URL")]
    InvalidUrl,
    
    #[error("Profile signature verification failed")]
    InvalidSignature,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_profile_creation() {
        let peer_id = PeerId::random();
        let profile = Profile::new(peer_id);
        
        assert!(profile.username.is_none());
        assert!(!profile.display_name.is_empty());
        assert!(profile.peer_id().is_some());
    }

    #[test]
    fn test_username_validation() {
        let peer_id = PeerId::random();
        let mut profile = Profile::new(peer_id);
        
        assert!(profile.set_username("alice").is_ok());
        assert_eq!(profile.username, Some("alice".to_string()));
    }

    #[test]
    fn test_username_lowercase() {
        let peer_id = PeerId::random();
        let mut profile = Profile::new(peer_id);
        
        profile.set_username("Alice").unwrap();
        assert_eq!(profile.username, Some("alice".to_string()));
    }

    #[test]
    fn test_social_link() {
        let link = SocialLink::new("GitHub", "https://github.com/user")
            .with_icon("github")
            .private();
        
        assert!(link.is_private);
        assert_eq!(link.icon, Some("github".to_string()));
    }
}
