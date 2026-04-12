use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Theme {
    pub preset: Option<ThemePreset>,
    pub colors: ColorScheme,
    pub fonts: FontSettings,
    pub custom_css: Option<String>,
    pub spacing: SpacingSettings,
    pub border_radius: BorderRadiusSettings,
    pub animations: AnimationSettings,
}

impl Default for Theme {
    fn default() -> Self {
        Self {
            preset: Some(ThemePreset::Default),
            colors: ColorScheme::default(),
            fonts: FontSettings::default(),
            custom_css: None,
            spacing: SpacingSettings::default(),
            border_radius: BorderRadiusSettings::default(),
            animations: AnimationSettings::default(),
        }
    }
}

impl Theme {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn from_preset(preset: ThemePreset) -> Self {
        Self {
            preset: Some(preset.clone()),
            colors: preset.colors().clone(),
            ..Default::default()
        }
    }

    pub fn set_primary_color(&mut self, hex: &str) -> Result<(), ThemeError> {
        self.colors.set_primary(hex)?;
        self.preset = None;
        Ok(())
    }

    pub fn set_accent_color(&mut self, hex: &str) -> Result<(), ThemeError> {
        self.colors.set_accent(hex)?;
        self.preset = None;
        Ok(())
    }

    pub fn set_custom_css(&mut self, css: &str) -> Result<(), ThemeError> {
        if css.len() > 10000 {
            return Err(ThemeError::CssTooLong);
        }
        
        if !Self::validate_css_safety(css) {
            return Err(ThemeError::UnsafeCss);
        }
        
        self.custom_css = Some(css.to_string());
        Ok(())
    }

    pub fn clear_custom_css(&mut self) {
        self.custom_css = None;
    }

    pub fn to_css(&self) -> String {
        let mut css = String::new();
        
        css.push_str(":root {\n");
        css.push_str(&format!("  --primary: {};\n", self.colors.primary));
        css.push_str(&format!("  --primary-hover: {};\n", self.colors.primary_hover));
        css.push_str(&format!("  --secondary: {};\n", self.colors.secondary));
        css.push_str(&format!("  --accent: {};\n", self.colors.accent));
        css.push_str(&format!("  --background: {};\n", self.colors.background));
        css.push_str(&format!("  --surface: {};\n", self.colors.surface));
        css.push_str(&format!("  --text-primary: {};\n", self.colors.text_primary));
        css.push_str(&format!("  --text-secondary: {};\n", self.colors.text_secondary));
        css.push_str(&format!("  --border: {};\n", self.colors.border));
        css.push_str(&format!("  --error: {};\n", self.colors.error));
        css.push_str(&format!("  --success: {};\n", self.colors.success));
        css.push('\n');
        
        css.push_str(&format!("  --spacing-xs: {}px;\n", self.spacing.xs));
        css.push_str(&format!("  --spacing-sm: {}px;\n", self.spacing.sm));
        css.push_str(&format!("  --spacing-md: {}px;\n", self.spacing.md));
        css.push_str(&format!("  --spacing-lg: {}px;\n", self.spacing.lg));
        css.push_str(&format!("  --spacing-xl: {}px;\n", self.spacing.xl));
        css.push('\n');
        
        css.push_str(&format!("  --radius-sm: {}px;\n", self.border_radius.sm));
        css.push_str(&format!("  --radius-md: {}px;\n", self.border_radius.md));
        css.push_str(&format!("  --radius-lg: {}px;\n", self.border_radius.lg));
        css.push('\n');
        
        css.push_str("}\n\n");
        
        css.push_str(&format!("body {{\n  font-family: {}, sans-serif;\n}}\n", self.fonts.body));
        css.push_str(&format!("h1, h2, h3 {{\n  font-family: {}, serif;\n}}\n", self.fonts.heading));
        
        if let Some(ref custom) = self.custom_css {
            css.push_str("\n/* Custom CSS */\n");
            css.push_str(custom);
        }
        
        css
    }

    fn validate_css_safety(css: &str) -> bool {
        let dangerous_patterns = [
            "javascript:",
            "expression(",
            "behavior:",
            "-moz-binding:",
            "url(",
            "import",
            "filter:",
        ];
        
        for pattern in dangerous_patterns {
            if css.to_lowercase().contains(pattern) {
                return false;
            }
        }
        
        true
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ColorScheme {
    pub primary: String,
    pub primary_hover: String,
    pub secondary: String,
    pub accent: String,
    pub background: String,
    pub surface: String,
    pub text_primary: String,
    pub text_secondary: String,
    pub border: String,
    pub error: String,
    pub success: String,
}

impl Default for ColorScheme {
    fn default() -> Self {
        Self {
            primary: "#6366f1".to_string(),
            primary_hover: "#4f46e5".to_string(),
            secondary: "#64748b".to_string(),
            accent: "#f59e0b".to_string(),
            background: "#0f172a".to_string(),
            surface: "#1e293b".to_string(),
            text_primary: "#f8fafc".to_string(),
            text_secondary: "#94a3b8".to_string(),
            border: "#334155".to_string(),
            error: "#ef4444".to_string(),
            success: "#22c55e".to_string(),
        }
    }
}

impl ColorScheme {
    pub fn set_primary(&mut self, hex: &str) -> Result<(), ThemeError> {
        Self::validate_hex(hex)?;
        self.primary = hex.to_string();
        self.primary_hover = Self::darken(hex, 10);
        Ok(())
    }

    pub fn set_accent(&mut self, hex: &str) -> Result<(), ThemeError> {
        Self::validate_hex(hex)?;
        self.accent = hex.to_string();
        Ok(())
    }

    pub fn set_background(&mut self, hex: &str) -> Result<(), ThemeError> {
        Self::validate_hex(hex)?;
        self.background = hex.to_string();
        
        let is_light = Self::is_light_color(hex);
        if is_light {
            self.text_primary = "#1e293b".to_string();
            self.text_secondary = "#64748b".to_string();
            self.border = "#e2e8f0".to_string();
        } else {
            self.text_primary = "#f8fafc".to_string();
            self.text_secondary = "#94a3b8".to_string();
            self.border = "#334155".to_string();
        }
        Ok(())
    }

    fn validate_hex(hex: &str) -> Result<(), ThemeError> {
        let hex = hex.trim_start_matches('#');
        if hex.len() != 6 && hex.len() != 3 {
            return Err(ThemeError::InvalidColor);
        }
        if !hex.chars().all(|c| c.is_ascii_hexdigit()) {
            return Err(ThemeError::InvalidColor);
        }
        Ok(())
    }

    fn darken(hex: &str, percent: i32) -> String {
        let hex = hex.trim_start_matches('#');
        let r = u8::from_str_radix(&hex[0..2], 16).unwrap_or(0);
        let g = u8::from_str_radix(&hex[2..4], 16).unwrap_or(0);
        let b = u8::from_str_radix(&hex[4..6], 16).unwrap_or(0);
        
        let factor = 1.0 - (percent as f32 / 100.0);
        let r = (r as f32 * factor) as u8;
        let g = (g as f32 * factor) as u8;
        let b = (b as f32 * factor) as u8;
        
        format!("#{:02x}{:02x}{:02x}", r, g, b)
    }

    fn is_light_color(hex: &str) -> bool {
        let hex = hex.trim_start_matches('#');
        let r = u8::from_str_radix(&hex[0..2], 16).unwrap_or(0);
        let g = u8::from_str_radix(&hex[2..4], 16).unwrap_or(0);
        let b = u8::from_str_radix(&hex[4..6], 16).unwrap_or(0);
        
        let luminance = (0.299 * r as f32 + 0.587 * g as f32 + 0.114 * b as f32) / 255.0;
        luminance > 0.5
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FontSettings {
    pub body: String,
    pub heading: String,
    pub mono: String,
    pub size_scale: f32,
}

impl Default for FontSettings {
    fn default() -> Self {
        Self {
            body: "Inter".to_string(),
            heading: "Playfair Display".to_string(),
            mono: "JetBrains Mono".to_string(),
            size_scale: 1.0,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpacingSettings {
    pub xs: u32,
    pub sm: u32,
    pub md: u32,
    pub lg: u32,
    pub xl: u32,
}

impl Default for SpacingSettings {
    fn default() -> Self {
        Self {
            xs: 4,
            sm: 8,
            md: 16,
            lg: 24,
            xl: 32,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BorderRadiusSettings {
    pub sm: u32,
    pub md: u32,
    pub lg: u32,
}

impl Default for BorderRadiusSettings {
    fn default() -> Self {
        Self {
            sm: 4,
            md: 8,
            lg: 16,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AnimationSettings {
    pub enabled: bool,
    pub duration: u32,
    pub easing: String,
}

impl Default for AnimationSettings {
    fn default() -> Self {
        Self {
            enabled: true,
            duration: 200,
            easing: "ease-in-out".to_string(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ThemePreset {
    Default,
    Midnight,
    Ocean,
    Forest,
    Sunset,
    Minimal,
    Hacker,
}

impl ThemePreset {
    pub fn colors(&self) -> &ColorScheme {
        match self {
            ThemePreset::Default => &ColorScheme::default(),
            ThemePreset::Midnight => &ColorScheme {
                primary: "#818cf8".to_string(),
                primary_hover: "#6366f1".to_string(),
                secondary: "#475569".to_string(),
                accent: "#fbbf24".to_string(),
                background: "#020617".to_string(),
                surface: "#0f172a".to_string(),
                text_primary: "#f1f5f9".to_string(),
                text_secondary: "#94a3b8".to_string(),
                border: "#1e293b".to_string(),
                error: "#f87171".to_string(),
                success: "#4ade80".to_string(),
            },
            ThemePreset::Ocean => &ColorScheme {
                primary: "#06b6d4".to_string(),
                primary_hover: "#0891b2".to_string(),
                secondary: "#64748b".to_string(),
                accent: "#f472b6".to_string(),
                background: "#082f49".to_string(),
                surface: "#0c4a6e".to_string(),
                text_primary: "#e0f2fe".to_string(),
                text_secondary: "#7dd3fc".to_string(),
                border: "#155e75".to_string(),
                error: "#f87171".to_string(),
                success: "#4ade80".to_string(),
            },
            ThemePreset::Forest => &ColorScheme {
                primary: "#22c55e".to_string(),
                primary_hover: "#16a34a".to_string(),
                secondary: "#64748b".to_string(),
                accent: "#f59e0b".to_string(),
                background: "#052e16".to_string(),
                surface: "#064e3b".to_string(),
                text_primary: "#ecfdf5".to_string(),
                text_secondary: "#86efac".to_string(),
                border: "#065f46".to_string(),
                error: "#f87171".to_string(),
                success: "#4ade80".to_string(),
            },
            ThemePreset::Sunset => &ColorScheme {
                primary: "#f97316".to_string(),
                primary_hover: "#ea580c".to_string(),
                secondary: "#64748b".to_string(),
                accent: "#ec4899".to_string(),
                background: "#1c1917".to_string(),
                surface: "#292524".to_string(),
                text_primary: "#fef3c7".to_string(),
                text_secondary: "#fcd34d".to_string(),
                border: "#44403c".to_string(),
                error: "#f87171".to_string(),
                success: "#4ade80".to_string(),
            },
            ThemePreset::Minimal => &ColorScheme {
                primary: "#18181b".to_string(),
                primary_hover: "#09090b".to_string(),
                secondary: "#71717a".to_string(),
                accent: "#a855f7".to_string(),
                background: "#ffffff".to_string(),
                surface: "#fafafa".to_string(),
                text_primary: "#09090b".to_string(),
                text_secondary: "#52525b".to_string(),
                border: "#e4e4e7".to_string(),
                error: "#ef4444".to_string(),
                success: "#22c55e".to_string(),
            },
            ThemePreset::Hacker => &ColorScheme {
                primary: "#00ff00".to_string(),
                primary_hover: "#00cc00".to_string(),
                secondary: "#0aff0a".to_string(),
                accent: "#ff00ff".to_string(),
                background: "#000000".to_string(),
                surface: "#0a0a0a".to_string(),
                text_primary: "#00ff00".to_string(),
                text_secondary: "#00cc00".to_string(),
                border: "#003300".to_string(),
                error: "#ff0000".to_string(),
                success: "#00ff00".to_string(),
            },
        }
    }

    pub fn display_name(&self) -> &'static str {
        match self {
            ThemePreset::Default => "Midnight Purple",
            ThemePreset::Midnight => "Midnight Blue",
            ThemePreset::Ocean => "Ocean Deep",
            ThemePreset::Forest => "Forest Green",
            ThemePreset::Sunset => "Warm Sunset",
            ThemePreset::Minimal => "Clean Minimal",
            ThemePreset::Hacker => "Hacker Terminal",
        }
    }
}

#[derive(Debug, thiserror::Error)]
pub enum ThemeError {
    #[error("Invalid hex color format")]
    InvalidColor,
    
    #[error("CSS content too long (max 10000 characters)")]
    CssTooLong,
    
    #[error("CSS contains potentially unsafe content")]
    UnsafeCss,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_theme_preset() {
        let theme = Theme::from_preset(ThemePreset::Midnight);
        assert!(theme.preset.is_some());
        assert_eq!(theme.colors.primary, "#818cf8");
    }

    #[test]
    fn test_color_validation() {
        let mut colors = ColorScheme::default();
        assert!(colors.set_primary("#ff0000").is_ok());
        assert!(colors.set_primary("invalid").is_err());
    }

    #[test]
    fn test_css_generation() {
        let theme = Theme::default();
        let css = theme.to_css();
        assert!(css.contains("--primary:"));
        assert!(css.contains(":root"));
    }

    #[test]
    fn test_css_safety() {
        assert!(!Theme::validate_css_safety("javascript:alert(1)"));
        assert!(Theme::validate_css_safety("color: red; background: blue;"));
    }
}
