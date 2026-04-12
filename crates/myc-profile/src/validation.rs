use regex::Regex;
use crate::profile::SocialLink;

pub struct ContentValidator {
    username_regex: Regex,
    url_regex: Regex,
    email_patterns: Vec<Regex>,
    phone_patterns: Vec<Regex>,
    address_patterns: Vec<Regex>,
}

impl Default for ContentValidator {
    fn default() -> Self {
        Self::new()
    }
}

impl ContentValidator {
    pub fn new() -> Self {
        Self {
            username_regex: Regex::new(r"^[a-z][a-z0-9_-]{2,29}$").unwrap(),
            url_regex: Regex::new(r"^https?://[^\s/$.?#].[^\s]*$").unwrap(),
            email_patterns: vec![
                Regex::new(r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}").unwrap(),
            ],
            phone_patterns: vec![
                Regex::new(r"\b\d{3}[-.]?\d{3}[-.]?\d{4}\b").unwrap(),
                Regex::new(r"\b\(\d{3}\)\s*\d{3}[-.]?\d{4}\b").unwrap(),
                Regex::new(r"\b\+\d{1,3}[-.\s]?\(?\d{1,4}\)?[-.\s]?\d{1,4}[-.\s]?\d{1,9}\b").unwrap(),
            ],
            address_patterns: vec![
                Regex::new(r"\b\d+\s+[A-Za-z]+\s+(Street|St|Avenue|Ave|Road|Rd|Boulevard|Blvd|Lane|Ln|Drive|Dr|Court|Ct|Circle|Cir)\b").unwrap(),
                Regex::new(r"\b[A-Z][a-z]+,\s*[A-Z]{2}\s*\d{5}\b").unwrap(),
                Regex::new(r"\b\d{5}(-\d{4})?\b").unwrap(),
            ],
        }
    }

    pub fn validate_username(&self, username: &str) -> Result<(), ValidationError> {
        let trimmed = username.trim();
        
        if trimmed.is_empty() {
            return Err(ValidationError::EmptyUsername);
        }
        
        if trimmed.len() > 30 {
            return Err(ValidationError::UsernameTooLong);
        }
        
        if trimmed.len() < 3 {
            return Err(ValidationError::UsernameTooShort);
        }
        
        if !self.username_regex.is_match(trimmed) {
            return Err(ValidationError::InvalidUsernameFormat);
        }
        
        let reserved = ["admin", "root", "system", "moderator", "mod", "support", "help", "api", "www", "mail", "ftp", "null", "undefined", "true", "false"];
        if reserved.contains(&trimmed.to_lowercase().as_str()) {
            return Err(ValidationError::UsernameReserved);
        }
        
        Ok(())
    }

    pub fn validate_content(&self, content: &str) -> Result<(), ValidationError> {
        if content.contains("script>") || content.contains("javascript:") {
            return Err(ValidationError::ContainsScript);
        }
        
        for pattern in &self.email_patterns {
            if pattern.is_match(content) {
                return Err(ValidationError::ContainsEmail);
            }
        }
        
        for pattern in &self.phone_patterns {
            if pattern.is_match(content) {
                return Err(ValidationError::ContainsPhone);
            }
        }
        
        for pattern in &self.address_patterns {
            if pattern.is_match(content) {
                return Err(ValidationError::ContainsAddress);
            }
        }
        
        if content.to_lowercase().contains("social security") 
            || content.to_lowercase().contains("ssn")
            || content.to_lowercase().contains("passport number") {
            return Err(ValidationError::ContainsPII);
        }
        
        Ok(())
    }

    pub fn validate_link(&self, link: &SocialLink) -> Result<(), ValidationError> {
        if link.title.is_empty() {
            return Err(ValidationError::EmptyLinkTitle);
        }
        
        if link.title.len() > 50 {
            return Err(ValidationError::LinkTitleTooLong);
        }
        
        if !self.url_regex.is_match(&link.url) {
            return Err(ValidationError::InvalidUrl);
        }
        
        let blocked_domains = ["facebook.com", "twitter.com", "instagram.com", "tiktok.com"];
        let url_lower = link.url.to_lowercase();
        for domain in blocked_domains {
            if url_lower.contains(domain) {
                return Err(ValidationError::BlockedDomain);
            }
        }
        
        Ok(())
    }

    pub fn sanitize_html(&self, html: &str) -> String {
        let mut result = html.to_string();
        
        let dangerous_tags = ["script", "style", "iframe", "object", "embed", "form", "input", "button"];
        for tag in dangerous_tags {
            let pattern = format!(r"<{}[^>]*>.*?</{}>", tag, tag);
            if let Ok(re) = Regex::new(&pattern) {
                result = re.replace_all(&result, "").to_string();
            }
            
            let self_closing = format!(r"<{}[^>]*/?>", tag);
            if let Ok(re) = Regex::new(&self_closing) {
                result = re.replace_all(&result, "").to_string();
            }
        }
        
        let event_attrs = ["onclick", "onload", "onerror", "onmouseover", "onfocus", "onblur"];
        for attr in event_attrs {
            let pattern = format!(r"\s{}=", attr);
            if let Ok(re) = Regex::new(&pattern) {
                result = re.replace_all(&result, " data-blocked=").to_string();
            }
        }
        
        result
    }
}

pub struct PrivacyValidator;

impl PrivacyValidator {
    pub fn validate_privacy_settings(settings: &crate::myc_social::PrivacySettings) -> bool {
        match &settings.guestbook_policy {
            crate::myc_social::GuestbookPolicy::Closed => true,
            crate::myc_social::GuestbookPolicy::Open 
            | crate::myc_social::GuestbookPolicy::ApproveOnce 
            | crate::myc_social::GuestbookPolicy::Approval => {
                settings.profile_visibility != crate::layout::Visibility::Private 
                    || settings.follow_approval
            }
        }
    }
}

#[derive(Debug, thiserror::Error, Clone)]
pub enum ValidationError {
    #[error("Username cannot be empty")]
    EmptyUsername,
    
    #[error("Username too long (max 30 characters)")]
    UsernameTooLong,
    
    #[error("Username too short (min 3 characters)")]
    UsernameTooShort,
    
    #[error("Invalid username format (use letters, numbers, underscore, hyphen)")]
    InvalidUsernameFormat,
    
    #[error("This username is reserved")]
    UsernameReserved,
    
    #[error("Content contains prohibited script tags")]
    ContainsScript,
    
    #[error("Content appears to contain an email address")]
    ContainsEmail,
    
    #[error("Content appears to contain a phone number")]
    ContainsPhone,
    
    #[error("Content appears to contain a physical address")]
    ContainsAddress,
    
    #[error("Content contains potentially sensitive personal information")]
    ContainsPII,
    
    #[error("Link title cannot be empty")]
    EmptyLinkTitle,
    
    #[error("Link title too long (max 50 characters)")]
    LinkTitleTooLong,
    
    #[error("Invalid URL format")]
    InvalidUrl,
    
    #[error("This domain is not allowed")]
    BlockedDomain,
}

pub struct PIIPatterns;

impl PIIPatterns {
    pub fn contains_pii(content: &str) -> bool {
        let validator = ContentValidator::new();
        validator.validate_content(content).is_err()
    }

    pub fn contains_email(content: &str) -> bool {
        let pattern = Regex::new(r"[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}").unwrap();
        pattern.is_match(content)
    }

    pub fn contains_phone(content: &str) -> bool {
        let pattern = Regex::new(r"\b\d{3}[-.]?\d{3}[-.]?\d{4}\b").unwrap();
        pattern.is_match(content)
    }

    pub fn contains_ssn(content: &str) -> bool {
        let pattern = Regex::new(r"\b\d{3}-\d{2}-\d{4}\b").unwrap();
        pattern.is_match(content)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_username_validation() {
        let validator = ContentValidator::new();
        
        assert!(validator.validate_username("alice").is_ok());
        assert!(validator.validate_username("alice123").is_ok());
        assert!(validator.validate_username("alice_smith").is_ok());
        assert!(validator.validate_username("a").is_err());
        assert!(validator.validate_username("admin").is_err());
    }

    #[test]
    fn test_content_validation() {
        let validator = ContentValidator::new();
        
        assert!(validator.validate_content("Hello world!").is_ok());
        assert!(validator.validate_content("Check out https://example.com").is_ok());
        assert!(validator.validate_content("Contact me at test@email.com").is_err());
        assert!(validator.validate_content("Call me at 555-123-4567").is_err());
        assert!(validator.validate_content("<script>alert(1)</script>").is_err());
    }

    #[test]
    fn test_html_sanitization() {
        let validator = ContentValidator::new();
        
        let dirty = "<script>alert(1)</script><p>Safe text</p>";
        let clean = validator.sanitize_html(dirty);
        
        assert!(!clean.contains("<script>"));
        assert!(clean.contains("<p>"));
    }

    #[test]
    fn test_pii_detection() {
        assert!(PIIPatterns::contains_email("Email: test@example.com"));
        assert!(PIIPatterns::contains_phone("Phone: 555-123-4567"));
        assert!(PIIPatterns::contains_ssn("SSN: 123-45-6789"));
    }
}
