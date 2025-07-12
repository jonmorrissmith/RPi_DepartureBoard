// Optimized HTML Processing for Train Service Parser
// Designed for Raspberry Pi performance constraints with ARM NEON SIMD optimization
// Jon Morris Smith - 2025

#ifndef HTML_PROCESSOR_H
#define HTML_PROCESSOR_H

#include <string>
#include <cstring>
#include <string_view>
#include <array>
#include <unordered_map>

// ARM NEON support detection
#ifdef __ARM_NEON
#include <arm_neon.h>
#define NEON_AVAILABLE 1
#else
#define NEON_AVAILABLE 0
#endif

class HTMLProcessor {
private:
    // Pre-computed lookup table for HTML entities (much faster than string comparisons)
    struct EntityLookup {
        const char* entity;
        char replacement;
        uint8_t length;
    };
    
    // Static lookup table - sorted by frequency of occurrence in NRCC messages
    static constexpr std::array<EntityLookup, 6> entities = {{
        {"&quot;", '"', 6},   // Most common
        {"&amp;", '&', 5},    // Second most common
        {"&lt;", '<', 4},
        {"&gt;", '>', 4},
        {"&#39;", '\'', 5},   // Single quote
        {"&nbsp;", ' ', 6}    // Non-breaking space
    }};
    
    // Reusable buffer to avoid allocations
    mutable std::string buffer_;
    
    // Performance tracking
    mutable struct {
        bool neon_available = false;
        size_t neon_threshold = 64;  // Use NEON for strings longer than this
        size_t neon_calls = 0;
        size_t regular_calls = 0;
    } perf_stats_;
    
    // Private implementation methods
    std::string processHtmlTagsRegular(std::string_view html) const;
    
#if NEON_AVAILABLE
    std::string processHtmlTagsNEON(std::string_view html) const;
    size_t findNextSpecialCharNEON(const char* data, size_t start, size_t end, char target) const;
    void processChunkNEON(const char* data, size_t start, size_t chunk_size, bool& inTag) const;
#endif
    
    void initializePerformanceStats() const;
    
public:
    HTMLProcessor();
    
    // Main processing function - automatically chooses best implementation
    std::string processHtmlTags(std::string_view html) const;
    
    // In-place version for when you own the string (even faster)
    void processHtmlTagsInPlace(std::string& html) const;
    
    // Fast entity-only processing (no tag removal)
    std::string processEntitiesOnly(std::string_view html) const;
    
    // Performance and capability queries
    bool isNEONAvailable() const { return perf_stats_.neon_available; }
    void setNEONThreshold(size_t threshold) { perf_stats_.neon_threshold = threshold; }
    size_t getNEONThreshold() const { return perf_stats_.neon_threshold; }
    
    // Performance statistics (useful for optimization)
    struct PerformanceStats {
        size_t neon_calls;
        size_t regular_calls;
        bool neon_available;
        size_t neon_threshold;
    };
    PerformanceStats getPerformanceStats() const;
    void resetPerformanceStats() const;
    
    // Force specific implementation (for testing/benchmarking)
    std::string processHtmlTagsRegularForced(std::string_view html) const;
#if NEON_AVAILABLE
    std::string processHtmlTagsNEONForced(std::string_view html) const;
#endif
};

// Compile-time feature detection
constexpr bool hasNEONSupport() {
    return NEON_AVAILABLE;
}

#endif // HTML_PROCESSOR_H
