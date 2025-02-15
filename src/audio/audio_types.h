#ifndef AUDIO_TYPES_H
#define AUDIO_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Frame types for audio processing
#define FRAME_TYPE_VOICE 1
#define FRAME_TYPE_DTX   2
#define FRAME_TYPE_CNG   3

// Audio analysis constants
#define ANALYSIS_WINDOW_SIZE 160
#define MAX_FADE_LENGTH_MS 20

#include <opus/opus.h>

typedef struct {
    OpusEncoder *encoder;         // OPUS encoder state
    OpusDecoder *decoder;         // OPUS decoder state
    int sample_rate;             // Sample rate in Hz
    int channels;                // Number of channels
    int application;             // OPUS application type
    int frame_size;              // Frame size in samples
    int max_packet_size;         // Maximum packet size
    uint8_t *opus_buffer;        // Opus working buffer
} opus_state_t;

typedef struct {
    uint32_t packets_received;     // Total packets received
    uint32_t packets_lost;         // Total packets lost
    uint32_t dropped_packets;      // Packets dropped due to buffer overflow
    uint32_t late_packets;         // Packets that arrived too late
    uint32_t out_of_order_packets; // Out of order packets
    uint32_t plc_events;           // Number of PLC events
    uint32_t concealed_ms;         // Total concealed milliseconds
    double current_jitter;         // Current jitter in microseconds
    double max_jitter;             // Maximum jitter seen
    uint32_t buffer_size;          // Current jitter buffer size
    uint32_t buffer_target;        // Target jitter buffer size
    double packet_loss_rate;       // Current packet loss rate
    uint32_t last_frame_type;      // Last processed frame type
    uint32_t current_bitrate;      // Current encoding bitrate
    bool fec_used;                 // FEC was used for recovery
    bool plc_used;                 // PLC was used for concealment
    uint32_t plc_duration_ms;      // Total PLC duration in ms
} jitter_stats_t;

#endif // AUDIO_TYPES_H
