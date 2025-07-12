// Train Display API Client
// Jon Morris Smith - Feb 2025
// Version 1.1 - Refactored
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board

#include "API_client.h"
#include <sys/stat.h> // for mkdir on Unix systems

// Helper functions for C++11 compatibility - DEFINITIONS
std::string longToString(long value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string sizeToString(size_t value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// Define static constants (C++11 compatible)
const char* const APIClient::STAFF_API_BASE_URL =
    "https://api1.raildata.org.uk/1010-live-arrival-and-departure-boards---staff-version1_0/LDBSVWS/api/20220120/GetArrDepBoardWithDetails/";
const char* const APIClient::REASON_CODE_URL =
    "https://api1.raildata.org.uk/1010-reference-data1_0/LDBSVWS/api/ref/20211101/GetReasonCodeList";
const char* const APIClient::API_KEY_HEADER_PREFIX = "x-apikey:";

// Static callback function for CURL
size_t APIClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* str = static_cast<std::string*>(userp);  // Fixed: removed 'auto' for C++11
    str->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// RAII CURL Handle implementation
APIClient::CurlHandle::CurlHandle() : curl_(curl_easy_init()) {
    if (!curl_) {
        throw NetworkException("Failed to initialize CURL");
    }
}

APIClient::CurlHandle::~CurlHandle() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

// RAII CURL Headers implementation
void APIClient::CurlHeaders::append(const std::string& header) {
    headers_ = curl_slist_append(headers_, header.c_str());
}

// Main APIClient implementation
APIClient::APIClient(const APIConfig& config) : APIConfig_(config), departure_data_version(0) {
    if (APIConfig_.staff_api_key.empty()) {
        throw std::invalid_argument("Staff API key cannot be empty");
    }
}

std::string APIClient::fetchDepartures(const std::string& station_code) const {
    if (station_code.empty()) {
        throw std::invalid_argument("Station code cannot be empty");
    }

    const std::string url = std::string(STAFF_API_BASE_URL) + station_code + "/" + getCurrentDateTime();
    debugPrint("Fetching departures for station: " + station_code);
    debugPrint("URL: " + url);
    
    // Fixed: corrected typo from 'fech_add' to 'fetch_add'
    departure_data_version.fetch_add(1, std::memory_order_release);

    return makeApiCall(url, APIConfig_.staff_api_key, "departures");
}

std::string APIClient::fetchReasonCodes() const {
    if (APIConfig_.reason_code_api_key.empty()) {
        throw std::invalid_argument("Reason code API key not configured");
    }
    
    debugPrint("Fetching reason codes");
    debugPrint("URL: " + std::string(REASON_CODE_URL));
    
    return makeApiCall(REASON_CODE_URL, APIConfig_.reason_code_api_key, "reason_codes");
}

std::string APIClient::makeApiCall(const std::string& url, const std::string& api_key,
                                  const std::string& log_prefix) const {
    CurlHandle curl;
    CurlHeaders headers;
    std::string response;
    debugPrint("Making API Call");
    // Configure CURL options
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L); // 30 second timeout
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    
    // Set API key header
    if (!api_key.empty()) {
        headers.append(std::string(API_KEY_HEADER_PREFIX) + api_key);
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    }
    
    // Configure debug logging if enabled
    FILE* debug_file = NULL;
    if (APIConfig_.debug_mode && !log_prefix.empty()) {
        const std::string debug_path = APIConfig_.debug_log_dir + "/traindisplay_" + log_prefix + "_debug.log";
        debug_file = fopen(debug_path.c_str(), "w");
        if (debug_file) {
            curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl.get(), CURLOPT_STDERR, debug_file);
            debugPrint("Debug logs will be written to: " + debug_path);
        } else {
            debugPrint("Warning: Could not open debug log file: " + debug_path);
        }
    }
    
    // Perform the request
    const CURLcode res = curl_easy_perform(curl.get());
    
    // Clean up debug file
    if (debug_file) {
        fclose(debug_file);
    }
    
    if (res != CURLE_OK) {
        throw NetworkException("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    // Check HTTP response code
    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 400) {
        throw APIException("HTTP error " + longToString(response_code) + " from API");
    }
    
    debugPrint("Response received, length: " + sizeToString(response.length()));
    
    // Write debug files if enabled
    if (APIConfig_.debug_mode && !log_prefix.empty()) {
        writeDebugFiles(response, log_prefix);
    }
    
    if (response.empty()) {
        throw APIException("Received empty response from API");
    }
    debugPrint("Completed API Call");
    return response;
}

std::string APIClient::getCurrentDateTime() const {
    const std::time_t now = std::time(NULL);
    const std::tm* timeinfo = std::localtime(&now);
    
    std::ostringstream ss;
    ss << std::put_time(timeinfo, "%Y%m%dT%H%M%S");
    return ss.str();
}

void APIClient::writeDebugFiles(const std::string& response, const std::string& log_prefix) const {
    const std::string json_path = APIConfig_.debug_log_dir + "/traindisplay_" + log_prefix + "_response.json";
    
    std::ofstream json_file(json_path.c_str()); // C++11 compatible
    if (json_file.is_open()) {
        json_file << response;
        json_file.close();
        debugPrint("Response written to: " + json_path);
    } else {
        debugPrint("Warning: Could not write API response to: " + json_path);
    }
}

void APIClient::debugPrint(const std::string& message) const {
    if (APIConfig_.debug_mode) {
        std::cerr << "[API] " << message << std::endl;
    }
}
