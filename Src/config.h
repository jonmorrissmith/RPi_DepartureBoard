// Train Display - Configuration handler
// Jon Morris Smith - Feb 2025
// Version 1.0
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board
//
// IMPORTANT - settings in config.txt (if used) are used in preference to defaults set here.
//
//
#ifndef CONFIG_H
#define CONFIG_H

#include <led-matrix.h>
#include <map>
#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <utility>

// Forward declarations
class MatrixDriver;

// Forward declaration for the debug printing macro
extern bool debug_mode;
#define DEBUG_PRINT(x) if(debug_mode) { std::cerr << x << std::endl; }

using namespace rgb_matrix;

class Config {
private:
    std::map<std::string, std::string> settings;
    
    const std::map<std::string, std::string> defaults = {
        {"location", ""},
        {"ShowLocation", ""},
        {"StaffAPIKey", ""},
        {"DelayCancelAPIKey", ""},
        {"fontPath", ""},
        {"calling_point_slowdown", "8000"},
        {"nrcc_message_slowdown", "10000"},
        {"refresh_interval_seconds", "60"},
        {"Message_Refresh_interval", "20"},
        {"matrixcols", "128"},
        {"matrixrows", "64"},
        {"matrixchain_length", "3"},
        {"matrixparallel", "1"},
        {"matrixhardware_mapping", ""},         // WARNING - setting adafruit-hat-pwm here is not over-ridden with a blank in the config file (important if you're using multiple adapters).
        {"gpio_slowdown", "4"},
        {"first_line_y", "18"},
        {"second_line_y", "38"},
        {"third_line_y", "58"},
        {"fourth_line_y", "72"},
        {"third_line_refresh_seconds", "10"},
        {"third_line_scroll_in", "true"},
        {"ETD_coach_refresh_seconds", "3"},
        {"ShowCallingPointETD", "Yes"},
        {"ShowMessages", "Yes"},
        {"ShowPlatforms", "Yes"},
        {"platform", ""},
        
        // Debug
        {"debug_mode", "true"},
        {"debug_log_dir", "/tmp"},
        
        // RGB Matrix defaults
        {"led-multiplexing", "0"},
        {"led-pixel-mapper", ""},
        {"led-pwm-bits", "1"},
        {"led-brightness", "100"},
        {"led-scan-mode", "0"},
        {"led-row-addr-type", "0"},
        {"led-show-refresh", "false"},
        {"led-limit-refresh", "0"},
        {"led-inverse", "false"},
        {"led-rgb-sequence", "RGB"},
        {"led-pwm-lsb-nanoseconds", "130"},
        {"led-pwm-dither-bits", "0"},
        {"led-no-hardware-pulse", "false"},
        {"led-panel-type", ""},
        {"led-daemon", "false"},
        {"led-no-drop-privs", "false"},
        {"led-drop-priv-user", "daemon"},
        {"led-drop-priv-group", "daemon"}
    };

    // Cache for frequently requested configuration values
    mutable std::map<std::string, std::string> value_cache;
    
    // Helper to convert a string to lowercase for case-insensitive comparisons
    std::string toLower(std::string str) const {
        std::transform(str.begin(), str.end(), str.begin(),
                      [](unsigned char c){ return std::tolower(c); });
        return str;
    }
    
    // Helper to convert a string to uppercase for case-insensitive comparisons
    std::string toUpper(std::string str) const {
        std::transform(str.begin(), str.end(), str.begin(),
                      [](unsigned char c){ return std::toupper(c); });
        return str;
    }
    
    // Helper to trim whitespace from beginning and end of string
    std::string trim(const std::string& str) const {
        const auto strBegin = str.find_first_not_of(" \t\r\n");
        if (strBegin == std::string::npos)
            return ""; // no content

        const auto strEnd = str.find_last_not_of(" \t\r\n");
        const auto strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }

public:
    Config();

    void loadFromFile(const std::string& filename);
    
    // Define matrix_options struct here to avoid circular dependency
    struct matrix_options {
        int matrixrows;
        int matrixcols;
        int matrixchain_length;
        int matrixparallel;
        std::string  matrixhardware_mapping;
        int led_multiplexing;
        std::string led_pixel_mapper;
        int led_pwm_bits;
        int led_brightness;
        int led_scan_mode;
        int led_row_addr_type;
        bool led_show_refresh;
        int led_limit_refresh;
        bool led_inverse;
        std::string led_rgb_sequence;
        int led_pwm_lsb_nanoseconds;
        int led_pwm_dither_bits;
        bool led_no_hardware_pulse;
        std::string led_panel_type;
        int gpio_slowdown;
        bool led_daemon;
    };
    
    matrix_options getMatrixOptions() const;
    
    // Core configuration access method
    std::string get(const std::string& key) const;        // returns config string
    std::string getUpper(const std::string& key) const;   // returns config string in uppercase
    
    // Type-specific getters with default fallbacks
    std::string getStringWithDefault(const std::string& key, const std::string& defaultValue) const;
    int getInt(const std::string& key) const;
    int getIntWithDefault(const std::string& key, int defaultValue) const;
    bool getBool(const std::string& key) const;
    bool getBoolWithDefault(const std::string& key, bool defaultValue) const;
    
    // Configuration modification
    void set(const std::string& key, const std::string& value);
    
    // Clear cache after modifying configuration
    void clearCache() const;
    
    // Validates if a key exists
    bool hasKey(const std::string& key) const;
    
    // Debug helper
    void debugPrintConfig() const;
};

#endif // CONFIG_H
