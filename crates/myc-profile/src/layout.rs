use crate::widget::WidgetType;
use crate::validation::ContentValidator;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Layout {
    pub sections: Vec<Section>,
    pub version: u32,
}

impl Default for Layout {
    fn default() -> Self {
        Self {
            sections: vec![
                Section::new("header", WidgetType::Bio),
                Section::new("main", WidgetType::Guestbook),
                Section::new("sidebar", WidgetType::Links),
            ],
            version: 1,
        }
    }
}

impl Layout {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_section(&mut self, section: Section) -> Result<(), LayoutError> {
        if self.sections.len() >= 20 {
            return Err(LayoutError::TooManySections);
        }
        
        if self.sections.iter().any(|s| s.id == section.id) {
            return Err(LayoutError::SectionExists);
        }
        
        self.sections.push(section);
        self.version += 1;
        Ok(())
    }

    pub fn remove_section(&mut self, section_id: &str) -> bool {
        let len = self.sections.len();
        self.sections.retain(|s| s.id != section_id);
        if self.sections.len() != len {
            self.version += 1;
            true
        } else {
            false
        }
    }

    pub fn move_section(&mut self, section_id: &str, new_position: usize) -> Result<(), LayoutError> {
        let current_pos = self.sections.iter().position(|s| s.id == section_id)
            .ok_or(LayoutError::SectionNotFound)?;
        
        if new_position >= self.sections.len() {
            return Err(LayoutError::InvalidPosition);
        }
        
        let section = self.sections.remove(current_pos);
        self.sections.insert(new_position, section);
        self.version += 1;
        Ok(())
    }

    pub fn update_section(&mut self, section_id: &str, config: serde_json::Value) -> Result<(), LayoutError> {
        let section = self.sections.iter_mut()
            .find(|s| s.id == section_id)
            .ok_or(LayoutError::SectionNotFound)?;
        
        section.config = config;
        self.version += 1;
        Ok(())
    }

    pub fn get_section(&self, section_id: &str) -> Option<&Section> {
        self.sections.iter().find(|s| s.id == section_id)
    }

    pub fn get_section_mut(&mut self, section_id: &str) -> Option<&mut Section> {
        self.sections.iter_mut().find(|s| s.id == section_id)
    }

    pub fn reorder_widgets(&mut self, section_id: &str, widget_ids: Vec<String>) -> Result<(), LayoutError> {
        let section = self.get_section_mut(section_id)
            .ok_or(LayoutError::SectionNotFound)?;
        
        section.widget_order = widget_ids;
        self.version += 1;
        Ok(())
    }

    pub fn visible_sections(&self, viewer_is_owner: bool, viewer_is_follower: bool) -> Vec<&Section> {
        self.sections.iter()
            .filter(|s| s.is_visible(viewer_is_owner, viewer_is_follower))
            .collect()
    }

    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(self)
    }

    pub fn from_json(json: &str) -> Result<Self, serde_json::Error> {
        serde_json::from_str(json)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Section {
    pub id: String,
    pub widget_type: WidgetType,
    pub config: serde_json::Value,
    pub visibility: Visibility,
    pub widget_order: Vec<String>,
    pub title: Option<String>,
}

impl Section {
    pub fn new(id: &str, widget_type: WidgetType) -> Self {
        Self {
            id: id.to_string(),
            widget_type,
            config: serde_json::json!({}),
            visibility: Visibility::default(),
            widget_order: Vec::new(),
            title: None,
        }
    }

    pub fn with_title(mut self, title: &str) -> Self {
        self.title = Some(title.to_string());
        self
    }

    pub fn with_visibility(mut self, visibility: Visibility) -> Self {
        self.visibility = visibility;
        self
    }

    pub fn with_config(mut self, config: serde_json::Value) -> Self {
        self.config = config;
        self
    }

    pub fn is_visible(&self, is_owner: bool, is_follower: bool) -> bool {
        self.visibility.is_visible(is_owner, is_follower)
    }

    pub fn update_config(&mut self, key: &str, value: serde_json::Value) {
        if let Some(obj) = self.config.as_object_mut() {
            obj.insert(key.to_string(), value);
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Visibility {
    Public,
    FollowersOnly,
    Private,
}

impl Default for Visibility {
    fn default() -> Self {
        Self::Public
    }
}

impl Visibility {
    pub fn is_visible(&self, is_owner: bool, is_follower: bool) -> bool {
        match self {
            Visibility::Public => true,
            Visibility::FollowersOnly => is_owner || is_follower,
            Visibility::Private => is_owner,
        }
    }

    pub fn as_str(&self) -> &'static str {
        match self {
            Visibility::Public => "public",
            Visibility::FollowersOnly => "followers",
            Visibility::Private => "private",
        }
    }

    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "public" => Some(Visibility::Public),
            "followers" | "followers_only" => Some(Visibility::FollowersOnly),
            "private" => Some(Visibility::Private),
            _ => None,
        }
    }
}

pub struct LayoutManager {
    templates: HashMap<String, Layout>,
}

impl LayoutManager {
    pub fn new() -> Self {
        let mut templates = HashMap::new();
        
        templates.insert("default".to_string(), Layout::default());
        templates.insert("minimal".to_string(), Layout::minimal());
        templates.insert("magazine".to_string(), Layout::magazine());
        templates.insert("portfolio".to_string(), Layout::portfolio());
        
        Self { templates }
    }

    pub fn get_template(&self, name: &str) -> Option<Layout> {
        self.templates.get(name).cloned()
    }

    pub fn list_templates(&self) -> Vec<&str> {
        self.templates.keys().map(|s| s.as_str()).collect()
    }

    pub fn validate_layout(&self, layout: &Layout) -> Result<(), LayoutError> {
        if layout.sections.is_empty() {
            return Err(LayoutError::EmptyLayout);
        }
        
        let ids: Vec<&str> = layout.sections.iter().map(|s| s.id.as_str()).collect();
        let unique_ids: std::collections::HashSet<_> = ids.iter().collect();
        
        if unique_ids.len() != ids.len() {
            return Err(LayoutError::DuplicateSectionId);
        }
        
        Ok(())
    }
}

impl Default for LayoutManager {
    fn default() -> Self {
        Self::new()
    }
}

impl Layout {
    pub fn minimal() -> Self {
        Self {
            sections: vec![
                Section::new("main", WidgetType::Bio),
            ],
            version: 1,
        }
    }

    pub fn magazine() -> Self {
        Self {
            sections: vec![
                Section::new("hero", WidgetType::Bio).with_title("About"),
                Section::new("content", WidgetType::RecentPosts),
                Section::new("sidebar", WidgetType::Links),
                Section::new("social", WidgetType::Guestbook).with_title("Guestbook"),
            ],
            version: 1,
        }
    }

    pub fn portfolio() -> Self {
        Self {
            sections: vec![
                Section::new("header", WidgetType::Bio).with_title("Portfolio"),
                Section::new("links", WidgetType::Links).with_title("Projects"),
                Section::new("social", WidgetType::Guestbook).with_title("Leave a Note"),
            ],
            version: 1,
        }
    }
}

#[derive(Debug, thiserror::Error)]
pub enum LayoutError {
    #[error("Too many sections (max 20)")]
    TooManySections,
    
    #[error("Section with this ID already exists")]
    SectionExists,
    
    #[error("Section not found")]
    SectionNotFound,
    
    #[error("Invalid position")]
    InvalidPosition,
    
    #[error("Layout cannot be empty")]
    EmptyLayout,
    
    #[error("Duplicate section ID")]
    DuplicateSectionId,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_layout() {
        let layout = Layout::default();
        assert_eq!(layout.sections.len(), 3);
    }

    #[test]
    fn test_add_remove_section() {
        let mut layout = Layout::default();
        
        layout.add_section(Section::new("test", WidgetType::Links)).unwrap();
        assert_eq!(layout.sections.len(), 4);
        
        assert!(layout.remove_section("test"));
        assert_eq!(layout.sections.len(), 3);
    }

    #[test]
    fn test_visibility() {
        assert!(Visibility::Public.is_visible(false, false));
        assert!(Visibility::FollowersOnly.is_visible(false, true));
        assert!(!Visibility::FollowersOnly.is_visible(false, false));
        assert!(Visibility::Private.is_visible(true, false));
        assert!(!Visibility::Private.is_visible(false, false));
    }

    #[test]
    fn test_templates() {
        let manager = LayoutManager::new();
        
        assert!(manager.get_template("minimal").is_some());
        assert!(manager.get_template("magazine").is_some());
        assert!(manager.get_template("invalid").is_none());
    }
}
