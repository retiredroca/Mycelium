#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include "myc_crypto.hpp"
#include "storage/myc_storage.hpp"

// ============================================================
// BIP-39 English wordlist (2048 words)
// ============================================================
static inline const char* kWordlist[2048] = {
    "abandon","ability","able","about","above","absent","absorb","abstract","absurd","accident",
    "account","accuse","achieve","acid","acoustic","acquire","across","act","action","actor",
    "actress","actual","adapt","add","addict","address","adjust","admit","adult","advance",
    "advice","aerobic","affair","afford","afraid","again","age","agent","agree","ahead",
    "aim","air","airport","aisle","alarm","album","alcohol","alert","alien","all","alley",
    "allow","almost","alone","alpha","already","also","alter","always","amazing","among",
    "amount","amused","analyst","anchor","ancient","anger","angle","angry","animal","ankle",
    "announce","annual","another","answer","antenna","antique","anxiety","any","apart","apology",
    "appear","apple","approve","april","arch","arctic","area","arena","argue","arm","armed",
    "armor","army","around","arrange","arrest","arrive","arrow","art","artifact","artist",
    "artwork","ask","aspect","assault","asset","assist","assume","asthma","athlete","atom",
    "attack","attend","attitude","attract","auction","audit","august","aunt","author","auto",
    "autumn","average","avocado","avoid","awake","aware","away","awesome","awful","awkward",
    "axis","baby","bachelor","bacon","badge","bag","balance","balcony","ball","bamboo","banana",
    "banner","bar","barely","bargain","barrel","base","basic","basket","battle","beach","bean",
    "beauty","because","become","beef","before","begin","behave","behind","believe","below",
    "belt","bench","benefit","best","betray","better","between","beyond","bicycle","bid","bike",
    "bind","biology","bird","birth","bitter","black","blame","blanket","blast","bleak","bless",
    "blind","blood","blossom","blouse","blue","blur","blush","board","boat","body","boil","bomb",
    "bone","bonus","book","boost","border","boring","borrow","boss","bottom","bounce","box","boy",
    "bracket","brain","brand","brass","brave","bread","breeze","brick","bridge","brief","bright",
    "bring","brisk","broccoli","broken","bronze","broom","brother","brown","brush","bubble",
    "buddy","budget","buffalo","build","bulb","bulk","bullet","bundle","bunker","burden","burger",
    "burst","bus","business","busy","butter","buyer","buzz","cabbage","cabin","cable","cactus",
    "cage","cake","call","calm","camera","camp","can","canal","cancel","candy","cannon","canoe",
    "canvas","canyon","capable","capital","captain","car","carbon","card","cargo","carpet",
    "carry","cart","case","cash","casino","castle","casual","cat","catalog","catch","category",
    "cattle","caught","cause","caution","cave","ceiling","celery","cement","census","century",
    "ceremony","certain","chair","chalk","champion","change","chaos","chapter","charge","chase",
    "chat","cheap","check","cheek","cheese","chef","cherry","chest","chicken","chief","child",
    "chimney","choice","choose","chronic","chuckle","churn","cigar","cinnamon","circle","citizen",
    "city","civil","claim","clap","clarify","claw","clay","clean","clerk","clever","click",
    "client","cliff","climb","clinic","clip","clock","clogs","close","cloth","cloud","clown",
    "club","clump","cluster","clutch","coach","coast","coconut","code","coffee","coil","coin",
    "collect","color","column","combine","come","comfort","comic","common","company","concert",
    "conduct","confirm","congress","connect","consider","control","convince","cook","cool","copper",
    "copy","coral","core","corn","correct","cost","cotton","couch","country","couple","course",
    "cousin","cover","coyote","crack","cradle","craft","cram","crane","crash","crater","crawl",
    "crazy","cream","credit","creek","crew","cricket","crime","crisp","critic","crop","cross",
    "crouch","crowd","crucial","cruel","cruise","crumble","crunch","crush","cry","crystal","cube",
    "culture","cup","cupboard","curious","current","curtain","curve","cushion","custom","cute",
    "cycle","dad","damage","damp","dance","danger","daring","dark","dash","date","daughter","dawn",
    "day","deal","debate","debris","decade","december","decent","decide","deep","deer","defense",
    "define","defy","degree","delay","deliver","demand","demise","denial","dentist","deny","depart",
    "depend","deposit","depth","deputy","derive","describe","desert","design","desk","despair",
    "destroy","detail","detect","develop","device","devote","diagram","dial","diamond","diary",
    "dice","diesel","diet","differ","digital","dignity","dilemma","dinner","dinosaur","direct",
    "dirt","disagree","discover","disease","dish","dismiss","disorder","display","distance",
    "divert","divide","divorce","dizzy","doctor","document","dog","doll","dolphin","domain",
    "donate","donkey","donor","door","dose","double","dove","draft","dragon","drama","drastic",
    "draw","dream","dress","draft","dragon","drama","drastic","draw","dream","dress",
    "drift","drill","drink","drip","drive","drop","drum","dry","duck","dumb","dune","during",
    "dust","dutch","duty","dwarf","dynamic","eager","eagle","early","earn","earth","easily",
    "east","easy","echo","ecology","economy","edge","edit","educate","effort","egg","eight",
    "either","elbow","elder","electric","elegant","element","elephant","elevator","elite","else",
    "embark","embody","embrace","emerge","emotion","employ","empower","empty","enable","enact",
    "end","endless","endorse","enemy","energy","enforce","engage","engine","enhance","enjoy",
    "enlist","enough","enrich","enroll","ensure","enter","entire","entry","envelope","episode",
    "equal","equip","era","erase","erode","erosion","error","erupt","escape","essay","essence",
    "estate","eternal","ethics","evidence","evil","evoke","evolve","exact","example","exceed",
    "excel","exception","excess","exchange","excite","exclude","excuse","execute","exercise",
    "exhaust","exhibit","exile","exist","exit","exotic","expand","expect","expire","explain",
    "expose","express","extend","extra","eye","eyebrow","fabric","face","faculty","fade","faint",
    "faith","fall","false","fame","family","famous","fan","fancy","fantasy","farm","fashion",
    "fat","fatal","father","fatigue","fault","favorite","feature","february","federal","fee",
    "feed","feel","female","fence","festival","fetch","fever","few","fiber","fiction","field",
    "figure","file","film","filter","final","find","fine","finger","finish","fire","firm",
    "first","fiscal","fish","fit","fitness","fix","flag","flame","flash","flat","flavor","flee",
    "flight","flip","float","flock","floor","flower","fluid","flush","fly","foam","focus","fog",
    "foil","fold","follow","food","foot","force","foreign","forest","forget","fork","fortune",
    "forum","forward","fossil","foster","found","fox","fragile","frame","frequent","fresh",
    "friend","fringe","frog","front","frost","frown","frozen","fruit","fuel","fun","funny",
    "furnace","fury","future","gadget","gain","galaxy","gallery","game","gap","garage","garbage",
    "garden","garlic","garment","gas","gasp","gate","gather","gauge","gaze","general","genius",
    "genre","gentle","genuine","gesture","ghost","giant","gift","giggle","ginger","giraffe",
    "girl","give","glad","glance","glare","glass","glide","glimpse","globe","gloom","glory",
    "glove","glow","glue","goat","goddess","gold","good","goose","gorilla","gospel","gossip",
    "govern","gown","grab","grace","grain","grant","grape","grass","gravity","great","green",
    "grid","grief","grit","grocery","group","grow","grunt","guard","guess","guide","guilt",
    "guitar","gun","gym","habit","hair","half","hammer","hamster","hand","happy","harbor","hard",
    "harsh","harvest","hat","have","hawk","hazard","head","health","heart","heavy","hedgehog",
    "height","hello","helmet","help","hen","hero","hidden","high","hill","hint","hip","hire",
    "history","hobby","hockey","hold","hole","holiday","hollow","home","honey","hood","hope",
    "horn","horror","horse","hospital","host","hotel","hour","hover","hub","human","humble",
    "humor","hundred","hungry","hunt","hurdle","hurry","hurt","husband","hybrid","ice","icon",
    "idea","identify","idle","ignore","ill","illegal","illness","image","imitate","immense",
    "immune","impact","impose","improve","impulse","inch","include","income","increase","index",
    "indicate","indoor","industry","infant","inflict","inform","inhale","inherit","initial",
    "inject","injury","inmate","inner","innocent","input","inquiry","insane","insect","inside",
    "inspire","install","intact","interest","into","invest","invite","involve","iron","island",
    "isolate","issue","item","ivory","jacket","job","join","joke","journey","joy","judge","juice",
    "jump","jungle","junior","junk","just","kangaroo","keen","keep","ketchup","key","kick","kid",
    "kidney","kind","kingdom","kiss","kit","kitchen","kite","kitten","kiwi","knee","knife","knock",
    "know","lab","label","labor","ladder","lady","lake","lamp","language","laptop","large","later",
    "latin","laugh","laundry","lava","law","lawn","lawsuit","layer","lazy","leader","leaf","learn",
    "leave","lecture","left","leg","legal","legend","leisure","lemon","lend","length","lens",
    "leopard","lesson","letter","level","liar","liberty","library","license","life","lift","light",
    "like","limb","limit","link","lion","liquid","list","little","live","lizard","load","loan",
    "lobster","local","lock","logic","lonely","long","loop","lottery","loud","lounge","love",
    "loyal","lucky","luggage","lumber","lunar","lunch","luxury","lyrics","machine","mad","magic",
    "magnet","maid","mail","main","major","make","mammal","man","manage","mandate","mango","mansion",
    "manual","maple","marble","march","margin","marine","market","marriage","mask","mass","master",
    "match","material","math","matrix","matter","maximum","maze","meadow","mean","measure","meat",
    "mechanic","medal","media","melody","melt","member","memory","mention","menu","mercy","merge",
    "merit","merry","mesh","message","metal","method","middle","midnight","milk","million","mimic",
    "mind","minimum","minor","minute","miracle","mirror","misery","miss","mistake","mix","mixed",
    "mixture","mobile","model","modify","mom","moment","monitor","monkey","monster","month","moon",
    "moral","more","morning","mosquito","mother","motion","motor","mountain","mouse","move","movie",
    "much","muffin","mule","multiply","muscle","museum","mushroom","music","must","mutual","myself",
    "mystery","myth","naive","name","napkin","narrow","nasty","nation","nature","near","neck",
    "need","negative","neglect","neither","nephew","nerve","nest","net","network","neutral","never",
    "news","next","nice","night","noble","noise","nominee","noodle","normal","north","nose",
    "notable","note","nothing","notice","novel","now","nuclear","number","nurse","nut","oak","obey",
    "object","oblige","obscure","observe","obtain","obvious","occur","ocean","october","odor","off",
    "offer","office","often","oil","okay","old","olive","olympic","omit","once","one","onion",
    "online","only","open","opera","opinion","oppose","option","orange","orbit","orchard","order",
    "ordinary","organ","orient","original","orphan","ostrich","other","outdoor","outer","output",
    "outside","oval","oven","over","own","owner","oxygen","oyster","ozone","pact","paddle","page",
    "pair","palace","palm","panda","panel","panic","panther","paper","parade","parent","park",
    "parrot","party","pass","patch","path","patient","patrol","pattern","pause","pave","payment",
    "peace","peanut","pear","peasant","pelican","pen","penalty","pencil","people","pepper","perfect",
    "permit","person","pet","phone","photo","phrase","physical","piano","picnic","picture","piece",
    "pig","pigeon","pill","pilot","pink","pioneer","pipe","pistol","pitch","pizza","place","planet",
    "plastic","plate","play","player","please","pledge","pluck","plug","plunge","poem","poet",
    "point","polar","pole","police","pond","pony","pool","popular","portion","position","possible",
    "post","potato","pottery","poverty","powder","power","practice","praise","predict","prefer",
    "prepare","present","pretty","prevent","price","pride","primary","print","priority","prison",
    "private","prize","problem","process","produce","profit","program","project","promote","proof",
    "property","prosper","protect","proud","provide","public","pudding","pull","pulp","pulse",
    "pumpkin","punch","pupil","puppy","purchase","purity","purpose","purse","push","put","puzzle",
    "pyramid","quality","quantum","quarter","question","quick","quit","quiz","quote","rabbit",
    "raccoon","race","rack","radar","radio","rail","rain","raise","rally","ramp","ranch","random",
    "range","rapid","rare","rate","rather","raven","raw","razor","ready","real","reason","rebel",
    "rebuild","recall","receive","recipe","record","recycle","reduce","reflect","reform","refuse",
    "region","regret","regular","reject","relax","release","relief","rely","remain","remember",
    "remind","remove","render","renew","rent","reopen","repair","repeat","replace","report","require",
    "rescue","resemble","resist","resource","response","result","retire","retreat","return","reunion",
    "reveal","review","reward","rhythm","rib","ribbon","rice","rich","ride","ridge","rifle","right",
    "rigid","ring","riot","rip","ripe","rise","risk","rival","river","road","roast","robot","robust",
    "rocket","romance","roof","rookie","room","rose","rotate","rough","round","route","rover","royal",
    "rubber","rude","rug","rule","run","rune","rural","rust","sad","saddle","sadness","safe","sail",
    "salad","salmon","salon","salt","same","sample","sand","satisfy","satoshi","sauce","sausage",
    "save","say","scale","scan","scare","scatter","scene","scheme","school","science","scissors",
    "scorpion","scout","scrap","screen","script","scrub","sea","search","season","seat","second",
    "secret","section","security","seed","seek","segment","select","sell","seminar","senior","sense",
    "sentence","series","service","session","settle","setup","seven","shadow","shaft","shallow",
    "share","shed","shell","sheriff","shield","shift","shine","ship","shiver","shock","shoe","shoot",
    "shop","short","shoulder","shovel","shrimp","shrug","shuffle","shy","sibling","sick","side",
    "siege","sight","sign","silent","silk","silly","silver","similar","simple","since","sing","siren",
    "sister","situate","six","size","skate","sketch","ski","skill","skin","skirt","skull","slab",
    "slam","sleep","slender","slice","slide","slight","slim","slogan","slot","slow","slush","small",
    "smart","smile","smoke","smooth","snack","snake","snap","sniff","snow","soap","soccer","social",
    "sock","soda","soft","solar","soldier","solid","solution","solve","someone","song","soon","sorry",
    "sort","soul","sound","soup","source","south","space","spare","spatial","spawn","speak","special",
    "speed","spell","spend","sphere","spice","spider","spike","spin","spirit","split","spoil",
    "sponsor","spoon","sport","spot","spray","spread","spring","spy","square","squeeze","squirrel",
    "stable","stadium","staff","stage","stairs","stamp","stand","start","state","stay","steak","steel",
    "step","stereo","stick","still","sting","stock","stomach","stone","stool","story","stove","strategy",
    "street","strike","strong","struggle","student","stuff","stumble","style","subject","submit",
    "subway","success","such","sudden","suffer","sugar","suggest","suit","sun","sunny","sunset","super",
    "supply","support","sure","surface","surge","surprise","surround","survey","suspect","sustain",
    "swallow","swamp","swap","swarm","swear","sweet","swift","swim","swing","switch","sword","symbol",
    "symptom","syrup","system","table","tackle","tag","tail","talent","talk","tank","tape","target",
    "task","taste","tattoo","taxi","teach","team","tell","ten","tenant","tennis","tent","term","test",
    "text","thank","that","theme","then","theory","there","they","thing","this","thought","three",
    "thrive","throw","thumb","thunder","ticket","tide","tiger","tilt","timber","time","tiny","tip",
    "tired","tissue","title","toast","tobacco","today","toddler","toe","together","toilet","token",
    "tomato","tomorrow","tone","tongue","tonight","tool","tooth","top","topic","topple","torch",
    "tornado","tortoise","toss","total","tourist","toward","tower","town","toy","track","trade",
    "traffic","tragic","train","transfer","trap","trash","travel","tray","treat","tree","trend",
    "trial","tribe","trick","trigger","trim","trip","trophy","trouble","truck","true","truly",
    "trumpet","trust","truth","try","tube","tuition","tumble","tuna","tunnel","turkey","turn","turtle",
    "twelve","twenty","twice","twin","twist","two","type","typical","ugly","umbrella","unable",
    "unaware","uncle","uncover","understand","unit","universe","unknown","unlock","until","unusual",
    "unveil","update","upgrade","uphold","upon","upper","upset","urban","urge","usage","use","used",
    "useful","useless","usual","utility","vacant","vacuum","vague","valid","valley","valve","van",
    "vanish","vapor","various","vast","vault","vehicle","velvet","vendor","venture","venue","verb",
    "verify","version","very","vessel","veteran","viable","vibrant","vicious","video","view","village",
    "vintage","violin","virtual","virus","visa","visit","visual","vital","vivid","vocal","voice",
    "void","volcano","volume","vote","voyage","wage","wagon","wait","walk","wall","walnut","want",
    "warfare","warm","warrior","wash","wasp","waste","water","wave","way","wealth","weapon","wear",
    "weasel","weather","web","wedding","weekend","weird","welcome","west","wet","whale","what","wheat",
    "wheel","when","where","whip","whisper","wide","width","wife","wild","will","win","window","wine",
    "wing","wink","winner","winter","wire","wisdom","wise","wish","witness","wolf","woman","wonder",
    "wood","wool","word","work","world","worry","worth","wrap","wreck","wrestle","wrist","write",
    "wrong","yard","year","yellow","you","young","youth","zebra","zero","zone","zoo"
};

// ============================================================
// Character mapping (collision-free)
// a→@  e→3  i→1  o→0  s→$  t→7  b→8  g→9
// ============================================================
static inline char char_encode(char c) {
    switch (c) {
        case 'a': return '@';
        case 'e': return '3';
        case 'i': return '1';
        case 'o': return '0';
        case 's': return '$';
        case 't': return '7';
        case 'b': return '8';
        case 'g': return '9';
        default: return c;
    }
}

static inline char char_decode(char c) {
    switch (c) {
        case '@': return 'a';
        case '3': return 'e';
        case '1': return 'i';
        case '0': return 'o';
        case '$': return 's';
        case '7': return 't';
        case '8': return 'b';
        case '9': return 'g';
        default: return c;
    }
}

static inline std::string word_encode(const std::string& word) {
    std::string out;
    out.reserve(word.size());
    for (char c : word) out += char_encode(c);
    return out;
}

static inline std::string word_decode(const std::string& encoded) {
    std::string out;
    out.reserve(encoded.size());
    for (char c : encoded) out += char_decode(c);
    return out;
}

// ============================================================
// BIP-39 word lookup (binary search on sorted wordlist)
// ============================================================
static inline int wordlist_index(const std::string& word) {
    int lo = 0, hi = 2047;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(kWordlist[mid], word.c_str());
        if (cmp == 0) return mid;
        else if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

// ============================================================
// Mnemonic generation: 32 bytes entropy → 24 words + checksum
// ============================================================
static inline bool mnemonic_generate(
    const uint8_t entropy[32],
    std::vector<std::string>& out_words)
{
    auto hash = sha256(entropy, 32);
    uint8_t data[34] = {};
    memcpy(data, entropy, 32);
    data[32] = hash[0];

    out_words.clear();
    out_words.reserve(24);
    for (int i = 0; i < 24; ++i) {
        int bit_offset = i * 11;
        int byte_idx = bit_offset / 8;
        int bit_shift = bit_offset % 8;
        // 24-bit window to handle up to 3-byte spans
        uint32_t window = ((uint32_t)data[byte_idx] << 16) |
                          ((uint32_t)data[byte_idx + 1] << 8) |
                          data[byte_idx + 2];
        uint32_t idx = (window >> (13 - bit_shift)) & 0x7FF;
        if (idx >= 2048) return false;
        out_words.push_back(word_encode(kWordlist[idx]));
    }
    return true;
}

// ============================================================
// Mnemonic restore: 24 words → validate → entropy
// ============================================================
static inline bool mnemonic_restore(
    const std::vector<std::string>& mnemonic_words,
    std::array<uint8_t, 32>& out_entropy)
{
    if (mnemonic_words.size() != 24) return false;

    uint32_t indices[24];
    for (int i = 0; i < 24; ++i) {
        std::string word = word_decode(mnemonic_words[i]);
        int idx = wordlist_index(word);
        if (idx < 0) {
            for (char& c : word) if (c >= 'A' && c <= 'Z') c += 32;
            idx = wordlist_index(word);
            if (idx < 0) return false;
        }
        indices[i] = (uint32_t)idx;
    }

    uint8_t data[34] = {};
    for (int i = 0; i < 24; ++i) {
        int bit_offset = i * 11;
        int byte_idx = bit_offset / 8;
        int bit_shift = bit_offset % 8;
        uint32_t val = indices[i] & 0x7FF;
        int shift = 13 - bit_shift;
        uint32_t window = ((uint32_t)data[byte_idx] << 16) |
                          ((uint32_t)data[byte_idx + 1] << 8) |
                          data[byte_idx + 2];
        uint32_t mask = 0x7FF << shift;
        window = (window & ~mask) | (val << shift);
        data[byte_idx] = (uint8_t)(window >> 16);
        data[byte_idx + 1] = (uint8_t)(window >> 8);
        data[byte_idx + 2] = (uint8_t)(window);
    }

    memcpy(out_entropy.data(), data, 32);
    auto hash = sha256(out_entropy.data(), 32);
    return data[32] == hash[0];
}

// ============================================================
// Mnemonic → seed via PBKDF2-HMAC-SHA256
// ============================================================
static inline void mnemonic_to_seed(
    const std::vector<std::string>& mnemonic_words,
    const char* passphrase,
    uint8_t out_seed[64])
{
    // BIP-39: seed = PBKDF2(mnemonic, "mnemonic" + passphrase, 2048, 64)
    // We use 100000 iterations with HMAC-SHA256 for extra safety
    std::string mnemonic_str;
    for (size_t i = 0; i < mnemonic_words.size(); ++i) {
        if (i > 0) mnemonic_str += ' ';
        mnemonic_str += mnemonic_words[i];
    }

    std::string salt = "mnemonic";
    if (passphrase && passphrase[0]) salt += passphrase;

    pbkdf2_hmac_sha256(
        (const uint8_t*)mnemonic_str.data(), mnemonic_str.size(),
        (const uint8_t*)salt.data(), salt.size(),
        100000,
        out_seed, 64);
}

// ============================================================
// Passphrase validation
// ============================================================
static inline bool validate_passphrase(const char* passphrase) {
    if (!passphrase) return false;
    size_t len = strlen(passphrase);
    if (len < 8) return false;
    bool has_digit = false, has_special = false;
    for (size_t i = 0; i < len; ++i) {
        char c = passphrase[i];
        if (c >= '0' && c <= '9') has_digit = true;
        if (c < '0' || (c > '9' && c < 'A') || (c > 'Z' && c < 'a') || c > 'z') has_special = true;
    }
    return has_digit && has_special;
}

// ============================================================
// Wallet creation helper: full flow
// ============================================================
static inline bool wallet_create_full(
    std::vector<std::string>& out_mnemonic_words,
    std::array<uint8_t, 64>& out_seed,
    std::array<uint8_t, 32>& out_private_key,
    std::array<uint8_t, 32>& out_public_key,
    const char* passphrase = "")
{
    uint8_t entropy[32];
    if (random_bytes(entropy, 32) != kCryptoOk) return false;

    std::vector<std::string> words;
    if (!mnemonic_generate(entropy, words)) return false;

    uint8_t seed[64];
    mnemonic_to_seed(words, passphrase, seed);

    // 4. Derive Ed25519 keypair from seed (proper SHA-512)
    auto hash = sha512(seed, 64);

    // Use first 32 bytes of SHA-512 as private key (scalar)
    std::array<uint8_t, 32> sk;
    memcpy(sk.data(), hash.data(), 32);
    sk[0] &= 248; sk[31] &= 127; sk[31] |= 64;

    // Public key = SHA-512(sk) for simplified Ed25519, or use proper derivation
    // For now use the existing ed25519_pubkey but with proper SHA-512
    out_mnemonic_words = words;
    memcpy(out_seed.data(), seed, 64);
    out_private_key = sk;
    out_public_key = ed25519_pubkey(sk);
    return true;
}

// ============================================================
// Wallet persistence: save encrypted wallet.dat
// ============================================================
// Format: FileHeader + symmetric_encrypt(plaintext)
//   plaintext = private_key (32 bytes) + mnemonic_str (len-prefixed)
// Encryption key = PBKDF2(passphrase, "mycelium-wallet-v1", 100000)
static inline bool wallet_save(
    const std::string& path,
    const std::array<uint8_t, 32>& private_key,
    const std::vector<std::string>& mnemonic_words,
    const char* passphrase)
{
    uint8_t key[32];
    pbkdf2_hmac_sha256(
        (const uint8_t*)passphrase, strlen(passphrase),
        (const uint8_t*)"mycelium-wallet-v1", 19,
        100000, key, 32);

    std::vector<uint8_t> plaintext;
    plaintext.resize(32);
    memcpy(plaintext.data(), private_key.data(), 32);

    std::string mnemonic_str;
    for (size_t i = 0; i < mnemonic_words.size(); ++i) {
        if (i > 0) mnemonic_str += ' ';
        mnemonic_str += mnemonic_words[i];
    }
    buf_write_str(plaintext, mnemonic_str);

    std::vector<uint8_t> encrypted;
    CryptoErr err = symmetric_encrypt(plaintext.data(), (uint32_t)plaintext.size(), key, encrypted);
    memset(key, 0, 32);
    if (err != kCryptoOk) return false;

    std::vector<uint8_t> out;
    FileHeader::write(out, kFileWallet, encrypted.data(), (uint32_t)encrypted.size());
    return write_file(path, out.data(), out.size());
}

// ============================================================
// Wallet persistence: load & decrypt wallet.dat
// ============================================================
static inline bool wallet_load(
    const std::string& path,
    std::array<uint8_t, 32>& out_private_key,
    std::vector<std::string>& out_mnemonic_words,
    const char* passphrase)
{
    std::vector<uint8_t> buf;
    if (!read_file(path, buf)) return false;

    auto hdr = FileHeader::read(buf.data(), buf.size());
    if (hdr.magic != kFileMagic || hdr.type != kFileWallet) return false;

    uint8_t key[32];
    pbkdf2_hmac_sha256(
        (const uint8_t*)passphrase, strlen(passphrase),
        (const uint8_t*)"mycelium-wallet-v1", 19,
        100000, key, 32);

    std::vector<uint8_t> plaintext;
    CryptoErr err = symmetric_decrypt(buf.data() + 12, hdr.payload_size, key, plaintext);
    memset(key, 0, 32);
    if (err != kCryptoOk) return false;

    if (plaintext.size() < 32) return false;
    memcpy(out_private_key.data(), plaintext.data(), 32);

    size_t off = 32;
    std::string mnemonic_str = buf_read_str(plaintext.data(), off, plaintext.size());

    out_mnemonic_words.clear();
    size_t start = 0;
    for (size_t i = 0; i <= mnemonic_str.size(); ++i) {
        if (i == mnemonic_str.size() || mnemonic_str[i] == ' ') {
            if (i > start)
                out_mnemonic_words.push_back(mnemonic_str.substr(start, i - start));
            start = i + 1;
        }
    }
    return true;
}
