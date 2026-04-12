use crate::{Profile, PublicProfile, compute_profile_cid};
use anyhow::Result;
use chrono::Utc;
use std::collections::HashMap;
use std::path::PathBuf;
use tokio::sync::RwLock;
use std::sync::Arc;

#[derive(Debug, Clone)]
pub struct ProfileStore {
    profiles: Arc<RwLock<HashMap<String, StoredProfile>>>,
    data_dir: PathBuf,
}

#[derive(Debug, Clone)]
struct StoredProfile {
    profile: Profile,
    profile_cid: String,
    synced: bool,
}

impl ProfileStore {
    pub async fn new(data_dir: PathBuf) -> Result<Self> {
        let store = Self {
            profiles: Arc::new(RwLock::new(HashMap::new())),
            data_dir: data_dir.clone(),
        };
        
        tokio::fs::create_dir_all(&data_dir).await?;
        store.load_all_from_disk().await?;
        
        Ok(store)
    }

    pub async fn load_all_from_disk(&self) -> Result<()> {
        let profiles_dir = self.data_dir.join("profiles");
        
        if !profiles_dir.exists() {
            return Ok(());
        }
        
        let mut entries = tokio::fs::read_dir(&profiles_dir).await?;
        
        while let Some(entry) = entries.next_entry().await? {
            let path = entry.path();
            if path.extension().map_or(false, |e| e == "json") {
                let content = tokio::fs::read_to_string(&path).await?;
                if let Ok(profile) = serde_json::from_str::<Profile>(&content) {
                    let peer_id_str = if let Some(peer_id) = profile.peer_id() {
                        peer_id.to_base58()
                    } else {
                        continue;
                    };
                    
                    let profile_cid = compute_profile_cid(&profile);
                    let stored = StoredProfile {
                        profile,
                        profile_cid,
                        synced: false,
                    };
                    
                    self.profiles.write().await.insert(peer_id_str, stored);
                }
            }
        }
        
        Ok(())
    }

    pub async fn save_to_disk(&self, peer_id_str: &str, profile: &Profile) -> Result<()> {
        let profiles_dir = self.data_dir.join("profiles");
        tokio::fs::create_dir_all(&profiles_dir).await?;
        
        let filename = format!("{}.json", peer_id_str);
        let path = profiles_dir.join(&filename);
        
        let json = serde_json::to_string_pretty(profile)?;
        tokio::fs::write(&path, json).await?;
        
        Ok(())
    }

    pub async fn save(&self, profile: Profile) -> Result<String> {
        let peer_id_str = if let Some(peer_id) = profile.peer_id() {
            peer_id.to_base58()
        } else {
            return Err(anyhow::anyhow!("Invalid peer ID"));
        };
        
        let profile_cid = compute_profile_cid(&profile);
        let profile_for_disk = profile.clone();
        
        let stored = StoredProfile {
            profile,
            profile_cid: profile_cid.clone(),
            synced: false,
        };
        
        self.profiles.write().await.insert(peer_id_str.clone(), stored);
        self.save_to_disk(&peer_id_str, &profile_for_disk).await?;
        
        Ok(profile_cid)
    }

    pub async fn get(&self, peer_id_str: &str) -> Option<Profile> {
        let profiles = self.profiles.read().await;
        profiles.get(peer_id_str).map(|s| s.profile.clone())
    }

    pub async fn get_public(&self, peer_id_str: &str, viewer_peer_id: Option<&str>) -> Option<PublicProfile> {
        let profiles = self.profiles.read().await;
        
        if let Some(stored) = profiles.get(peer_id_str) {
            let viewer_is_follower = viewer_peer_id
                .and_then(|vp| profiles.get(vp))
                .map(|s| s.profile.links.iter().any(|l| l.id == peer_id_str))
                .unwrap_or(false);
            
            Some(stored.profile.public_view(viewer_is_follower))
        } else {
            None
        }
    }

    pub async fn get_cid(&self, peer_id_str: &str) -> Option<String> {
        let profiles = self.profiles.read().await;
        profiles.get(peer_id_str).map(|s| s.profile_cid.clone())
    }

    pub async fn get_all_cids(&self) -> Vec<(String, String)> {
        let profiles = self.profiles.read().await;
        profiles
            .iter()
            .map(|(k, v)| (k.clone(), v.profile_cid.clone()))
            .collect()
    }

    pub async fn cache_remote(&mut self, profile: Profile) -> Result<String> {
        let peer_id_str = if let Some(peer_id) = profile.peer_id() {
            peer_id.to_base58()
        } else {
            return Err(anyhow::anyhow!("Invalid peer ID"));
        };
        
        let profile_cid = compute_profile_cid(&profile);
        
        let stored = StoredProfile {
            profile,
            profile_cid: profile_cid.clone(),
            synced: true,
        };
        
        self.profiles.write().await.insert(peer_id_str, stored);
        
        Ok(profile_cid)
    }

    pub async fn mark_synced(&self, peer_id_str: &str) {
        let mut profiles = self.profiles.write().await;
        if let Some(stored) = profiles.get_mut(peer_id_str) {
            stored.synced = true;
        }
    }

    pub async fn get_unsynced(&self) -> Vec<(String, Profile)> {
        let profiles = self.profiles.read().await;
        profiles
            .iter()
            .filter(|(_, s)| !s.synced)
            .map(|(k, s)| (k.clone(), s.profile.clone()))
            .collect()
    }

    pub async fn delete(&self, peer_id_str: &str) -> bool {
        let removed = self.profiles.write().await.remove(peer_id_str).is_some();
        
        if removed {
            let path = self.data_dir.join("profiles").join(format!("{}.json", peer_id_str));
            let _ = tokio::fs::remove_file(&path).await;
        }
        
        removed
    }

    pub async fn len(&self) -> usize {
        self.profiles.read().await.len()
    }

    pub async fn is_empty(&self) -> bool {
        self.profiles.read().await.is_empty()
    }
}

impl Default for ProfileStore {
    fn default() -> Self {
        Self {
            profiles: Arc::new(RwLock::new(HashMap::new())),
            data_dir: PathBuf::from("."),
        }
    }
}

pub struct ProfileManager {
    store: ProfileStore,
    current_peer_id: Option<String>,
}

impl ProfileManager {
    pub async fn new(data_dir: PathBuf, peer_id: libp2p::PeerId) -> Result<Self> {
        let store = ProfileStore::new(data_dir).await?;
        let current_peer_id = Some(peer_id.to_base58());
        
        Ok(Self {
            store,
            current_peer_id,
        })
    }

    pub async fn get_my_profile(&self) -> Option<Profile> {
        if let Some(ref peer_id) = self.current_peer_id {
            self.store.get(peer_id).await
        } else {
            None
        }
    }

    pub async fn get_my_cid(&self) -> Option<String> {
        if let Some(ref peer_id) = self.current_peer_id {
            self.store.get_cid(peer_id).await
        } else {
            None
        }
    }

    pub async fn create_profile(&self, display_name: &str, username: Option<&str>) -> Result<Profile> {
        if self.current_peer_id.is_none() {
            return Err(anyhow::anyhow!("No peer ID configured"));
        }

        let peer_id = libp2p::PeerId::from_base58(self.current_peer_id.as_ref().unwrap())
            .map_err(|_| anyhow::anyhow!("Invalid peer ID"))?;

        let mut profile = Profile::builder(peer_id)
            .display_name(display_name)?;
        
        if let Some(name) = username {
            profile.set_username(name)?;
        }
        
        self.store.save(profile.clone()).await?;
        
        Ok(profile)
    }

    pub async fn update_profile<F>(&self, updater: F) -> Result<Profile>
    where
        F: FnOnce(&mut Profile),
    {
        let peer_id_str = self.current_peer_id.as_ref()
            .ok_or_else(|| anyhow::anyhow!("No peer ID configured"))?;
        
        let mut profile = self.store.get(peer_id_str).await
            .ok_or_else(|| anyhow::anyhow!("Profile not found"))?;
        
        updater(&mut profile);
        self.store.save(profile.clone()).await?;
        
        Ok(profile)
    }

    pub async fn set_display_name(&self, name: &str) -> Result<Profile> {
        self.update_profile(|p| {
            p.set_display_name(name).ok();
        }).await
    }

    pub async fn set_bio(&self, bio: &str) -> Result<Profile> {
        self.update_profile(|p| {
            p.set_bio(bio).ok();
        }).await
    }

    pub async fn set_username(&self, username: &str) -> Result<Profile> {
        self.update_profile(|p| {
            p.set_username(username).ok();
        }).await
    }

    pub async fn set_avatar(&self, cid: String) -> Result<Profile> {
        self.update_profile(|p| {
            p.set_avatar(cid);
        }).await
    }

    pub async fn set_banner(&self, cid: String) -> Result<Profile> {
        self.update_profile(|p| {
            p.set_banner(cid);
        }).await
    }

    pub async fn add_link(&self, title: &str, url: &str) -> Result<Profile> {
        let link = crate::SocialLink::new(title, url);
        self.update_profile(|p| {
            p.add_link(link).ok();
        }).await
    }

    pub async fn remove_link(&self, link_id: &str) -> Result<Profile> {
        self.update_profile(|p| {
            p.remove_link(link_id);
        }).await
    }

    pub async fn set_theme(&self, preset: crate::ThemePreset, primary: Option<&str>) -> Result<Profile> {
        use crate::Theme;
        
        self.update_profile(|p| {
            p.theme = Theme::from_preset(preset);
            if let Some(primary) = primary {
                p.theme.set_primary_color(primary).ok();
            }
        }).await
    }

    pub async fn set_layout(&self, template: &str) -> Result<Profile> {
        use crate::LayoutManager;
        
        self.update_profile(|p| {
            let manager = LayoutManager::new();
            p.layout = manager.get_template(template).unwrap_or_else(|| p.layout.clone());
        }).await
    }

    pub async fn get_profile(&self, peer_id_str: &str) -> Option<Profile> {
        self.store.get(peer_id_str).await
    }

    pub async fn get_public_profile(&self, peer_id_str: &str) -> Option<PublicProfile> {
        self.store.get_public(peer_id_str, self.current_peer_id.as_deref()).await
    }

    pub async fn cache_profile(&self, profile: Profile) -> Result<String> {
        self.store.cache_remote(profile).await
    }
}
