pub mod profile;
pub mod layout;
pub mod theme;
pub mod widget;
pub mod validation;
pub mod storage;
pub mod sync;

pub use profile::{Profile, ProfileBuilder, ProfileError, SocialLink};
pub use layout::{Layout, Section, LayoutManager};
pub use theme::{Theme, ThemePreset, ColorScheme};
pub use widget::{WidgetType, Widget, WidgetConfig};
pub use validation::{ContentValidator, PrivacyValidator};
pub use storage::{ProfileStore, ProfileManager};
pub use sync::{ProfileMessage, ProfileMessageType, ProfileSyncService, ProfileBroadcast, PROFILE_TOPIC};

use chrono::Utc;
use libp2p::PeerId;
use sha2::{Sha256, Digest};

pub fn compute_profile_cid(profile: &Profile) -> String {
    let data = serde_json::to_string(profile).unwrap();
    let mut hasher = Sha256::new();
    hasher.update(data.as_bytes());
    let result = hasher.finalize();
    format!("Qm{}", &base64::Engine::encode(&base64::engine::general_purpose::STANDARD, &result[..16])[..44])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_profile_cid() {
        let profile = Profile::default();
        let cid = compute_profile_cid(&profile);
        assert!(cid.starts_with("Qm"));
    }
}
