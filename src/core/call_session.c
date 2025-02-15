/*
 * Call Session Management Implementation
 * 
 * This module manages VoIP call session state tracking and analysis.
 * It maintains information about active calls, including:
 * - SIP dialog state
 * - RTP stream statistics
 * - Call quality metrics
 * - Session timing information
 *
 * The implementation supports:
 * - Multiple RTP stream tracking
 * - Call quality analysis
 * - Session state transitions
 * - Statistical data collection
 */

#include "call_session.h"
#include "debug.h"
#include "packet_capture.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Global session state
struct call_session current_session;

/*
 * Initialize Call Session
 * 
 * Sets up a new call session structure with default values:
 * - Zeroes all counters and statistics
 * - Sets initial dialog state
 * - Records session start time
 * - Prepares RTP stream tracking
 */
void init_call_session(struct call_session *session) {
    if (!session) return;
    
    // Initialize all fields to 0
    memset(session, 0, sizeof(struct call_session));
    
    // Initialize stream array
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        session->streams[i].active = 0;
        session->streams[i].ssrc = 0;
        session->streams[i].payload_type = 0;
        session->streams[i].direction = 0;
        
        // Initialize timespec structs to zero
        session->streams[i].start_time.tv_sec = 0;
        session->streams[i].start_time.tv_nsec = 0;
        session->streams[i].last_packet_time.tv_sec = 0;
        session->streams[i].last_packet_time.tv_nsec = 0;
        
        // Clear IP addresses
        memset(session->streams[i].src_ip, 0, INET6_ADDRSTRLEN);
        memset(session->streams[i].dst_ip, 0, INET6_ADDRSTRLEN);
        memset(session->streams[i].nat64_ip, 0, INET6_ADDRSTRLEN);
        session->streams[i].src_port = 0;
        session->streams[i].dst_port = 0;
        session->streams[i].nat64_port = 0;
    }
    
    // Set initial state
    session->sip_state = SIP_STATE_INIT;
    session->start_time = time(NULL);
    session->dialog.state = DIALOG_INIT;
}

void reset_session(struct call_session *session) {
    if (!session) return;
    
    // Reset all session fields
    session->total_packets = 0;
    session->sip_packets = 0;
    session->start_time = 0;
    session->last_rtp_seen = 0;
    session->last_sip_seen = 0;
    session->last_bye_seen = 0;
    session->sip_state = SIP_STATE_INIT;
    session->dialog.state = DIALOG_INIT;
    
    // Reset all streams
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        session->streams[i].active = 0;
        session->streams[i].ssrc = 0;
        session->streams[i].payload_type = 0;
        session->streams[i].direction = 0;
        session->streams[i].packets_received = 0;
        session->streams[i].lost_packets = 0;
        session->streams[i].out_of_order = 0;
        session->streams[i].base_seq = 0;
        session->streams[i].max_seq = 0;
        session->streams[i].bad_seq = 0;
        session->streams[i].cycles = 0;
        session->streams[i].received = 0;
        session->streams[i].received_prior = 0;
        session->streams[i].expected_prior = 0;
        session->streams[i].probation = MIN_SEQUENTIAL;
        session->streams[i].last_timestamp = 0;
        session->streams[i].transit = 0;
        session->streams[i].jitter = 0;
        session->streams[i].start_time.tv_sec = 0;
        session->streams[i].start_time.tv_nsec = 0;
        session->streams[i].last_packet_time.tv_sec = 0;
        session->streams[i].last_packet_time.tv_nsec = 0;
        session->streams[i].clock_rate = 0;
        
        // Clear IP addresses
        memset(session->streams[i].src_ip, 0, INET6_ADDRSTRLEN);
        memset(session->streams[i].dst_ip, 0, INET6_ADDRSTRLEN);
        memset(session->streams[i].nat64_ip, 0, INET6_ADDRSTRLEN);
        session->streams[i].src_port = 0;
        session->streams[i].dst_port = 0;
        session->streams[i].nat64_port = 0;
    }
}

/*
 * Check Call Activity Status
 * 
 * Determines if a call is currently active by checking:
 * - SIP dialog state (ESTABLISHED vs TERMINATED)
 * - RTP stream activity and timeouts
 * - Number of active media streams
 * 
 * Returns:
 * 1 if call is active
 * 0 if call is inactive or terminated
 */
int is_call_active(const struct call_session *session) {
    if (!session) return 0;
    
    // Check if call is established and not terminated
    if (session->dialog.state != DIALOG_ESTABLISHED) {
        // If call was established but now terminated, return 0
        if (session->dialog.state == DIALOG_TERMINATED) {
            DEBUG_PRINT("Call terminated via SIP signaling");
            return 0;
        }
        return 0;
    }
    
    // Check for RTP activity
    time_t now = time(NULL);
    if (now - session->last_rtp_seen > RTP_TIMEOUT) {
        DEBUG_PRINT("No RTP activity for %d seconds", RTP_TIMEOUT);
        return 0;
    }
    
    // Check if any streams are active
    int active_streams = 0;
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        if (session->streams[i].active) {
            active_streams++;
        }
    }
    
    if (active_streams == 0) {
        DEBUG_PRINT("No active RTP streams");
    }
    
    return active_streams > 0;
}

/*
 * Get Session Statistics
 * 
 * Retrieves overall session statistics including:
 * - Total packets processed
 * - SIP signaling packets
 * - Session duration
 * 
 * Parameters are nullable for flexible data retrieval
 */
void get_session_stats(const struct call_session *session,
                      uint32_t *total_packets,
                      uint32_t *sip_packets,
                      uint32_t *rtp_streams) {
    if (!session) return;
    
    if (total_packets) *total_packets = session->total_packets;
    if (sip_packets) *sip_packets = session->sip_packets;
    
    if (rtp_streams) {
        *rtp_streams = 0;
        for (int i = 0; i < MAX_RTP_STREAMS; i++) {
            if (session->streams[i].active) (*rtp_streams)++;
        }
    }
}

/*
 * Get Call Quality Statistics
 * 
 * Calculates and returns quality metrics:
 * - Average jitter across streams
 * - Total lost packets
 * - Out-of-order packet count
 * 
 * Aggregates data from all active RTP streams
 */
void get_call_quality_stats(const struct call_session *session,
                           double *avg_jitter,
                           uint32_t *lost_packets,
                           uint32_t *out_of_order) {
    if (!session) return;
    
    double total_jitter = 0;
    uint32_t total_lost = 0;
    uint32_t total_ooo = 0;
    int active_streams = 0;
    
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        if (session->streams[i].active) {
            total_jitter += session->streams[i].jitter;
            total_lost += session->streams[i].lost_packets;
            total_ooo += session->streams[i].out_of_order;
            active_streams++;
        }
    }
    
    if (avg_jitter) {
        *avg_jitter = active_streams > 0 ? total_jitter / active_streams : 0;
    }
    if (lost_packets) *lost_packets = total_lost;
    if (out_of_order) *out_of_order = total_ooo;
}

/*
 * Get Individual Stream Metrics
 * 
 * Retrieves metrics for a single RTP stream:
 * - Stream-specific jitter
 * - Packet loss count
 * - Out-of-order packets
 * 
 * Used for detailed per-stream analysis
 */
void get_stream_metrics(const struct rtp_stream *stream,
                       double *jitter,
                       uint32_t *lost,
                       uint32_t *out_of_order) {
    if (!stream) return;
    
    if (jitter) *jitter = stream->jitter;
    if (lost) *lost = stream->lost_packets;
    if (out_of_order) *out_of_order = stream->out_of_order;
}

/*
 * Cleanup Call Session
 * 
 * Releases resources and resets session state:
 * - Frees stream info structures
 * - Resets stream data
 * - Zeroes session counters and statistics
 */
void cleanup_call_session(struct call_session *session) {
    if (!session) return;
    
    // Free stream info structures
    for (int i = 0; i < MAX_RTP_STREAMS; i++) {
        if (session->stream_info[i]) {
            free(session->stream_info[i]);
            session->stream_info[i] = NULL;
        }
        // Reset stream data
        session->streams[i].active = 0;
        session->streams[i].jitter = 0;
        session->streams[i].lost_packets = 0;
        session->streams[i].out_of_order = 0;
        session->streams[i].base_seq = 0;
        session->streams[i].last_timestamp = 0;
    }
    
    // Reset session data
    memset(session, 0, sizeof(struct call_session));
}