//
//  departureboard.cpp
//  Departure_Board
//
//  Created by Jon Morris-Smith on 23/06/2025.
//

#include <signal.h>
#include "departureboard.h"

bool debug_mode = false;                                                                // Global debug flag

DepartureBoard* display_ptr = nullptr;                                                  // Pointer to departure board for signal handling

void signalHandler(int signum) {                                                        // Signal handler for graceful shutdown
    std::cout << "\nReceived signal " << signum << ". Shutting down..." << std::endl;
    if (display_ptr) {
        display_ptr->stop();
    }
}

// Display usage information
void showUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS] [LOCATION]\n"
              << "Options:\n"
              << "  -d, --debug               Enable debug output\n"
              << "  -f, --config FILE         Specify configuration file\n"
              << "  -h, --help                Show this help message\n"
              << "\nExample:\n"
              << "  " << programName << " KGX\n"
              << "    Shows trains from London Kings Cross\n";
}

// Helper function to process command line arguments
void processCommandLineArgs(int argc, char* argv[], Config& config) {
    std::string config_file;
    std::string location;
    std::vector<std::string> station_args;
    
    // First pass - handle config file and debug mode
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--debug") {
            debug_mode = true;
        } else if (arg == "-h" || arg == "--help") {
            showUsage(argv[0]);
            exit(0);
        } else if (arg == "-f" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: Config file path not provided after " << arg << std::endl;
                showUsage(argv[0]);
                exit(1);
            }
        } else if (arg.substr(0, 9) == "--config=") {
            config_file = arg.substr(9);
        } else if (arg[0] != '-') {
            // Store non-option arguments for second pass
            station_args.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            showUsage(argv[0]);
            exit(1);
        }
    }
    
    // Load configuration file
    if (config_file.empty()) {
        config_file = "./config.txt";
    }
    try {
        config.loadFromFile(config_file);
    } catch (const std::exception& e) {
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        exit(1);
    }
    
    // Process location arguments
    if (station_args.size() > 0) {
        config.set("location", station_args[0]);
        DEBUG_PRINT("Overriding 'location' with command line value: " << station_args[0]);
    }
    
    // If debug_mode is set, update the configuration
    if(debug_mode){
        config.set("debug_mode", "true");
        DEBUG_PRINT("Overriding 'debug_mode' with command line value: " << debug_mode);
    }
    
}

int main(int argc, char* argv[]){
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        Config config;
        
        processCommandLineArgs(argc, argv, config);
        
        //config.loadFromFile("/home/display/Matrix_Driver/config.txt");
        
        DepartureBoard departure_board(config);
        display_ptr = &departure_board;                                                     // Set global pointer for signal handler
        
        std::cout << "Departureboard running. Press Ctrl+C to exit." << std::endl;
        departure_board.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "Train display shut down successfully." << std::endl;
    return 0;
}
