/*
 * SIP (Session Initiation Protocol) Utilities
 * 
 * This module provides functionality for SIP signaling analysis and call state tracking.
 * It handles SIP message parsing, dialog state management, and call setup/teardown
 * detection.
 *
 * Key Features:
 * - SIP message parsing and validation
 * - Dialog state tracking
 * - Call setup and teardown detection
 * - Multi-dialog support
 */

#ifndef SIP_UTILS_H
#define SIP_UTILS_H

#include <stdint.h>
#include <time.h>

/*
 * SIP Dialog States
 * Represents the current state of a SIP dialog:
 * - INIT: Initial state before any messages
 * - TRYING: INVITE sent, waiting for response
 * - ESTABLISHED: Call established (200 OK received)
 * - TERMINATED: Call ended (BYE processed)
 */
enum sip_dialog_state {
    DIALOG_INIT,
    DIALOG_TRYING,
    DIALOG_ESTABLISHED,
    DIALOG_TERMINATED
};

/*
 * SIP Dialog Structure
 * Maintains state for a single SIP dialog:
 * - state: Current dialog state
 * - call_id: Unique call identifier
 * - local_tag: Local endpoint tag
 * - remote_tag: Remote endpoint tag
 */
struct sip_dialog {
    enum sip_dialog_state state;
    char call_id[128];
    char local_tag[64];
    char remote_tag[64];
};

/*
 * Call State Structure
 * Tracks overall call status:
 * - call_established: Flag indicating if call is active
 * - call_terminated: Flag indicating if call has ended
 * - state_changed: Timestamp of last state change
 */
struct call_state {
    int call_established;
    int call_terminated;
    time_t state_changed;
};

/*
 * Process SIP Packet
 * Analyzes a SIP message and updates dialog state
 * 
 * Parameters:
 * - payload: Pointer to SIP message data
 * - payload_len: Length of SIP message
 * - direction: Message direction (0=incoming, 1=outgoing)
 */
void process_sip_packet(const uint8_t *payload, int payload_len, int direction);

#endif // SIP_UTILS_H