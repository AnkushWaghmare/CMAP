/*
 * Debug and Logging Utilities
 * 
 * This module provides debugging and logging functionality with support
 * for colored terminal output and different message categories.
 * It includes macros for various logging levels and message types.
 *
 * Key Features:
 * - Colored terminal output
 * - Multiple logging levels
 * - Protocol-specific debugging
 * - Configurable output control
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

/*
 * ANSI Terminal Color Codes
 * Used for colorizing different types of output messages
 */
#define COLOR_RED     "\033[31m"  // Error messages
#define COLOR_GREEN   "\033[32m"  // Success messages
#define COLOR_YELLOW  "\033[33m"  // Warning messages
#define COLOR_BLUE    "\033[34m"  // General information
#define COLOR_MAGENTA "\033[35m"  // SIP protocol messages
#define COLOR_CYAN    "\033[36m"  // RTP protocol messages
#define COLOR_RESET   "\033[0m"   // Reset to default color

/*
 * Semantic Color Mappings
 * Maps colors to specific message types for consistent output
 */
#define COLOR_ERROR   COLOR_RED     // Error messages
#define COLOR_SUCCESS COLOR_GREEN   // Success notifications
#define COLOR_WARNING COLOR_YELLOW  // Warning messages
#define COLOR_INFO    COLOR_CYAN    // Informational messages

/*
 * Text Style Codes
 * ANSI escape codes for text formatting
 */
#define STYLE_BOLD    "\033[1m"   // Bold text
#define STYLE_DIM     "\033[2m"   // Dimmed text
#define STYLE_NORMAL  "\033[22m"  // Normal text weight

/*
 * Global Debug Control Flags
 * Control various aspects of debug output
 */
extern int debug_mode;        // Enable/disable debug messages
extern int silent_mode;       // Suppress all output when enabled
extern int rtp_msg_only;      // Show only RTP-related messages
extern int show_packet_count; // Enable packet count messages

/*
 * Debug Print Macros
 * Various macros for different types of debug output
 */

// General debug messages
#define DEBUG_PRINT(fmt, ...) \
    if (!silent_mode && !rtp_msg_only && debug_mode && show_packet_count) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

// Error messages (always shown unless silent)
#define ERROR_PRINT(fmt, ...) \
    if (!silent_mode && !rtp_msg_only) fprintf(stderr, COLOR_ERROR "[ERROR] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

// Warning messages
#define WARNING_PRINT(fmt, ...) \
    if (!silent_mode && !rtp_msg_only) fprintf(stderr, COLOR_WARNING "[WARNING] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

// Informational messages
#define INFO_PRINT(fmt, ...) \
    if (!silent_mode && !rtp_msg_only) fprintf(stderr, COLOR_INFO "[INFO] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

// RTP-specific debug messages
#define DEBUG_RTP(fmt, ...) \
    if (debug_mode && !silent_mode) fprintf(stderr, COLOR_CYAN "[RTP] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

// SIP-specific debug messages
#define DEBUG_SIP(fmt, ...) \
    if (debug_mode && !silent_mode && !rtp_msg_only) fprintf(stderr, COLOR_MAGENTA "[SIP] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

// State change debug messages
#define DEBUG_STATE(fmt, ...) \
    if (debug_mode && !silent_mode && !rtp_msg_only) fprintf(stderr, COLOR_GREEN "[STATE] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

#endif // DEBUG_H