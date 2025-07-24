// Train Display - an RGB matrix departure board for the Raspberry Pi
// JSON parsing
// Jon Morris Smith - Feb 2025
// Version 1.0
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board
//
// With thanks to:
// https://github.com/nlohmann/json
//
#include "train_service_parser.h"

// Load the cancellation/delay reason codes and cache them.
void TrainServiceParser::loadReasonCodes(const std::string& reasonJsonString){
    
    json refdata;
    size_t num_reasons;
    //size_t i;
    size_t code;
    //size_t new_index;
    std::vector<DelayCancelReason> new_delay_cancel_reasons;
    DelayCancelReason new_reason;
    
    refdata = json::parse(reasonJsonString);
    num_reasons = refdata.size();
    new_delay_cancel_reasons.reserve(num_reasons);
    
    DEBUG_PRINT("[Parser] Loading Cancellation/Delay Reason Codes");
    
    for(i=0; i < num_reasons; i++){                                                                                                         // Go through the reason code JSON and store in a new_reason object.
        code = refdata[i]["code"].get<size_t>();
        new_reason.code = std::to_string(code);
        new_reason.delayReason = extractJSONvalue<std::string>(refdata[i], "lateReason", "No Reason");
        new_reason.cancelReason = extractJSONvalue<std::string>(refdata[i], "cancReason", "No Reason");
        
        new_index = reason_codes.size();                                                                                                    // Store the code in the map
        reason_codes[new_reason.code] = new_index;
        new_delay_cancel_reasons.push_back(new_reason);                                                                                     // Add the code to the new delay/cancel reason vector
    }
    DEBUG_PRINT("[Parser] Delay/Cancellation codes loaded - " << num_reasons << " in the cache");
    delay_cancel_reasons.swap(new_delay_cancel_reasons);                                                                                    // Store the extracted reference data
    refdata_loaded = true;                                                                                                                  // Set the flag to indicate reference data has been loaded
    DEBUG_PRINT("[Parser] Creating null Basic/Additional Service items");
    CreateNullServiceInfo();
}

// Return the API version of the cached data
int64_t TrainServiceParser::getCacheAPIVersion(){
    return api_data_version;
}

// Wrapper function to execute pre-fetch and hydrate the departure cache.
void TrainServiceParser::updateCache(const std::string &jsonString, const int64_t &version){
    
    DEBUG_PRINT("[Parser] Updating the cache - pre-fetch and hydration of departure cache");
    prefetchCache(jsonString, version);
    hydrateDepartureCache();
    DEBUG_PRINT("[Parser] Cache updated");
}

// Wrapper function to load the reason codes, execute pre-fetch and hydrate the departure cache.
void TrainServiceParser::createFromJSON(const std::string &datajsonString, const std::string &reasonJsonString, const int64_t &version){
    
    if(!refdata_loaded){
        loadReasonCodes(reasonJsonString);
    }
    updateCache(datajsonString, version);
}

// Prefetch the cache and determine the order of departures
// Populate only the essentials to create the ordered list of departures
// std::time_t std;
// std::time_t etd;
// bool etd_specified
// std::string platform;
//
// Basic service information - the ServiceInfo data structure
// To update we build a new vector of BasicServiceInfo items (in the hydrateBasicServiceInfo method)
// This is swapped with the vector stored in the class
//
// Additional service information - the AdditionalServiceInfo data structure
// We need a copy of this so we can rebuild the vector stored in the class
// This is required so the two vectors remain in sync.

void TrainServiceParser::prefetchCache(const std::string& jsonString, const int64_t& api_version){
        
    std::vector<ServiceSequence> new_services_sequence(max_json_size);                                                                          // New Sequence of Services
    new_services_sequence.reserve(max_json_size);
    
    std::vector<BasicServiceInfo> new_services_basic(max_json_size);                                                                            // New Basic Service Info
    new_services_basic.reserve(max_json_size);
    
    std::vector<AdditionalServiceInfo> new_services_additions(max_json_size);                                                                   // New Additional Service Info
    new_services_additions.reserve(max_json_size);
    
    std::vector<CallingPointsInfo> new_services_callingpoints(max_json_size);                                                                   // New Calling Point Info
    new_services_callingpoints.reserve(max_json_size);
    
    json new_data;                                                                                                                              // New JSON data - use this and update the stored JSON at the end of the method
    
    std::unordered_map<std::string, size_t> new_cached_trainIDs;                                                                                // List of found TrainIDs
    size_t new_index;
    
    std::time_t now = std::time(nullptr);                                                                                                       // Use current time as the default value
    
    try {
        DEBUG_PRINT("[Parser] Cache pre-fetch Started");
        if(!refdata_loaded){
            throw std::out_of_range("Reference Data not loaded - fatal error!");
        }
        
        new_data = json::parse(jsonString);
        
        // Extract metadata
        prefetchMetaData(new_data);
        
        // Extract std, etd, platform and TrainID from the JSON
        // Check if the service is already cached - re-use if it is or create new Basic and Additional objects if it isn't.
        // Note: number_of_services is calculated in the prefetchMetaData method
        for (i=0; i< number_of_services; i++){
            new_services_sequence[i].std_specified = extractJSONvalue<bool>(new_data["trainServices"][i], "stdSpecified", false);
            if (new_services_sequence[i].std_specified) {                                                                                       // Valid Scheduled Departure Time means it's a departure!
                new_services_sequence[i].std = extractJSONTime(new_data["trainServices"][i], "std", now);
                
                new_services_sequence[i].etd_specified = extractJSONvalue<bool>(new_data["trainServices"][i], "etdSpecified", false);           // Is there an Estimated Departure Time?
                if (new_services_sequence[i].etd_specified) {                                                                                   // If there is then extract and store
                    new_services_sequence[i].etd = extractJSONTime(new_data["trainServices"][i], "etd", now);
                    new_services_sequence[i].departure_time = new_services_sequence[i].etd;                                                     // Departure Time is then the ETD
                } else {
                    new_services_sequence[i].departure_time = new_services_sequence[i].std;                                                     // No Estimated Departure Time, so Departure Time is then STD
                }
                
            } else {                                                                                                                            // No valid Schdeuled Departure Time means the service terminates here.
                new_services_sequence[i].std = INVALID_TIME;
                new_services_sequence[i].std_specified = false;
                new_services_sequence[i].etd = INVALID_TIME;
                new_services_sequence[i].etd_specified = false;
                new_services_sequence[i].departure_time = INVALID_TIME;
            }
            
            new_services_sequence[i].platform = extractJSONvalue<std::string>(new_data["trainServices"][i], "platform", "");
            new_services_sequence[i].trainid = extractJSONvalue<std::string>(new_data["trainServices"][i], "trainid", "");
            new_services_sequence[i].api_version = api_version;
                              
            // Check if we already have this service in the cache - if we do then re-use the existing Basic and Additiona Info structs.
            auto it = cached_trainIDs.find(new_services_sequence[i].trainid);
            
            if (it != cached_trainIDs.end()) {                                                                                                  // Service Information is cached
                extract = it->second;
                DEBUG_PRINT("   [Parser] Service " << it->first << " in the JSON is cached at index " << extract << ". Moving to new index: " << i << " and setting callingpoint/location data as stale. (valid service: " << new_services_sequence[extract].std_specified <<")." );
                new_services_basic[i] = services_basic[extract];                                                                                // Pull in the basic service information we have cached.
                
                                                                                                                                                // As this service is cached, the additional service information and calling point information also needs to be kept
                new_services_additions[i] = services_additions[extract];                                                                        // Store the additional information at the new position
                new_services_callingpoints[i] = services_callingpoints[extract];                                                                // Store the calling point information at the new position
                new_services_callingpoints[i].callingPointsCached = false;                                                                      // Flag calling point data as stale
                new_services_callingpoints[i].service_location_cached = false;                                                                  // Flag service-location data as stale
                
            } else {                                                                                                                            // No Service Information is cached. These are new, unpopulated objects
                DEBUG_PRINT("   [Parser] Service at position " << i << " (trainID " << new_services_sequence[i].trainid << ") in the JSON is not cached. Flagging all Basic and Additional static data as stale");
                new_services_additions[i].static_data_available = false;                                                                        // New items don't have the static data stored.
                new_services_basic[i].static_data_available = false;
                new_services_callingpoints[i].callingPointsCached = false;
                new_services_callingpoints[i].service_location_cached = false;
                
                new_services_additions[i].trainid = new_services_sequence[i].trainid;                                                           // Store the TrainID - a useful identifying to check things are in sync
                new_services_basic[i].trainid = new_services_sequence[i].trainid;
                new_services_callingpoints[i].trainid = new_services_sequence[i].trainid;
            }
        
            // Add the trainID to the unordered map
            new_index = new_cached_trainIDs.size();
            new_cached_trainIDs[new_services_sequence[i].trainid] = new_index;
        }
        
        {   // Store the newly parsed data in the private data structures and update the TrainID to index mapping
            std::lock_guard<std::mutex> lock(dataMutex);
            data = std::move(new_data);
            services_basic.swap(new_services_basic);
            services_additions.swap(new_services_additions);
            services_callingpoints.swap(new_services_callingpoints);
            services_sequence.swap(new_services_sequence);
            cached_trainIDs.swap(new_cached_trainIDs);
            api_data_version = api_version;
            
            if(debug_mode) {
                std::cout << "[Parser] ----- Prefetch: Cached Services and Indices -----" << std::endl;
                debugPrintTrainIDIndices();
            }
            
        }
        // Create the list of ordered departures
        orderTheDepartureList();
        // Void the existing service_list (of the next departures).
        std::fill(service_List.begin(), service_List.end(), 999);
        DEBUG_PRINT("[Parser] Cache pre-fetch Completed");
    } catch (const json::parse_error& e) {
        throw std::runtime_error("[Parser] Failed to parse JSON in cache pre-fetch " + std::string(e.what()));
    }
}

// Extract the meta-data from the JSON
// NRCC Messages
// Number of Services in the JSON
// Location of the departure board
void TrainServiceParser::prefetchMetaData(const json& new_data){
    // Temporary variables
    //std::stringstream ss;
    //char c;
    
    if (new_data.find("trainServices") != new_data.end() &&                                                                                 // Get the Number of Services in the JSON
        !new_data["trainServices"].is_null()) {
        number_of_services = new_data["trainServices"].size();
    } else {
        number_of_services = 0;
    }
    
    DEBUG_PRINT("[Parser] Cache pre-fetch meta-data - " << number_of_services << " services in JSON.");
    if(number_of_services > max_json_size) {
        number_of_services = max_json_size;
        DEBUG_PRINT("[Parser] Limiting to " << max_json_size << " services (configured maximum)");
    }
    
    // Cache the meta data which applies to all services
    if (location_name.empty()){                                                                                                             // Location - only need to populate this once.
        if (new_data.find("locationName") != new_data.end() &&
            !new_data["locationName"].is_null() &&
            !new_data["locationName"].empty()) {
            location_name = new_data["locationName"];
        } else {
            location_name = "";
        }
    }
    
    DEBUG_PRINT("   [Parser] Caching NRCC Messages");
    
    if (new_data.find("nrccMessages") != new_data.end() &&
        new_data["nrccMessages"].is_array() &&
        !new_data["nrccMessages"].empty()) {
        
        std::string combined_message;
        combined_message.reserve(512); // Pre-allocate for typical message size
        
        for (i = 0; i < new_data["nrccMessages"].size(); ++i) {
            if (i > 0) combined_message += " | ";
            
            const auto& messageObj = new_data["nrccMessages"][i];
            if (messageObj.contains("xhtmlMessage") && !messageObj["xhtmlMessage"].is_null()) {
                std::string message = messageObj["xhtmlMessage"].get<std::string>();
                
                // Use in-place processing for better performance
                html_processor_.processHtmlTagsInPlace(message);
                
                // Remove leading newline if present
                if (!message.empty() && message[0] == '\n') {
                    message.erase(0, 1);
                }
                
                combined_message += message;
            } else {
                DEBUG_PRINT("   [Parser] Message at index " << i << " has no valid content.");
            }
        }
        
        NRCC_message = std::move(combined_message);
        DEBUG_PRINT("   [Parser] NRCC Message cached");
    } else {
        NRCC_message.clear();
        DEBUG_PRINT("   [Parser] No NRCC Message found");
    }
    
    if (debug_mode) {
        auto stats = html_processor_.getPerformanceStats();
        if ((stats.neon_calls + stats.regular_calls) % 100 == 0) {
            DEBUG_PRINT("   [Parser] HTML Processor stats - NEON: " << stats.neon_calls << ", Regular: " << stats.regular_calls);
        }
    }
    
    DEBUG_PRINT("[Parser] Caching NRCC Messages complete");
}

// Store the sequence of departures in ETDOrderedList
// Use Scheduled (std) or Estimated (etd) time
// Note that 'departure time' in the pre-fetch data is std or etd (if an Estimated Time is specified)
// Note that if the destination is the same as the location then the service terminates here and must be excluded from ordered departures.

void TrainServiceParser::orderTheDepartureList() {
    //std::array<std::time_t, MAX_JSON_SIZE> time_list;
    std::vector<std::time_t> time_list(max_json_size);
    time_list.reserve(max_json_size);
    
    try {
        DEBUG_PRINT("[Parser] Ordering departure times Starting");
        // Get current time to use for date information
        std::time_t now = std::time(nullptr);
        std::tm *now_tm = std::localtime(&now);
        
        // Pre-allocate a single tm structure for reuse
        std::tm departure_time = {};
        departure_time.tm_year = now_tm->tm_year;
        departure_time.tm_mon = now_tm->tm_mon;
        departure_time.tm_mday = now_tm->tm_mday;
        departure_time.tm_sec = 0;
        
        // Process each service
        for (size_t i = 0; i < number_of_services; i++) {
            time_list[i] = services_sequence[i].departure_time;
            }
        
        if (debug_mode) {
            DEBUG_PRINT("   [Parser] Unsorted Departures");
            for (size_t i = 0; i < number_of_services; i++) {
                DEBUG_PRINT("   Index " << i << " of time_list array: TrainID: " << services_sequence[i].trainid
                            << " Platform " << services_sequence[i].platform
                            << " Departure time " << timeToHHMM(time_list[i]) << " derived from"
                            << " std specified:" << services_sequence[i].std_specified << " std: " << timeToHHMM(services_sequence[i].std)
                            << " etd specified:" << services_sequence[i].etd_specified << " etd: " << timeToHHMM(services_sequence[i].etd)
                            << " departure time cached:" << timeToHHMM(services_sequence[i].departure_time));
            }
            if (number_of_services == 0) {
                DEBUG_PRINT("   [Parser] No train services available");
            }
        }

        // Initialize ETDOrderedList with actual service count
        std::fill(ETDOrderedList.begin(), ETDOrderedList.end(), 999);
        for (size_t i = 0; i < number_of_services; i++) {
            ETDOrderedList[i] = i;
        }
        
        std::sort(ETDOrderedList.begin(), ETDOrderedList.begin() + number_of_services, [&time_list](size_t a, size_t b)
        {
            time_t time_a = time_list[a];
            time_t time_b = time_list[b];
            
            // Invalid times go to the end
            if (time_a == INVALID_TIME && time_b == INVALID_TIME) {
                return a < b;  // Stable sort by original index
            }
            if (time_a == INVALID_TIME) return false;  // a after b
            if (time_b == INVALID_TIME) return true;   // a before b
            
            // Both valid, sort by time
            return time_a < time_b;
        });
        
        // Debug output
        if (debug_mode) {
            DEBUG_PRINT("   [Parser] Departures in time order (Invalid times are at the end) ---");
            for (size_t i = 0; i < number_of_services; i++) {
                size_t idx = ETDOrderedList[i];
                std::tm* tm_time = std::localtime(&time_list[idx]);
                
                char buffer[10];
                std::strftime(buffer, sizeof(buffer), "%H:%M", tm_time);
                
                DEBUG_PRINT("   Position: " << i << " Index: " << idx << " TrainID: " << services_sequence[idx].trainid
                            << " Platform: " << services_sequence[idx].platform
                            << " Departure time: " << timeToHHMM(time_list[idx])  << " derived from"
                            << " std specified:" << services_sequence[idx].std_specified << " std: " << timeToHHMM(services_sequence[idx].std)
                            << " etd specified:" << services_sequence[idx].etd_specified << " etd: " << timeToHHMM(services_sequence[idx].etd)
                            << " departure time cached:" << timeToHHMM(services_sequence[idx].departure_time));
            }
        }
        DEBUG_PRINT("[Parser] Ordering departure times Completed");
    } catch (const json::exception& e) {
        throw std::runtime_error("[Parser] Error creating ordered list of departure times: " + std::string(e.what()));
    }
}


// Set the stored selected platform
void TrainServiceParser::setPlatform(std::string platform) {
    std::lock_guard<std::mutex> lock(dataMutex);
    selected_platform = std::move(platform);
    selectPlatform = true;
    DEBUG_PRINT("[Parser] Selected platform (" << selected_platform << ") stored and selectPlatform flag set to " << selectPlatform);
}


// Return the stored selected plaform
std::string TrainServiceParser:: getSelectedPlatform() {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (selectPlatform) {
        return selected_platform;
    } else {
        return "999";
    }
}

//Clear the stored selected platform
void TrainServiceParser::clearSelectedPlatform(){
    std::lock_guard<std::mutex> lock(dataMutex);
    selectPlatform = false;
    DEBUG_PRINT("[Parser] selectPlatform flag set to " << selectPlatform);
}

void TrainServiceParser::hydrateDepartureCache(){
    //size_t i;
    //size_t index;
    std::lock_guard<std::mutex> lock(dataMutex);
    
    try {
        DEBUG_PRINT("[Parser] Hydrating Departure Cache] for platform: " << selected_platform << "(Platform select flag: " << selectPlatform << ")");
        
        
        if (number_of_services == 0) {                                                                                          // Check if train services exist in the data
            DEBUG_PRINT("   [Parser] No train services found in the data");
            return;
        }
        
        if (selectPlatform) {                                                                                                   // Find departures for the selected platform
            
            size_t serviceCount = 0;
            DEBUG_PRINT("   [Parser] Searching for services at platform " << selected_platform);
            
            for (i = 0; i < number_of_services && serviceCount < service_List.size(); ++i) {                                    // Iterate through all train services in the pre-fetch cache
                index = ETDOrderedList[i];
                
                if (services_sequence[index].platform == selected_platform) {                                                   // Check if the platform matches the selected platform
                    
                    DEBUG_PRINT("   [Parser] Found service for platform " << selected_platform << " at service_index " << index);    // Found a service for the selected platform
                    if(services_sequence[index].std == INVALID_TIME) {
                        service_List[serviceCount] = 999;                                                                       // Service is an arrival - add 999 to the array (the sort function puts all of these at the end, so there will be no subsequent valid departures)
                        DEBUG_PRINT("   [Parser] Position " << i << " in the ordered departure list: Invalid departure at index " << index <<" (service terminates here - no subsequent valid departures.");
                    } else {
                        service_List[serviceCount] = index;                                                                     // Add its index to the array
                        DEBUG_PRINT("   [Parser] Position " << i << " in the ordered departure list: Valid departure at index " << index);
                    }
                    ++serviceCount;                                                                                             // Increment the count of services found
                }
            }
        } else {
            DEBUG_PRINT("   [Parser] Searching for services at all platforms ");
            for (i = 0; i < number_of_departures; i++) {                                                                        // Find the first NUM_OF_DEPARTURES departures
                if (i < number_of_services) {
                    if (ETDOrderedList[i] == 999) continue;                                                                     // Skip invalid services
                    if (services_sequence[ETDOrderedList[i] ].std == INVALID_TIME) continue;                                    // Skip arrivals at a terminus
                    service_List[i] = ETDOrderedList[i];
                    DEBUG_PRINT("   [Parser] Found service at position " << i << " in the ordered departure list.");            // Found a valid service
                }
            }
        }
        
        DEBUG_PRINT("  [Parser] Hydrating Basic Data Cache for the next " << number_of_departures << " departures");
        for (i=0; i< number_of_departures; i++) {
            index = service_List[i];
            DEBUG_PRINT("  [Parser] Departure: " << i << ". Initiating BasicData hydration for Service Index: " << index);
            if(index != 999) {                                                                                                  // Hydrate the basic cache for valid services
                hydrateBasicDataCacheInternal(index);
            }
        }
    
        
        if (debug_mode) {                                                                                                       // Debug information about the found service
            DEBUG_PRINT("   [Parser] --- Departure Cache Hydration Results ---");
            if (selectPlatform) {
                DEBUG_PRINT("   [Parser] Finding the first " << number_of_departures << " departures for platform " << selected_platform);
            } else {
                DEBUG_PRINT("   [Parser] Finding the first " << number_of_departures << " departures from all platforms ");
            }
            for (i=0; i < number_of_departures; i++) {
                index = service_List[i];
                if ( service_List[i] == 999) {
                    DEBUG_PRINT("   [Parser] Position " << i << " - Service Index: " << index <<". No valid service found (no departure or an arrival)");
                } else {
                    DEBUG_PRINT("   [Parser] Position " << i << " - Service Index: " << index << " Platform " << services_sequence[index].platform
                                << ": Sequence TrainID: " << services_sequence[index].trainid
                                << ": BasicInfo TrainID: " << services_basic[index].trainid
                                << ". Destination: " << services_basic[index].destination
                                << ". Scheduled departure: " << services_basic[index].scheduledDepartureTime
                                << " - Estimated departure: " << services_basic[index].estimatedDepartureTime
                                << " - Sequence Departure time:" << timeToHHMM(services_sequence[index].departure_time)
                                << ". Static Data available: " << services_basic[index].static_data_available);
                }
            }
            DEBUG_PRINT("   [Parser] End of Departure Cache Hydration Results ---");
            debugPrintServiceSequence();
            debugPrintTrainIDIndices();
            DEBUG_PRINT("[Parser] Hydrating Departure Cache Complete for platform: " << selected_platform << " (Plaform select flag: " << selectPlatform <<")");
        }
    } catch (const json::exception& e) {
        DEBUG_PRINT("[Parser] Error finding services: " << e.what());
    }
}




// Update the cache of basic service information
/* BasicServiceInfo data structure
   -------------------------
   * = populated in this method.
   S = static so doesn't need refreshing if the service is already cached.
   E = updated elsewhere
   F = Flag
 
   F  bool static_data_available
   F  int64_t apiDataVersion
   S* std::string trainid;
   S* std::string scheduledDepartureTime;
   S* std::string destination;
   S* std::string operator_name;
   S* std::string coaches;
   *  std::string estimatedDepartureTime;
   *  std::string platform;
   *  std::string cancelReason;
   *  std::string delayReason;
   *  std::string adhocAlerts;
   *  bool isCancelled;
   *  bool isDelayed;
*/

// Public version - thread-safe interface
void TrainServiceParser::hydrateBasicDataCache(size_t service_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    hydrateBasicDataCacheInternal(service_index);
}

// Internal version which assumes the caller already has a lock
void TrainServiceParser::hydrateBasicDataCacheInternal(size_t service_index) {
    BasicServiceInfo new_basic_item;
    
    ID = services_sequence[service_index].trainid;
    
    try {
        DEBUG_PRINT("[Parser] Basic Data cache hydration Starting for Service at Index " << service_index << ".");
        
        // Use TrainID to check this is the expected Service
        new_basic_item.trainid = extractJSONvalue<std::string>(data["trainServices"][service_index], "trainid", "");
        DEBUG_PRINT("  [Parser] Basic Data: Expected Service " << ID << " and got Service " << new_basic_item.trainid)
        if(new_basic_item.trainid != ID){
            throw std::out_of_range("Unexpected Service ID");
        }
        
        if (service_index >= max_json_size) {
            throw std::out_of_range("Service index exceeds maximum size");
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index exceeds current service count");
        }
        
        if(services_basic[service_index].static_data_available){                                                                                        // Basic Service Information is cached. Use the cached static data.Populate static data as it's new to the cache
            DEBUG_PRINT("  [Parser] Basic Data: Static data cached - re-using")
            new_basic_item = services_basic[service_index];
        } else {                                                                                                                                        // Basic Service Information is NOT cached. Populate static data as it's new to the cache
            DEBUG_PRINT("  [Parser] Basic Data: Static data not cached - hydrating");
            services_additions[service_index].static_data_available = false;                                                                            // If the Basic Service Information isn't cached, then any Additional Service Information is going to be stale.
            
            // Scheduled Time of Departure as a string
            new_basic_item.scheduledDepartureTime = extractJSONTimeString(data["trainServices"][service_index], "std", "");
            
            // Destination
            new_basic_item.destination = extractNestedJSONvalue<std::string>(data["trainServices"][service_index], "destination", 0, "locationName", "");
            
            // Operator
            new_basic_item.operator_name = extractJSONvalue<std::string>(data["trainServices"][service_index], "operator", "");
            
            // Coaches
            extract = extractJSONvalue<size_t>(data["trainServices"][service_index], "length", 0);
            if (extract !=0) {
                new_basic_item.coaches = std::to_string(extract);
            } else {
                new_basic_item.coaches = "";
            }
            
            new_basic_item.static_data_available = true;
        }
        
        // Update dynamic data if the cached dynamic data is from the latest API version
        
        if(new_basic_item.apiDataVersion != api_data_version){
            // Cancellation Status
            new_basic_item.isCancelled = extractJSONvalue<bool>(data["trainServices"][service_index], "isCancelled", false);
            
            // cancelReason
            extract = extractJSONvalue<size_t>(data["trainServices"][service_index]["cancelReason"], "Value", 0);
            new_basic_item.cancelReason = decodeCancelCode(extract);
            
            // delayReason
            extract = extractJSONvalue<size_t>(data["trainServices"][service_index]["delayReason"], "Value", 0);
            new_basic_item.delayReason = decodeDelayCode(extract);
            
            // adhocAlerts
            new_basic_item.adhocAlerts = extractJSONvalue<std::string>(data["trainServices"][service_index], "adhocAlerts", "");
            
            // EstimatedDepartureTime if an ETD is available
            //
            // We can obtain delay status from departureType.
            
            if(services_sequence[service_index].etd_specified) {                                                                                        // If an estimated time of departure is available...
                new_basic_item.estimatedDepartureTime = extractJSONTimeString(data["trainServices"][service_index], "etd", "");                         // ... then display that
                if (new_basic_item.estimatedDepartureTime == new_basic_item.scheduledDepartureTime) {                                                   // Catch situations where a service etd is set and the service is on time
                    new_basic_item.estimatedDepartureTime = "On Time";
                }
                DEBUG_PRINT("  [Parser] ETD found - storing " << new_basic_item.estimatedDepartureTime);
            } else {                                                                                                                                    // otherwise
                new_basic_item.estimatedDepartureTime = (new_basic_item.isCancelled) ? "Cancelled" : "On Time";                                         // Display 'On Time' or 'Cancelled'
                DEBUG_PRINT("  [Parser] No ETD found - storing " << new_basic_item.estimatedDepartureTime);
            }
            
            if(extractJSONvalue<std::string>(data["trainServices"][service_index], "departureType", "") == "Delayed" ) {                                // If a departure is marked as 'Delayed'
                DEBUG_PRINT("  [Parser] Service Departure Type is 'Delayed'. Setting the 'isDelayed' flag to true");
                new_basic_item.isDelayed = true;                                                                                                        // Set the delay flag
                if(!services_sequence[service_index].etd_specified) {                                                                                   // If no estimated time of departure is available then display 'Delayed' (otherwise leave the estimated departure time to be displayed)
                    new_basic_item.estimatedDepartureTime = "Delayed";
                    DEBUG_PRINT("  [Parser] Delayed and no ETD found - storing " << new_basic_item.estimatedDepartureTime);
                }
            } else {                                                                                                                                    // Otherwise it's not delayed
                new_basic_item.isDelayed = false;
                DEBUG_PRINT("  [Parser] Service Departure Type is not 'Delayed'. Setting the 'isDelayed' flag to false");
            }
     
            new_basic_item.apiDataVersion = api_data_version;                                                                                           // Store the new API Data version
            DEBUG_PRINT("  [Parser] New basic item API version: " << new_basic_item.apiDataVersion <<" from api_data_version: " << api_data_version);
        }
        // Store the service data
        services_basic[service_index] = new_basic_item;
        DEBUG_PRINT("[Parser] Basic Data cache hydration completed for Service at Index " << service_index << ".");
        
    } catch (const json::parse_error& e) {
        throw std::runtime_error("[Parse] Failed to parse JSON to hydrate basic data: " + std::string(e.what()));
    }
}


// Update cached additional service information
/* AdditionalServiceInfo data structure
   Additional Service Information
   -------------------------
   * = populated in this method.
   S = static so doesn't need refreshing if the service is already cached.
   E = updated elsewhere
 
   S* std::string trainid;
   *  uint64_t apiDataVersion;
   S* bool static_data_available;
   S* std::string origin;
   S* std::string loadingcategory;
   S* std::string loadingpercentage;
   S  std::vector<CoachInfo> Formation;
   *  bool platformIsHidden;
   S* bool serviceIsSupressed;
   S* bool isPassengerService;
   */

// Public version - thread-safe interface
void TrainServiceParser::hydrateAdditionalDataCache(size_t service_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    hydrateAdditionalDataCacheInternal(service_index);
}

// Internal version - assumes caller already has lock
void TrainServiceParser::hydrateAdditionalDataCacheInternal(size_t service_index){
    AdditionalServiceInfo new_additional_item;
    //std::string ID = services_sequence[service_index].trainid;
    ID = services_sequence[service_index].trainid;
    
    try {
        DEBUG_PRINT("[Parser] Additional Data cache hydration for service " << service_index <<".");
        
        // Use TrainID to check this is the expected Service
        new_additional_item.trainid = extractJSONvalue<std::string>(data["trainServices"][service_index], "trainid", "");
        DEBUG_PRINT("   [Parser] Additional Data: Expected Service " << ID << " and got Service " << new_additional_item.trainid)
        if(new_additional_item.trainid != ID){
            throw std::out_of_range("Unexpected Service ID");
        }
        
        if (service_index >= max_json_size) {
            throw std::out_of_range("Service index exceeds maximum size");
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index exceeds current service count");
        }
        
        if( services_additions[service_index].static_data_available){                            // If the version of the additional data matches the basic service data then the cache is valid
            DEBUG_PRINT("   [Parser] Additional service data at index " << service_index <<" is cached. Updating dynamic data (retaining any calling points)");
            new_additional_item = services_additions[service_index];                             // pull in cached additional info - only dynamic data needs to be updated
            
        } else {                                                                                // populate the static data.
            DEBUG_PRINT("   [Parser] Additional service data at index " << service_index <<" is new. Updating static and dynamic data (no calling points stored)");
            new_additional_item.static_data_available = true;                                   // flag that the static data is good
            
            // Origin
            new_additional_item.origin = extractNestedJSONvalue<std::string>(data["trainServices"][service_index], "origin", 0, "locationName", "");
            
            // Loading Category
            new_additional_item.loading_type = extractJSONvalue<std::string>(data["trainServices"][service_index]["formation"]["serviceLoading"]["loadingPercentage"], "type", "");
            
            // Loading Percentage
            new_additional_item.loadingPercentage = extractJSONvalue<size_t>(data["trainServices"][service_index]["formation"]["serviceLoading"]["loadingPercentage"], "value", 0);
            
            // Service is Suppressed
            new_additional_item.serviceIsSupressed = extractJSONvalue<bool>(data["trainServices"][service_index], "serviceIsSupressed", false);
            
            // Is a Passenger Service
            new_additional_item.isPassengerService = extractJSONvalue<bool>(data["trainServices"][service_index], "isPassengerService", false);
            
            // Populate std::vector<CoachInfo>
            
            services_additions[service_index].static_data_available = true;
        }
        
        // Following data is dynamic.
        if(new_additional_item.apiDataVersion != api_data_version){
           
            // Platform is hidden
            new_additional_item.platformIsHidden = extractJSONvalue<bool>(data["trainServices"][service_index], "platformIsHidden", false);
            
            new_additional_item.apiDataVersion = api_data_version;
        }
        {   // Store the newly parsed data in the private data structures
            services_additions[service_index] = new_additional_item;
            
            if(debug_mode) {
                std::cout << "[Parser] ----- Hydrate additional info: Cached Services and Indices -----" << std::endl;
                debugPrintTrainIDIndices();
            }
        }
        DEBUG_PRINT("[Parser] Additional Data cache hydration complete for service " << service_index <<".");
        
    } catch (const json::parse_error& e) {
        throw std::runtime_error("[Parser] Failed to parse JSON for additional data: " + std::string(e.what()));
    }
}

 
// Extract Location data for the calling points - data is used by the getCallingPoints and getLocation methods
/* CallingPointsInfo data structure
   Calling Points Information
   -------------------------
   * = populated in this method.
   S = static so doesn't need refreshing if the service is already cached.
   E = updated elsewhere
 
   E* std::string trainid;
   E* uint64_t apiDataVersion;
   E* bool callingPointsCached;
   E* std::string callingPoints;
   E* std::string callingPoints_with_ETD;
   *  std::vector<LocationInfo> PreviousCallingPoints;
   *  std::vector<LocationInfo> SubsequentCallingPoints;
   *  size_t num_previous_calling_points;
   *  size_t num_subsequent_calling_points;
   E* bool bool service_location_cached;
   E* std::string service_location;
 
   LocationInfo data structure
   Location Information for each Calling Point
   Note - since calling points are dynamic there is no static data
   -------------------------
   * = populated in this method.
   S = static so doesn't need refreshing if the service is already cached.
   E = updated elsewhere
 
   *  std::string locationName;
   *  bool isPass;
   *  bool isCancelled;
   *  std::string ArrivalTime;
   *  std::string ArrivalType;
   *  std::string DepartureTime;
 */


void TrainServiceParser::ExtractCallingPoints(size_t service_index, CallingPointDirection direction) {
    
    // Method to populate:
    //  std::vector<LocationInfo> PreviousCallingPoints;
    //  std::vector<LocationInfo> SubsequentCallingPoints
    // depending on the 'direction' parameter
    //
    // Output is stored in the CallingPointsInfo for the specified service 'service_index'
    
    
    std::vector<LocationInfo> new_list_of_calling_points;
    LocationInfo new_location;
    
    //std::string ID;
    //std::string trainid;
    
    json callingPoints;

    size_t number_of_calling_points;
    //size_t i;
    
    try {
        DEBUG_PRINT("[Parser] Extracting calling-points for service at index " << service_index);
        
        // Use TrainID to check this is the expected Service
        ID = services_sequence[service_index].trainid;
        trainid = extractJSONvalue<std::string>(data["trainServices"][service_index], "trainid", "");
        DEBUG_PRINT("   [Parser] Extract calling-points: Expected Service " << ID << " and got Service " << trainid)
        if(trainid != ID){
            throw std::out_of_range("Unexpected TrainID");
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        
        if(direction == SUBSEQUENT) {
            callingPoints = data["trainServices"][service_index]["subsequentLocations"];
            DEBUG_PRINT("   [Parser] Extracting Subsequent calling points");
        } else {
            callingPoints = data["trainServices"][service_index]["previousLocations"];
            DEBUG_PRINT("   [Parser] Extracting Previous calling points");
        }
        
        number_of_calling_points = callingPoints.size();
        new_list_of_calling_points.reserve(number_of_calling_points);
        
        // Calling points are a vector of LocationInfo objects.
        // This loop extracts each calling point from the JSON data and stores it in a vector (new_list_of_calling_points)
        // Once complete this the vector is stored in the calling-points information object (services_callingpoints)
        // number_of_calling_points is the number of calling points in the JSON (some of which may be passed)
 
        for (i = 0; i < number_of_calling_points; ++i) {
            // std::string locationName;
            //*  bool isPass;
            //*  bool isCancelled;
            //*  std::string ArrivalTime;
            //*  std::string DepartureTime;
            
            // If locationName exists, is not null, and isn't empty, store it
            new_location.locationName = extractJSONvalue<std::string>(callingPoints[i], "locationName", "");
            
            // If isPass exists and is not null, store it
            new_location.isPass = extractJSONvalue<bool>(callingPoints[i], "isPass", false);
            
            // If isCancelled exists and is not null, store it
            new_location.isCancelled = extractJSONvalue<bool>(callingPoints[i], "isCancelled", false);
            
            if(direction == SUBSEQUENT) {
                if(extractJSONvalue<bool>(callingPoints[i], "atdSpecified", false)){
                    new_location.DepartureTime = extractJSONTimeString(callingPoints[i], "atd", "");
                } else {
                    if(extractJSONvalue<bool>(callingPoints[i], "etdSpecified", false)){
                        new_location.DepartureTime = extractJSONTimeString(callingPoints[i], "etd", "");
                    } else {
                        if(extractJSONvalue<bool>(callingPoints[i], "stdSpecified", false)){
                            new_location.DepartureTime = extractJSONTimeString(callingPoints[i], "std", "");
                        }
                    }
                }
            } else {
                new_location.ArrivalType = extractJSONvalue<std::string>(callingPoints[i], "arrivalType", "");
                if(extractJSONvalue<bool>(callingPoints[i], "ataSpecified", false)){
                    new_location.ArrivalTime = extractJSONTimeString(callingPoints[i], "ata", "");
                } else {
                    if(extractJSONvalue<bool>(callingPoints[i], "etaSpecified", false)){
                        new_location.ArrivalTime = extractJSONTimeString(callingPoints[i], "eta", "");
                    } else {
                        if(extractJSONvalue<bool>(callingPoints[i], "staSpecified", false)){
                            new_location.ArrivalTime = extractJSONTimeString(callingPoints[i], "sta", "");
                        }
                    }
                }
            }
            // Add the new location object to the new list of calling points
            new_list_of_calling_points.push_back(new_location);
        }
        DEBUG_PRINT("   [Parser] " <<number_of_calling_points << " calling points found and stored (not all of these are used for departure boards).");
        
        if (direction == SUBSEQUENT) {
            services_callingpoints[service_index].num_subsequent_calling_points = number_of_calling_points;
            services_callingpoints[service_index].SubsequentCallingPoints.swap(new_list_of_calling_points);
        } else {
            services_callingpoints[service_index].num_previous_calling_points = number_of_calling_points;
            services_callingpoints[service_index].PreviousCallingPoints.swap(new_list_of_calling_points);
        }
        DEBUG_PRINT("[Parser] Extracting calling-points complete for service at index " << service_index);
    } catch (const json::exception& e) {
        throw std::runtime_error("[Parser] Error extracting Calling Points: " + std::string(e.what()));
    }
}


std::string TrainServiceParser::getCallingPoints(size_t service_index, CallingPointETD show_ETD) {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    std::string result;
    std::string result_with_etd;
    size_t estimated_size = services_callingpoints[service_index].num_subsequent_calling_points * 25;
    
    result.reserve(estimated_size);
    result_with_etd.reserve(estimated_size * 2);
    
    try {
        DEBUG_PRINT("[Parser] Creating the Calling Point string for service " << service_index << " (show the ETD: "<< show_ETD <<" )");
        // Use TrainID to check this is the expected Service
        ID = services_sequence[service_index].trainid;
        trainid = extractJSONvalue<std::string>(data["trainServices"][service_index], "trainid", "");
        DEBUG_PRINT("   [Parser] Calling Points: Expected Service " << ID << " and got Service " << trainid
                    << ". Calling Points cached flag: " << services_callingpoints[service_index].callingPointsCached
                    << ". Data version for cached calling points: " << services_callingpoints[service_index].apiDataVersion);
        if(trainid != ID){
            throw std::out_of_range("Unexpected TrainID");
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        
        // if this is cached then return the stored strings
        if (show_ETD == NOETD) {
            if (services_callingpoints[service_index].callingPointsCached && (services_callingpoints[service_index].apiDataVersion == api_data_version)){
                DEBUG_PRINT("   [Parser] Calling Points without ETD already cached.");
                return services_callingpoints[service_index].callingPoints;
            }
        } else {
            if (services_callingpoints[service_index].callingPointsCached && (services_callingpoints[service_index].apiDataVersion == api_data_version)){
                DEBUG_PRINT("   [Parser] Calling Points with ETD already cached");
                return services_callingpoints[service_index].callingPoints_with_ETD;
            }
        }

        // calling points aren't in the cache - so build and store the data
        
        // Refresh the subsequent calling point cache
        ExtractCallingPoints(service_index, SUBSEQUENT);
            
        DEBUG_PRINT("   [Parser] Creating calling-point string as not cached")
        
        bool first = true;
        for (size_t i = 0; i < services_callingpoints[service_index].num_subsequent_calling_points; i++) {
            if (!services_callingpoints[service_index].SubsequentCallingPoints[i].isPass) {
                if (!first) {
                    result += ", ";
                    result_with_etd += " ";
                }
                
                const auto& location = services_callingpoints[service_index].SubsequentCallingPoints[i];
                result += location.locationName;
                
                if(!location.DepartureTime.empty()){
                    result_with_etd += location.locationName;
                    result_with_etd += " (";
                    result_with_etd += location.DepartureTime;
                    result_with_etd += ")";
                }
                
                first = false;
            }
        }
            
        // Store both versions
        services_callingpoints[service_index].callingPoints = result;
        services_callingpoints[service_index].callingPoints_with_ETD = result_with_etd;
        
        services_callingpoints[service_index].callingPointsCached = true;
        services_callingpoints[service_index].apiDataVersion = api_data_version;
        services_callingpoints[service_index].trainid = services_sequence[service_index].trainid;
        
        DEBUG_PRINT("[Parser] Creating the Calling Point string complete for service " << service_index << " (show the ETD: "<< show_ETD <<" )");
        return (show_ETD == NOETD) ? result : result_with_etd;
        
    } catch (const json::exception& e) {
        DEBUG_PRINT(data);
        throw std::runtime_error("[Parser] Error creating calling points: " + std::string(e.what()));
        // Return an empty string
        return "";
    }
}

// Lazy load the Service Location.
/* CallingPointsInfo data structure
   Calling Points Information
   -------------------------
   * = populated in this method.
   S = static so doesn't need refreshing if the service is already cached.
   E = updated elsewhere
 
   S* std::string trainid;
   E* uint64_t apiDataVersion;
   E* bool callingPointsCached;
   E* std::string callingPoints;
   E* std::string callingPoints_with_ETD;
   E* std::vector<LocationInfo> PreviousCallingPoints;
   E* std::vector<LocationInfo> SubsequentCallingPoints;
   E* size_t num_previous_calling_points;
   E* size_t num_subsequent_calling_points;
   *  bool bool service_location_cached;
   *  std::string service_location;
*/

std::string TrainServiceParser::getServiceLocation(size_t service_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    //std::string ID;
    //std::string  trainid;
    //std::stringstream ss;
    //size_t i;
    size_t num_calling_points;
    size_t current_stop = 0;
    size_t next_stop = 0;
    
    try {
        DEBUG_PRINT("[Parser] Finding location of Service Starting] for service at index: " << service_index);
        if (service_index == 999){
            DEBUG_PRINT("   [Parser] WARNING - requested Service location for service_index 999 - returning an empty string");
            return "";
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        
        ID = services_sequence[service_index].trainid;                                                                                                       // Use TrainID to check this is the expected Service
        trainid = extractJSONvalue<std::string>(data["trainServices"][service_index], "trainid", "");
        DEBUG_PRINT("   [Parser] Expected Service " << ID << " and got Service " << trainid
                    << ". Location cached flag: " << services_callingpoints[service_index].service_location_cached
                    << ". Data version for cached location: " << services_callingpoints[service_index].apiDataVersion);
        if(trainid != ID){
            throw std::out_of_range("Unexpected TrainID");
        }
        
        if(services_callingpoints[service_index].service_location_cached && services_callingpoints[service_index].apiDataVersion == api_data_version) {     // If we have this cached then use that data.
            DEBUG_PRINT("   [Parser] Service location cached for index " << service_index << ". Using that data");
            return services_callingpoints[service_index].service_location;
        }
        
        DEBUG_PRINT("   [Parser] Service location not cached - calculating!");
        
        // Refresh the previoous calling point cache
        ExtractCallingPoints(service_index, PREVIOUS);
        
        num_calling_points = services_callingpoints[service_index].num_previous_calling_points;
        if(num_calling_points == 0) {                                                                                                                       // If there are no calling points, then the location is the origin of the Service
            DEBUG_PRINT("   [Parser] No previous calling points - the service starts here. Finding location completed for service at index: " << service_index);
            return "";
        }
        
        for (i=0; i < num_calling_points ; i++) {
            if (!services_callingpoints[service_index].PreviousCallingPoints[i].isPass) {
                if (next_stop == 0) {
                    if (services_callingpoints[service_index].PreviousCallingPoints[i].ArrivalType != "Actual") {                                         // This was originally 'Forecast' - setting to 'not Actual' (as we have both 'Forecast' and 'Delayed' to handle)
                        next_stop = i;
                    } else {
                        current_stop = i;
                    }
                }
                DEBUG_PRINT("   [Parser] current: " << current_stop << ". next: " << next_stop << ".  position: " << i << ". Location: "
                            << services_callingpoints[service_index].PreviousCallingPoints[i].locationName << ". Arrival: "
                            << services_callingpoints[service_index].PreviousCallingPoints[i].ArrivalTime << ". Arrival Type: "
                            << services_callingpoints[service_index].PreviousCallingPoints[i].ArrivalType <<".");
            }
        }
        
        ss.str("");
        
        if (next_stop == 0) {
            ss << "This service is between " << services_callingpoints[service_index].PreviousCallingPoints[current_stop].locationName << " and " << location_name;
        } else {
            ss << "This service is between " << services_callingpoints[service_index].PreviousCallingPoints[current_stop].locationName << " and " << services_callingpoints[service_index].PreviousCallingPoints[next_stop].locationName;
        }
        
        DEBUG_PRINT("   [Parser] Storing API version, location-cached-flag and service location ====> " << ss.str());
        services_callingpoints[service_index].service_location = ss.str();
        services_callingpoints[service_index].service_location_cached = true;
        services_callingpoints[service_index].apiDataVersion = api_data_version;
        
        DEBUG_PRINT("[Parser] Finding location of Service Completed for service at index: " << service_index);
        return services_callingpoints[service_index].service_location;
    }  catch (const json::exception& e) {
        throw std::runtime_error("[Parser] Error finding service location: " + std::string(e.what()));
        // Return an empty string
        return "";
        
    }
    
}

// Return the text stored in the cached reference data for the specified delay code
std::string TrainServiceParser::decodeDelayCode(size_t delay_code){
    
    // find the code in the cached reference data
    auto it = reason_codes.find(std::to_string(delay_code));
    //size_t extract;
    
    if (it != reason_codes.end()) {
        extract = it->second;
        DEBUG_PRINT("[Parser] Find Delay Code " << it->first << " found at location " << extract << " and decodes as " << delay_cancel_reasons[extract].delayReason);
        return delay_cancel_reasons[extract].delayReason;
    }
    return "";
}

// Return the text stored in the cached reference data for the specified cancellation code
std::string TrainServiceParser::decodeCancelCode(size_t delay_code){
    
    // find the code in the cached reference data
    auto it = reason_codes.find(std::to_string(delay_code));
    //size_t extract;
    
    if (it != reason_codes.end()) {
        extract = it->second;
        DEBUG_PRINT("[Parser] Find Cancellation Code " << it->first << " found at location " << extract << " and decodes as " << delay_cancel_reasons[extract].cancelReason);
        return delay_cancel_reasons[extract].cancelReason;
    }
    return "";
}


// All the 'Get' functions

TrainServiceParser::BasicServiceInfo TrainServiceParser::getBasicServiceInfo(size_t service_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    try {
        if (service_index == 999){
            DEBUG_PRINT("[Parser] WARNING - requested Basic Service info for service_index 999 - returning the null structure");
            return null_basic_service;
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        
        if (services_basic[service_index].apiDataVersion != api_data_version || services_basic[service_index].static_data_available == false ){
            DEBUG_PRINT("[Parser] Requested Basic Service info for service_index " << service_index
                        << ". Stored API version (" << services_basic[service_index].apiDataVersion << ") vs current data version ( " << api_data_version << ") or static data available flag (" << services_basic[service_index].static_data_available
                        << ") initiates hydration of Basic Data cache.");
            hydrateBasicDataCacheInternal(service_index);
        }
        
        return services_basic[service_index];
    } catch (const json::exception& e) {
        throw std::out_of_range("[Parser] Error getting service data-structure: " + std::string(e.what()));
    }
}

TrainServiceParser::AdditionalServiceInfo TrainServiceParser::getAdditionalServiceInfo(size_t service_index) {
    std::lock_guard<std::mutex> lock(dataMutex);
    try {
        if (service_index == 999){
            DEBUG_PRINT("   [Parser] WARNING - requested Additional Service Information for service_index 999 - returning the null structure");
            return null_additional_service;
        }
        
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        
        if (services_additions[service_index].apiDataVersion != api_data_version){
            DEBUG_PRINT("[Parser] Requested Additional Service info for service_index " << service_index
                        << ". Stored API version (" << services_additions[service_index].apiDataVersion << ") vs current data version ( " << api_data_version << ") or static data available flag (" << services_additions[service_index].static_data_available
                        << ") initiates hydration of Additional Data cache.");
            hydrateAdditionalDataCacheInternal(service_index);
        }
        
        return services_additions[service_index];
    } catch (const json::exception& e) {
        throw std::out_of_range("[Parser] Error getting service data-structure: " + std::string(e.what()));
    }
}

size_t TrainServiceParser::getOrdinalDeparture(size_t service_number) {
    try {
        if (service_number == 0 || service_number > number_of_departures) {  //Check against departures
            throw std::out_of_range("Ordinal service number out of range");
        }
        
        if (service_number - 1 >= service_List.size()) {  // Additional safety check
            throw std::out_of_range("Service list index out of range");
        }
        
        //return service_List[service_number - 1];
        return safe_at(service_List, service_number - 1, static_cast<size_t>(999));
    } catch (const json::exception& e) {
        throw std::out_of_range("[Parser] Error getting service data-structure: " + std::string(e.what()));
    }
}

std::string TrainServiceParser::getPlatform(size_t service_index){
    if (service_index > number_of_departures) {
        return "";
    } else {
        return services_sequence[service_index].platform;
    }
}

size_t TrainServiceParser::getFirstDeparture() {
    std::lock_guard<std::mutex> lock(dataMutex);
    //return service_List[0];
    return safe_at(service_List, 0, static_cast<size_t>(999));
}

size_t TrainServiceParser::getSecondDeparture(){
    std::lock_guard<std::mutex> lock(dataMutex);
    //return service_List[1];
    return safe_at(service_List, 1, static_cast<size_t>(999));
}

size_t TrainServiceParser::getThirdDeparture(){
    std::lock_guard<std::mutex> lock(dataMutex);
    //return service_List[2];
    return safe_at(service_List, 2, static_cast<size_t>(999));
}

void TrainServiceParser::CreateNullServiceInfo(){
    /*
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
     */
    
    null_basic_service.trainid = "9999";
    null_basic_service.destination = "Nowhere";
    null_basic_service.scheduledDepartureTime = "99:99";
    null_basic_service.estimatedDepartureTime = "99:99";
    null_basic_service.operator_name = "Nobody";
    null_basic_service.coaches = "";
    null_basic_service.isCancelled = false;
    null_basic_service.isDelayed = false;
    null_basic_service.cancelReason = "Null Service - Cancellation Reason";
    null_basic_service.delayReason = "Null Service - Delay Reason";
    null_basic_service.apiDataVersion = 0;
    null_basic_service.static_data_available = true;
    
    /*
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
     */
    
    null_additional_service.trainid = "9999";
    null_additional_service.apiDataVersion = 0;
    null_additional_service.static_data_available = true;
    null_additional_service.origin = "Nowhere";
    null_additional_service.loading_type = "999";
    null_additional_service.loadingPercentage = 99;
    null_additional_service.platformIsHidden = false;
    null_additional_service.serviceIsSupressed = false;
    null_additional_service.isPassengerService = true;
    
    DEBUG_PRINT("   [Parser] Null Basic and Additional service items created");
}


/*
 time_t std;                                 // Scheduled Departure Time
 bool etd_specified;                         // Is there an ETD?
 time_t etd;                                 // Estimated Departure Time
 time_t departure_time;                      // Departure time - std or etd (if specified)
 std::string platform;                       // Platform
 std::string trainid;                        // Train Identifier
 uint64_t api_version;                       // Version of the API data used
 */

void TrainServiceParser::debugPrintServiceSequence(){
    //size_t i;
    
    std::cout << "[Parser] Service Sequence Information: " << std::endl;
    
    for(i = 0; i < number_of_services; i++){
        std::cerr << "   Index: " << i <<" ->   std:" << timeToHHMM(services_sequence[i].std) <<
        ". etd_specified:" << services_sequence[i].etd_specified <<
        ". departure_time:" << timeToHHMM(services_sequence[i].departure_time) <<
        ". platform:" << services_sequence[i].platform <<
        ". trainid:" << services_sequence[i].trainid <<
        ". api_version:" << services_sequence[i].api_version << std::endl;
    }
}


/*
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
 */
void TrainServiceParser::debugPrintBasicServiceInfo(size_t service_index) {
    try {
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        std::cout << "[Parser] -----------------------" << std::endl;
        std::cout << "Basic Information for Service: " << service_index << std::endl;
        
        std::cout << "trainid: " << services_basic[service_index].trainid << std::endl;
        std::cout << "apiDataVersion: " << services_basic[service_index].apiDataVersion << std::endl;
        std::cout << "static_data_available: " << services_basic[service_index].static_data_available << std::endl;
        
        std::cout << "scheduledDepartureTime: " << services_basic[service_index].scheduledDepartureTime << std::endl;
        std::cout << "estimatedDepartureTime: " << services_basic[service_index].estimatedDepartureTime << std::endl;
        
        std::cout << "destination: " << services_basic[service_index].destination << std::endl;
        std::cout << "operator_name: " << services_basic[service_index].operator_name << std::endl;
        std::cout << "coaches: " << services_basic[service_index].coaches << std::endl;
        
        std::cout << "isCancelled: " << services_basic[service_index].isCancelled << std::endl;
        std::cout << "cancelReason: " << services_basic[service_index].cancelReason << std::endl;
        
        std::cout << "isDelayed: " << services_basic[service_index].isDelayed << std::endl;
        std::cout << "delayReason: " << services_basic[service_index].delayReason << std::endl;
        
        std::cout << "adhocAlerts: " << services_basic[service_index].adhocAlerts << std::endl;
        
        std::cout << "[Parser] -----------------------" << std::endl;
        
    } catch (const json::exception& e) {
        throw std::runtime_error("Error printing service data-structure: " + std::string(e.what()));
    }
}

/*
 std::string trainid;
 uint64_t apiDataVersion;
 bool static_data_available;
 std::string origin;
 std::string loadingcategory;
 std::string loadingpercentage;
 std::vector<CoachInfo> Formation;
 bool platformIsHidden;
 bool serviceIsSupressed;
 bool isPassengerService;
 */

void TrainServiceParser::debugPrintAdditionalServiceInfo(size_t service_index) {
    try {
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        std::cout << "[Parser] ----------------------------" << std::endl;
        std::cout << "Additional Information for Service: " << service_index << std::endl;
        
        std::cout <<  "trainid: " << services_additions[service_index].trainid << std::endl;
        std::cout <<  "apiDataVersion: " << services_additions[service_index].apiDataVersion << std::endl;
        std::cout <<  "static_data_available: " << services_additions[service_index].static_data_available << std::endl;
        
        std::cout <<  "origin: " << services_additions[service_index].origin << std::endl;
        
        std::cout <<  "loadingcategory: " << services_additions[service_index].loading_type << std::endl;
        std::cout <<  "loadingpercentage: " << services_additions[service_index].loadingPercentage << std::endl;
        
        std::cout <<  "platformIsHidden: " << services_additions[service_index].platformIsHidden << std::endl;
        std::cout <<  "serviceIsSupressed: " << services_additions[service_index].serviceIsSupressed << std::endl;
        std::cout <<  "isPassengerService: " << services_additions[service_index].isPassengerService << std::endl;

        std::cout << "[Parser] ----------------------------" << std::endl;
        
    } catch (const json::exception& e) {
        throw std::runtime_error("Error printing service data-structure: " + std::string(e.what()));
    }
}

/*  std::string trainid;
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
*/
void TrainServiceParser::debugPrintCallingPointsInfo(size_t service_index){
    try {
        if (service_index >= number_of_services) {
            throw std::out_of_range("Service index out of range");
        }
        std::cout << "[Parser] -----------------------------" << std::endl;
        std::cout << "Calling Point Information for Service: " << service_index << std::endl;
        std::cout <<  "apiDataVersion: " << services_callingpoints[service_index].apiDataVersion << std::endl;
        
        std::cout <<  "callingPointsCached: " << services_callingpoints[service_index].callingPointsCached << std::endl;
        std::cout <<  "callingPoints: " << services_callingpoints[service_index].callingPoints << std::endl;
        std::cout <<  "callingPoints_with_ETD: " << services_callingpoints[service_index].callingPoints_with_ETD << std::endl;
        
        std::cout <<  "num_previous_calling_points: " << services_callingpoints[service_index].num_previous_calling_points << std::endl;
        std::cout <<  "num_subsequent_calling_points: " << services_callingpoints[service_index].num_subsequent_calling_points << std::endl;
        
        std::cout <<  "service_location_cached: " << services_callingpoints[service_index].service_location_cached << std::endl;
        std::cout <<  "service_location: " << services_callingpoints[service_index].service_location << std::endl;

        std::cout << "[Parser] ----------------------------" << std::endl;
        
    } catch (const json::exception& e) {
        throw std::runtime_error("Error printing service data-structure: " + std::string(e.what()));
    }
}




void TrainServiceParser::debugPrintTrainIDIndices() {
    std::cout << "[Parser] Cache indices for TrainIDs" << std::endl;
    std::cout << "   [Parser] Total entries in map: " << cached_trainIDs.size() << std::endl;
    std::cout << "   Counter: first (hex) -> second. Basic TrainID (API ver., static data flag) Additional TrainID (API ver., static data flag)." << std::endl;
    
    int counter = 0;
    for (auto it = cached_trainIDs.begin(); it != cached_trainIDs.end(); it++, counter++) {
        // Convert empty strings to a visible placeholder
        std::string key_display = it->first.empty() ? "[EMPTY]" : it->first;
        
        // Print with counter and hex representation for troubleshooting
        std::cout << "   " << counter << ": " << key_display << " (";
        
        // Print hex values of each character in the key to spot invisible chars
        for (char c : it->first) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(c)) << " ";
        }
        
        std::cout << std::dec << ") -> index " << it->second << ". Basic TrainID: " << services_basic[it->second].trainid << " (APIversion: " << services_basic[it->second].apiDataVersion <<  ", static data available: " << services_basic[it->second].static_data_available <<  "). Additional info: " << services_additions[it->second].trainid << " (APIversion " << services_additions[it->second].apiDataVersion << ", static data available: " << services_additions[it->second].static_data_available << ")." << std::endl;
    }
    std::cout << "[Parser] End cache indices for TrainIDs" << std::endl;
}

void TrainServiceParser::debugPrintReasonCancelCodes(){
    std::cout << "[Parser]Indices for Cancellation and Delay Codes" << std::endl;
    std::cout << "   [Parser]Total entries in map: " << delay_cancel_reasons.size() << std::endl;
    std::cout << "   [Parser]Counter: first (hex) -> second. Code.  Cancellation Reason.  Delay Reason." << std::endl;
    
    int counter = 0;
    for (auto it = reason_codes.begin(); it != reason_codes.end(); it++, counter++) {
        // Convert empty strings to a visible placeholder
        std::string key_display = it->first.empty() ? "[EMPTY]" : it->first;
        
        // Print with counter and hex representation for troubleshooting
        std::cout << counter << ": " << key_display << " (";
        
        // Print hex values of each character in the key to spot invisible chars
        for (char c : it->first) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(c)) << " ";
        }
        
        std::cout << std::dec << ") -> index " << it->second << ". Code: " << delay_cancel_reasons[it->second].code << " Cancellation Reason: " << delay_cancel_reasons[it->second].cancelReason <<  ", delay reason: " << delay_cancel_reasons[it->second].delayReason << std::endl;
    }
    std::cout << "[Parser] End Cancellation and Delay Codes" << std::endl;
}


