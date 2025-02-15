/*
 * Platform-Specific Definitions
 * 
 * This header provides platform-specific macros and definitions
 * for macOS compatibility.
 *
 * Key Features:
 * - Network-related includes
 * - macOS-specific definitions
 */

#ifndef PLATFORM_H
#define PLATFORM_H

/*
 * Platform detection
 * PLATFORM_MACOS: Defined for macOS systems
 */
#if !defined(__APPLE__)
    #error "This software only supports macOS"
#endif

#define PLATFORM_MACOS

/*
 * Include required headers for network operations
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#endif // PLATFORM_H