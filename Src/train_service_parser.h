// Train Display - an LED matrix departure board for the Raspberry Pi
// JSON parsing - updated to use Staff data
// Jon Morris Smith - April 2025
// Version 2.0
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board
//
// With thanks to:
// https://github.com/nlohmann/json
//
#ifndef TRAIN_SERVICE_PARSER_H
#define TRAIN_SERVICE_PARSER_H

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
#include "HTML_processor.h"

using json = nlohmann::json;

// Forward declaration for the debug printing macro
extern bool debug_mode;
#define DEBUG_PRINT(x) if(debug_mode) { std::cerr << x << std::endl; }
#define DEBUG_PRINT_JSON(x) if(debug_mode) { std::cerr << std::setw(4) << x << std::endl; }

class TrainServiceParser {
    
public:
    TrainServiceParser (size_t max_services = 10, size_t max_departures = 3)
    : max_json_size(max_services),
    number_of_departures(max_departures),
    selectPlatform(false),
    refdata_loaded(false),
    location_name(""),
    selected_platform(),
    NRCC_message(),
    api_data_version(0),
    services_sequence(max_services),
    services_basic(max_services),
    services_additions(max_services),
    services_callingpoints(max_services),
    dataMutex()
    {
        service_List.resize(number_of_departures, 999);
        service_List.reserve(number_of_departures);
        
        ETDOrderedList.resize(max_json_size, 999);
        ETDOrderedList.reserve(max_json_size);
    }
    
    
    // Data structure for Basic Service information
    /* Original order
    struct BasicServiceInfo {
        std::string trainid;
        uint64_t apiDataVersion;
        bool static_data_available;
        std::string scheduledDepartureTime;
        std::string estimatedDepartureTime;
        std::string destination;
        std::string operator_name;
        std::string coaches;
        bool isCancelled;
        bool isDelayed;
        std::string cancelReason;
        std::string delayReason;
        std::string adhocAlerts;
    }; */
    
    // Optimized for line-caching
    struct alignas(64) BasicServiceInfo {
        // CACHE LINE 1: Hot data (always accessed for display)
        std::string trainid;                  // Always needed
        std::string destination;              // Always displayed
        std::string scheduledDepartureTime;   // Always shown
        std::string estimatedDepartureTime;   // Always shown
        
        // CACHE LINE 2: Warm data (frequently accessed)
        std::string operator_name;            // Often displayed
        std::string coaches;                  // Alternates with ETD
        bool isCancelled;                     // Display logic
        bool isDelayed;                       // Display logic
        
        // CACHE LINE 3+: Cold data (rarely accessed)
        std::string cancelReason;             // Only when cancelled
        std::string delayReason;              // Only when delayed
        std::string adhocAlerts;              // Rarely used
        uint64_t apiDataVersion;              // Internal bookkeeping
        bool static_data_available;           // Internal flag
    };
    
    // Location data - can be a calling point or the station the data is created for
    
    struct LocationInfo {
        std::string locationName;
        bool isPass;
        bool isCancelled;
        std::string ArrivalTime;
        std::string ArrivalType;
        std::string DepartureTime;
        
        /* Available in the JSON but not used
         bool isOperational;
         std::string crs;
         std::string adhocAlerts;
         std::string platform;
         std::string atd;
         std::string departureType;
         std::string departureSource;
         std::string departureSourceInstance;
         std::string lateness;
         std::string uncertainty;
         std::string affectedBy;
         std::string sta;
         bool ataSpecified;
         std::string ata;
         bool etaSpecified;
         std::string eta;
         bool arrivalTypeSpecified;
         std::string arrivalType;
         std::string std;
         bool etdSpecified;
         std::string etd;
         */
    };
    
    // Information about the coaches on the service
    struct CoachInfo {
        bool standard_class;
        bool first_class;
        size_t service_loading;
        bool standard_toilet;
        bool accessible_toilet;
        std::string number;
    };
    
    // Data structure for Additional Service information
    struct AdditionalServiceInfo {
        std::string trainid;
        uint64_t apiDataVersion;
        bool static_data_available;
        std::string origin;
        std::string loading_type;
        size_t loadingPercentage;
        std::vector<CoachInfo> Formation;
        bool platformIsHidden;
        bool serviceIsSupressed;
        bool isPassengerService;
        
        /* Available in the JSON but not used
         std::string sta;
         std::string ata;
         std::string eta;
         std::string arrivalType;
         std::string atd;
         std::string departureType;
         bool isCharter;
         */
    };
    
    struct CallingPointsInfo {
        std::string trainid;
        uint64_t apiDataVersion;
        bool callingPointsCached;
        std::string callingPoints;
        std::string callingPoints_with_ETD;
        std::vector<LocationInfo> PreviousCallingPoints;
        std::vector<LocationInfo> SubsequentCallingPoints;
        size_t num_previous_calling_points;
        size_t num_subsequent_calling_points;
        bool service_location_cached;
        std::string service_location;
    };
    
    enum CallingPointDirection {SUBSEQUENT , PREVIOUS};
    enum CallingPointETD {SHOWETD, NOETD};
    
    // Public Functions
    
    // Cache hydration/update
    void loadReasonCodes(const std::string& reasonJsonString);                      // Load the JSON reference data for delay and cancellation reasons
    void prefetchCache(const std::string& jsonString, const int64_t& version);      // Minimally populate the Basic Data and created ordered list of departures
    void hydrateDepartureCache();                                                   // Hydrate the cache for the next NUM_OF_DEPARTURES departures from the selected platorm (or all platforms if none selected)
    void updateCache(const std::string& jsonString, const int64_t& version);        // Execute the pre-fetch and hydrate the departure cache - users don't have to remember to hydrate the departure cache after each pre-fetch
    void createFromJSON(const std::string& datajsonString, const std::string& reasonJsonString, const int64_t& version); // Combines updateCache and loadReasonCodes
    int64_t getCacheAPIVersion();                                                   // Return the version of the API data stored in the cache
    
    // Platform selection
    void setPlatform(std::string platform);                                         // Set the stored selected platform
    std::string getSelectedPlatform();                                              // Return the stored selected plaform
    void clearSelectedPlatform();                                                   // Clear the stored selected platform
    std::string getPlatform(size_t service_index);                                  // Return the Platform for a specific service
    
    // Extract specified service from the cache
    BasicServiceInfo getBasicServiceInfo(size_t serviceIndex);                      // Get the train service data structure for a specific service
    AdditionalServiceInfo getAdditionalServiceInfo(size_t serviceIndex);            // Get the train service data structure for a specific service
    
    // Get departure information
    size_t getNumberOfServices () const {return number_of_services;}                // Return the number of Services in the cache
    size_t getOrdinalDeparture(size_t service_number);                              // Return the index for the ordinal service (i.e from 1 to NUM_OF_DEPARTURES)
    size_t getFirstDeparture();                                                     // Return the index for the first departure
    size_t getSecondDeparture();                                                    // Return the index for the second departure
    size_t getThirdDeparture();                                                     // Return the index for the third departure
    
    // Get Service Meta-Data
    std::string getNrccMessages() const { return NRCC_message;}                     // Return Network Rail messages
    std::string getLocationName() const { return location_name;}                    // Return the name of the location for the departure data
    
    // Calling points for departures
    std::string getCallingPoints(size_t serviceIndex, CallingPointETD show_ETD=NOETD); // Return the calling points for the selected service - option to show the Estimated Time of Departure from calling points
    
    // Where is the next arrival
    std::string getServiceLocation(size_t serviceIndex);                            // Calculates location of specified service using Previous Calling Points
    
    // Print data structures for debugging
    void debugPrintBasicServiceInfo(size_t service_index);                          // Prints the Basic Service data-structure for a specific service
    void debugPrintAdditionalServiceInfo(size_t service_index);                     // Prints the Additional Service data-structure for a specific service
    void debugPrintServiceSequence();                                               // Prints the essential sequencing information
    void debugPrintCallingPointsInfo(size_t service_index);                         // Prints the calling point data-structure for a specific service
    void debugPrintTrainIDIndices();                                                // Prints the stored TrainIDs and their indices
    void debugPrintReasonCancelCodes();                                             // Prints the stored cancellation and delay reason codes
      
private:

    // Configuration
    size_t max_json_size;                                                           // Max number of Services in JSON data
    size_t number_of_departures;                                                    // Number of primary departures

    // Cache control
    std::unordered_map<std::string, size_t> cached_trainIDs;                        // Cached train IDs and their indices
    
    // Platform flags and configuration
    bool selectPlatform;                                                            // Flag to indicate whether departures for a specific platform are selected
    std::string selected_platform;                                                  // Store the selected platform
    
    // Reference Data
    json refdata;                                                                   // Raw JSON Reference data (for cancellation and delay reasons)
    bool refdata_loaded;                                                            // Flag to indicate if Reference Data is available
    std::unordered_map<std::string, size_t> reason_codes;                           // Indices of Cached Delay/Cancellation codes
    struct DelayCancelReason {                                                      // Storing delay/cancel reason and the reason-code
        std::string delayReason;
        std::string cancelReason;
        std::string code;
    };
    std::vector<DelayCancelReason> delay_cancel_reasons;                            // Cached Delay/Cancellation codes
    std::string decodeDelayCode(size_t delay_code);                                 // Return the Delay Reason string corresponding to the Delay Code
    std::string decodeCancelCode(size_t delay_code);                                // Return the Cancellation Reason string corresponding to the Cancellation Code
    
    // Meta-data for the location
    std::string location_name;                                                      // Location name
    std::string NRCC_message;                                                       // Network Rail service messages
    
    // Parsing and parsed data
    json data;                                                                      // Raw JSON data
    int64_t api_data_version;                                                       // Raw JSON data version
    
    // Services and Calling Points
    static constexpr time_t INVALID_TIME = 0;                                       // A flag for no valid time
    struct ServiceSequence {
        bool std_specified;                                                         // Is there an STD?
        time_t std;                                                                 // Scheduled Departure Time
        bool etd_specified;                                                         // Is there an ETD?
        time_t etd;                                                                 // Estimated Departure Time
        time_t departure_time;                                                      // Departure time - std or etd (if specified)
        std::string platform;                                                       // Platform
        std::string trainid;                                                        // Train Identifier
        uint64_t api_version;                                                       // Version of the API data used
    };
    std::vector<ServiceSequence> services_sequence;                                 // Essential information for sequencing services.
    std::vector<BasicServiceInfo> services_basic;                                   // Basic service information
    std::vector<AdditionalServiceInfo> services_additions;                          // Additional service information
    std::vector<CallingPointsInfo> services_callingpoints;                          // Calling point information
    
    BasicServiceInfo null_basic_service;                                            // A 'null' Basic Service to return if the service index is 999
    AdditionalServiceInfo null_additional_service;                                  // A 'null' Additional Service to return if the service index is 999
       
    // Managing the stored services
    size_t number_of_services;
    /*std::array<size_t, NUM_OF_DEPARTURES> service_List;                           // Array for the primary departures
    std::array<size_t, MAX_JSON_SIZE> ETDOrderedList;                               // Array of Service Indices in ETD order */
    std::vector<size_t> service_List;                                               // Array for the primary departures
    std::vector<size_t> ETDOrderedList;                                             // Array of Service Indices in ETD order
    
    // Process Management
    std::mutex dataMutex;                                                          // Process control
    
    // Often-used temporary variables
    std::stringstream ss;
    size_t i;
    size_t extract;
    size_t new_index;
    size_t index;
    std::string trainid;
    std::string ID;
    char c;
    

    // Private Methods
    
    // HTML processing
    mutable HTMLProcessor html_processor_;                                              // Reusable processor
    
    std::string processHtmlTags(const std::string& html) const {                        // Where you can't modify the original string
        return html_processor_.processHtmlTags(html);
    }
    
    void processHtmlTagsInPlace(std::string& html) const {                              // For cases where you can modify the original string
        html_processor_.processHtmlTagsInPlace(html);
    }
    
    // Cache prefetch
    void prefetchMetaData(const json& new_data);                                        // Cache the meta-data for all Services. Location, NRCC messages, Number of Services, etd/std and Departure Times
    
    // Cache hydration
    void hydrateBasicDataCache(size_t serviceIndex);                                    // Populate/refresh Basic Service Information cache for selected service.
    void hydrateBasicDataCacheInternal(size_t service_index);
    
    void hydrateAdditionalDataCache(size_t serviceIndex);                               // Populate/refresh Additional Service Information cache for selected service
    void hydrateAdditionalDataCacheInternal(size_t service_index);
    
    // Ordering and sorting departures for display
    void orderTheDepartureList();                                                       // Create an array of indices in order of departure time (STD and ETD - whichever is later)
    
    // Calling point extraction
    void ExtractCallingPoints(size_t serviceIndex, CallingPointDirection direction);    // Extract the SubsequentCallingPoints and PreviousCallingPoints vectors for the AdditionalServiceInfo data structure
    
    // Create null Basic and Additional Service data structures
    void CreateNullServiceInfo();                                                       // Create 'null' Basic and Additional Service Info to return if the service index is 999

    // Helper Functions
    
    // Safe container access utility
        template<typename Container>
        typename Container::value_type safe_at(
            const Container& container,
            size_t index,
            const typename Container::value_type& default_val = {}
        ) const {
            return (index < container.size()) ? container[index] : default_val;
        }
    
    // time_t to std::string (HH:MM format)
    std::string timeToHHMM(time_t timestamp) const {
        struct tm* tm_ptr = std::localtime(&timestamp); // Use localtime for display
        
        char buffer[6]; // "HH:MM\0"
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", tm_ptr->tm_hour, tm_ptr->tm_min);
        return std::string(buffer, 5); // Avoid strlen call
    }
    
    // JSON extractor functions
    
    // Return a value of selected type
    template<typename T> T extractJSONvalue(const json& source, const std::string& key, const T& default_value = T()) noexcept {
        try {
            auto it = source.find(key);
            if (it != source.end() && !it->is_null()) {
                if constexpr (std::is_same_v<T, std::string>) {
                    std::string value = it->get<std::string>();
                    return value.empty() ? default_value : value;
                } else {
                    return it->get<T>();
                }
            }
        } catch (...) {
            DEBUG_PRINT("Error extracting JSON value for key '" << key << "'");
        }
        return default_value;
    }
    
    // Return time value as a string
    std::string extractJSONTimeString(const json& source, const std::string& key, std::string default_value) {  // Handle repetative JSON extraction of time into a string
        try {
            if (source.contains(key) && !source[key].is_null()) {
                std::string value = source[key].get<std::string>();
                // Special handling for strings to check if they're empty
                if constexpr (std::is_same_v<std::string, std::string>) {
                    if (value.empty()) {
                        return default_value;
                    }
                }
                return value.substr(11,5);
            }
        } catch (const json::exception& e) {
            DEBUG_PRINT("Error extracting JSON value for key '" << key << "': " << e.what());
        }
        return default_value;
    }
    
    // Return time value as a time_t
    time_t extractJSONTime(const json& source, const std::string& key, const time_t& default_time) {
        try {
            auto it = source.find(key);
            if (it == source.end() || it->is_null()) {
                return default_time;
            }
            
            std::string time_str = it->get<std::string>();
            if (time_str.empty()) {
                return default_time;
            }
            
            std::tm tm = {};
            std::istringstream ss(time_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            
            if (ss.fail()) {
                DEBUG_PRINT("Failed to parse time: " << time_str);
                return default_time;
            }
            
            // Use timegm for UTC or consider timezone handling
            return std::mktime(&tm);
            
        } catch (const std::exception& e) {
            DEBUG_PRINT("Time extraction error: " << e.what());
            return default_time;
        }
    }
   
    // Extract value of selected type from nested JSON structure
    template<typename T>
    T extractNestedJSONvalue(const json& source, const std::string& key1, size_t index, const std::string& key2, const T& default_value = T()) {
        try {
            if (source.contains(key1) && source[key1].is_array() &&
                source[key1].size() > index &&
                source[key1][index].contains(key2) &&
                !source[key1][index][key2].is_null()) {
                
                T value = source[key1][index][key2].get<T>();
                
                // Special handling for strings to check if they're empty
                if constexpr (std::is_same_v<T, std::string>) {
                    if (value.empty()) {
                        return default_value;
                    }
                }
                return value;
            }
        } catch (const json::exception& e) {
            DEBUG_PRINT("Error extracting nested JSON value: " << e.what());
        }
        return default_value;
    }
};

#endif // TRAIN_SERVICE_PARSER_H




