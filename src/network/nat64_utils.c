/*
 * NAT64 (Network Address Translation IPv6 to IPv4) Utilities Implementation
 * 
 * This module implements functionality for detecting and handling NAT64
 * address translation in VoIP traffic monitoring. It provides functions
 * to identify addresses that are part of NAT64 translation.
 *
 * Key Features:
 * - NAT64 prefix detection
 * - Support for standard and local prefixes
 * - Documentation prefix support
 */

#include "nat64_utils.h"
#include <string.h>
#include <stdio.h>

/*
 * NAT64 Address Detection
 * 
 * Determines if an IPv6 address uses a NAT64 prefix by checking
 * against known prefix patterns:
 * - 64:ff9b:: (Well-known prefix, RFC 6052)
 * - 64:ff9b:1:: (Local prefix)
 * - 2001:db8:64:: (Documentation/Example prefix)
 * 
 * Parameters:
 * - addr: IPv6 address string to check
 * 
 * Returns:
 * - 1 if address matches a NAT64 prefix
 * - 0 if no match or invalid input
 */
int is_nat64_address(const char *addr) {
    // Standard NAT64 prefix patterns
    static const char *nat64_prefixes[] = {
        "64:ff9b::",        // Well-known prefix (RFC 6052)
        "64:ff9b:1::",      // Local prefix
        "2001:db8:64::",    // Documentation/Example
        NULL
    };
    
    // Validate input
    if (!addr) return 0;
    
    // Check each prefix pattern
    for (const char **prefix = nat64_prefixes; *prefix; prefix++) {
        if (strncmp(addr, *prefix, strlen(*prefix)) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Extract IPv4 Address from NAT64 Address
 * 
 * Extracts the embedded IPv4 address from a NAT64 IPv6 address.
 * NAT64 addresses contain an embedded IPv4 address in the last 32 bits.
 * 
 * Parameters:
 * - nat64_addr: NAT64 IPv6 address string
 * - ipv4_buf: Buffer to store extracted IPv4 address
 * - buf_size: Size of ipv4_buf
 * 
 * Returns:
 * - 1 on success
 * - 0 on failure or invalid input
 */
int extract_ipv4_from_nat64(const char *nat64_addr, char *ipv4_buf, size_t buf_size) {
    if (!nat64_addr || !ipv4_buf || buf_size < INET_ADDRSTRLEN) return 0;
    
    // Verify it's a NAT64 address
    if (!is_nat64_address(nat64_addr)) return 0;
    
    // Find the last 4 groups of hex digits (32 bits)
    const char *last_part = strrchr(nat64_addr, ':');
    if (!last_part) return 0;
    last_part++; // Skip the colon
    
    // Convert hex groups to decimal octets
    unsigned int a, b, c, d;
    if (sscanf(last_part, "%02x%02x:%02x%02x", &a, &b, &c, &d) == 4) {
        snprintf(ipv4_buf, buf_size, "%u.%u.%u.%u", a, b, c, d);
        return 1;
    }
    
    return 0;
}