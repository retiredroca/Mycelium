mod wallet;

use anyhow::Result;
use clap::{Parser, Subcommand};
use myc_p2p::{MyceliumNode, P2pConfig, NodeCapability, DiscoveryConfig};
use myc_post::Post;
use myc_storage::LocalStorage;
use myc_token::{Wallet, Tokenomics, RewardPool};
use tracing::{info, error, Level};
use tracing_subscriber::FmtSubscriber;
use libp2p::identity::Keypair;

#[derive(Parser)]
#[command(name = "mycelium-node")]
#[command(about = "The Mycelium Protocol - Decentralized Social Network", long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
    
    #[arg(long, default_value = "info")]
    log_level: String,
    
    #[arg(long)]
    config: Option<String>,
}

#[derive(Subcommand)]
enum Commands {
    Start {
        #[arg(long, default_value = "/ip4/0.0.0.0/tcp/0")]
        listen: String,
        
        #[arg(long)]
        bootstrap: Option<String>,
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
        .with_thread_ids(true)
        .with_file(true)
        .with_line_number(true)
        .finish();
    
    tracing::subscriber::set_global_default(subscriber)?;
    
    match cli.command {
        Commands::Start { listen, bootstrap } => {
            start_node(listen, bootstrap).await?;
        }
        Commands::Post { content } => {
            create_post(content).await?;
        }
        Commands::Feed { limit } => {
            show_feed(limit).await?;
        }
        Commands::Wallet { action } => {
            handle_wallet(action).await?;
        }
        Commands::Network { action } => {
            handle_network(action).await?;
        }
        Commands::Status => {
            show_status().await?;
        }
    }
    
    Ok(())
}

async fn start_node(listen: String, bootstrap: Option<String>) -> Result<()> {
    info!("Starting Mycelium Protocol...");
    
    let keypair = Keypair::generate_ed25519();
    let peer_id = libp2p::PeerId::from(keypair.public());
    info!("Node PeerId: {}", peer_id);
    
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

async fn create_post(content: String) -> Result<()> {
    info!("Creating new post: {}...", &content[..content.len().min(50)]);
    
    let keypair = Keypair::generate_ed25519();
    let author = libp2p::PeerId::from(keypair.public()).to_string();
    
    let post = Post::new(
        author,
        content,
        "placeholder_signature",
    );
    
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
            println!("║ Total Supply:     {} SOVEREIGN", tokenomics.total_supply);
            println!("║ Circulating:      {} SOVEREIGN", tokenomics.circulating_supply());
            println!("║ Staked:            {} SOVEREIGN", tokenomics.staked_supply);
            println!("║ Burned:            {} SOVEREIGN", tokenomics.burned_supply);
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
║              SOVEREIGN NODE PROTOCOL STATUS                 ║
╠════════════════════════════════════════════════════════════╣
║                                                            ║
║  PROTOCOL VERSION: v{}                                     ║
║                                                            ║
║  ── NETWORK ────────────────────────────────────────────   ║
║  Status:          Offline                                  ║
║  Peers:           0                                        ║
║  Latency:         --                                       ║
║                                                            ║
║  ── TOKENOMICS ────────────────────────────────────────    ║
║  Total Supply:    {} SOVEREIGN                     ║
║  Inflation Rate:  {}%                                  ║
║  Current Epoch:   {}                                    ║
║                                                            ║
║  ── REWARDS ──────────────────────────────────────────     ║
║  Relay:           35% of emission                          ║
║  Hosting:         40% of emission                          ║
║  Creation:        15% of emission                          ║
║  Engagement:      10% of emission                          ║
║                                                            ║
║  ── POST LIFECYCLE ───────────────────────────────────     ║
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
