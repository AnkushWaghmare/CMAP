/*
 * RTP (Real-time Transport Protocol) Type Definitions
 * 
 * This header defines data structures and types used for RTP stream tracking
 * and analysis. It includes structures for maintaining stream state, statistics,
 * and NAT64 translation information.
 *
 * Key Features:
 * - RTP stream state tracking
 * - Stream statistics collection
 * - NAT64 translation support
 * - Traffic direction classification
 */

#ifndef RTP_TYPES_H
#define RTP_TYPES_H

#include <stdint.h>
#include <time.h>
#include <netinet/in.h> // Added for INET6_ADDRSTRLEN
#include "audio_quality.h" // Add audio quality header

/*
 * Stream Limits and Buffer Sizes
 */
#define MAX_STREAMS 32     // Maximum number of concurrent RTP streams

/*
 * Traffic Direction Enumeration
 * Used to classify packet flow direction:
 * - UNKNOWN: Direction not yet determined
 * - INCOMING: Traffic from external to local
 * - OUTGOING: Traffic from local to external
 * - LOCAL: Traffic between local endpoints
 */
enum {
    DIRECTION_UNKNOWN = 0,
    DIRECTION_INCOMING,
    DIRECTION_OUTGOING,
    DIRECTION_LOCAL
};

/*
 * RTP Stream Structure
 * Maintains state and statistics for a single RTP stream
 * 
 * Stream Identification:
 * - active: Stream activity flag
 * - ssrc: Synchronization source identifier
 * - payload_type: RTP payload type (codec)
 * - direction: Traffic direction
 *
 * Network Addresses:
 * - src_ip/port: Source endpoint
 * - dst_ip/port: Destination endpoint
 * - nat64_ip/port: NAT64 translation
 *
 * Stream Statistics:
 * - packets_received: Total packet count
 * - lost_packets: Missing packets
 * - out_of_order: Sequence violations
 * - last_seq: Last sequence number
 * - last_timestamp: Last RTP timestamp
 * - transit: Inter-packet timing
 * - jitter: Timing variation
 * - start_time: Stream creation time
 * - last_packet_time: Last activity
 */
struct rtp_stream {
    int active;                     // Stream is active
    uint32_t ssrc;                 // RTP SSRC identifier
    uint8_t payload_type;          // RTP payload type
    int direction;                 // Traffic direction
    
    // Network endpoints
    char src_ip[INET6_ADDRSTRLEN]; // Source IP
    uint16_t src_port;             // Source port
    char dst_ip[INET6_ADDRSTRLEN]; // Destination IP
    uint16_t dst_port;             // Destination port
    
    // NAT64 handling
    char nat64_ip[INET6_ADDRSTRLEN]; // NAT64 translated address
    uint16_t nat64_port;             // NAT64 translated port

    // Stream statistics
    uint32_t packets_received;     // Total packets received
    uint32_t lost_packets;         // Number of lost packets
    uint32_t out_of_order;        // Out of order packets
    uint32_t clock_rate;          // RTP clock rate (for jitter)
    uint32_t inserted_silence_ms;  // Total inserted silence duration
    uint32_t corrected_timestamps; // Count of corrected timestamps
    uint32_t jitter_spikes;       // Count of significant jitter spikes
    
    // Sequence tracking
    uint16_t base_seq;            // First sequence number
    uint16_t max_seq;             // Highest sequence number received
    uint16_t last_seq;            // Last sequence number processed
    uint16_t bad_seq;             // Last 'bad' sequence number
    uint32_t cycles;              // Count of sequence number cycles
    uint32_t received;            // Packets received since last wrap
    uint32_t received_prior;      // Packets received before last wrap
    
    // Jitter calculation
    double smoothed_jitter;       // Exponentially smoothed jitter
    
    // Stream statistics
    struct {
        uint32_t buffer_size_ms;    // Current jitter buffer size in ms
        uint32_t buffer_target_ms;   // Target buffer size in ms
        double packet_loss_rate;     // Current packet loss rate
        uint32_t last_frame_type;    // Last processed frame type
        uint32_t current_bitrate;    // Current encoding bitrate
        bool fec_used;               // FEC was used for recovery
        bool plc_used;               // PLC was used for concealment
        uint32_t plc_duration_ms;    // Total PLC duration in ms
    } stats;
    
    // Audio frame handling
    uint32_t frame_size;          // Size of audio frame in samples
    void *last_good_frame;        // Last successfully received frame
    void *current_frame;          // Current frame being processed
    void *plc_buffer;            // Packet loss concealment buffer
    
    // Jitter and quality metrics
    uint32_t total_packets;       // Total packets expected
    uint32_t recovered_packets;   // Packets recovered via FEC
    uint32_t concealed_ms;       // Duration of concealed audio
    uint32_t expected_prior;      // Packets expected before last wrap
    uint8_t probation;           // Sequence validation counter
    
    // Timing and jitter
    uint32_t last_timestamp;      // Last RTP timestamp
    int32_t transit;             // Relative transit time
    double jitter;               // Estimated jitter (RFC 3550)
    
    // Enhanced quality metrics
    uint32_t consecutive_losses;   // Count of consecutive packet losses
    double loss_rate;             // Short-term packet loss rate
    double mean_frame_size;       // Average audio frame size
    
    // Adaptive jitter buffer
    uint32_t min_playout_delay;   // Minimum playout delay (ms)
    uint32_t max_playout_delay;   // Maximum playout delay (ms)
    uint32_t optimal_delay;       // Current optimal playout delay
    double buffer_adaptation_rate; // Rate of buffer size adjustment
    
    // Timing stats
    struct timespec start_time;    // Stream start time
    struct timespec last_packet_time; // Last packet arrival
    
    // Quality enhancement
    audio_quality_ctx_t *audio_ctx; // Audio quality enhancement context
};

// Quality thresholds
#define MAX_JITTER_SPIKE_MS 10      // Jitter spike threshold (ms)
#define MAX_TIMESTAMP_DEVIATION_MS 10 // Max allowed timestamp deviation (reduced from 20ms)
#define MAX_OOO_WINDOW 50           // Max out-of-order window size in packets
#define MAX_REORDER_WAIT_MS 40      // Max time to wait for out-of-order packet
#define MIN_SEQUENCE_SPACING 2      // Min sequence numbers between packets

// Codec-specific configurations
#define PCMU_SAMPLES_PER_FRAME 160  // 8kHz * 20ms
#define G722_SAMPLES_PER_FRAME 320  // 16kHz * 20ms
#define DEFAULT_SAMPLES_PER_FRAME(rate) ((rate)/50)  // 20ms worth of samples

// Jitter Buffer Configuration
#define MIN_JITTER_BUFFER_MS 20    // Minimum jitter buffer size in ms
#define MAX_JITTER_BUFFER_MS 150   // Maximum jitter buffer size in ms
#define BUFFER_ADAPT_THRESHOLD 50  // Packet count before adaptation
#define LOSS_WINDOW_SIZE 100      // Window for loss rate calculation

#endif // RTP_TYPES_H