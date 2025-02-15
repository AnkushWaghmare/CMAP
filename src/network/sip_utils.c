/*
 * SIP (Session Initiation Protocol) Utilities Implementation
 * 
 * This module implements SIP message parsing and analysis for the
 * Call Monitor and Analyzer Program (CMAP). It handles SIP signaling
 * and SDP content parsing for VoIP call setup and teardown.
 *
 * Key Features:
 * - SIP message parsing
 * - SDP content extraction
 * - Dialog state tracking
 * - Media stream configuration
 */

#include "sip_utils.h"
#include "call_session.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Reference to global session
extern struct call_session current_session;

// Internal function declaration
static void process_sdp(const char *sdp, int direction);

/*
 * Process SIP Packet
 * 
 * Main entry point for SIP packet processing:
 * - Validates packet content
 * - Updates session state
 * - Extracts and processes SDP
 * - Manages dialog state
 * 
 * The function handles all types of SIP messages:
 * - INVITE: Call setup
 * - BYE: Call teardown
 * - ACK: Response acknowledgment
 * - Other methods
 * 
 * Parameters:
 * - payload: Raw SIP message
 * - payload_len: Message length
 * - direction: Traffic direction
 */
void process_sip_packet(const uint8_t *payload, int payload_len, int direction) {
    // Validate input
    if (!payload || payload_len <= 0) {
        DEBUG_PRINT("Invalid SIP packet payload");
        return;
    }

    // Extract first line for logging
    char first_line[256] = {0};
    const char *eol = memchr(payload, '\r', payload_len);
    if (eol) {
        int line_len = eol - (char*)payload;
        if (line_len > 255) line_len = 255;
        memcpy(first_line, payload, line_len);
        DEBUG_SIP("Processing SIP message: %s", first_line);
    }

    // Update session statistics
    current_session.sip_packets++;
    current_session.last_sip_seen = time(NULL);
    DEBUG_SIP("Processing SIP packet #%u, current state: %d, direction: %s", 
              current_session.sip_packets, 
              current_session.dialog.state,
              direction == DIRECTION_INCOMING ? "incoming" : "outgoing");

    // Prepare message for processing
    char *sip_msg = (char *)malloc(payload_len + 1);
    if (!sip_msg) {
        DEBUG_PRINT("Failed to allocate memory for SIP message");
        return;
    }
    
    memcpy(sip_msg, payload, payload_len);
    sip_msg[payload_len] = '\0';
    
    // Extract and process SDP content
    char *sdp_start = strstr(sip_msg, "\r\n\r\n");
    if (sdp_start) {
        sdp_start += 4;  // Skip empty line
        process_sdp(sdp_start, direction);
    }
    
    // Extract SIP method/response
    char method[32] = {0};
    char uri[256] = {0};
    int response_code = 0;
    char *cseq = NULL;  // Move declaration outside switch
    
    // Try to parse as response first
    if (sscanf(sip_msg, "SIP/2.0 %d", &response_code) == 1) {
        // Handle SIP responses
        switch (response_code) {
            case 200:
                cseq = strstr(sip_msg, "CSeq:");
                if (!cseq) {
                    DEBUG_SIP("No CSeq header found in 200 OK");
                    break;
                }
                
                if (strstr(cseq, "INVITE")) {
                    current_session.dialog.state = DIALOG_ESTABLISHED;
                    DEBUG_SIP("200 OK for INVITE received, call established");
                } else if (strstr(cseq, "BYE")) {
                    current_session.dialog.state = DIALOG_TERMINATED;
                    if (current_session.last_bye_seen == 0) {
                        current_session.last_bye_seen = time(NULL);
                        DEBUG_SIP("200 OK for BYE received, starting grace period");
                    } else {
                        DEBUG_SIP("200 OK for BYE received after BYE, continuing grace period");
                    }
                }
                break;
                
            case 486: // Busy Here
            case 487: // Request Terminated
            case 603: // Decline
                current_session.dialog.state = DIALOG_TERMINATED;
                DEBUG_SIP("Call rejected/terminated with response: %d", response_code);
                break;
        }
    }
    // Try to parse as request
    else if (sscanf(sip_msg, "%31s %255s", method, uri) == 2) {
        // Handle SIP requests
        if (strcmp(method, "INVITE") == 0) {
            current_session.dialog.state = DIALOG_TRYING;
            DEBUG_SIP("INVITE received, dialog state -> TRYING");
        }
        else if (strcmp(method, "BYE") == 0) {
            current_session.dialog.state = DIALOG_TERMINATED;
            // Only set last_bye_seen if not already set
            if (current_session.last_bye_seen == 0) {
                current_session.last_bye_seen = time(NULL);
                DEBUG_SIP("BYE received, waiting for 200 OK and grace period");
            } else {
                DEBUG_SIP("Additional BYE received, continuing grace period");
            }
        }
        else if (strcmp(method, "CANCEL") == 0) {
            current_session.dialog.state = DIALOG_TERMINATED;
            DEBUG_SIP("CANCEL received, dialog state -> TERMINATED");
        }
    }
    
    free(sip_msg);
}

/*
 * Process SDP Content
 * 
 * Parses SDP (Session Description Protocol) content:
 * - Media stream configuration
 * - Codec information
 * - Network endpoints
 * 
 * The function extracts:
 * - Media types (audio/video)
 * - Port numbers
 * - Codec parameters
 * - Format-specific settings
 * 
 * Parameters:
 * - sdp: SDP content string
 * - direction: Traffic direction
 */
static void process_sdp(const char *sdp, int direction) {
    char line[256];
    const char *ptr = sdp;
    int media_section = 0;
    struct rtp_stream_info *stream_info = NULL;
    
    while (*ptr) {
        // Get next line
        const char *eol = strstr(ptr, "\r\n");
        if (!eol) break;
        size_t len = (size_t)(eol - ptr);  // Use size_t for length
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        strncpy(line, ptr, len);
        line[len] = '\0';
        
        // Process SDP line
        if (line[0] == 'm' && line[1] == '=') {
            // Media line (m=audio PORT RTP/AVP ...)
            media_section = 1;
            stream_info = malloc(sizeof(struct rtp_stream_info));
            if (stream_info) {
                memset(stream_info, 0, sizeof(struct rtp_stream_info));
                stream_info->direction = direction;
                sscanf(line, "m=audio %hu RTP/AVP %d",
                       &stream_info->port,
                       &stream_info->payload_type);
            }
        }
        else if (media_section && line[0] == 'a' && line[1] == '=') {
            if (strncmp(line+2, "rtpmap:", 7) == 0) {
                // RTP payload mapping (a=rtpmap:PT codec/rate)
                int pt;
                char codec[32];
                int rate;
                if (sscanf(line+9, "%d %31[^/]/%d",
                          &pt, codec, &rate) == 3) {
                    if (stream_info && pt == stream_info->payload_type) {
                        strncpy(stream_info->codec, codec,
                               sizeof(stream_info->codec)-1);
                        stream_info->sample_rate = rate;
                    }
                }
            }
            else if (strncmp(line+2, "fmtp:", 5) == 0) {
                // Format parameters
                int pt;
                char params[128];
                if (sscanf(line+7, "%d %127[^\r\n]",
                          &pt, params) == 2) {
                    if (stream_info && pt == stream_info->payload_type) {
                        strncpy(stream_info->fmtp, params,
                               sizeof(stream_info->fmtp)-1);
                    }
                }
            }
        }
        
        ptr = eol + 2;  // Skip \r\n
    }
    
    if (stream_info) {
        // Store stream info for later use
        for (int i = 0; i < MAX_RTP_STREAMS; i++) {
            if (!current_session.stream_info[i]) {
                current_session.stream_info[i] = stream_info;
                DEBUG_SIP("Stored RTP stream info: PT=%d codec=%s rate=%d",
                         stream_info->payload_type,
                         stream_info->codec,
                         stream_info->sample_rate);
                break;
            }
        }
    }
} 