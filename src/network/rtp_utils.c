/*
 * RTP (Real-time Transport Protocol) Utilities Implementation
 * 
 * This module implements RTP packet analysis and stream tracking for the
 * Call Monitor and Analyzer Program (CMAP). It handles RTP packet validation,
 * stream management, and statistics collection.
 *
 * Key Features:
 * - RTP packet validation
 * - Stream tracking and management
 * - Quality metrics calculation
 * - NAT64 support
 * - Audio quality enhancement
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <time.h>
#include <math.h>

#include "rtp_utils.h"
#include "debug.h"
#include "audio_quality.h"

// Reference to global session
extern struct call_session current_session;

/*
 * Reorder buffer for out-of-sequence packets
 */
#define REORDER_BUFFER_SIZE 128  // Increased buffer size for better handling of out-of-sequence packets
#define FEC_PACKET_INTERVAL 5  // Send FEC packet every N packets
#define MAX_JITTER_BUFFER_SIZE 200  // Maximum jitter buffer size in ms
#define MIN_JITTER_BUFFER_SIZE 40   // Minimum jitter buffer size in ms
#define JITTER_BUFFER_TARGET_DELAY 60  // Target delay for jitter buffer in ms

struct rtp_packet_info {
    uint16_t seq;
    uint32_t timestamp;
    uint32_t arrival_time;
    int valid;
    uint8_t *data;           // Packet data for FEC
    int data_len;           // Length of packet data
    int is_fec;             // Flag for FEC packets
    uint8_t *fec_data;      // Forward Error Correction data
    int fec_len;           // Length of FEC data
};

static struct rtp_packet_info reorder_buffer[REORDER_BUFFER_SIZE];
static int reorder_head = 0;

// Forward declarations of static functions
static void init_quality_params(struct rtp_stream *stream);
static void update_quality_metrics(struct rtp_stream *stream, uint16_t seq, uint32_t size);
static void adapt_jitter_buffer(struct rtp_stream *stream);
static int store_packet_info(uint16_t seq, uint32_t timestamp, uint32_t arrival, const uint8_t *data, int data_len);
static int find_next_packet(struct rtp_stream *stream);
static void handle_silence_insertion(struct rtp_stream *stream, uint16_t seq);
static void update_jitter_metrics(struct rtp_stream *stream, uint32_t timestamp, uint32_t packet_time);
static void interpolate_lost_packets(struct rtp_stream *stream, int gap);
static void generate_fec_packet(int start_idx) {
    if (start_idx < 0 || start_idx >= REORDER_BUFFER_SIZE) return;
    
    struct rtp_packet_info *fec_packet = &reorder_buffer[start_idx];
    if (!fec_packet->valid || !fec_packet->data || fec_packet->data_len <= 0) return;
    
    // Find max packet size in the FEC group
    int max_len = fec_packet->data_len;
    for (int i = 1; i < FEC_PACKET_INTERVAL; i++) {
        int idx = (start_idx + i) % REORDER_BUFFER_SIZE;
        if (reorder_buffer[idx].valid && reorder_buffer[idx].data_len > max_len) {
            max_len = reorder_buffer[idx].data_len;
        }
    }
    
    // Allocate and initialize FEC data
    uint8_t *fec_data = malloc(max_len);
    if (!fec_data) return;
    
    // Copy first packet's data as initial FEC value
    memcpy(fec_data, fec_packet->data, fec_packet->data_len);
    memset(fec_data + fec_packet->data_len, 0, max_len - fec_packet->data_len);
    
    // XOR with subsequent packets
    for (int i = 1; i < FEC_PACKET_INTERVAL; i++) {
        int idx = (start_idx + i) % REORDER_BUFFER_SIZE;
        struct rtp_packet_info *curr = &reorder_buffer[idx];
        
        if (curr->valid && curr->data) {
            // XOR up to the smaller of the two lengths
            int xor_len = curr->data_len;
            for (int j = 0; j < xor_len; j++) {
                fec_data[j] ^= curr->data[j];
            }
        }
    }
    
    // Store FEC data
    if (fec_packet->fec_data) free(fec_packet->fec_data);
    fec_packet->fec_data = fec_data;
    fec_packet->fec_len = max_len;
    fec_packet->is_fec = 1;
}
static int try_fec_recovery(struct rtp_stream *stream, uint16_t start_seq, int gap);
static void recover_lost_packet(struct rtp_stream *stream, int fec_idx, uint16_t lost_seq);
static void process_rtp_payload(struct rtp_stream *stream, const uint8_t *payload, int payload_len, uint8_t pt, uint32_t timestamp);
// Process RTP Payload with audio quality enhancement - implementation moved to static declaration below
static void interpolate_lost_packets(struct rtp_stream *stream, int gap) {
    if (!stream || !stream->last_good_frame) return;
    
    // Simple linear interpolation between last good frame and current frame
    int frame_size = stream->frame_size;
    int16_t *last_frame = (int16_t *)stream->last_good_frame;
    int16_t *curr_frame = (int16_t *)stream->current_frame;
    
    for (int i = 0; i < gap; i++) {
        float ratio = (i + 1.0f) / (gap + 1.0f);
        for (int j = 0; j < frame_size/2; j++) {
            int16_t interpolated = (int16_t)(
                last_frame[j] * (1.0f - ratio) +
                curr_frame[j] * ratio
            );
            // Store interpolated frame
            if (stream->plc_buffer) {
                ((int16_t *)stream->plc_buffer)[i * frame_size/2 + j] = interpolated;
            }
        }
    }
}

static void generate_comfort_noise(struct rtp_stream *stream, int gap) {
    if (!stream) return;
    
    // Simple comfort noise generation
    int frame_size = stream->frame_size;
    float noise_level = -60.0f;  // dB
    
    for (int i = 0; i < gap; i++) {
        for (int j = 0; j < frame_size; j++) {
            if (stream->plc_buffer) {
                float random = (float)rand() / RAND_MAX * 2.0f - 1.0f;
                ((int16_t *)stream->plc_buffer)[i * frame_size/2 + j] = 
                    (int16_t)(random * powf(10.0f, noise_level/20.0f) * 32767.0f);
            }
        }
    }
}

static int try_fec_recovery(struct rtp_stream *stream, uint16_t start_seq, int gap) {
    if (!stream) return 0;
    
    // Look for FEC packet
    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
        if (reorder_buffer[i].valid && reorder_buffer[i].is_fec) {
            // Check if FEC packet can help recover lost packet
            if (reorder_buffer[i].seq > start_seq && 
                reorder_buffer[i].seq <= start_seq + gap) {
                // Attempt recovery using XOR
                recover_lost_packet(stream, i, start_seq);
                return 1;
            }
        }
    }
    
    return 0;
}

static void recover_lost_packet(struct rtp_stream *stream, int fec_idx, uint16_t lost_seq) {
    if (!stream) return;
    
    // XOR all received packets in the FEC group to recover lost packet
    uint8_t *recovered = malloc(reorder_buffer[fec_idx].data_len);
    if (!recovered) return;
    
    memcpy(recovered, reorder_buffer[fec_idx].data, reorder_buffer[fec_idx].data_len);
    
    for (int i = 0; i < FEC_PACKET_INTERVAL; i++) {
        int idx = (fec_idx - i + REORDER_BUFFER_SIZE) % REORDER_BUFFER_SIZE;
        if (reorder_buffer[idx].valid && !reorder_buffer[idx].is_fec && 
            reorder_buffer[idx].seq != lost_seq) {
            for (int j = 0; j < reorder_buffer[idx].data_len; j++) {
                recovered[j] ^= reorder_buffer[idx].data[j];
            }
        }
    }
    
    // Store recovered packet
    process_rtp_payload(stream, recovered, reorder_buffer[fec_idx].data_len,
                       stream->payload_type, reorder_buffer[fec_idx].timestamp);
    
    free(recovered);
}

static int validate_rtp_timestamp(struct rtp_stream *stream, uint32_t *timestamp, uint32_t packet_time);
static int handle_out_of_sequence(struct rtp_stream *stream, uint16_t seq, uint32_t timestamp, uint32_t arrival_time);
static int is_valid_timestamp_jump(uint32_t prev_ts, uint32_t curr_ts, uint32_t clock_rate);

/*
 * Store packet info in reorder buffer
 * Returns: Buffer index if stored, -1 if buffer full
 */
static int store_packet_info(uint16_t seq, uint32_t timestamp, uint32_t arrival, const uint8_t *data, int data_len) {
    int idx = reorder_head;
    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
        if (!reorder_buffer[idx].valid) {
            reorder_buffer[idx].seq = seq;
            reorder_buffer[idx].timestamp = timestamp;
            reorder_buffer[idx].arrival_time = arrival;
            reorder_buffer[idx].valid = 1;
            reorder_buffer[idx].fec_data = NULL;
            reorder_buffer[idx].fec_len = 0;
            
            // Store packet data for FEC
            if (data && data_len > 0) {
                reorder_buffer[idx].data = malloc(data_len);
                if (reorder_buffer[idx].data) {
                    memcpy(reorder_buffer[idx].data, data, data_len);
                    reorder_buffer[idx].data_len = data_len;
                }
            }
            
            // Generate FEC packet if needed
            if (seq % FEC_PACKET_INTERVAL == 0) {
                generate_fec_packet(idx);
            }
            
            return idx;
        }
        idx = (idx + 1) % REORDER_BUFFER_SIZE;
    }
    return -1;
}

/*
 * Find best packet to process next from reorder buffer
 * Uses timing and sequence information to make decision
 * Returns: Buffer index if found, -1 if none ready
 */
static int find_next_packet(struct rtp_stream *stream) {
    if (!stream) return -1;
    
    uint32_t min_seq_diff = UINT32_MAX;
    int best_idx = -1;
    uint32_t current_time = time(NULL);
    
    // First try to find in-sequence packet
    int idx = reorder_head;
    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
        if (reorder_buffer[idx].valid) {
            uint16_t seq_diff = reorder_buffer[idx].seq - stream->last_seq;
            if (seq_diff == 1) {
                return idx;  // Perfect next packet found
            }
            if (seq_diff < min_seq_diff) {
                min_seq_diff = seq_diff;
                best_idx = idx;
            }
        }
        idx = (idx + 1) % REORDER_BUFFER_SIZE;
    }
    
    // If no perfect match, check if we should use best available
    if (best_idx >= 0) {
        uint32_t wait_time = current_time - reorder_buffer[best_idx].arrival_time;
        if (wait_time >= MAX_REORDER_WAIT_MS) {
            return best_idx;  // Waited long enough, use this packet
        }
    }
    
    return -1;  // Nothing ready yet
}

/*
 * Enhanced out-of-sequence handling with reorder buffer
 */
static int handle_out_of_sequence(struct rtp_stream *stream, uint16_t seq, 
                                uint32_t timestamp, uint32_t arrival_time) {
    if (!stream) return 0;
    
    // Check if packet is too old
    if (seq < stream->max_seq) {
        uint16_t diff = stream->max_seq - seq;
        if (diff > MAX_OOO_WINDOW) {
            DEBUG_RTP("Dropping too old packet: seq=%u, max_seq=%u", seq, stream->max_seq);
            return 0;
        }
    }
    
    // Store in reorder buffer
    int buf_idx = store_packet_info(seq, timestamp, arrival_time, NULL, 0);  // No data for timing-only check
    if (buf_idx < 0) {
        DEBUG_RTP("Reorder buffer full, dropping packet: seq=%u", seq);
        return 0;
    }
    
    // Check if this packet is ready for processing
    int next_idx = find_next_packet(stream);
    if (next_idx == buf_idx) {
        reorder_buffer[buf_idx].valid = 0;  // Clear buffer slot
        reorder_head = (buf_idx + 1) % REORDER_BUFFER_SIZE;
        return 1;  // Process this packet
    }
    
    return 0;  // Wait for more packets
}

/*
 * Improved timestamp validation with historical tracking
 */
static int validate_rtp_timestamp(struct rtp_stream *stream, uint32_t *timestamp, 
                                uint32_t packet_time) {
    if (!stream || !timestamp) return 1;
    if (!stream->last_timestamp) return 1;
    
    // Calculate expected timestamp range
    uint32_t expected_ts = stream->last_timestamp + 
                          (stream->clock_rate / 1000) * 
                          (packet_time - stream->last_packet_time.tv_sec * 1000 - 
                           stream->last_packet_time.tv_nsec / 1000000);
    
    // Use narrower deviation window for better accuracy
    uint32_t max_deviation = (stream->clock_rate / 1000) * 10;  // 10ms window
    
    uint32_t diff = (*timestamp > expected_ts) ? 
                   *timestamp - expected_ts : 
                   expected_ts - *timestamp;
    
    if (diff > max_deviation) {
        // Additional check: look for timestamp pattern
        uint32_t pattern_interval = stream->clock_rate / 50;  // 20ms typical frame
        if (diff % pattern_interval <= max_deviation) {
            // Timestamp follows a valid pattern, keep it
            DEBUG_RTP("Unusual but valid timestamp pattern: ts=%u, expected=%u", 
                     *timestamp, expected_ts);
            return 1;
        }
        
        // Timestamp appears wrong, correct it
        *timestamp = expected_ts;
        stream->corrected_timestamps++;
        DEBUG_RTP("Corrected invalid timestamp: old=%u, new=%u", *timestamp, expected_ts);
        return 0;
    }
    
    return 1;
}

/*
 * Validate RTP Timestamp Jump
 * 
 * Checks if timestamp jump is reasonable:
 * - Accounts for different clock rates
 * - Handles wrap-around
 * - Detects large discontinuities
 * Returns: 1 if jump is valid, 0 if invalid
 */
static int is_valid_timestamp_jump(uint32_t prev_ts, uint32_t curr_ts, uint32_t clock_rate) {
    // Handle wrap-around
    uint32_t diff;
    if (curr_ts < prev_ts) {
        diff = (0xFFFFFFFF - prev_ts) + curr_ts + 1;
    } else {
        diff = curr_ts - prev_ts;
    }

    // Convert diff to milliseconds based on clock rate
    // Use 64-bit arithmetic to avoid overflow
    uint64_t ms = ((uint64_t)diff * 1000) / clock_rate;
    
    // Allow jumps up to 5 seconds (reasonable for most cases)
    // But reject extremely small jumps that indicate duplicates
    return ms <= 5000 && ms >= 1;
}

/*
 * Initialize stream quality parameters
 */
static void init_quality_params(struct rtp_stream *stream) {
    if (!stream) {
        return;
    }
    
    // Initialize packet tracking
    stream->packets_received = 0;
    stream->lost_packets = 0;
    stream->out_of_order = 0;
    stream->total_packets = 0;
    stream->recovered_packets = 0;
    stream->concealed_ms = 0;
    
    // Initialize frame handling
    stream->frame_size = 0;
    stream->last_good_frame = NULL;
    stream->current_frame = NULL;
    stream->plc_buffer = NULL;
    
    // Initialize timing and jitter
    stream->jitter = 0.0;
    stream->last_seq = 0;
    stream->last_timestamp = 0;
    stream->clock_rate = 8000;  // Default to 8kHz
    stream->inserted_silence_ms = 0;
    stream->corrected_timestamps = 0;
    stream->jitter_spikes = 0;
    stream->stats.buffer_size_ms = 60;  // Start with 60ms buffer
    
    // Initialize audio quality enhancement
    stream->audio_ctx = audio_quality_init();
    if (!stream->audio_ctx) {
        DEBUG_PRINT("Failed to initialize audio quality context");
    }
}

/*
 * Update stream quality metrics
 * Calculates enhanced quality metrics including:
 * - Loss rate over sliding window
 * - Mean frame size
 * - Consecutive packet losses
 */
static void update_quality_metrics(struct rtp_stream *stream, uint16_t seq, uint32_t size) {
    // Update consecutive losses
    if (seq != (stream->max_seq + 1) && seq > stream->max_seq) {
        stream->consecutive_losses += (seq - stream->max_seq - 1);
    } else {
        stream->consecutive_losses = 0;
    }
    
    // Update loss rate using sliding window
    double current_loss_rate = (double)stream->lost_packets / 
                             (stream->packets_received + stream->lost_packets);
    stream->loss_rate = (stream->loss_rate * (LOSS_WINDOW_SIZE - 1) + 
                        current_loss_rate) / LOSS_WINDOW_SIZE;
    
    // Update mean frame size using exponential moving average
    if (stream->mean_frame_size == 0) {
        stream->mean_frame_size = size;
    } else {
        stream->mean_frame_size = 0.95 * stream->mean_frame_size + 0.05 * size;
    }
}

/*
 * Adapt jitter buffer size based on network conditions
 * Uses network metrics to optimize buffer size:
 * - Current jitter
 * - Packet loss rate
 * - Network delay variation
 */
// Constants for jitter buffer adaptation
#define JITTER_SMOOTHING_FACTOR 0.125  // Exponential smoothing factor (1/8)
#define MAX_JITTER_MULTIPLIER 4.0      // Maximum jitter buffer size multiplier
#define MIN_JITTER_MULTIPLIER 1.5      // Minimum jitter buffer size multiplier
#define RAPID_CHANGE_THRESHOLD 50      // ms, threshold for rapid network changes

static void adapt_jitter_buffer(struct rtp_stream *stream) {
    if (!stream) return;
    
    // Calculate target size based on current jitter
    double current_jitter = stream->jitter / (double)stream->clock_rate * 1000.0;
    double packet_interval = 20.0;  // Assuming 20ms packet interval
    
    // Apply smoothing to jitter measurement
    if (!stream->smoothed_jitter) {
        stream->smoothed_jitter = current_jitter;
    } else {
        stream->smoothed_jitter = stream->smoothed_jitter * (1.0 - JITTER_SMOOTHING_FACTOR) +
                                 current_jitter * JITTER_SMOOTHING_FACTOR;
    }
    
    // Calculate packet loss rate
    double packet_loss_rate = stream->lost_packets / (double)stream->total_packets;
    
    // Calculate base target size
    double target_size = stream->smoothed_jitter * 2.0 + packet_interval;
    
    // Detect rapid network changes
    double jitter_delta = fabs(current_jitter - stream->smoothed_jitter);
    if (jitter_delta > RAPID_CHANGE_THRESHOLD || packet_loss_rate > 0.05) {
        // Increase buffer temporarily
        target_size *= 1.5;
    }
    
    // Apply min/max constraints
    double min_size = packet_interval * MIN_JITTER_MULTIPLIER;
    double max_size = packet_interval * MAX_JITTER_MULTIPLIER;
    
    if (target_size < min_size) target_size = min_size;
    if (target_size > max_size) target_size = max_size;
    
    // Update buffer size smoothly
    stream->stats.buffer_size_ms = (uint32_t)(
        0.8 * stream->stats.buffer_size_ms +
        0.2 * target_size
    );
    
    // Update statistics
    stream->stats.buffer_size_ms = stream->stats.buffer_size_ms;  // Maintain current size
    stream->stats.buffer_target_ms = (uint32_t)target_size;
    
    DEBUG_RTP("Adapted jitter buffer: size=%ums, jitter=%fms, loss=%f%%",
              stream->stats.buffer_size_ms, current_jitter, packet_loss_rate * 100.0);
}

/*
 * Minimum RTP Packet Size
 * 12 bytes for header + minimum 1 byte payload
 */
#define MIN_RTP_SIZE 13

/*
 * Validate RTP Payload Type
 * 
 * Checks if a payload type is valid for audio codecs:
 * - 0-34: Standard audio codecs
 * - 96-127: Dynamic payload types
 * 
 * Parameters:
 * - pt: Payload type to check
 * 
 * Returns:
 * 1 if valid, 0 if invalid
 */
int is_valid_rtp_payload_type(uint8_t pt) {
    // Common audio payload types
    if (pt <= 34) return 1;  // Standard audio codecs
    if (pt >= 96 && pt <= 127) return 1;  // Dynamic payload types
    return 0;
}

/*
 * Validate RTP Packet Size
 * 
 * Checks if payload length is valid for codec type:
 * - Minimum 20 bytes for any codec
 * - PCMU (pt=0): Minimum 160 bytes
 * - PCMA (pt=8): Minimum 160 bytes
 * 
 * Parameters:
 * - payload_len: Length of RTP payload
 * - pt: Payload type
 * 
 * Returns:
 * 1 if valid, 0 if invalid
 */
int is_valid_rtp_packet_size(int payload_len, uint8_t pt) {
    // Minimum sizes for common codecs
    if (payload_len < 20) return 0;  // Too small for any codec
    if (pt == 0 && payload_len < 160) return 0;  // PCMU
    if (pt == 8 && payload_len < 160) return 0;  // PCMA
    return 1;
}

/*
 * Compare Addresses with NAT64 Support
 * 
 * Helper function to compare IP addresses considering NAT64:
 * - Direct string comparison for regular addresses
 * - Special handling for NAT64 addresses
 * 
 * Parameters:
 * - addr1, addr2: Addresses to compare
 * 
 * Returns:
 * 1 if match (including NAT64), 0 if no match
 */
static int addresses_match(const char *addr1, const char *addr2) {
    if (!addr1 || !addr2) return 0;

    // Direct string comparison first
    if (strcmp(addr1, addr2) == 0) return 1;

    // Check if either address is NAT64
    int addr1_nat64 = is_nat64_address(addr1);
    int addr2_nat64 = is_nat64_address(addr2);

    if (!addr1_nat64 && !addr2_nat64) {
        return 0;  // Neither is NAT64, and direct match failed
    }

    // Extract IPv4 portion from NAT64 addresses for comparison
    char addr1_v4[INET_ADDRSTRLEN] = {0};
    char addr2_v4[INET_ADDRSTRLEN] = {0};
    
    if (addr1_nat64) {
        if (!extract_ipv4_from_nat64(addr1, addr1_v4, sizeof(addr1_v4))) {
            return 0;
        }
    } else {
        strncpy(addr1_v4, addr1, sizeof(addr1_v4) - 1);
    }
    
    if (addr2_nat64) {
        if (!extract_ipv4_from_nat64(addr2, addr2_v4, sizeof(addr2_v4))) {
            return 0;
        }
    } else {
        strncpy(addr2_v4, addr2, sizeof(addr2_v4) - 1);
    }

    // Compare the IPv4 portions
    return strcmp(addr1_v4, addr2_v4) == 0;
}

/*
 * Find or Create RTP Stream
 * 
 * Locates existing stream or creates new one based on packet info:
 * - Searches by SSRC and direction
 * - Creates new stream if none found
 * - Updates stream information
 * 
 * Parameters:
 * - session: Current call session
 * - src_ip, src_port: Source endpoint
 * - dst_ip, dst_port: Destination endpoint
 * - ssrc: RTP SSRC identifier
 * - payload_type: RTP payload type
 * - direction: Traffic direction
 * 
 * Returns:
 * Pointer to stream or NULL on error
 */
struct rtp_stream* find_or_create_stream(struct call_session *session,
                                       const char *src_ip, uint16_t src_port,
                                       const char *dst_ip, uint16_t dst_port,
                                       uint32_t ssrc, uint8_t payload_type,
                                       int direction) {
    if (!session) return NULL;

    // First try to find existing stream by SSRC and direction
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        struct rtp_stream *stream = &session->streams[i];
        if (stream->active && stream->ssrc == ssrc && stream->direction == direction &&
            addresses_match(stream->src_ip, src_ip) && addresses_match(stream->dst_ip, dst_ip)) {
            // Update addresses if NAT64 translation occurred
            if (is_nat64_address(src_ip) && !is_nat64_address(stream->src_ip)) {
                strncpy(stream->nat64_ip, src_ip, sizeof(stream->nat64_ip)-1);
                stream->nat64_port = src_port;
            } else if (is_nat64_address(dst_ip) && !is_nat64_address(stream->dst_ip)) {
                strncpy(stream->nat64_ip, dst_ip, sizeof(stream->nat64_ip)-1);
                stream->nat64_port = dst_port;
            }
            return stream;
        }
    }

    // Find free slot for new stream
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        struct rtp_stream *stream = &session->streams[i];
        if (!stream->active) {
            // Initialize new stream
            memset(stream, 0, sizeof(struct rtp_stream));
            stream->active = 1;
            stream->ssrc = ssrc;
            stream->payload_type = payload_type;
            stream->direction = direction;
            stream->packets_received = 0;
            stream->base_seq = 0;  // Will be set on first packet
            stream->max_seq = 0;   // Will be set on first packet
            stream->cycles = 0;
            stream->received = 0;
            stream->lost_packets = 0;
            stream->out_of_order = 0;
            stream->start_time.tv_sec = 0;
            stream->start_time.tv_nsec = 0;
            stream->last_packet_time.tv_sec = 0;
            stream->last_packet_time.tv_nsec = 0;
            stream->probation = MIN_SEQUENTIAL;

            // Set addresses
            strncpy(stream->src_ip, src_ip, sizeof(stream->src_ip)-1);
            stream->src_port = src_port;
            strncpy(stream->dst_ip, dst_ip, sizeof(stream->dst_ip)-1);
            stream->dst_port = dst_port;

            // Initialize NAT64 fields
            stream->nat64_ip[0] = '\0';
            stream->nat64_port = 0;

            // If source or destination is NAT64, store it
            if (is_nat64_address(src_ip)) {
                strncpy(stream->nat64_ip, src_ip, sizeof(stream->nat64_ip)-1);
                stream->nat64_port = src_port;
            } else if (is_nat64_address(dst_ip)) {
                strncpy(stream->nat64_ip, dst_ip, sizeof(stream->nat64_ip)-1);
                stream->nat64_port = dst_port;
            }

            // Initialize quality parameters for new stream
            init_quality_params(stream);

            DEBUG_RTP("Created new RTP stream: SSRC=%u, PT=%d, Dir=%d, %s:%d -> %s:%d",
                     ssrc, payload_type, direction, src_ip, src_port, dst_ip, dst_port);
            return stream;
        }
    }

    return NULL;  // No free slots
}

/*
 * Update Stream Statistics
 * 
 * Updates stream quality metrics:
 * - Sequence analysis
 * - Jitter calculation
 * - Packet loss detection
 * 
 * Parameters:
 * - stream: Stream to update
 * - seq: RTP sequence number
 * - timestamp: RTP timestamp
 * - packet_time: Packet arrival time
 */
void update_stream_stats(struct rtp_stream *stream,
                        uint16_t seq, uint32_t timestamp,
                        uint32_t packet_time) {
    if (!stream) return;

    stream->packets_received++;
    stream->last_packet_time.tv_sec = packet_time;
    stream->last_packet_time.tv_nsec = 0;

    // Handle sequence numbers
    if (stream->packets_received == 1) {
        // Initialize sequence tracking on first packet
        stream->base_seq = seq;
        stream->max_seq = seq;
        stream->bad_seq = (uint16_t)(seq + 1);  // Initialize to next expected
        stream->cycles = 0;
        stream->received = 0;
        stream->received_prior = 0;
        stream->expected_prior = 0;
        stream->last_timestamp = timestamp;
        stream->transit = 0;
        stream->jitter = 0;
        stream->probation = MIN_SEQUENTIAL;
    } else {
        // Source is not valid until MIN_SEQUENTIAL packets received
        if (stream->probation) {
            // Packet is in sequence
            if (seq == stream->max_seq + 1) {
                stream->probation--;
                stream->max_seq = seq;
                if (stream->probation == 0) {
                    stream->base_seq = seq;
                    stream->received = 0;
                }
            } else {
                stream->probation = MIN_SEQUENTIAL - 1;
                stream->max_seq = seq;
            }
            return;
        }

        uint16_t udelta = seq - stream->max_seq;
        
        // Handle sequence number wrap-around
        if (udelta < MAX_DROPOUT) {
            // In order, with permissible gap
            if (seq < stream->max_seq) {
                // Sequence number wrapped - count another 64K cycle
                stream->cycles += RTP_SEQ_MOD;
            }
            stream->max_seq = seq;
        } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
            // Large jump but within window, handle as out of order
            stream->out_of_order++;
            // Don't update max_seq here to handle proper reordering
        } else {
            // Duplicate or out of order packet
            stream->out_of_order++;
        }

        // Calculate extended sequence number
        uint32_t extended_seq = stream->cycles + seq;
        stream->received++;

        // Calculate expected packets and lost packets
        uint32_t expected = extended_seq - stream->base_seq + 1;
        stream->lost_packets = expected - stream->received;

        // Update jitter calculation using timestamps
        if (stream->last_timestamp != timestamp) {
            int32_t arrival = packet_time * stream->clock_rate;
            int32_t transit = arrival - timestamp;
            int32_t d = transit - stream->transit;
            
            // Protect against negative jitter
            if (d < 0) d = -d;
            stream->transit = transit;
            // Fix jitter calculation to use proper floating point
            stream->jitter = stream->jitter + (((double)d - stream->jitter) / 16.0);
        }
        stream->last_timestamp = timestamp;
    }

    // Update enhanced quality metrics
    update_quality_metrics(stream, seq, sizeof(timestamp));
    adapt_jitter_buffer(stream);
}

/*
 * Process RTP Payload with audio quality enhancement
 */
static void process_rtp_payload(struct rtp_stream *stream,
                              const uint8_t *payload,
                              int payload_len,
                              uint8_t pt,
                              uint32_t timestamp) {
    if (!stream || !payload || payload_len <= 0) {
        return;
    }

    // Get current time in milliseconds
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t current_time = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;

    // Process through audio quality enhancement if available
    if (stream->audio_ctx) {
        int processed_len = audio_quality_process_packet(
            stream->audio_ctx,
            payload,
            payload_len,
            stream->last_seq,
            timestamp,
            current_time
        );

        if (processed_len < 0) {
            DEBUG_PRINT("Failed to process packet through audio quality enhancement");
        }
    }

    // Original payload processing
    if (pt == 0 || pt == 8) {  // PCMU or PCMA
        // Process standard audio
        if (payload_len < 160) {
            DEBUG_PRINT("Short audio frame received: %d bytes", payload_len);
        }
    } else if (pt >= 96 && pt <= 127) {
        // Process dynamic payload type
        DEBUG_PRINT("Dynamic payload type: %d, length: %d", pt, payload_len);
    }
}

/*
 * Get next packet for playout with quality enhancement
 */
int get_next_audio_packet(struct rtp_stream *stream, uint8_t *buffer, int max_size) {
    if (!stream || !stream->audio_ctx || !buffer) {
        return -1;
    }

    // Get current time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t current_time = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;

    return audio_quality_get_next_packet(stream->audio_ctx, buffer, max_size, current_time);
}

/*
 * Clean up stream resources
 */
void cleanup_rtp_stream(struct rtp_stream *stream) {
    if (!stream) return;
    
    // Free audio frame buffers
    if (stream->last_good_frame) {
        free(stream->last_good_frame);
        stream->last_good_frame = NULL;
    }
    if (stream->current_frame) {
        free(stream->current_frame);
        stream->current_frame = NULL;
    }
    if (stream->plc_buffer) {
        free(stream->plc_buffer);
        stream->plc_buffer = NULL;
    }
    
    // Cleanup audio quality context
    if (stream->audio_ctx) {
        audio_quality_cleanup(stream->audio_ctx);
        stream->audio_ctx = NULL;
    }
    
    memset(stream, 0, sizeof(struct rtp_stream));

    // Clean up audio quality enhancement
    if (stream->audio_ctx) {
        audio_quality_cleanup(stream->audio_ctx);
        stream->audio_ctx = NULL;
    }

    // Add any other cleanup needed
}

/*
 * Calculate packet playout time and handle silence insertion
 */
static void handle_silence_insertion(struct rtp_stream *stream, uint16_t seq) {
    if (!stream || !stream->last_seq) return;
    
    // Calculate gap size
    uint16_t gap = seq - stream->last_seq - 1;
    if (gap == 0) return;
    
    // Try to recover lost packets using FEC first
    if (try_fec_recovery(stream, stream->last_seq + 1, gap)) {
        DEBUG_RTP("Recovered %u lost packets using FEC", gap);
        stream->recovered_packets += gap;
        stream->last_seq = seq;
        return;
    }
    
    // If FEC recovery failed, use packet loss concealment
    if (stream->last_good_frame) {
        // Use waveform substitution
        interpolate_lost_packets(stream, gap);
        DEBUG_RTP("Applied waveform interpolation for %u lost packets", gap);
    } else {
        // Fallback to comfort noise generation
        generate_comfort_noise(stream, gap);
        DEBUG_RTP("Generated comfort noise for %u lost packets", gap);
    }
    
    // Use codec-specific frame size for stats
    uint32_t samples_per_frame;
    switch (stream->payload_type) {
        case 0:  // PCMU
        case 8:  // PCMA
            samples_per_frame = 160;  // 20ms @ 8kHz
            break;
        case 9:  // G722
            samples_per_frame = 320;  // 20ms @ 16kHz
            break;
        default:
            samples_per_frame = stream->clock_rate / 50;  // Default 20ms
    }
    
    // Update stream stats
    uint32_t concealed_samples = gap * samples_per_frame;
    uint32_t concealed_ms = (concealed_samples * 1000) / stream->clock_rate;
    stream->concealed_ms += concealed_ms;
    
    stream->last_seq = seq;
}

/*
 * Calculate and update jitter metrics
 */
static void update_jitter_metrics(struct rtp_stream *stream, uint32_t timestamp, uint32_t packet_time) {
    if (!stream) return;
    
    // RFC 3550 jitter calculation
    if (stream->last_timestamp) {
        int32_t transit = packet_time - 
            (timestamp * 1000 / stream->clock_rate);
        int32_t d = transit - stream->transit;
        
        stream->transit = transit;
        if (d < 0) d = -d;
        stream->jitter += (1.0/16.0) * ((double)d - stream->jitter);
        
        // Detect jitter spikes
        uint32_t jitter_threshold = stream->clock_rate / 100; // 10ms spike
        if ((uint32_t)d > jitter_threshold) {
            stream->jitter_spikes++;
            DEBUG_RTP("Jitter spike detected: %dms", d * 1000 / stream->clock_rate);
        }
    }
}

/*
 * Process RTP Packet
 * 
 * Main RTP packet processing function:
 * - Extracts RTP header fields
 * - Validates packet
 * - Updates stream statistics
 * 
 * Parameters:
 * - header: Packet capture header
 * - ip: IP header
 * - udp: UDP header
 * - payload: RTP payload
 * - payload_len: Payload length
 * - direction: Traffic direction
 */
void process_rtp_packet(const struct pcap_pkthdr *header,
                       const struct ip_header *ip,
                       const struct udphdr *udp,
                       const uint8_t *payload,
                       int payload_len,
                       int direction) {
    char src_ip[INET6_ADDRSTRLEN], dst_ip[INET6_ADDRSTRLEN];
    uint16_t src_port = ntohs(udp->uh_sport);
    uint16_t dst_port = ntohs(udp->uh_dport);
    
    // Convert IP addresses to strings
    if (IP_VERSION(ip->ip_vhl) == 6) {
        const struct ip6_hdr *ip6 = (const struct ip6_hdr *)ip;
        inet_ntop(AF_INET6, &ip6->ip6_src, src_ip, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET6, &ip6->ip6_dst, dst_ip, INET6_ADDRSTRLEN);
    } else {
        inet_ntop(AF_INET, &ip->saddr, src_ip, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET, &ip->daddr, dst_ip, INET6_ADDRSTRLEN);
    }
    
    // Get RTP header fields
    const struct rtp_header *rtp = (const struct rtp_header *)payload;
    uint32_t ssrc = ntohl(rtp->ssrc);
    uint16_t seq = ntohs(rtp->sequence_number);
    uint32_t ts = ntohl(rtp->timestamp);
    uint8_t pt = rtp->payload_type;
    
    // Use SSRC directly without modification - fixes stream identification
    uint32_t stream_ssrc = ssrc;
    
    if (direction == DIRECTION_INCOMING) {
        DEBUG_RTP("Processing incoming RTP: %s:%d -> %s:%d SSRC=%u seq=%u PT=%d",
                 src_ip, src_port, dst_ip, dst_port, ssrc, seq, pt);
    } else {
        DEBUG_RTP("Processing outgoing RTP: %s:%d -> %s:%d SSRC=%u seq=%u PT=%d",
                 src_ip, src_port, dst_ip, dst_port, ssrc, seq, pt);
    }

    // Validate RTP packet
    if (!is_valid_rtp_payload_type(pt) || !is_valid_rtp_packet_size(payload_len, pt)) {
        return;
    }

    // Find or create stream
    struct rtp_stream *stream = find_or_create_stream(&current_session, src_ip, src_port, 
                                                    dst_ip, dst_port, stream_ssrc, pt, direction);
    if (!stream) {
        return;
    }

    // Set clock rate based on payload type if not already set
    if (stream->clock_rate == 0) {
        switch (pt) {
            case 0:  // PCMU
            case 8:  // PCMA
                stream->clock_rate = 8000;
                break;
            case 9:  // G722
                stream->clock_rate = 16000;
                break;
            default:
                stream->clock_rate = 8000;  // Default to 8kHz
        }
    }

    // Validate timestamp if not first packet
    if (stream->packets_received > 1 && 
        !is_valid_timestamp_jump(stream->last_timestamp, ts, stream->clock_rate)) {
        DEBUG_RTP("Warning: Large timestamp jump detected: prev=%u curr=%u",
                 stream->last_timestamp, ts);
    }

    // Process payload after basic validation
    process_rtp_payload(stream, payload, payload_len, pt, ts);

    // Validate and potentially correct timestamp
    validate_rtp_timestamp(stream, &ts, (uint32_t)header->ts.tv_sec);

    // Enhanced out-of-sequence handling
    if (!handle_out_of_sequence(stream, seq, ts, (uint32_t)header->ts.tv_sec)) {
        return;  // Packet will be processed later or dropped
    }
    
    // Stricter timestamp validation
    if (!validate_rtp_timestamp(stream, &ts, (uint32_t)header->ts.tv_sec)) {
        DEBUG_RTP("Using corrected timestamp for packet seq=%u", seq);
    }
    
    // Smoother silence insertion
    handle_silence_insertion(stream, seq);
    
    // Update jitter metrics
    update_jitter_metrics(stream, ts, (uint32_t)header->ts.tv_sec);
    
    // Update stream statistics with validated values
    update_stream_stats(stream, seq, ts, (uint32_t)header->ts.tv_sec);
    current_session.last_rtp_seen = header->ts.tv_sec;
    current_session.total_packets++;
}

/*
 * Validate RTP Packet
 * 
 * Checks if a packet is valid RTP:
 * - Version check
 * - Payload type validation
 * - Size validation
 * - SSRC verification
 * 
 * Parameters:
 * - payload: Packet payload
 * - payload_len: Payload length
 * 
 * Returns:
 * 1 if valid RTP, 0 if not
 */
int is_rtp_packet(const uint8_t *payload, int payload_len) {
    if (!payload || payload_len < MIN_RTP_SIZE) {
        return 0;
    }

    struct rtp_header *rtp = (struct rtp_header *)payload;

    // Check version (0-2 allowed for compatibility)
    if (rtp->version > 2) {
        return 0;
    }

    // Check payload type (dynamic range 96-127 also allowed)
    if (rtp->payload_type > 127) {
        return 0;
    }

    // Calculate minimum length including CSRC
    int min_length = sizeof(struct rtp_header) + (rtp->csrc_count * 4);
    if (rtp->extension) {
        min_length += 4;  // Extension header
    }
    if (rtp->padding) {
        if (payload_len <= min_length) {
            return 0;
        }
        uint8_t padding_len = payload[payload_len - 1];
        min_length += padding_len;
    }

    if (payload_len < min_length) {
        return 0;
    }

    return 1;
}