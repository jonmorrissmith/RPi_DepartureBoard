//
//  lazy_data.h
//  StaffParser
//
//  Created by Jon Morris-Smith on 25/05/2025.
//

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <nlohmann/json.hpp>
#include <string>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <array>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <ctime>
#include <tuple>
#include <algorithm>
#include <vector>

namespace time_utils {
    // Fast ISO 8601 parser for the format "YYYY-MM-DDTHH:MM:SS"
    struct IsoTimestamp {
        int year, month, day;
        int hour, minute, second;
        
        static std::optional<IsoTimestamp> parse(const std::string& iso_str) {
            IsoTimestamp ts;
            
            // Check minimum length (YYYY-MM-DDTHH:MM:SS = 19 chars)
            if (iso_str.length() < 19) return std::nullopt;
            
            // Use sscanf for fast parsing
            if (sscanf(iso_str.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d",
                       &ts.year, &ts.month, &ts.day,
                       &ts.hour, &ts.minute, &ts.second) == 6) {
                return ts;
            }
            return std::nullopt;
        }
        
        // Convert to time_t for comparisons
        std::time_t to_time_t() const {
            std::tm tm = {};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;
            tm.tm_isdst = -1;  // Let system determine DST
            return std::mktime(&tm);
        }
        
        // Fast string parsing that only extracts time portion
        static std::pair<int, int> extractHourMinute(const std::string& iso_str) {
            // Skip to 'T' character
            size_t t_pos = iso_str.find('T');
            if (t_pos == std::string::npos || t_pos + 5 >= iso_str.length()) {
                return {-1, -1};  // Invalid format
            }
            
            int hour = (iso_str[t_pos + 1] - '0') * 10 + (iso_str[t_pos + 2] - '0');
            int minute = (iso_str[t_pos + 4] - '0') * 10 + (iso_str[t_pos + 5] - '0');
            
            return {hour, minute};
        }
    };
}
#endif
