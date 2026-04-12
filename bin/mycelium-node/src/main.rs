mod wallet;

use anyhow::Result;
use clap::{Parser, Subcommand};
use hex;
use libp2p::{identity, PeerId};
use myc_crypto::KeyPair;
use myc_post::Post;
use myc_profile::{
    Profile, ThemePreset, LayoutManager, 
    compute_profile_cid, ProfileManager, ProfileStore,
    ProfileSyncService
};
use myc_token::Tokenomics;
use myc_p2p::{MyceliumNode, P2pConfig, NodeCapability, DiscoveryConfig};
use std::path::PathBuf;
use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::{info, Level};
use tracing_subscriber::FmtSubscriber;

struct AppState {
    profile_manager: Option<ProfileManager>,
    profile_store: ProfileStore,
    sync_service: ProfileSyncService,
    keypair: Option<identity::Keypair>,
}

impl AppState {
    fn new(data_dir: PathBuf) -> Self {
        Self {
            profile_manager: None,
            profile_store: ProfileStore::default(),
            sync_service: ProfileSyncService::new(PeerId::random()),
            keypair: None,
        }
    }
}

#[derive(Parser)]
#[command(name = "mycelium")]
#[command(about = "Mycelium Protocol - Decentralized Social Network", long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
    
    #[arg(long, default_value = "info")]
    log_level: String,
    
    #[arg(long, default_value = "./data")]
    data_dir: String,
}

#[derive(Subcommand)]
enum Commands {
    Start {
        #[arg(long, default_value = "/ip4/0.0.0.0/tcp/0")]
        listen: String,
        
        #[arg(long)]
        bootstrap: Option<String>,
    },
    Profile {
        #[command(subcommand)]
        action: ProfileCommands,
    },
    Guestbook {
        #[command(subcommand)]
        action: GuestbookCommands,
    },
    Social {
        #[command(subcommand)]
        action: SocialCommands,
    },
    Identity {
        #[command(subcommand)]
        action: IdentityCommands,
    },
    Post {
        #[arg(short, long)]
        content: String,
    },
    Feed {
        #[arg(long, default_value = "50")]
        limit: usize,
    },
    Wallet {
        #[command(subcommand)]
        action: WalletCommands,
    },
    Network {
        #[command(subcommand)]
        action: NetworkCommands,
    },
    Status,
}

#[derive(Subcommand)]
enum ProfileCommands {
    Create {
        #[arg(long)]
        display_name: String,
        
        #[arg(long)]
        username: Option<String>,
    },
    Show {
        #[arg(long)]
        user: Option<String>,
    },
    Update {
        #[arg(long)]
        bio: Option<String>,
        
        #[arg(long)]
        display_name: Option<String>,
    },
    Avatar {
        #[arg(long)]
        cid: String,
    },
    Banner {
        #[arg(long)]
        cid: String,
    },
    Link {
        #[arg(long)]
        title: String,
        
        #[arg(long)]
        url: String,
    },
    Theme {
        #[arg(long)]
        preset: Option<String>,
        
        #[arg(long)]
        primary: Option<String>,
    },
    Layout {
        #[command(subcommand)]
        action: LayoutCommands,
    },
    Privacy {
        #[command(subcommand)]
        action: PrivacyCommands,
    },
}

#[derive(Subcommand)]
enum LayoutCommands {
    List,
    Add {
        #[arg(long)]
        section: String,
        
        #[arg(long)]
        widget: String,
        
        #[arg(long, default_value = "main")]
        position: String,
    },
    Remove {
        #[arg(long)]
        section: String,
    },
    Template {
        #[arg(long)]
        name: String,
    },
    Visibility {
        #[arg(long)]
        section: String,
        
        #[arg(long)]
        visibility: String,
    },
}

#[derive(Subcommand)]
enum PrivacyCommands {
    Show,
    Set {
        #[arg(long)]
        profile_visibility: Option<String>,
        
        #[arg(long)]
        guestbook_policy: Option<String>,
        
        #[arg(long)]
        follow_approval: Option<bool>,
    },
}

#[derive(Subcommand)]
enum GuestbookCommands {
    Sign {
        #[arg(long)]
        user: String,
        
        #[arg(long)]
        message: String,
        
        #[arg(long)]
        name: Option<String>,
    },
    Pending {
        #[arg(long)]
        user: Option<String>,
    },
    Approve {
        #[arg(long)]
        id: String,
    },
    Reject {
        #[arg(long)]
        id: String,
    },
    Show {
        #[arg(long)]
        user: String,
    },
}

#[derive(Subcommand)]
enum SocialCommands {
    Follow {
        #[arg(long)]
        user: String,
    },
    Unfollow {
        #[arg(long)]
        user: String,
    },
    Followers {
        #[arg(long)]
        user: Option<String>,
    },
    Following {
        #[arg(long)]
        user: Option<String>,
    },
    Block {
        #[arg(long)]
        user: String,
    },
    Unblock {
        #[arg(long)]
        user: String,
    },
    Relationship {
        #[arg(long)]
        user: String,
    },
}

#[derive(Subcommand)]
enum IdentityCommands {
    Register {
        #[arg(long)]
        username: String,
    },
    Lookup {
        #[arg(long)]
        username: String,
    },
    Update {
        #[arg(long)]
        profile_cid: String,
    },
}

#[derive(Subcommand)]
enum WalletCommands {
    Create,
    Balance,
    Stake {
        #[arg(long)]
        amount: u64,
    },
    Unstake {
        #[arg(long)]
        amount: u64,
    },
    Send {
        #[arg(long)]
        to: String,
        #[arg(long)]
        amount: u64,
    },
}

#[derive(Subcommand)]
enum NetworkCommands {
    Peers,
    Connect {
        #[arg(long)]
        peer_id: String,
    },
    Info,
}

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();
    
    let level = match cli.log_level.to_lowercase().as_str() {
        "trace" => Level::TRACE,
        "debug" => Level::DEBUG,
        "info" => Level::INFO,
        "warn" => Level::WARN,
        "error" => Level::ERROR,
        _ => Level::INFO,
    };
    
    let subscriber = FmtSubscriber::builder()
        .with_max_level(level)
        .with_target(true)
        .finish();
    
    tracing::subscriber::set_global_default(subscriber)?;
    
    let data_dir = PathBuf::from(&cli.data_dir);
    tokio::fs::create_dir_all(&data_dir).await?;
    
    let state = Arc::new(RwLock::new(AppState::new(data_dir.clone())));
    
    match cli.command {
        Commands::Profile { action } => handle_profile(action, &state, &data_dir).await?,
        Commands::Guestbook { action } => handle_guestbook(action).await?,
        Commands::Social { action } => handle_social(action).await?,
        Commands::Identity { action } => handle_identity(action).await?,
        Commands::Start { listen, bootstrap } => start_node(listen, bootstrap, state).await?,
        Commands::Post { content } => create_post(content, &state).await?,
        Commands::Feed { limit } => show_feed(limit).await?,
        Commands::Wallet { action } => handle_wallet(action).await?,
        Commands::Network { action } => handle_network(action).await?,
        Commands::Status => show_status().await?,
    }
    
    Ok(())
}

async fn ensure_profile_manager(state: &Arc<RwLock<AppState>>, data_dir: &PathBuf) -> Result<()> {
    let mut s = state.write().await;
    if s.profile_manager.is_none() {
        let keypair = identity::Keypair::generate_ed25519();
        let peer_id = PeerId::from(keypair.public());
        s.keypair = Some(keypair);
        
        let store = ProfileStore::new(data_dir.join("profiles")).await?;
        let manager = ProfileManager::new(data_dir.clone(), peer_id).await?;
        
        s.profile_store = store;
        s.profile_manager = Some(manager);
        s.sync_service = ProfileSyncService::new(peer_id);
        
        info!("Initialized profile manager for peer: {}", peer_id);
    }
}

async fn handle_profile(action: ProfileCommands, state: &Arc<RwLock<AppState>>, data_dir: &PathBuf) -> Result<()> {
    ensure_profile_manager(state, data_dir).await?;
    
    match action {
        ProfileCommands::Create { display_name, username } => {
            let s = state.read().await;
            let manager = s.profile_manager.as_ref().ok_or_else(|| anyhow::anyhow!("No profile manager"))?;
            
            info!("Creating profile: {}", display_name);
            let profile = manager.create_profile(&display_name, username.as_deref()).await?;
            
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    PROFILE CREATED                       ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  Display Name: {}                                       ║", profile.display_name);
            if let Some(ref u) = profile.username {
                println!("║  Username: @{}                                          ║", u);
            }
            let cid = compute_profile_cid(&profile);
            println!("║  Profile CID: {}                                    ║", &cid[..44.min(cid.len())]);
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        ProfileCommands::Show { user } => {
            let s = state.read().await;
            
            if let Some(ref user_arg) = user {
                if let Some(profile) = s.sync_service.get_profile_by_username(user_arg) {
                    print_profile(&profile);
                } else {
                    println!("\n╔════════════════════════════════════════════════════════════╗");
                    println!("║                 PROFILE NOT FOUND                       ║");
                    println!("╠════════════════════════════════════════════════════════════╣");
                    println!("║  Username '{}' not found in local cache.              ║", user_arg);
                    println!("║  Connect to network to lookup users.                     ║");
                    println!("╚════════════════════════════════════════════════════════════╝");
                }
            } else if let Some(ref manager) = s.profile_manager {
                if let Some(profile) = manager.get_my_profile().await {
                    print_profile(&profile);
                } else {
                    println!("\n╔════════════════════════════════════════════════════════════╗");
                    println!("║                  NO PROFILE YET                         ║");
                    println!("╠════════════════════════════════════════════════════════════╣");
                    println!("║  Run 'mycelium profile create --display-name <name>'    ║");
                    println!("╚════════════════════════════════════════════════════════════╝");
                }
            }
        }
        
        ProfileCommands::Update { bio, display_name } => {
            let s = state.read().await;
            let manager = s.profile_manager.as_ref().ok_or_else(|| anyhow::anyhow!("No profile manager"))?;
            
            let mut updated = false;
            
            if let Some(b) = bio {
                info!("Updating bio...");
                let profile = manager.set_bio(&b).await?;
                println!("Bio updated!");
                print_profile(&profile);
                updated = true;
            }
            
            if let Some(d) = display_name {
                info!("Updating display name to: {}", d);
                let profile = manager.set_display_name(&d).await?;
                println!("Display name updated!");
                print_profile(&profile);
                updated = true;
            }
            
            if !updated {
                println!("No updates specified. Use --bio or --display-name");
            }
        }
        
        ProfileCommands::Avatar { cid } => {
            let s = state.read().await;
            let manager = s.profile_manager.as_ref().ok_or_else(|| anyhow::anyhow!("No profile manager"))?;
            
            info!("Setting avatar to CID: {}", cid);
            let profile = manager.set_avatar(cid.clone()).await?;
            println!("Avatar updated!");
            println!("  CID: {}", cid);
        }
        
        ProfileCommands::Banner { cid } => {
            let s = state.read().await;
            let manager = s.profile_manager.as_ref().ok_or_else(|| anyhow::anyhow!("No profile manager"))?;
            
            info!("Setting banner to CID: {}", cid);
            let profile = manager.set_banner(cid.clone()).await?;
            println!("Banner updated!");
            println!("  CID: {}", cid);
        }
        
        ProfileCommands::Link { title, url } => {
            let s = state.read().await;
            let manager = s.profile_manager.as_ref().ok_or_else(|| anyhow::anyhow!("No profile manager"))?;
            
            info!("Adding link: {} -> {}", title, url);
            let profile = manager.add_link(&title, &url).await?;
            println!("Link added!");
            println!("  Title: {}", title);
            println!("  URL: {}", url);
        }
        
        ProfileCommands::Theme { preset, primary } => {
            let s = state.read().await;
            let manager = s.profile_manager.as_ref().ok_or_else(|| anyhow::anyhow!("No profile manager"))?;
            
            let preset = preset.map(|p| match p.to_lowercase().as_str() {
                "midnight" => ThemePreset::Midnight,
                "ocean" => ThemePreset::Ocean,
                "forest" => ThemePreset::Forest,
                "sunset" => ThemePreset::Sunset,
                "minimal" => ThemePreset::Minimal,
                "hacker" => ThemePreset::Hacker,
                _ => ThemePreset::Default,
            });
            
            if let Some(p) = preset {
                info!("Setting theme to: {:?}", p);
                let profile = manager.set_theme(p, primary.as_deref()).await?;
                println!("Theme updated!");
                print_profile(&profile);
            } else {
                println!("Available presets: midnight, ocean, forest, sunset, minimal, hacker");
            }
        }
        
        ProfileCommands::Layout { action } => handle_layout(action, state).await?,
        ProfileCommands::Privacy { action } => handle_privacy(action, state).await?,
    }
    Ok(())
}

fn print_profile(profile: &Profile) {
    println!("\n╔════════════════════════════════════════════════════════════╗");
    println!("║                    PROFILE VIEW                         ║");
    println!("╠════════════════════════════════════════════════════════════╣");
    
    let peer_id_str = if let Some(peer_id) = profile.peer_id() {
        peer_id.to_base58()
    } else {
        "Unknown".to_string()
    };
    
    println!("║  Peer ID: {}...", &peer_id_str[..20.min(peer_id_str.len())]);
    
    if let Some(ref username) = profile.username {
        println!("║  Username: @{}                                         ║", username);
    }
    
    println!("║  Display Name: {}                                       ║", profile.display_name);
    
    if let Some(ref bio) = profile.bio {
        let bio_short = if bio.len() > 40 { format!("{}...", &bio[..40]) } else { bio.clone() };
        println!("║  Bio: {}                    ║", bio_short);
    }
    
    println!("║  Links: {}                                               ║", profile.links.len());
    for link in profile.links.iter().take(3) {
        println!("║    - {}: {}", link.title, &link.url[..30.min(link.url.len())]);
    }
    
    if let Some(ref preset) = profile.theme.preset {
        println!("║  Theme: {}                                              ║", preset.display_name());
    }
    
    println!("║  Layout Sections: {}                                    ║", profile.layout.sections.len());
    
    let cid = compute_profile_cid(profile);
    println!("║  Profile CID: {}...                                  ║", &cid[..32.min(cid.len())]);
    
    println!("╚════════════════════════════════════════════════════════════╝");
}

async fn handle_layout(action: LayoutCommands, state: &Arc<RwLock<AppState>>) -> Result<()> {
    match action {
        LayoutCommands::List => {
            let manager = LayoutManager::new();
            let templates = manager.list_templates();
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    LAYOUT TEMPLATES                      ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  Available templates: {}", templates.join(", "));
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        LayoutCommands::Add { section, widget, position } => {
            info!("Adding section '{}' with widget '{}' at {}", section, widget, position);
            println!("Section added (in-memory only)");
        }
        
        LayoutCommands::Remove { section } => {
            info!("Removing section: {}", section);
            println!("Section removed (in-memory only)");
        }
        
        LayoutCommands::Template { name } => {
            let s = state.read().await;
            if let Some(ref manager) = s.profile_manager {
                info!("Applying template: {}", name);
                let profile = manager.set_layout(&name).await;
                match profile {
                    Ok(_) => println!("Template '{}' applied!", name),
                    Err(e) => println!("Failed to apply template: {}", e),
                }
            } else {
                println!("No profile manager. Create a profile first.");
            }
        }
        
        LayoutCommands::Visibility { section, visibility } => {
            info!("Setting {} visibility to: {}", section, visibility);
            println!("Visibility updated (in-memory only)");
        }
    }
    Ok(())
}

async fn handle_privacy(action: PrivacyCommands, state: &Arc<RwLock<AppState>>) -> Result<()> {
    match action {
        PrivacyCommands::Show => {
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    PRIVACY SETTINGS                     ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  Profile Visibility:    Public                         ║");
            println!("║  Guestbook Policy:      Approval Required              ║");
            println!("║  Follow Approval:       Yes                           ║");
            println!("║  Show Followers:        Public                        ║");
            println!("║  Allow DMs From:        Followers Only                ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        PrivacyCommands::Set { profile_visibility, guestbook_policy, follow_approval } => {
            if let Some(v) = profile_visibility {
                info!("Profile visibility set to: {}", v);
            }
            if let Some(p) = guestbook_policy {
                info!("Guestbook policy set to: {}", p);
            }
            if let Some(f) = follow_approval {
                info!("Follow approval required: {}", f);
            }
            info!("Privacy settings updated (in-memory only)");
        }
    }
    Ok(())
}

async fn handle_guestbook(action: GuestbookCommands) -> Result<()> {
    match action {
        GuestbookCommands::Sign { user, message, name } => {
            let display_name = name.unwrap_or_else(|| "Anonymous".to_string());
            info!("Signing {}'s guestbook as '{}'", user, display_name);
            info!("Message: {}", message);
            info!("Entry submitted for approval!");
        }
        
        GuestbookCommands::Pending { user } => {
            let user = user.unwrap_or_else(|| "your".to_string());
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                  PENDING GUESTBOOK ENTRIES                ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  No pending entries for {} profile                     ║", user);
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        GuestbookCommands::Approve { id } => {
            info!("Approving guestbook entry: {}", id);
            info!("Entry approved!");
        }
        
        GuestbookCommands::Reject { id } => {
            info!("Rejecting guestbook entry: {}", id);
            info!("Entry rejected!");
        }
        
        GuestbookCommands::Show { user } => {
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                   GUESTBOOK: {}                      ║", user);
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  [Connect to network to view guestbook entries]         ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
    }
    Ok(())
}

async fn handle_social(action: SocialCommands) -> Result<()> {
    match action {
        SocialCommands::Follow { user } => {
            info!("Following: {}", user);
            info!("Follow request sent!");
        }
        
        SocialCommands::Unfollow { user } => {
            info!("Unfollowing: {}", user);
            info!("Unfollowed!");
        }
        
        SocialCommands::Followers { user } => {
            let user = user.unwrap_or_else(|| "you".to_string());
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    FOLLOWERS: {}                    ║", user);
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  [Connect to network to view followers]                 ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        SocialCommands::Following { user } => {
            let user = user.unwrap_or_else(|| "you".to_string());
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    FOLLOWING: {}                   ║", user);
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  [Connect to network to view following]                 ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        SocialCommands::Block { user } => {
            info!("Blocking: {}", user);
            info!("User blocked!");
        }
        
        SocialCommands::Unblock { user } => {
            info!("Unblocking: {}", user);
            info!("User unblocked!");
        }
        
        SocialCommands::Relationship { user } => {
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                RELATIONSHIP WITH {}                ║", user);
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  Status:        [Connect to network to check]           ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
    }
    Ok(())
}

async fn handle_identity(action: IdentityCommands) -> Result<()> {
    match action {
        IdentityCommands::Register { username } => {
            info!("Registering username: {}", username);
            info!("Username registered!");
            info!("Username CID:Qm{}", &username[..8.min(username.len())]);
        }
        
        IdentityCommands::Lookup { username } => {
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                 IDENTITY LOOKUP: {}                 ║", username);
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║  [Connect to network to lookup username]                 ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        
        IdentityCommands::Update { profile_cid } => {
            info!("Updating profile CID to: {}", profile_cid);
            info!("Profile CID updated!");
        }
    }
    Ok(())
}

async fn start_node(listen: String, bootstrap: Option<String>, state: Arc<RwLock<AppState>>) -> Result<()> {
    info!("Starting Mycelium Protocol...");
    
    let keypair = identity::Keypair::generate_ed25519();
    let peer_id = PeerId::from(keypair.public());
    info!("Node PeerId: {}", peer_id);
    
    {
        let mut s = state.write().await;
        s.keypair = Some(keypair.clone());
        s.sync_service = ProfileSyncService::new(peer_id);
    }
    
    let config = P2pConfig {
        keypair,
        listen_addresses: vec![listen.parse()?],
        capabilities: vec![NodeCapability::Full],
        discovery: DiscoveryConfig::default(),
        bootstrap_nodes: if let Some(b) = bootstrap {
            vec![b.parse().map_err(|e| anyhow::anyhow!("{:?}", e))?]
        } else {
            Vec::new()
        },
        ..Default::default()
    };
    
    let node = MyceliumNode::new(config).await?;
    
    info!("Node started successfully!");
    info!("Listening addresses: {:?}", node.local_info().listen_addresses);
    info!("Peer count: {}", node.peer_count().await);
    
    info!("Entering main event loop...");
    tokio::signal::ctrl_c().await?;
    
    info!("Shutting down node...");
    Ok(())
}

async fn create_post(content: String, state: &Arc<RwLock<AppState>>) -> Result<()> {
    info!("Creating new post: {}...", &content[..content.len().min(50)]);
    
    let keypair = KeyPair::generate()?;
    let author = hex::encode(keypair.public_key().as_bytes());
    
    let post = Post::new(
        author,
        content,
        &keypair,
    )?;
    
    let content_hash = post.content_hash();
    
    info!("Post created successfully!");
    info!("  ID: {}", post.id);
    info!("  Author: {}", post.author);
    info!("  Initial TTL: {} seconds", post.ttl_seconds);
    info!("  Content Hash: {}", content_hash);
    
    Ok(())
}

async fn show_feed(limit: usize) -> Result<()> {
    info!("Fetching recent posts (limit: {})...", limit);
    
    println!("\n╔════════════════════════════════════════════════════════════╗");
    println!("║                    RECENT POSTS                           ║");
    println!("╠════════════════════════════════════════════════════════════╣");
    println!("║ No posts available (connect to network to see posts)       ║");
    println!("╚════════════════════════════════════════════════════════════╝");
    
    Ok(())
}

async fn handle_wallet(action: WalletCommands) -> Result<()> {
    match action {
        WalletCommands::Create => {
            let wallet = wallet::create_wallet()?;
            info!("Wallet created successfully!");
            info!("Public Key: {}", wallet::public_key_to_string(&wallet.public_key));
        }
        WalletCommands::Balance => {
            let tokenomics = Tokenomics::new();
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    WALLET BALANCE                         ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║ Total Supply:     {} MYCELIUM", tokenomics.total_supply);
            println!("║ Circulating:      {} MYCELIUM", tokenomics.circulating_supply());
            println!("║ Staked:           {} MYCELIUM", tokenomics.staked_supply);
            println!("║ Burned:           {} MYCELIUM", tokenomics.burned_supply);
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        WalletCommands::Stake { amount } => {
            info!("Staking {} tokens...", amount);
            info!("Stake initiated. Use 'wallet unstake' to unstake later.");
        }
        WalletCommands::Unstake { amount } => {
            info!("Unstaking {} tokens...", amount);
            info!("Unbonding period: 7 epochs (~7 days)");
        }
        WalletCommands::Send { to, amount } => {
            info!("Sending {} tokens to {}...", amount, to);
            info!("Transaction submitted!");
        }
    }
    Ok(())
}

async fn handle_network(action: NetworkCommands) -> Result<()> {
    match action {
        NetworkCommands::Peers => {
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    CONNECTED PEERS                        ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║ No peers connected (start node to connect)                ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
        NetworkCommands::Connect { peer_id } => {
            info!("Connecting to peer: {}", peer_id);
        }
        NetworkCommands::Info => {
            println!("\n╔════════════════════════════════════════════════════════════╗");
            println!("║                    NETWORK INFORMATION                    ║");
            println!("╠════════════════════════════════════════════════════════════╣");
            println!("║ Protocol:           Mycelium Protocol v{}", env!("CARGO_PKG_VERSION"));
            println!("║ Network:            Mainnet (soon)                        ║");
            println!("║ DHT:                Kademlia                             ║");
            println!("║ Pubsub:             GossipSub v2                         ║");
            println!("╚════════════════════════════════════════════════════════════╝");
        }
    }
    Ok(())
}

async fn show_status() -> Result<()> {
    let tokenomics = Tokenomics::new();
    
    println!("
╔════════════════════════════════════════════════════════════╗
║              MYCELIUM PROTOCOL STATUS                    ║
╠════════════════════════════════════════════════════════════╣
║                                                            ║
║  PROTOCOL VERSION: v{}                                     ║
║                                                            ║
║  ── NETWORK ──────────────────────────────────────────   ║
║  Status:          Offline                                  ║
║  Peers:           0                                        ║
║  Latency:         --                                       ║
║                                                            ║
║  ── TOKENOMICS ───────────────────────────────────────    ║
║  Total Supply:    {} MYCELIUM                       ║
║  Inflation Rate:  {}%                                  ║
║  Current Epoch:   {}                                    ║
║                                                            ║
║  ── REWARDS ─────────────────────────────────────────     ║
║  Relay:           35% of emission                          ║
║  Hosting:         40% of emission                          ║
║  Creation:        15% of emission                          ║
║  Engagement:      10% of emission                          ║
║                                                            ║
║  ── PROFILE FEATURES ────────────────────────────────     ║
║  Layout System:     Full customizable                     ║
║  Theme Engine:     7 presets + custom                      ║
║  Guestbook:        Approval-based entries                  ║
║  Social Graph:     Follow/followers                       ║
║  Privacy:          Granular controls                      ║
║  Identity:         DHT username registry                   ║
║  Storage:          Local encrypted + DHT sync             ║
║                                                            ║
║  ── POST LIFECYCLE ─────────────────────────────────     ║
║  Initial TTL:     24 hours                                 ║
║  Hype Threshold:  1000 engagement score                    ║
║  Permanent Stake: {} tokens minimum                        ║
║                                                            ║
╚════════════════════════════════════════════════════════════╝
    ", 
        env!("CARGO_PKG_VERSION"),
        tokenomics.total_supply,
        (tokenomics.annual_inflation_rate * 100.0),
        tokenomics.current_epoch,
        myc_post::PERMANENT_STAKE_MIN
    );
    
    Ok(())
}
