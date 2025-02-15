/*
 * Platform-Specific Definitions
 * 
 * This header provides platform-specific macros and definitions
 * to ensure compatibility across different operating systems.
 * Currently supports BSD-based systems (macOS, FreeBSD) and Linux.
 *
 * Key Features:
 * - Platform detection macros
 * - Network-related includes
 * - Cross-platform compatibility
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <sys/types.h>      // System types
#include <netinet/in.h>     // Internet address family
#include <arpa/inet.h>      // Internet operations

/*
 * Platform Detection Macros
 * 
 * PLATFORM_BSD: Defined for BSD-based systems (macOS, FreeBSD)
 * PLATFORM_LINUX: Defined for Linux-based systems
 */
#if defined(__APPLE__) || defined(__FreeBSD__)
    #define PLATFORM_BSD
#elif defined(__linux__)
    #define PLATFORM_LINUX
#endif

#endif // PLATFORM_H