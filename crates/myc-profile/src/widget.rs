use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum WidgetType {
    Bio,
    Links,
    Guestbook,
    RecentPosts,
    Followers,
    Following,
    CustomHTML,
    Media,
    Stats,
}

impl WidgetType {
    pub fn display_name(&self) -> &'static str {
        match self {
            WidgetType::Bio => "Bio",
            WidgetType::Links => "Links",
            WidgetType::Guestbook => "Guestbook",
            WidgetType::RecentPosts => "Recent Posts",
            WidgetType::Followers => "Followers",
            WidgetType::Following => "Following",
            WidgetType::CustomHTML => "Custom HTML",
            WidgetType::Media => "Media Gallery",
            WidgetType::Stats => "Statistics",
        }
    }

    pub fn default_section(&self) -> &'static str {
        match self {
            WidgetType::Bio => "header",
            WidgetType::Links => "sidebar",
            WidgetType::Guestbook => "main",
            WidgetType::RecentPosts => "main",
            WidgetType::Followers => "sidebar",
            WidgetType::Following => "sidebar",
            WidgetType::CustomHTML => "custom",
            WidgetType::Media => "main",
            WidgetType::Stats => "sidebar",
        }
    }

    pub fn allows_custom_title(&self) -> bool {
        true
    }

    pub fn supports_visibility(&self) -> bool {
        true
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Widget {
    pub widget_type: WidgetType,
    pub config: WidgetConfig,
    pub visibility: crate::layout::Visibility,
}

impl Widget {
    pub fn new(widget_type: WidgetType) -> Self {
        Self {
            widget_type,
            config: WidgetConfig::default_for(widget_type),
            visibility: crate::layout::Visibility::default(),
        }
    }

    pub fn with_config(mut self, config: WidgetConfig) -> Self {
        self.config = config;
        self
    }

    pub fn with_visibility(mut self, visibility: crate::layout::Visibility) -> Self {
        self.visibility = visibility;
        self
    }

    pub fn to_section(&self, id: &str) -> crate::layout::Section {
        crate::layout::Section::new(id, self.widget_type)
            .with_visibility(self.visibility)
            .with_config(self.config.to_json())
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetConfig {
    #[serde(flatten)]
    pub settings: serde_json::Value,
}

impl Default for WidgetConfig {
    fn default() -> Self {
        Self {
            settings: serde_json::json!({}),
        }
    }
}

impl WidgetConfig {
    pub fn default_for(widget_type: WidgetType) -> Self {
        let settings = match widget_type {
            WidgetType::Bio => serde_json::json!({
                "max_length": 500,
                "show_timestamp": false,
            }),
            WidgetType::Links => serde_json::json!({
                "max_links": 10,
                "show_icons": true,
                "open_in_new_tab": true,
            }),
            WidgetType::Guestbook => serde_json::json!({
                "entries_per_page": 20,
                "show_avatars": false,
                "allow_html": false,
                "max_entry_length": 1000,
            }),
            WidgetType::RecentPosts => serde_json::json!({
                "max_posts": 10,
                "show_images": true,
                "show_stats": true,
            }),
            WidgetType::Followers => serde_json::json!({
                "max_display": 24,
                "show_counts": true,
                "grid_layout": true,
            }),
            WidgetType::Following => serde_json::json!({
                "max_display": 24,
                "show_counts": true,
            }),
            WidgetType::CustomHTML => serde_json::json!({
                "html": "",
                "sanitize": true,
                "allow_css": false,
            }),
            WidgetType::Media => serde_json::json!({
                "max_items": 20,
                "columns": 3,
                "lightbox": true,
            }),
            WidgetType::Stats => serde_json::json!({
                "show_posts": true,
                "show_followers": true,
                "show_following": true,
                "show_guestbook_entries": true,
            }),
        };

        Self { settings }
    }

    pub fn get_bool(&self, key: &str) -> Option<bool> {
        self.settings.get(key)?.as_bool()
    }

    pub fn get_u64(&self, key: &str) -> Option<u64> {
        self.settings.get(key)?.as_u64()
    }

    pub fn get_str(&self, key: &str) -> Option<&str> {
        self.settings.get(key)?.as_str()
    }

    pub fn set(&mut self, key: &str, value: serde_json::Value) {
        if let Some(obj) = self.settings.as_object_mut() {
            obj.insert(key.to_string(), value);
        }
    }

    pub fn to_json(&self) -> serde_json::Value {
        self.settings.clone()
    }

    pub fn from_json(json: serde_json::Value) -> Self {
        Self { settings: json }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WidgetManifest {
    pub widget_type: WidgetType,
    pub display_name: String,
    pub description: String,
    pub default_section: String,
    pub config_schema: serde_json::Value,
    pub version: u32,
}

impl WidgetManifest {
    pub fn all() -> Vec<Self> {
        vec![
            WidgetManifest {
                widget_type: WidgetType::Bio,
                display_name: "Bio".to_string(),
                description: "Display your bio and profile information".to_string(),
                default_section: "header".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "max_length": {"type": "integer", "default": 500},
                        "show_timestamp": {"type": "boolean", "default": false},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::Links,
                display_name: "Links".to_string(),
                description: "Display links to your other profiles and websites".to_string(),
                default_section: "sidebar".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "max_links": {"type": "integer", "default": 10},
                        "show_icons": {"type": "boolean", "default": true},
                        "open_in_new_tab": {"type": "boolean", "default": true},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::Guestbook,
                display_name: "Guestbook".to_string(),
                description: "Allow visitors to leave messages on your profile".to_string(),
                default_section: "main".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "entries_per_page": {"type": "integer", "default": 20},
                        "show_avatars": {"type": "boolean", "default": false},
                        "allow_html": {"type": "boolean", "default": false},
                        "max_entry_length": {"type": "integer", "default": 1000},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::RecentPosts,
                display_name: "Recent Posts".to_string(),
                description: "Display your most recent posts".to_string(),
                default_section: "main".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "max_posts": {"type": "integer", "default": 10},
                        "show_images": {"type": "boolean", "default": true},
                        "show_stats": {"type": "boolean", "default": true},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::Followers,
                display_name: "Followers".to_string(),
                description: "Display your followers".to_string(),
                default_section: "sidebar".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "max_display": {"type": "integer", "default": 24},
                        "show_counts": {"type": "boolean", "default": true},
                        "grid_layout": {"type": "boolean", "default": true},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::CustomHTML,
                display_name: "Custom HTML".to_string(),
                description: "Embed custom HTML content".to_string(),
                default_section: "custom".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "html": {"type": "string", "default": ""},
                        "sanitize": {"type": "boolean", "default": true},
                        "allow_css": {"type": "boolean", "default": false},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::Media,
                display_name: "Media Gallery".to_string(),
                description: "Display images and media".to_string(),
                default_section: "main".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "max_items": {"type": "integer", "default": 20},
                        "columns": {"type": "integer", "default": 3},
                        "lightbox": {"type": "boolean", "default": true},
                    }
                }),
                version: 1,
            },
            WidgetManifest {
                widget_type: WidgetType::Stats,
                display_name: "Statistics".to_string(),
                description: "Display profile statistics".to_string(),
                default_section: "sidebar".to_string(),
                config_schema: serde_json::json!({
                    "type": "object",
                    "properties": {
                        "show_posts": {"type": "boolean", "default": true},
                        "show_followers": {"type": "boolean", "default": true},
                        "show_following": {"type": "boolean", "default": true},
                        "show_guestbook_entries": {"type": "boolean", "default": true},
                    }
                }),
                version: 1,
            },
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_widget_config_defaults() {
        let config = WidgetConfig::default_for(WidgetType::Guestbook);
        assert_eq!(config.get_u64("entries_per_page"), Some(20));
    }

    #[test]
    fn test_widget_manifest_all() {
        let manifests = WidgetManifest::all();
        assert_eq!(manifests.len(), 8);
    }

    #[test]
    fn test_widget_to_section() {
        let widget = Widget::new(WidgetType::Bio);
        let section = widget.to_section("test-section");
        assert_eq!(section.id, "test-section");
    }
}
