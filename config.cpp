// Train Display - Configuration handler
// Jon Morris Smith - Feb 2025
// Version 1.0
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board
//
// IMPORTANT - set defaults in config.h
//

#include "config.h"

Config::Config() {
    // Start with defaults
    settings = defaults;
    DEBUG_PRINT("[Config: Configuration initialized with default values]");
}

void Config::loadFromFile(const std::string& filename) {
    DEBUG_PRINT("Config: Loading configuration from " << filename);

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Config: Could not open config file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (!key.empty()) {
            // Set the value, even if it's empty - we'll handle fallbacks in get()
            settings[key] = value;
            DEBUG_PRINT("Config: Loaded config: " << key << " = " << (value.empty() ? "<empty>" : value));
        }
    }
    
    // Clear cache after loading new configuration
    clearCache();
    
    DEBUG_PRINT("[Config: Configuration loaded successfully] from " << filename);
}

std::string Config::get(const std::string& key) const {
    // First check the cache
    auto cache_it = value_cache.find(key);
    if (cache_it != value_cache.end()) {
        return cache_it->second;
    }
    
    std::string result;
    
    // Check if it exists in settings from config.txt
    auto it = settings.find(key);
    if (it != settings.end() && !it->second.empty()) {
        result = it->second;
    } else {
        // Fall back to defaults
        auto default_it = defaults.find(key);
        if (default_it != defaults.end()) {
            if (!default_it->second.empty()) {
                result = default_it->second;
            } else {
                // Both settings and defaults have empty values
                if (key == "platform" || key == "led-pixel-mapper" || key == "led-panel-type") {
                    // These keys are allowed to be empty
                    result = "";
                } else {
                    DEBUG_PRINT("[Config Warning]: Configuration key '" << key << "' has empty value in both config file and defaults");
                    result = "";
                }
            }
        } else {
            throw std::runtime_error("[Config] Configuration key not found: " + key);
        }
    }
    
    // Cache the result
    value_cache[key] = result;
    return result;
}

std::string Config::getUpper(const std::string& key) const {
    return toUpper(get(key));
}

std::string Config::getStringWithDefault(const std::string& key, const std::string& defaultValue) const {
    try {
        std::string value = get(key);
        if (value.empty()) {
            DEBUG_PRINT("[Config Warning] Using provided default for empty key " << key);
            return defaultValue;
        }
        return value;
    } catch (const std::exception& e) {
        DEBUG_PRINT("[Config Warning] " << e.what() << " - Using provided default");
        return defaultValue;
    }
}

int Config::getInt(const std::string& key) const {
    std::string value = get(key);
    if (value.empty()) {
        throw std::runtime_error("[Config] Cannot convert empty string to integer for key: " + key);
    }
    
    try {
        return std::stoi(value);
    } catch (const std::exception& e) {
        throw std::runtime_error("[Config] Invalid integer value for key '" + key + "': " + value);
    }
}

int Config::getIntWithDefault(const std::string& key, int defaultValue) const {
    try {
        return getInt(key);
    } catch (const std::exception& e) {
        DEBUG_PRINT("[Config Warning] " << e.what() << " - Using default value " << defaultValue);
        return defaultValue;
    }
}

bool Config::getBool(const std::string& key) const {
    std::string value = toLower(get(key));
    // Check for empty string
    if (value.empty()) {
        throw std::runtime_error("[Config Warning] Empty boolean value for key: " + key);
    }
    
    // Check for various true values
    if (value == "true" || value == "yes" || value == "1" || value == "on") {
        return true;
    }
    
    // Check for various false values
    if (value == "false" || value == "no" || value == "0" || value == "off") {
        return false;
    }
    
    // If it's neither clearly true nor false, throw an exception
    throw std::runtime_error("[Config] Invalid boolean value for key '" + key + "': " + value);
}

bool Config::getBoolWithDefault(const std::string& key, bool defaultValue) const {
    try {
        return getBool(key);
    } catch (const std::exception& e) {
        DEBUG_PRINT("[Config Warning]  " << e.what() << " - Using default value " << (defaultValue ? "true" : "false"));
        return defaultValue;
    }
}

void Config::set(const std::string& key, const std::string& value) {
    settings[key] = value;
    // Clear cache entry if it exists
    value_cache.erase(key);
    DEBUG_PRINT("[Config] Set config: " << key << " = " << value);
}

void Config::clearCache() const {
    value_cache.clear();
    DEBUG_PRINT("[Config] Configuration cache cleared");
}

bool Config::hasKey(const std::string& key) const {
    return settings.find(key) != settings.end() || defaults.find(key) != defaults.end();
}

Config::matrix_options Config::getMatrixOptions() const{
    Config::matrix_options params;
    
    // Basic matrix configuration
    params.matrixrows = getIntWithDefault("matrixrows", 64);
    params.matrixcols = getIntWithDefault("matrixcols", 128);
    params.matrixchain_length = getIntWithDefault("matrixchain_length", 3);
    params.matrixparallel = getIntWithDefault("matrixparallel", 1);
        
    // String parameters - now stored as std::string
    params.matrixhardware_mapping = get("matrixhardware_mapping");
    params.led_multiplexing = getIntWithDefault("led-multiplexing", 0);
    params.led_pixel_mapper = getStringWithDefault("led-pixel-mapper", "");
        
    // Display quality settings
    params.led_pwm_bits = getIntWithDefault("led-pwm-bits", 11);
    params.led_brightness = getIntWithDefault("led-brightness", 100);
    params.led_scan_mode = getIntWithDefault("led-scan-mode", 0);
    params.led_row_addr_type = getIntWithDefault("led-row-addr-type", 0);
        
    // Display behavior settings
    params.led_show_refresh = getBoolWithDefault("led-show-refresh", false);
    params.led_limit_refresh = getIntWithDefault("led-limit-refresh", 0);
        
    // Color settings
    params.led_inverse = getBoolWithDefault("led-inverse", false);
    params.led_rgb_sequence = getStringWithDefault("led-rgb-sequence", "RGB");
        
    // Advanced PWM settings
    params.led_pwm_lsb_nanoseconds = getIntWithDefault("led-pwm-lsb-nanoseconds", 130);
    params.led_pwm_dither_bits = getIntWithDefault("led-pwm-dither-bits", 0);
    params.led_no_hardware_pulse = getBoolWithDefault("led-no-hardware-pulse", false);
    params.led_panel_type = getStringWithDefault("led-panel-type", "");
        
    // Runtime options
    params.gpio_slowdown = getIntWithDefault("gpio_slowdown", 1);
    params.led_daemon = getBoolWithDefault("led-daemon", false);
    
    return params;
}

void Config::debugPrintConfig() const {
    DEBUG_PRINT("[Config] Current configuration:");
    for (const auto& pair : settings) {
        DEBUG_PRINT("  " << pair.first << " = " << (pair.second.empty() ? "<empty>" : pair.second));
    }
}
