// Train Display API Client
// Jon Morris Smith - Feb 2025
// Version 1.1 - Refactored
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board

#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <curl/curl.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <memory>
#include <fstream>
#include <atomic>
#include <thread>
#include <cstdlib> // for getenv

class APIClient {
public:
    // Configuration struct for better organization
    struct APIConfig {
        std::string staff_api_key;
        std::string reason_code_api_key;
        bool debug_mode = false;
        std::string debug_log_dir = "/tmp";
    };

    explicit APIClient(const APIConfig& config);

    // Main API methods
    std::string fetchDepartures(const std::string& station_code) const;
    std::string fetchReasonCodes() const;
    
    // Departure Data version control
    uint64_t getCurrentAPIVersion() const {
        return departure_data_version.load(std::memory_order_acquire);
    }

    // Utility methods
    void setDebugMode(bool enabled) { APIConfig_.debug_mode = enabled; }
    bool isDebugMode() const { return APIConfig_.debug_mode; }

private:
    APIConfig APIConfig_;
    
    // Mutable allows modification in const methods (for version tracking)
    mutable std::atomic<uint64_t> departure_data_version;
    
    // Constants - using static const instead of constexpr
    static const char* const STAFF_API_BASE_URL;
    static const char* const REASON_CODE_URL;
    static const char* const API_KEY_HEADER_PREFIX;

    // Helper methods
    std::string makeApiCall(const std::string& url, const std::string& api_key,
                           const std::string& log_prefix = "") const;
    std::string getCurrentDateTime() const;
    void writeDebugFiles(const std::string& response, const std::string& log_prefix) const;
    void debugPrint(const std::string& message) const;
    
    // CURL callback
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    
    // RAII wrapper for CURL
    class CurlHandle {
    public:
        CurlHandle();
        ~CurlHandle();
        CURL* get() const { return curl_; }
        bool isValid() const { return curl_ != nullptr; }
    private:
        CURL* curl_;
    };
    
    // RAII wrapper for curl_slist
    class CurlHeaders {
    public:
        CurlHeaders() : headers_(nullptr) {}
        ~CurlHeaders() { if (headers_) curl_slist_free_all(headers_); }
        void append(const std::string& header);
        curl_slist* get() const { return headers_; }
    private:
        curl_slist* headers_;
    };
};

// Custom exceptions
class APIException : public std::runtime_error {
public:
    explicit APIException(const std::string& message)
        : std::runtime_error("API Error: " + message) {}
};

class NetworkException : public std::runtime_error {
public:
    explicit NetworkException(const std::string& message)
        : std::runtime_error("Network Error: " + message) {}
};

// Helper functions for C++11 compatibility - DECLARATIONS ONLY
std::string longToString(long value);
std::string sizeToString(size_t value);

#endif // API_CLIENT_H
