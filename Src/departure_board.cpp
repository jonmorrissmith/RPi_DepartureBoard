//
//  departure_board.cpp
//  Departure_Board
//
//  Created by Jon Morris-Smith on 23/06/2025.
//

#include <stdexcept>
#include <iostream>
#include "departure_board.h"


DepartureBoard::DepartureBoard(const Config& cfg) :
board_config(cfg),                                                                                  // Store reference
api_config{
    cfg.get("StaffAPIKey"),                                                                         // staff_api_key
    cfg.get("DelayCancelAPIKey"),                                                                   // reason_code_api_key
    cfg.getBoolWithDefault("debug_mode", true),                                                     // debug_mode
    cfg.getStringWithDefault("debug_log_dir", "/tmp")                                               // debug_log_dir
},
api_client(api_config),                                                                             // Pass config to APIClient
parser(10, 3),                                                                                      // max_services=10, max_departures=3
matrix(cfg)                                                                                         // Pass config to MatrixDriver
{
    debug_mode = board_config.getBoolWithDefault("debug_mode", false);
    DEBUG_PRINT("[Departure_Board] constructor: Initializing components");
    initialise();
}

DepartureBoard::DepartureBoard(const Config& cfg, const std::string& staff_api_key, const std::string& reason_code_api_key) :
board_config(cfg),
      
api_config{                                                                                         // Initialize APIConfig struct
    staff_api_key,                                                                                  // Use provided key
    reason_code_api_key,                                                                            // Use provided key
    cfg.getBoolWithDefault("debug_mode", true),                                                     // debug_mode
    cfg.getStringWithDefault("debug_log_dir", "/tmp")
},
api_client(api_config),
parser(cfg.getIntWithDefault("max_services", 10), cfg.getIntWithDefault("max_departures", 3)),
matrix(cfg)
{
    debug_mode = board_config.getBoolWithDefault("debug_mode", false);
    DEBUG_PRINT("[Departure_Board] constructor: Initializing with explicit API keys");
    initialise();
}

DepartureBoard::~DepartureBoard() {
    if (is_running) {
        stop();
    }
  
    if (api_thread.joinable()) {                                                                    // Clean up the API thread if it's still running
        try {
            api_thread.join();
        } catch (const std::exception& e) {
            std::cerr << "Error joining API thread in destructor: " << e.what() << std::endl;
        }
    }
    
    DEBUG_PRINT("[DepartureBoard destructor] Cleanup complete");
}

void DepartureBoard::initialise(){
    
    initialiseAPI();
    initialiseParser();
    initialiseDisplay();
}

void DepartureBoard::initialiseAPI(){
    try {
        DEBUG_PRINT("[Departure_Board] Initialising API client");
        
        if (api_config.staff_api_key.empty()) {
            throw std::runtime_error("Staff API key not configured");
        }
        
        if (api_config.reason_code_api_key.empty()) {
            throw std::runtime_error("Delay/Cancel API key not configured");
        }
        
        refdata = api_client.fetchReasonCodes();
        location_code = board_config.get("location");
        departures = api_client.fetchDepartures(location_code);
        api_data_version = api_client.getCurrentAPIVersion();
        
        data_refresh_interval = board_config.getInt("refresh_interval_seconds");
        last_data_refresh = std::chrono::steady_clock::now();
        data_refresh_completed.store(false);                                            // Set flags to indicate completion
        data_refresh_pending.store(false);
        DEBUG_PRINT("[Departure_Board] API client initialised");
        
    } catch(const std::exception& e) {
        std::cerr << "[Departure_Board] Error configuring API" << e.what() << std::endl;
    }
}

void DepartureBoard::initialiseParser(){
    try {
        DEBUG_PRINT("[Departure_Board] Initialising Parser");
        
        if(board_config.get("platform").empty()){
            selected_platform = "";
            DEBUG_PRINT("   [Departure_Board] Parser initialisation: No platform set");
        } else {
            selected_platform = board_config.get("platform");
            parser.setPlatform(selected_platform);
            DEBUG_PRINT("   [Departure_Board] Parser initialisation: Platform set to " << selected_platform);
        }
        
        parser.createFromJSON(departures, refdata, api_data_version);
        DEBUG_PRINT("[Departure_Board] Parser initialised with Delay/Cancel data and initial Departure information");

    } catch(const std::exception& e) {
        std::cerr << "[Departure_Board] Error configuring Parser" << e.what() << std::endl;
    }
}

void DepartureBoard::initialiseDisplay(){
    try {
        DEBUG_PRINT("[Departure_Board] Initialising Display");
        
        is_running = false;
        show_platforms = board_config.getBool("ShowPlatforms");
        location = parser.getLocationName();
        if(board_config.getBool("ShowCallingPointETD")) {
            show_calling_point_etd = TrainServiceParser::SHOWETD;
        } else {
            show_calling_point_etd = TrainServiceParser::NOETD;
        }
        
        DEBUG_PRINT("[Departure_Board] Display initialised]");
        
    } catch(const std::exception& e) {
        std::cerr << "[Departure_Board] Error initialing Display" << e.what() << std::endl;
    }
    
}

void DepartureBoard::updateDisplay(){
    
    try {
        DEBUG_PRINT("[Departure_Board] Updating display");
        
        first_row_data.destination.reset();
        first_row_data.coaches.reset();
        second_row_data.calling_points.reset();
        second_row_data.has_calling_points = true;
        second_row_data.service_message.reset();
        third_row_data.second_departure.reset();
        third_row_data.third_departure.reset();
        
        if (departure_1_index != 999) {                                                                     // If there's a first departure, there may be others.
            
            if(show_platforms) {
                first_row_data.destination << "Plat " << parser.getPlatform(departure_1_index) << " ";
            }
            
            if(departure_1.coaches.empty()) {
                first_row_data.coach_info_available = false;
            } else {
                first_row_data.coach_info_available = true;
                first_row_data.coaches = departure_1.coaches;
            }
            
            if(departure_1.isCancelled){
                first_row_data.destination << departure_1.scheduledDepartureTime << " " << departure_1.destination;
                first_row_data.estimated_depature_time = "Cancelled";
                first_row_data.coach_info_available = false;
                second_row_data.has_calling_points = false;
                second_row_data.service_message = departure_1.cancelReason;
            } else {
                
                first_row_data.destination << departure_1.scheduledDepartureTime << " " << departure_1.destination;
                first_row_data.estimated_depature_time = departure_1.estimatedDepartureTime;
                
                if (!departure_1.coaches.empty()) {
                    if (!departure_1.operator_name.empty()) {
                        second_row_data.service_message << "A " << departure_1.operator_name << " service formed of " << departure_1.coaches << " coaches. " << departure_1.delayReason;
                    } else {
                        second_row_data.service_message << "A " << departure_1.coaches << " coach service. ";
                    }
                } else {
                    if (!departure_1.operator_name.empty()) {
                        second_row_data.service_message << "A " << departure_1.operator_name << " service. ";
                    } 
                }
                
                second_row_data.service_message << "  " << parser.getServiceLocation(departure_1_index);
                second_row_data.calling_points << parser.getCallingPoints(departure_1_index, show_calling_point_etd);
            }
            
            DEBUG_PRINT("   [Departure_Board] Service Message: " << second_row_data.service_message);
            DEBUG_PRINT("   [Departure_Board] Has calling points: " << second_row_data.has_calling_points << ". Calling points: " << second_row_data.calling_points);
            
            if(departure_2_index != 999) {
                third_row_data.second_departure << "2nd: ";
                if(show_platforms) {
                    third_row_data.second_departure << "Plat " << parser.getPlatform(departure_2_index) << " ";
                }
                third_row_data.second_departure << departure_2.scheduledDepartureTime << " " << departure_2.destination;
                third_row_data.second_departure_estimated_departure_time = departure_2.estimatedDepartureTime;
            }
            
            if(departure_3_index != 999) {
                third_row_data.third_departure << "3rd: ";
                if(show_platforms) {
                    third_row_data.third_departure << "Plat " << parser.getPlatform(departure_3_index) << " ";
                }
                third_row_data.third_departure << departure_3.scheduledDepartureTime << " " << departure_3.destination;
                third_row_data.third_departure_estimated_departure_time = departure_3.estimatedDepartureTime;
            } else {
                if(departure_2_index != 999) {
                    third_row_data.third_departure = third_row_data.second_departure;
                    third_row_data.third_departure_estimated_departure_time = third_row_data.second_departure_estimated_departure_time;
                }
            }
        } else {
            first_row_data.destination << "No More Services";
            first_row_data.coach_info_available = false;
        }
        DEBUG_PRINT("  [Departure_Board] Pushing data to the Matrix Driver");
        fourth_row_data.message = parser.getNrccMessages();
        fourth_row_data.location = location;
        
        first_row_data.api_version = api_data_version;
        second_row_data.api_version = api_data_version;
        third_row_data.api_version = api_data_version;
        fourth_row_data.api_version = api_data_version;
        
        matrix.updateFirstRow(first_row_data);
        matrix.updateSecondRow(second_row_data);
        matrix.updateThirdRow(third_row_data);
        matrix.updateFourthRow(fourth_row_data);
        
        /*matrix.debugPrintFirstRowData();
        matrix.debugPrintSecondRowData();
        matrix.debugPrintThirdRowData();
        matrix.debugPrintFourthRowData();*/
        
        DEBUG_PRINT("[Departure_Board] Pushing data to the Matrix Driver complete");
    } catch(const std::exception& e) {
        std::cerr << "[Departure_Board] Error updating Display" << e.what() << std::endl;
    }
}

void DepartureBoard::refreshData() {
    
    DEBUG_PRINT("[Departure_board] Initialising parser cache refresh");
    
    parser.updateCache(departures, api_data_version);
    
    DEBUG_PRINT("   [Departure_board] Cache refresh: getting next 3 departure indices");
    departure_1_index = parser.getFirstDeparture();
    departure_2_index = parser.getSecondDeparture();
    departure_3_index = parser.getThirdDeparture();
    
    DEBUG_PRINT("   [Departure_board] Cache refresh: getting next 3 departure BasicServiceInfo");
    departure_1 = parser.getBasicServiceInfo(departure_1_index);
    departure_2 = parser.getBasicServiceInfo(departure_2_index);
    departure_3 = parser.getBasicServiceInfo(departure_3_index);
    
    DEBUG_PRINT("   [Departure_board] Cache refresh: getting location of 1st Service");
    departure_1_location = parser.getServiceLocation(departure_1_index);
    DEBUG_PRINT("[Departure_board] Parser Cache updated and key data extracted");
}

void DepartureBoard::getDataFromAPI(){
    DEBUG_PRINT("[Departure_board] Initialising update of data from the Staff departure API]");
    DEBUG_PRINT("   [Departure_board] Attempting to start background API refresh.");
    DEBUG_PRINT("   [Departure_board] Current Data version: " << api_data_version);
    
    if (data_refresh_pending.load()) {                                                                              // If a refresh is already pending, don't start another one
        return;
    }
    
    data_refresh_pending.store(true);                                                                               // Set flag to indicate a refresh is pending
    
    if (api_thread.joinable()) {                                                                                    // Start a new thread for the API call
        api_thread.join();                                                                                          // Clean up previous thread if any
    }
    
    api_thread = std::thread([this]() {
        try {
            if (shutdown_requested.load()) return;                                                                  // Early exit
            
            raw_api_data = api_client.fetchDepartures(location_code);                                               // Fetch data from API
            
            if (shutdown_requested.load()) return;                                                                  // Check again after network call
            
            {                                                                                                       // Store the data safely
                std::lock_guard<std::mutex> lock(api_data_mutex);
                new_api_data = raw_api_data;
            }
            
            data_refresh_completed.store(true);                                                                     // Set flags to indicate completion
            data_refresh_pending.store(false);
            
            DEBUG_PRINT("   [Departure_board] Background API refresh completed. Data version from API client: " << api_client.getCurrentAPIVersion());
            DEBUG_PRINT("[Departure_board] Completed retrieval of data from the Staff departure API");
        } catch (const std::exception& e) {
            std::cerr << "[Departure_board] Error refreshing data in background thread: " << e.what() << std::endl;
            data_refresh_pending.store(false);
        }
    });
    
    api_thread.detach();                                                                                            // Detach the thread so it runs independently
}

void DepartureBoard::run() {
    DEBUG_PRINT("[Departure_board] Attemping to Start the Departure board");
    is_running = true;
    refreshData();
    updateDisplay();
    DEBUG_PRINT("   [Departure_board] Departure board Running!");
    
    while (is_running) {
        try {
            auto now = std::chrono::steady_clock::now();
            
            if (!data_refresh_pending.load() && now - last_data_refresh >= std::chrono::seconds(data_refresh_interval)) {       // Check if it's time to start a new data refresh
                getDataFromAPI();
                last_data_refresh = std::chrono::steady_clock::now();
            }
            
            if (data_refresh_completed.load()) {                                                                                // Apply the new data to the parser and update display
                DEBUG_PRINT("   [Departure_board] API refresh complete - attempting cache/display refresh ");
                {
                    std::lock_guard<std::mutex> lock(api_data_mutex);
                    departures = new_api_data;
                    api_data_version = api_client.getCurrentAPIVersion();
                }
                refreshData();
                updateDisplay();
                
                // Reset the completion flag
                data_refresh_completed.store(false);
                 
                DEBUG_PRINT("   [Departure_board] Cache refresh and display update completed. New Data verion: " << api_data_version);
            }
            
            matrix.render();
            
            
        } catch (const std::exception& e) {
            std::cerr << "[Departure_board] Display error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(data_refresh_interval));
        }
    }
    if (api_thread.joinable()) {                                                                                             // Clean up the API thread if we're shutting down
        api_thread.join();
    }
    DEBUG_PRINT("[Departure_board] Terminated Running Departure board");
}

void DepartureBoard::stop() {
    DEBUG_PRINT("[Departure_board] Stopping the departure board");
    /*is_running = false;
     
     if (api_thread.joinable()) {                                                                                            // Clean up the API thread if it's still running
     api_thread.join();
     } */
    
    is_running = false;                                                                                                     // Signal all components to stop
    shutdown_requested.store(true);
    data_refresh_pending.store(false);
    
    if (api_thread.joinable()) {                                                                                            // Wait for API thread with timeout
        auto future = std::async(std::launch::async, [this]() {
            api_thread.join();
        });
        
        if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            DEBUG_PRINT("[Departure_board] API thread didn't respond to shutdown, detaching...");
            api_thread.detach();
        } else {
            DEBUG_PRINT("[Departure_board] API thread shut down gracefully");
        }
    }
    
}



