//
//  departure_board.h
//  Departure_Board
//
//  Created by Jon Morris-Smith on 23/06/2025.
//

#ifndef DEPARTURE_BOARD_H
#define DEPARTURE_BOARD_H

#include <memory.h>
#include <future>
#include <nlohmann/json.hpp>
#include "API_client.h"
#include "config.h"
#include "matrix_driver.h"
#include "train_service_parser.h"

using json = nlohmann::json;

// Forward declaration for the debug printing macro
extern bool debug_mode;
#define DEBUG_PRINT(x) if(debug_mode) { std::cerr << x << std::endl; }


class DepartureBoard {
public:
    
    explicit DepartureBoard(const Config& cfg);                                                                     // Constructor that takes only a Config reference. Everything else is initialised internally.
    DepartureBoard(const Config& cfg, const std::string& staff_api_key, const std::string& reason_code_api_key);    // Alternative constructor if you need to pass API keys separately
    
    ~DepartureBoard();
    
    // Main operations
    void run();                                                                                                     // run
    void stop();                                                                                                    // stop
    
private:
    
    const Config& board_config;                                                                                     // Configuration (stored as const reference)
    
    // Key components
    APIClient::APIConfig api_config;
    APIClient api_client;
    TrainServiceParser parser;
    MatrixDriver matrix;
    
    // Internal state
    bool is_running;
    
    // Raw Data
    std::string location_code;                                                                                           // Location of the departure board
    std::string refdata;
    std::string departures;
    std::atomic<uint64_t> api_data_version;                                                                         // Version control of api data
    size_t data_refresh_interval;
    std::chrono::steady_clock::time_point last_data_refresh;
    
    
    // Parsed Data
    TrainServiceParser::BasicServiceInfo departure_1;
    TrainServiceParser::BasicServiceInfo departure_2;
    TrainServiceParser::BasicServiceInfo departure_3;
    TrainServiceParser::AdditionalServiceInfo additional_departure_info;
    size_t departure_1_index;
    size_t departure_2_index;
    size_t departure_3_index;
    std::string departure_1_location;
    
    // Display options
    bool show_platforms;
    TrainServiceParser::CallingPointETD  show_calling_point_etd;
    std::string selected_platform;
    
    // Display data;
    DisplayText location;                                                                                           // Location of the departure board
    MatrixDriver::first_row_data first_row_data;
    MatrixDriver::second_row_data second_row_data;
    MatrixDriver::third_row_data third_row_data;
    MatrixDriver::fourth_row_data fourth_row_data;
    
    
    // API background-refresh configuration
    std::thread api_thread;                        // Thread for API calls
    std::atomic<bool> shutdown_requested{false};   // Shutdown Request
    std::atomic<bool> data_refresh_pending;        // Flag to indicate data refresh is in progress
    std::atomic<bool> data_refresh_completed;      // Flag to indicate new data is available
    std::mutex api_data_mutex;                     // Mutex for thread-safe access to API data
    std::string new_api_data;                      // Buffer for new API data
    std::string raw_api_data;
            
    
    // Helper methods
    
    // Initialisers
    void initialise();                                                                                              // Initialise
    void initialiseAPI();
    void initialiseParser();
    void initialiseDisplay();
    
    // Update methods
    void updateDisplay();
    void refreshData();
    void getDataFromAPI();
};

#endif


