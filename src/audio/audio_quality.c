#include "audio_quality.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <opus/opus.h>

// Opus application type if not defined
#ifndef OPUS_APPLICATION_VOIP
#define OPUS_APPLICATION_VOIP 2048
#endif

#ifndef OPUS_OK
#define OPUS_OK 0
#endif

// Opus request constants if not defined
#ifndef OPUS_SET_COMPLEXITY_REQUEST
#define OPUS_SET_COMPLEXITY_REQUEST        4002
#define OPUS_SET_BITRATE_REQUEST          4004
#define OPUS_SET_INBAND_FEC_REQUEST       4012
#define OPUS_SET_DTX_REQUEST              4016
#define OPUS_SET_PACKET_LOSS_PERC_REQUEST 4014
#define OPUS_GET_BITRATE_REQUEST          4005
#endif

// Opus constants if not defined
#ifndef OPUS_SET_COMPLEXITY
#define OPUS_SET_COMPLEXITY(x) OPUS_SET_COMPLEXITY_REQUEST, __opus_check_int(x)
#endif

#ifndef OPUS_SET_BITRATE
#define OPUS_SET_BITRATE(x) OPUS_SET_BITRATE_REQUEST, __opus_check_int(x)
#endif

#ifndef OPUS_SET_INBAND_FEC
#define OPUS_SET_INBAND_FEC(x) OPUS_SET_INBAND_FEC_REQUEST, __opus_check_int(x)
#endif

#ifndef OPUS_SET_DTX
#define OPUS_SET_DTX(x) OPUS_SET_DTX_REQUEST, __opus_check_int(x)
#endif

#ifndef OPUS_SET_PACKET_LOSS_PERC
#define OPUS_SET_PACKET_LOSS_PERC(x) OPUS_SET_PACKET_LOSS_PERC_REQUEST, __opus_check_int(x)
#endif

#ifndef OPUS_GET_BITRATE
#define OPUS_GET_BITRATE(x) OPUS_GET_BITRATE_REQUEST, __opus_check_int_ptr(x)
#endif

// Using opus_defines.h macros for opus parameters
#include <stdio.h>
#include <limits.h>

// Constants for internal use
#define NOISE_PROFILE_SIZE 1024
#define ECHO_PROFILE_SIZE 1024
#define GAIN_PROFILE_SIZE 256
#define MAX_PREV_SAMPLES 960  // 60ms at 16kHz
#define DEFAULT_PLAYOUT_DELAY_MS 40
#define MAX_PACKET_SIZE 1500
#define MAX_SEQUENCE_HISTORY 1024

// Default high-quality settings
static const audio_enhance_config_t default_config = {
    .enable_denoise = 1,
    .enable_agc = 1,
    .enable_vad = 1,
    .enable_echo_cancel = 1,
    .plc_mode = PLC_MODE_ADVANCED,
    .comfort_noise_level = 30,
    .agc_target_level = -18,    // dB
    .noise_gate_threshold = -45, // dB
    .speech_expand_ratio = 1.2,  // Slight speech expansion
    .enable_dtx = 1,
    .opus = {
        .sample_rate = 48000,
        .channels = 1,
        .application = OPUS_APPLICATION_VOIP,
        .complexity = 10,
        .bitrate = 64000,
        .use_inband_fec = 1,
        .use_dtx = 1,
        .packet_loss_perc = 10,
        .max_payload_size = 1500,
        .jitter_control = {
            .min_delay_ms = 20,
            .max_delay_ms = 100,
            .target_delay_ms = 40,
            .jitter_factor = 1.5,
            .fec_lookahead_ms = 20,
            .sequence_history_size = 32
        }
    }
};

// Initialize sequence history
sequence_history_t* init_sequence_history(size_t size) {
    sequence_history_t* history = malloc(sizeof(sequence_history_t));
    if (!history) return NULL;
    
    history->sequences = calloc(size, sizeof(uint16_t));
    if (!history->sequences) {
        free(history);
        return NULL;
    }
    
    history->size = size;
    history->head = 0;
    history->count = 0;
    return history;
}

// Check if sequence number is out of order
bool is_sequence_out_of_order(sequence_history_t* history, uint16_t sequence) {
    if (!history || !history->sequences) return false;
    
    // Check against recent sequences
    for (size_t i = 0; i < history->count; i++) {
        size_t idx = (history->head - i - 1 + history->size) % history->size;
        if (history->sequences[idx] == sequence) {
            return true;  // Duplicate sequence
        }
    }
    
    // Add new sequence
    history->sequences[history->head] = sequence;
    history->head = (history->head + 1) % history->size;
    if (history->count < history->size) {
        history->count++;
    }
    
    return false;
}

// Enhanced packet insertion with timing control
int insert_packet_with_timing(audio_quality_ctx_t *ctx, audio_packet_t *packet) {
    if (!ctx || !packet) return -1;

    // Calculate expected play time based on timestamp and jitter buffer settings
    int64_t now = packet->arrival_time;
    int target_delay = ctx->config.opus.jitter_control.target_delay_ms * 1000; // Convert to microseconds
    packet->expected_play_time = now + target_delay;

    // Adjust target delay based on jitter
    if (ctx->stats.current_jitter > 0) {
        float jitter_delay = ctx->stats.current_jitter * ctx->config.opus.jitter_control.jitter_factor;
        int max_delay = ctx->config.opus.jitter_control.max_delay_ms * 1000;
        int adjusted_delay = (int)(target_delay + jitter_delay);
        if (adjusted_delay > max_delay) adjusted_delay = max_delay;
        packet->expected_play_time = now + adjusted_delay;
    }

    // Find insertion point based on expected play time
    int insert_pos = ctx->write_ptr;
    for (int i = 0; i < ctx->buffer_size; i++) {
        int idx = (ctx->read_ptr + i) % MAX_JITTER_BUFFER_PACKETS;
        if (packet->expected_play_time < ctx->buffer[idx].expected_play_time) {
            insert_pos = idx;
            break;
        }
    }

    // Handle buffer overflow
    if (ctx->buffer_size >= MAX_JITTER_BUFFER_PACKETS) {
        ctx->stats.packets_lost++;  // Count as a lost packet
        return -1;
    }

    // Shift packets if necessary
    if (ctx->buffer_size > 0 && insert_pos != ctx->write_ptr) {
        int move_start = insert_pos;
        int move_end = ctx->write_ptr;
        if (move_start > move_end) move_end += MAX_JITTER_BUFFER_PACKETS;
        
        for (int i = move_end - 1; i >= move_start; i--) {
            int from_idx = i % MAX_JITTER_BUFFER_PACKETS;
            int to_idx = (i + 1) % MAX_JITTER_BUFFER_PACKETS;
            ctx->buffer[to_idx] = ctx->buffer[from_idx];
        }
    }

    // Insert packet
    ctx->buffer[insert_pos] = *packet;
    if (insert_pos == ctx->write_ptr) {
        ctx->write_ptr = (ctx->write_ptr + 1) % MAX_JITTER_BUFFER_PACKETS;
    }
    ctx->buffer_size++;

    return 0;
}

// Initialize Opus encoder and decoder
int init_opus_codec(audio_quality_ctx_t *ctx) {
    int error;
    opus_config_t *config = &ctx->config.opus;

    // Create encoder
    OpusEncoder *enc = opus_encoder_create(
        config->sample_rate,
        config->channels,
        config->application,
        &error
    );
    if (error != OPUS_OK || !enc) {
        return -1;
    }
    ctx->opus.encoder = enc;

    // Configure encoder
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_COMPLEXITY(config->complexity));
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_BITRATE(config->bitrate));
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_INBAND_FEC(config->use_inband_fec));
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_DTX(config->use_dtx));
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_PACKET_LOSS_PERC(config->packet_loss_perc));

    // Create decoder
    OpusDecoder *dec = opus_decoder_create(
        config->sample_rate,
        config->channels,
        &error
    );
    if (error != OPUS_OK || !dec) {
        opus_encoder_destroy((OpusEncoder*)ctx->opus.encoder);
        return -1;
    }
    ctx->opus.decoder = dec;

    // Initialize frame size and buffer
    ctx->opus.frame_size = config->sample_rate / 50;  // 20ms frame
    ctx->opus.max_packet_size = config->max_payload_size;
    ctx->opus.opus_buffer = (uint8_t *)malloc(ctx->opus.max_packet_size);

    return 0;
}

// Process audio through Opus encoder
// Constants for OPUS enhancement
#define VOICE_ACTIVITY_THRESHOLD 0.1
#define BITRATE_ADJUSTMENT_STEP 1000
#define MIN_BITRATE 6000
#define MAX_BITRATE 64000

static void update_opus_bitrate(audio_quality_ctx_t *ctx) {
    if (!ctx || !ctx->opus.encoder) return;
    
    // Calculate packet loss rate over last second
    float loss_rate = ctx->stats.packet_loss_rate;
    
    // Adjust bitrate based on loss rate
    int32_t current_bitrate;
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_GET_BITRATE(&current_bitrate));
    
    if (loss_rate > 0.1) {  // High loss rate
        current_bitrate -= BITRATE_ADJUSTMENT_STEP;
    } else if (loss_rate < 0.01) {  // Low loss rate
        current_bitrate += BITRATE_ADJUSTMENT_STEP;
    }
    
    // Clamp to valid range
    if (current_bitrate < MIN_BITRATE) current_bitrate = MIN_BITRATE;
    if (current_bitrate > MAX_BITRATE) current_bitrate = MAX_BITRATE;
    
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_BITRATE(current_bitrate));
}

int process_opus_encode(audio_quality_ctx_t *ctx, const uint8_t *input, int input_size,
                       uint8_t *output, int max_output_size) {
    if (!ctx || !input || !output || input_size <= 0 || max_output_size <= 0) {
        return -1;
    }

    if ((size_t)input_size < ctx->opus.frame_size * sizeof(int16_t)) {
        return -1;  // Input buffer too small
    }

    if (max_output_size < ctx->opus.max_packet_size) {
        return -1;  // Output buffer too small
    }

    // Check for voice activity
    float energy = calculate_energy_level(input, input_size);
    int is_voice = (energy > VOICE_ACTIVITY_THRESHOLD);
    
    // Update encoder settings
    update_opus_bitrate(ctx);
    
    // Enable DTX for non-voice frames
    opus_encoder_ctl((OpusEncoder*)ctx->opus.encoder, OPUS_SET_DTX(is_voice ? 0 : 1));
    
    // Encode the frame
    int encoded_size = opus_encode((OpusEncoder*)ctx->opus.encoder,
        (const int16_t*)input,
        ctx->opus.frame_size,
        output,
        max_output_size);


    
    // Update statistics
    ctx->stats.last_frame_type = is_voice ? FRAME_TYPE_VOICE : FRAME_TYPE_DTX;
    ctx->stats.current_bitrate = encoded_size * 8 * (1000 / 20);  // Assuming 20ms frames
    
    return encoded_size;
}

// Process audio through Opus decoder
int process_opus_decode(audio_quality_ctx_t *ctx, const uint8_t *input, int input_size,
                       uint8_t *output, int max_output_size) {
    if (!ctx || !output || max_output_size <= 0) {
        return -1;
    }

    if ((size_t)max_output_size < ctx->opus.frame_size * sizeof(opus_int16)) {
        return -1;  // Output buffer too small
    }

    // Enable FEC if packet is lost
    if (!input || input_size <= 0) {
        opus_decoder_ctl(ctx->opus.decoder, OPUS_SET_PACKET_LOSS_PERC(100));
    } else {
        opus_decoder_ctl(ctx->opus.decoder, OPUS_SET_PACKET_LOSS_PERC(0));
    }

    // Decode with FEC if available
    int decoded_size = opus_decode((OpusDecoder*)ctx->opus.decoder,
        input,
        input_size,
        (int16_t*)output,
        max_output_size / sizeof(int16_t),
        0);


    
    // Update statistics
    ctx->stats.fec_used = (input == NULL);
    
    return decoded_size * sizeof(opus_int16);
}

// Initialize audio quality enhancement
audio_quality_ctx_t* audio_quality_init_with_config(const audio_enhance_config_t *config) {
    if (!config) {
        return NULL;
    }

    audio_quality_ctx_t *ctx = calloc(1, sizeof(audio_quality_ctx_t));
    if (!ctx) {
        return NULL;
    }

    // Copy configuration
    ctx->config = *config;

    // Initialize Opus codec
    if (init_opus_codec(ctx) != 0) {
        free(ctx);
        return NULL;
    }

    // Allocate buffers
    ctx->buffer = calloc(MAX_JITTER_BUFFER_PACKETS, sizeof(audio_packet_t));
    ctx->noise_profile = calloc(MAX_PACKET_SIZE, sizeof(float));
    ctx->echo_profile = calloc(MAX_PACKET_SIZE, sizeof(float));
    ctx->gain_profile = calloc(MAX_PACKET_SIZE, sizeof(float));
    ctx->prev_samples = calloc(MAX_PREV_SAMPLES, sizeof(float));

    if (!ctx->buffer || !ctx->noise_profile || !ctx->echo_profile ||
        !ctx->gain_profile || !ctx->prev_samples) {
        audio_quality_cleanup(ctx);
        return NULL;
    }

    // Initialize timing control
    ctx->adaptive_delay = DEFAULT_PLAYOUT_DELAY_MS * 1000; // Convert to microseconds
    ctx->prev_samples_count = 0;
    ctx->speech_threshold = -30.0;  // dB
    ctx->noise_threshold = -45.0;   // dB

    return ctx;
}

// Initialize with default settings
audio_quality_ctx_t* audio_quality_init(void) {
    return audio_quality_init_with_config(&default_config);
}

// Process incoming packet with enhanced error handling
int audio_quality_process_packet(audio_quality_ctx_t *ctx,
                               const uint8_t *payload,
                               int payload_size,
                               uint16_t sequence,
                               uint32_t timestamp,
                               int64_t arrival_time) {
    if (!ctx || !payload || payload_size <= 0) {
        return -1;
    }

    // Check for out-of-sequence packet
    if (is_sequence_out_of_order(ctx->sequence_history, sequence)) {
        ctx->stats.packets_lost++;  // Count out of order as lost
        // Still process the packet but mark it
        sequence = ctx->last_sequence + 1;
    }
    ctx->last_sequence = sequence;

    // Process through Opus encoder with FEC
    int encoded_size = process_opus_encode(ctx, payload, payload_size,
                                         ctx->opus.opus_buffer,
                                         ctx->opus.max_packet_size);
    if (encoded_size < 0) {
        return -1;
    }

    // Create new packet entry
    audio_packet_t new_packet = {
        .timestamp = timestamp,
        .sequence = sequence,
        .arrival_time = arrival_time,
        .payload = malloc(encoded_size),
        .payload_size = encoded_size,
        .energy_level = calculate_energy_level(payload, payload_size),
        .is_speech = detect_voice_activity(ctx, payload, payload_size),
        .quality_score = 1.0,
        .original_sequence = sequence,
        .is_fec_packet = 0
    };

    if (!new_packet.payload) {
        return -1;
    }
    memcpy(new_packet.payload, ctx->opus.opus_buffer, encoded_size);

    // Insert packet with timing control
    if (insert_packet_with_timing(ctx, &new_packet) != 0) {
        free(new_packet.payload);
        return -1;
    }

    // Generate FEC packet if enabled
    if (ctx->config.opus.use_inband_fec && ctx->config.opus.jitter_control.fec_lookahead_ms > 0) {
        uint8_t fec_buffer[MAX_PACKET_SIZE];
        opus_int32 frame_size;
        opus_encoder_ctl(ctx->opus.encoder, OPUS_GET_LOOKAHEAD(&frame_size));
        int fec_size = opus_encode(ctx->opus.encoder, (const opus_int16 *)payload,
                                 frame_size, fec_buffer, sizeof(fec_buffer));
        
        if (fec_size > 0) {
            audio_packet_t fec_packet = new_packet;
            fec_packet.payload = malloc(fec_size);
            if (fec_packet.payload) {
                fec_packet.payload_size = fec_size;
                fec_packet.is_fec_packet = 1;
                fec_packet.sequence = sequence + 1;
                memcpy(fec_packet.payload, fec_buffer, fec_size);
                insert_packet_with_timing(ctx, &fec_packet);
            }
        }
    }

    return 0;
}

// Get next packet for playout
int audio_quality_get_next_packet(audio_quality_ctx_t *ctx,
                                uint8_t *payload,
                                int max_size,
                                int64_t current_time) {
    if (!ctx || !payload || ctx->buffer_size == 0) {
        return 0;
    }

    audio_packet_t *next_packet = &ctx->buffer[ctx->read_ptr];
    int64_t expected_playout_time = next_packet->expected_play_time;

    if (current_time < expected_playout_time) {
        return 0;  // Too early to play
    }

    // Check if packet is too late
    if (is_packet_too_late(ctx, next_packet->timestamp, current_time)) {
        ctx->stats.packets_lost++;  // Count late packets as lost
        ctx->stats.plc_used = true;
        
        // Apply packet loss concealment
        int concealed_size = ctx->opus.frame_size * sizeof(opus_int16);
        apply_packet_loss_concealment(ctx, payload, concealed_size);
        ctx->stats.plc_duration_ms += 20;  // Assuming 20ms frames
        return concealed_size;
    }

    // Copy packet payload
    int copy_size = (next_packet->payload_size < max_size) ? 
                    next_packet->payload_size : max_size;
    memcpy(payload, next_packet->payload, copy_size);

    // Update buffer state
    free(next_packet->payload);
    ctx->read_ptr = (ctx->read_ptr + 1) % MAX_JITTER_BUFFER_PACKETS;
    ctx->buffer_size--;

    // Store samples for interpolation if needed
    if (ctx->config.plc_mode == PLC_MODE_ADVANCED) {
        int samples_to_store = ctx->opus.frame_size;
        if (samples_to_store > MAX_PREV_SAMPLES) {
            samples_to_store = MAX_PREV_SAMPLES;
        }
        memcpy(ctx->prev_samples, payload, samples_to_store * sizeof(float));
        ctx->prev_samples_count = samples_to_store;
    }

    return copy_size;
}

// Update configuration
int audio_quality_update_config(audio_quality_ctx_t *ctx,
                              const audio_enhance_config_t *config) {
    if (!ctx || !config) {
        return -1;
    }

    // Update configuration
    ctx->config = *config;

    // Reinitialize Opus codec with new settings
    opus_encoder_destroy(ctx->opus.encoder);
    opus_decoder_destroy(ctx->opus.decoder);
    return init_opus_codec(ctx);
}

// Get statistics
void audio_quality_get_stats(audio_quality_ctx_t *ctx, jitter_stats_t *stats) {
    if (!ctx || !stats) {
        return;
    }
    *stats = ctx->stats;
}

// Clean up resources
void audio_quality_cleanup(audio_quality_ctx_t *ctx) {
    if (!ctx) {
        return;
    }

    // Free Opus resources
    if (ctx->opus.encoder) {
        opus_encoder_destroy((OpusEncoder*)ctx->opus.encoder);
    }
    if (ctx->opus.decoder) {
        opus_decoder_destroy((OpusDecoder*)ctx->opus.decoder);
    }
    free(ctx->opus.opus_buffer);

    // Free buffers
    if (ctx->buffer) {
        for (int i = 0; i < ctx->buffer_size; i++) {
            int idx = (ctx->read_ptr + i) % MAX_JITTER_BUFFER_PACKETS;
            free(ctx->buffer[idx].payload);
        }
        free(ctx->buffer);
    }

    free(ctx->noise_profile);
    free(ctx->echo_profile);
    free(ctx->gain_profile);
    free(ctx->prev_samples);
    free(ctx);
}

// Helper function implementations
void adjust_playout_delay(audio_quality_ctx_t *ctx) {
    if (!ctx) return;

    // Calculate target delay based on jitter and packet loss
    double jitter_ms = ctx->stats.current_jitter / 1000.0;  // Convert to ms
    double loss_factor = (ctx->stats.plc_used ? 5.0 : 0.0);  // Increase buffer if PLC was used
    
    int min_delay = ctx->config.opus.jitter_control.min_delay_ms;
    int max_delay = ctx->config.opus.jitter_control.max_delay_ms;
    
    // Target delay = base delay + jitter buffer + loss compensation
    int target_delay = (int)(ctx->config.opus.jitter_control.target_delay_ms +
                            jitter_ms * ctx->config.opus.jitter_control.jitter_factor +
                            loss_factor);
    
    // Clamp to configured limits
    if (target_delay < min_delay) target_delay = min_delay;
    if (target_delay > max_delay) target_delay = max_delay;
    
    // Gradually adjust current delay
    if (target_delay > ctx->adaptive_delay) {
        ctx->adaptive_delay += 2000;  // Faster increase (2ms) for better protection
    } else if (target_delay < ctx->adaptive_delay) {
        ctx->adaptive_delay -= 1000;  // Slower decrease (1ms) for stability
    }
}

int is_packet_too_late(audio_quality_ctx_t *ctx, uint32_t timestamp, int64_t current_time) {
    if (!ctx) return 1;

    // Convert timestamp to microseconds based on sample rate
    int64_t timestamp_us = (int64_t)timestamp * 1000000 / ctx->config.opus.sample_rate;
    int64_t max_delay = ctx->config.opus.jitter_control.max_delay_ms * 1000; // Convert to microseconds
    int64_t packet_delay = current_time - timestamp_us;

    return packet_delay > max_delay;
}

int64_t calculate_packet_delay(const audio_quality_ctx_t *ctx, 
                                    uint32_t packet_timestamp, 
                                    int64_t current_time) {
    if (!ctx) return 0;
    
    int64_t timestamp_us = (int64_t)packet_timestamp * 1000000 / ctx->config.opus.sample_rate;
    return current_time - timestamp_us;
}

bool validate_packet(const audio_quality_ctx_t *ctx, const audio_packet_t *packet) {
    if (!ctx || !packet || !packet->payload || packet->payload_size <= 0) {
        return false;
    }
    
    if (packet->payload_size > MAX_PACKET_SIZE) {
        return false;
    }
    
    return true;
}

float calculate_energy_level(const uint8_t *payload, int size) {
    if (!payload || size <= 0) return 0.0f;

    const opus_int16 *samples = (const opus_int16 *)payload;
    int num_samples = size / sizeof(opus_int16);
    float sum = 0.0f;

    for (int i = 0; i < num_samples; i++) {
        float sample = samples[i] / 32768.0f;  // Normalize to [-1, 1]
        sum += sample * sample;
    }

    return (float)(10.0 * log10(sum / num_samples + 1e-10));  // Convert to dB
}

int detect_voice_activity(audio_quality_ctx_t *ctx, const uint8_t *payload, int size) {
    if (!ctx || !payload || size <= 0) return 0;

    float db_level = calculate_energy_level(payload, size);
    return db_level > ctx->speech_threshold;
}

// Constants for PLC enhancement
#define PLC_FADE_LENGTH_MS 5
#define PLC_CROSSFADE_LENGTH_MS 2.5
#define MAX_PLC_HISTORY_MS 60

void apply_packet_loss_concealment(audio_quality_ctx_t *ctx, uint8_t *output, int size) {
    if (!ctx || !output || size <= 0) return;

    switch (ctx->config.plc_mode) {
        case PLC_MODE_SILENCE:
            memset(output, 0, size);
            break;

        case PLC_MODE_REPEAT:
            if (ctx->prev_samples_count > 0) {
                // Calculate sizes in samples
                int sample_rate = ctx->opus.sample_rate;
                int fade_samples = (MAX_FADE_LENGTH_MS * sample_rate) / 1000;
                int copy_size = (ctx->prev_samples_count < size) ? 
                               ctx->prev_samples_count : size;

                // Copy with fade out
                int16_t *out = (int16_t *)output;
                int16_t *prev = (int16_t *)ctx->prev_samples;
                int out_samples = copy_size / sizeof(int16_t);

                for (int i = 0; i < out_samples; i++) {
                    float fade = 1.0f;
                    if (i >= out_samples - fade_samples) {
                        fade = (float)(out_samples - i) / fade_samples;
                    }
                    out[i] = (int16_t)(prev[i] * fade);
                }
            }
            break;

        case PLC_MODE_ADVANCED:
            // Use advanced PLC with pattern matching and psychoacoustic masking
            if (ctx->prev_samples_count > 0) {
                int16_t *out = (int16_t *)output;
                int16_t *prev = (int16_t *)ctx->prev_samples;
                int out_samples = size / sizeof(int16_t);
                int prev_samples = ctx->prev_samples_count / sizeof(int16_t);

                // Find best matching segment using cross-correlation
                int best_offset = 0;
                float best_correlation = -1;
                for (int offset = 0; offset < prev_samples - ANALYSIS_WINDOW_SIZE; offset++) {
                    float correlation = 0;
                    for (int i = 0; i < ANALYSIS_WINDOW_SIZE; i++) {
                        correlation += prev[offset + i] * prev[i];
                    }
                    if (correlation > best_correlation) {
                        best_correlation = correlation;
                        best_offset = offset;
                    }
                }

                // Generate concealment using pattern matching and psychoacoustic masking
                for (int i = 0; i < out_samples; i++) {
                    // Get sample from best matching segment
                    int hist_idx = (best_offset + i) % prev_samples;
                    int16_t pattern_sample = prev[hist_idx];

                    // Calculate local energy for psychoacoustic masking
                    float local_energy = 0;
                    int window_start = (i < ANALYSIS_WINDOW_SIZE/2) ? 0 : (i - ANALYSIS_WINDOW_SIZE/2);
                    int window_end = (i + ANALYSIS_WINDOW_SIZE/2 > prev_samples) ? prev_samples : (i + ANALYSIS_WINDOW_SIZE/2);
                    for (int j = window_start; j < window_end; j++) {
                        local_energy += fabs((float)prev[j] / 32768.0f);
                    }
                    local_energy /= (window_end - window_start);

                    // Apply fade-out at end of concealment
                    float fade = 1.0f;
                    if (i >= out_samples - MAX_FADE_LENGTH_MS) {
                        fade = (float)(out_samples - i) / MAX_FADE_LENGTH_MS;
                    }

                    // Mix with comfort noise based on psychoacoustic masking
                    float noise_level = ctx->config.comfort_noise_level / 32768.0f;
                    float noise = ((float)rand() / RAND_MAX * 2 - 1) * noise_level;
                    noise *= (1.0f - local_energy) * fade;

                    out[i] = (int16_t)(pattern_sample * fade + noise * 32768);
                }
            } else {
                // Fallback to Opus PLC if no history
                process_opus_decode(ctx, NULL, 0, output, size);
            }
            break;

        default: {
            // Generate comfort noise with psychoacoustic shaping
            int16_t *out_buf = (int16_t *)output;
            int out_samples = size / sizeof(int16_t);
            float noise_level = ctx->config.comfort_noise_level / 32768.0f;
            float base_energy_factor = 1.0f;  // Can be adjusted based on context
            float sample_energy_factor;
            float random;
            int16_t *prev;
            int prev_idx;
            int i;
            
            for (i = 0; i < out_samples; i++) {
                random = (float)rand() / RAND_MAX * 2.0f - 1.0f;
                // Shape noise based on previous sample energy if available
                sample_energy_factor = base_energy_factor;
                if (ctx->prev_samples_count > 0) {
                    prev = (int16_t *)ctx->prev_samples;
                    prev_idx = i % (ctx->prev_samples_count / sizeof(int16_t));
                    sample_energy_factor *= fabs((float)prev[prev_idx] / 32768.0f);
                }
                out_buf[i] = (int16_t)(random * noise_level * sample_energy_factor * 32768.0f);
            }
        }
            break;
    }

    // Update statistics
    ctx->stats.plc_used = true;
    ctx->stats.plc_duration_ms += size * 1000 / (ctx->opus.sample_rate * sizeof(int16_t));
}


