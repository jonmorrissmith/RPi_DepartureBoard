// Optimized HTML Processing for Train Service Parser
// Designed for Raspberry Pi performance constraints with ARM NEON SIMD optimization
// Jon Morris Smith - 2025

#include "HTML_processor.h"
#include <algorithm>
#include <iostream>

// Constructor
HTMLProcessor::HTMLProcessor() {
    buffer_.reserve(512); // Pre-allocate for typical NRCC message size
    initializePerformanceStats();
}

void HTMLProcessor::initializePerformanceStats() const {
#if NEON_AVAILABLE
    perf_stats_.neon_available = true;
#else
    perf_stats_.neon_available = false;
#endif
    perf_stats_.neon_calls = 0;
    perf_stats_.regular_calls = 0;
}

// Main processing function - automatically chooses best implementation
std::string HTMLProcessor::processHtmlTags(std::string_view html) const {
    if (html.empty()) return {};
    
#if NEON_AVAILABLE
    // Use NEON for longer strings to amortize the setup cost
    if (html.length() >= perf_stats_.neon_threshold && perf_stats_.neon_available) {
        perf_stats_.neon_calls++;
        return processHtmlTagsNEON(html);
    }
#endif
    
    perf_stats_.regular_calls++;
    return processHtmlTagsRegular(html);
}

// Regular (non-SIMD) implementation - your original optimized version
std::string HTMLProcessor::processHtmlTagsRegular(std::string_view html) const {
    if (html.empty()) return {};
    
    // Clear and reserve buffer
    buffer_.clear();
    buffer_.reserve(html.length()); // Worst case: no tags/entities to remove
    
    bool inTag = false;
    const char* data = html.data();
    const size_t length = html.length();
    
    for (size_t i = 0; i < length; ++i) {
        const char c = data[i];
        
        if (c == '<') {
            inTag = true;
            continue;
        }
        
        if (c == '>') {
            inTag = false;
            continue;
        }
        
        if (inTag) {
            continue; // Skip content inside tags
        }
        
        if (c == '&') {
            // Fast entity lookup using compile-time table
            bool entityFound = false;
            const size_t remaining = length - i;
            
            // Check each entity in our lookup table
            for (const auto& entity : entities) {
                if (remaining >= entity.length &&
                    std::memcmp(data + i, entity.entity, entity.length) == 0) {
                    buffer_ += entity.replacement;
                    i += entity.length - 1; // -1 because loop will increment
                    entityFound = true;
                    break;
                }
            }
            
            if (!entityFound) {
                buffer_ += c; // Unknown entity, keep as-is
            }
        } else if (c != '\n' && c != '\r') {
            // Skip newlines, keep everything else
            buffer_ += c;
        }
    }
    
    return buffer_;
}

#if NEON_AVAILABLE
// NEON-optimized implementation
std::string HTMLProcessor::processHtmlTagsNEON(std::string_view html) const {
    if (html.empty()) return {};
    
    buffer_.clear();
    buffer_.reserve(html.length());
    
    const char* data = html.data();
    const size_t length = html.length();
    
    // NEON works best on 16-byte aligned chunks
    const size_t NEON_CHUNK_SIZE = 16;
    const size_t neon_end = (length / NEON_CHUNK_SIZE) * NEON_CHUNK_SIZE;
    
    // Prepare NEON constants for vectorized comparison
    const uint8x16_t less_than_vec = vdupq_n_u8('<');
    const uint8x16_t greater_than_vec = vdupq_n_u8('>');
    const uint8x16_t ampersand_vec = vdupq_n_u8('&');
    const uint8x16_t newline_vec = vdupq_n_u8('\n');
    const uint8x16_t carriage_vec = vdupq_n_u8('\r');
    
    bool inTag = false;
    size_t i = 0;
    
    // Process 16 characters at a time with NEON
    while (i < neon_end) {
        // Load 16 characters into NEON register
        uint8x16_t chars = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        
        // Vectorized comparisons - find all special characters at once
        uint8x16_t is_less = vceqq_u8(chars, less_than_vec);
        uint8x16_t is_greater = vceqq_u8(chars, greater_than_vec);
        uint8x16_t is_ampersand = vceqq_u8(chars, ampersand_vec);
        uint8x16_t is_newline = vceqq_u8(chars, newline_vec);
        uint8x16_t is_carriage = vceqq_u8(chars, carriage_vec);
        
        // Combine all special character masks
        uint8x16_t any_special = vorrq_u8(
            vorrq_u8(is_less, is_greater),
            vorrq_u8(
                vorrq_u8(is_ampersand, is_newline),
                is_carriage
            )
        );
        
        // Check if this chunk has any special characters
        uint64_t special_mask_low = vget_lane_u64(vreinterpret_u64_u8(vget_low_u8(any_special)), 0);
        uint64_t special_mask_high = vget_lane_u64(vreinterpret_u64_u8(vget_high_u8(any_special)), 0);
        
        if (special_mask_low == 0 && special_mask_high == 0) {
            // No special characters in this chunk
            if (!inTag) {
                // Fast path: append entire chunk if not inside a tag
                buffer_.append(data + i, NEON_CHUNK_SIZE);
            }
            i += NEON_CHUNK_SIZE;
        } else {
            // Special characters found, process byte by byte for this chunk
            // Extract the comparison results into arrays for easier processing
            uint8_t less_results[16], greater_results[16], amp_results[16];
            uint8_t newline_results[16], carriage_results[16];
            
            vst1q_u8(less_results, is_less);
            vst1q_u8(greater_results, is_greater);
            vst1q_u8(amp_results, is_ampersand);
            vst1q_u8(newline_results, is_newline);
            vst1q_u8(carriage_results, is_carriage);
            
            // Process each character in the chunk
            for (int j = 0; j < NEON_CHUNK_SIZE && (i + j) < length; ++j) {
                const char c = data[i + j];
                
                if (less_results[j]) {
                    inTag = true;
                    continue;
                }
                
                if (greater_results[j]) {
                    inTag = false;
                    continue;
                }
                
                if (inTag) {
                    continue; // Skip content inside tags
                }
                
                if (amp_results[j]) {
                    // Handle HTML entity
                    bool entityFound = false;
                    const size_t remaining = length - (i + j);
                    
                    for (const auto& entity : entities) {
                        if (remaining >= entity.length &&
                            std::memcmp(data + i + j, entity.entity, entity.length) == 0) {
                            buffer_ += entity.replacement;
                            j += entity.length - 1; // Skip ahead
                            entityFound = true;
                            break;
                        }
                    }
                    
                    if (!entityFound) {
                        buffer_ += c;
                    }
                } else if (!newline_results[j] && !carriage_results[j]) {
                    // Normal character (not newline or carriage return)
                    buffer_ += c;
                }
            }
            
            i += NEON_CHUNK_SIZE;
        }
    }
    
    // Process remaining characters (less than 16) with regular loop
    for (; i < length; ++i) {
        const char c = data[i];
        
        if (c == '<') {
            inTag = true;
            continue;
        }
        
        if (c == '>') {
            inTag = false;
            continue;
        }
        
        if (inTag) {
            continue;
        }
        
        if (c == '&') {
            bool entityFound = false;
            const size_t remaining = length - i;
            
            for (const auto& entity : entities) {
                if (remaining >= entity.length &&
                    std::memcmp(data + i, entity.entity, entity.length) == 0) {
                    buffer_ += entity.replacement;
                    i += entity.length - 1;
                    entityFound = true;
                    break;
                }
            }
            
            if (!entityFound) {
                buffer_ += c;
            }
        } else if (c != '\n' && c != '\r') {
            buffer_ += c;
        }
    }
    
    return buffer_;
}

// Helper function for finding special characters with NEON
size_t HTMLProcessor::findNextSpecialCharNEON(const char* data, size_t start, size_t end, char target) const {
    const uint8x16_t target_vec = vdupq_n_u8(target);
    
    for (size_t i = start; i + 16 <= end; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t matches = vceqq_u8(chunk, target_vec);
        
        // Check if any matches found
        uint64_t result_low = vget_lane_u64(vreinterpret_u64_u8(vget_low_u8(matches)), 0);
        uint64_t result_high = vget_lane_u64(vreinterpret_u64_u8(vget_high_u8(matches)), 0);
        
        if (result_low != 0 || result_high != 0) {
            // Found at least one match, find exact position
            for (int j = 0; j < 16; ++j) {
                if (data[i + j] == target) {
                    return i + j;
                }
            }
        }
    }
    
    // Handle remainder with regular search
    for (size_t i = end - (end % 16); i < end; ++i) {
        if (data[i] == target) return i;
    }
    
    return std::string::npos;
}

// Forced NEON implementation (for testing)
std::string HTMLProcessor::processHtmlTagsNEONForced(std::string_view html) const {
    return processHtmlTagsNEON(html);
}
#endif

// In-place processing version
void HTMLProcessor::processHtmlTagsInPlace(std::string& html) const {
    if (html.empty()) return;
    
    size_t writePos = 0;
    bool inTag = false;
    const size_t length = html.length();
    
    for (size_t readPos = 0; readPos < length; ++readPos) {
        const char c = html[readPos];
        
        if (c == '<') {
            inTag = true;
            continue;
        }
        
        if (c == '>') {
            inTag = false;
            continue;
        }
        
        if (inTag) {
            continue;
        }
        
        if (c == '&') {
            // Entity processing with in-place replacement
            bool entityFound = false;
            const size_t remaining = length - readPos;
            
            for (const auto& entity : entities) {
                if (remaining >= entity.length &&
                    std::memcmp(html.data() + readPos, entity.entity, entity.length) == 0) {
                    html[writePos++] = entity.replacement;
                    readPos += entity.length - 1; // -1 for loop increment
                    entityFound = true;
                    break;
                }
            }
            
            if (!entityFound) {
                html[writePos++] = c;
            }
        } else if (c != '\n' && c != '\r') {
            html[writePos++] = c;
        }
    }
    
    html.resize(writePos); // Truncate to actual content
}

// Entity-only processing
std::string HTMLProcessor::processEntitiesOnly(std::string_view html) const {
    if (html.empty()) return {};
    
    buffer_.clear();
    buffer_.reserve(html.length());
    
    const char* data = html.data();
    const size_t length = html.length();
    
    for (size_t i = 0; i < length; ++i) {
        const char c = data[i];
        
        if (c == '&') {
            bool entityFound = false;
            const size_t remaining = length - i;
            
            for (const auto& entity : entities) {
                if (remaining >= entity.length &&
                    std::memcmp(data + i, entity.entity, entity.length) == 0) {
                    buffer_ += entity.replacement;
                    i += entity.length - 1;
                    entityFound = true;
                    break;
                }
            }
            
            if (!entityFound) {
                buffer_ += c;
            }
        } else {
            buffer_ += c;
        }
    }
    
    return buffer_;
}

// Forced regular implementation (for testing)
std::string HTMLProcessor::processHtmlTagsRegularForced(std::string_view html) const {
    return processHtmlTagsRegular(html);
}

// Performance statistics
HTMLProcessor::PerformanceStats HTMLProcessor::getPerformanceStats() const {
    return {
        perf_stats_.neon_calls,
        perf_stats_.regular_calls,
        perf_stats_.neon_available,
        perf_stats_.neon_threshold
    };
}

void HTMLProcessor::resetPerformanceStats() const {
    perf_stats_.neon_calls = 0;
    perf_stats_.regular_calls = 0;
}
