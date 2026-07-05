#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "crypto/myc_crypto.hpp"
#include "protocol/myc_protocol.hpp"

static inline constexpr uint32_t kDefaultChunkSizeBytes = 4 << 20;
static inline constexpr uint32_t kMaxConcurrentStreamsDefault = 4;
static inline constexpr uint64_t kVideoBandwidthWeightBps = 100;

enum VideoCodec : uint8_t {
    kCodecH264 = 0,
    kCodecH265 = 1,
    kCodecVP9  = 2,
    kCodecAV1  = 3,
};

static inline const char* video_codec_name(VideoCodec c) {
    switch (c) {
        case kCodecH264: return "H.264";
        case kCodecH265: return "H.265";
        case kCodecVP9:  return "VP9";
        case kCodecAV1:  return "AV1";
        default:         return "unknown";
    }
}

struct VideoChunkEntry {
    std::string chunk_cid;
    uint32_t index = 0;
    uint64_t byte_offset = 0;
    uint64_t byte_length = 0;
    std::string host_peer_id;
    std::vector<uint8_t> chunk_hash;
};

struct VideoMetadata {
    std::string video_id;
    uint64_t duration_ms = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    VideoCodec codec = kCodecH264;
    uint64_t bitrate_bps = 0;
    std::string thumbnail_cid;
    std::vector<VideoChunkEntry> chunks;
    uint32_t chunk_size_bytes = kDefaultChunkSizeBytes;
    std::vector<uint8_t> encryption_key;
    int64_t created_at = 0;

    bool is_valid() const {
        return !video_id.empty() && width > 0 && height > 0 && duration_ms > 0;
    }

    uint64_t total_size_bytes() const {
        uint64_t total = 0;
        for (auto& c : chunks) total += c.byte_length;
        return total;
    }

    static inline VideoMetadata create(
        const std::string& id,
        uint64_t duration,
        uint32_t w,
        uint32_t h,
        VideoCodec c,
        uint64_t bitrate)
    {
        VideoMetadata m;
        m.video_id = id;
        m.duration_ms = duration;
        m.width = w;
        m.height = h;
        m.codec = c;
        m.bitrate_bps = bitrate;
        m.created_at = ProtocolMessage{}.now_sec();
        return m;
    }

    void add_chunk(const std::string& chunk_cid, uint32_t idx,
                    uint64_t offset, uint64_t length,
                    const std::vector<uint8_t>& hash) {
        VideoChunkEntry e;
        e.chunk_cid = chunk_cid;
        e.index = idx;
        e.byte_offset = offset;
        e.byte_length = length;
        e.chunk_hash = hash;
        chunks.push_back(e);
    }
};

static inline std::string compute_video_cid(const VideoMetadata& meta) {
    std::string input = meta.video_id + ":" +
        std::to_string(meta.duration_ms) + ":" +
        std::to_string(meta.width) + "x" + std::to_string(meta.height) + ":" +
        std::to_string(meta.bitrate_bps);
    auto h = sha256((const uint8_t*)input.data(), input.size());
    return "QmV" + base64_encode(h.data(), 16).substr(0, 44);
}
