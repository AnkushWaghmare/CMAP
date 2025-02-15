/*
 * Call Session Management Module
 * 
 * This module manages VoIP call sessions, including SIP dialog state,
 * RTP stream tracking, and call statistics collection. It provides
 * structures and functions for comprehensive call monitoring.
 *
 * Key Features:
 * - Call session state tracking
 * - RTP stream management
 * - Call quality metrics
 * - Session statistics
 */

#ifndef CALL_SESSION_H
#define CALL_SESSION_H

#include "rtp_types.h"
#include "sip_utils.h"
#include <time.h>

/*
 * Stream Limits
 */
#define MAX_RTP_STREAMS 8    // Maximum concurrent RTP streams per call

/*
 * RTP Stream Information Structure
 * Contains SDP-derived information about an RTP stream
 */
struct rtp_stream_info {
    int direction;           // Stream direction (in/out)
    uint16_t port;          // RTP port number
    int payload_type;       // RTP payload type
    char codec[32];         // Codec name
    int sample_rate;        // Audio sample rate
    char fmtp[128];         // Format parameters
};

/*
 * Call Session State
 */
enum sip_state {
    SIP_STATE_INIT = 0,
    SIP_STATE_INVITE_SENT,
    SIP_STATE_RINGING,
    SIP_STATE_ESTABLISHED,
    SIP_STATE_TERMINATED
};

/*
 * Call Session Structure
 * Main structure for tracking an active call session
 * 
 * Components:
 * - SIP dialog state tracking
 * - Call state information
 * - RTP stream array
 * - Stream information array
 * - Session statistics
 */
struct call_session {
    // Session state
    enum sip_state sip_state;          // Current SIP state
    struct sip_dialog dialog;          // SIP dialog state
    time_t start_time;                 // Session start time
    time_t last_rtp_seen;             // Last RTP packet time
    time_t last_sip_seen;             // Last SIP packet time
    time_t last_bye_seen;             // Last BYE request time

    // Packet statistics
    uint32_t total_packets;           // Total packets processed
    uint32_t sip_packets;             // Total SIP packets

    // RTP streams
    struct rtp_stream streams[MAX_RTP_STREAMS];
    struct rtp_stream_info *stream_info[MAX_RTP_STREAMS];
};

/*
 * Global Session Variable
 * Current active call session
 */
extern struct call_session current_session;

/*
 * Initialize Call Session
 * 
 * Sets up a new call session structure with default values
 * 
 * Parameters:
 * - session: Session to initialize
 */
void init_call_session(struct call_session *session);

/*
 * Reset Session
 * 
 * Resets a call session to its initial state
 * 
 * Parameters:
 * - session: Session to reset
 */
void reset_session(struct call_session *session);

/*
 * Check Call Activity
 * 
 * Determines if a call session is currently active
 * 
 * Parameters:
 * - session: Session to check
 * 
 * Returns:
 * 1 if active, 0 if inactive
 */
int is_call_active(const struct call_session *session);

/*
 * Get Session Statistics
 * 
 * Retrieves current session statistics
 * 
 * Parameters:
 * - session: Call session to get stats from
 * - total_packets: Total packets processed (optional)
 * - sip_packets: Total SIP packets (optional)
 * - rtp_streams: Number of active RTP streams (optional)
 */
void get_session_stats(const struct call_session *session,
                      uint32_t *total_packets,
                      uint32_t *sip_packets,
                      uint32_t *rtp_streams);

/*
 * Get Call Quality Statistics
 * 
 * Retrieves quality metrics for the call
 * 
 * Parameters:
 * - session: Source session
 * - avg_jitter: Average jitter
 * - lost_packets: Lost packet count
 * - out_of_order: Out of order count
 */
void get_call_quality_stats(const struct call_session *session,
                           double *avg_jitter,
                           uint32_t *lost_packets,
                           uint32_t *out_of_order);

/*
 * Get Stream Metrics
 * 
 * Retrieves metrics for a specific RTP stream
 * 
 * Parameters:
 * - stream: Source stream
 * - jitter: Stream jitter
 * - lost: Lost packet count
 * - out_of_order: Out of order count
 */
void get_stream_metrics(const struct rtp_stream *stream,
                       double *jitter,
                       uint32_t *lost,
                       uint32_t *out_of_order);

/*
 * Cleanup Call Session
 * 
 * Frees resources associated with a call session
 * 
 * Parameters:
 * - session: Session to cleanup
 */
void cleanup_call_session(struct call_session *session);

#endif // CALL_SESSION_H