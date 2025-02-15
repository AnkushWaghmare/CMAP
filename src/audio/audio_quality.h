#ifndef AUDIO_QUALITY_H
#define AUDIO_QUALITY_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include "rtp_defs.h"
#include "audio_types.h"

// Constants
#define MAX_JITTER_BUFFER_PACKETS 1000
#define MAX_PACKET_SIZE 1500
#define DEFAULT_PLAYOUT_DELAY_MS 40
#define MAX_PREV_SAMPLES 960  // 60ms at 16kHz

// Packet Loss Concealment modes
typedef enum {
    PLC_MODE_SILENCE,     // Fill with silence
    PLC_MODE_REPEAT,      // Repeat last good frame
    PLC_MODE_PATTERN,     // Pattern matching based
    PLC_MODE_ADVANCED     // Advanced interpolation
} plc_mode_t;

// Jitter control parameters
typedef struct {
    int min_delay_ms;         // Minimum delay in milliseconds
    int max_delay_ms;         // Maximum delay in milliseconds
    int target_delay_ms;      // Target delay in milliseconds
    float jitter_factor;      // Factor to multiply jitter by
    int fec_lookahead_ms;    // FEC lookahead window in milliseconds
    size_t sequence_history_size;  // Size of sequence history buffer
} jitter_control_t;



// Opus codec configuration
typedef struct {
    int sample_rate;          // Sample rate in Hz
    int channels;             // Number of channels
    int application;          // OPUS_APPLICATION_VOIP or OPUS_APPLICATION_AUDIO
    int complexity;           // Encoding complexity (0-10)
    int bitrate;             // Target bitrate in bits per second
    int use_inband_fec;      // Use forward error correction
    int use_dtx;             // Use discontinuous transmission
    int packet_loss_perc;    // Expected packet loss percentage
    int max_payload_size;    // Maximum payload size in bytes
    jitter_control_t jitter_control;  // Jitter control settings
} opus_config_t;

// Audio enhancement configuration
typedef struct {
    int enable_denoise;       // Enable noise reduction
    int enable_agc;          // Enable automatic gain control
    int enable_vad;          // Enable voice activity detection
    int enable_echo_cancel;  // Enable echo cancellation
    plc_mode_t plc_mode;     // Packet loss concealment mode
    int comfort_noise_level; // Comfort noise level (0-100)
    double agc_target_level; // Target level for AGC in dB
    double noise_gate_threshold; // Noise gate threshold in dB
    double speech_expand_ratio;  // Speech expansion ratio
    int enable_dtx;          // Enable discontinuous transmission
    opus_config_t opus;      // Opus-specific settings
} audio_enhance_config_t;

// Sequence history for detecting out-of-order packets
typedef struct {
    uint16_t *sequences;     // Circular buffer of sequence numbers
    size_t size;            // Size of the buffer
    size_t head;            // Current head position
    size_t count;           // Number of valid entries
} sequence_history_t;

// Audio packet structure
typedef struct {
    uint8_t *payload;        // Audio payload data
    int payload_size;        // Size of payload in bytes
    uint32_t timestamp;      // RTP timestamp
    uint16_t sequence;       // RTP sequence number
    int64_t arrival_time;    // Arrival time in microseconds
    int64_t expected_play_time;  // Expected playout time
    float energy_level;      // Energy level in dB
    bool is_speech;          // Voice activity flag
    float quality_score;     // Quality score (0-1)
    uint16_t original_sequence;  // Original sequence number
    bool is_fec_packet;      // FEC packet flag
} audio_packet_t;



// Main audio quality context
typedef struct {
    audio_enhance_config_t config;  // Configuration settings
    opus_state_t opus;             // Opus codec state
    audio_packet_t *buffer;        // Jitter buffer
    float *noise_profile;          // Noise reduction profile
    jitter_stats_t stats;          // Jitter and quality statistics
    float *echo_profile;           // Echo cancellation profile
    float *gain_profile;           // Gain control profile
    float *prev_samples;           // Previous samples for PLC
    int prev_samples_count;        // Count of previous samples
    sequence_history_t *sequence_history;  // Sequence tracking
    int buffer_size;               // Current buffer size
    int write_ptr;                 // Write pointer
    int read_ptr;                  // Read pointer
    uint16_t last_sequence;        // Last sequence number
    uint32_t last_timestamp;       // Last timestamp
    int64_t last_playout_time;     // Last playout time
    int64_t adaptive_delay;        // Adaptive playout delay
    float speech_threshold;        // Speech detection threshold
    float noise_threshold;         // Noise gate threshold

} audio_quality_ctx_t;

// Function declarations
sequence_history_t* init_sequence_history(size_t size);
bool is_sequence_out_of_order(sequence_history_t *history, uint16_t sequence);
int insert_packet_with_timing(audio_quality_ctx_t *ctx, audio_packet_t *packet);
audio_quality_ctx_t* audio_quality_init(void);
audio_quality_ctx_t* audio_quality_init_with_config(const audio_enhance_config_t *config);
int audio_quality_process_packet(audio_quality_ctx_t *ctx, const uint8_t *payload, int payload_size,
                               uint16_t sequence, uint32_t timestamp, int64_t arrival_time);
int audio_quality_get_next_packet(audio_quality_ctx_t *ctx, uint8_t *payload, int max_size,
                                int64_t current_time);
int audio_quality_update_config(audio_quality_ctx_t *ctx, const audio_enhance_config_t *config);
void audio_quality_get_stats(audio_quality_ctx_t *ctx, jitter_stats_t *stats);
void audio_quality_cleanup(audio_quality_ctx_t *ctx);

// Helper function declarations
float calculate_energy_level(const uint8_t *payload, int size);
int detect_voice_activity(audio_quality_ctx_t *ctx, const uint8_t *payload, int size);
void apply_packet_loss_concealment(audio_quality_ctx_t *ctx, uint8_t *output, int size);
// Removed external declaration of generate_comfort_noise - now static in rtp_utils.c
int is_packet_too_late(audio_quality_ctx_t *ctx, uint32_t timestamp, int64_t current_time);
void adjust_playout_delay(audio_quality_ctx_t *ctx);

#endif // AUDIO_QUALITY_H
