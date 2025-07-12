// Train Display - an RGB matrix departure board for the Raspberry Pi
//
// Display text
// A struct to hold:
//   string
//   position
//   width
//   version
//
// FontCache
// A cache for character sizes
//
// Jon Morris Smith - April 2025
// Version 1.0
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board
//
// With thanks to:
// https://github.com/hzeller/rpi-rgb-led-matrix
//

#ifndef DISPLAY_TEXT_H
#define DISPLAY_TEXT_H

#include <iostream>
#include <iomanip>
#include <string>
#include <array>
#include <sstream>
#include <stdexcept>
#include <led-matrix.h>
#include <unordered_map>

using namespace rgb_matrix;

// Forward declaration for the debug printing macro
extern bool debug_mode;
#define DEBUG_PRINT(x) if(debug_mode) { std::cerr << x << std::endl; }

/**
 * fixed-size LRU (Least Recently Used) cache
 * A memory-efficient alternative to  std::unordered_map
 */

class FixedLRUCache {
private:
    struct Entry {
        std::string key;                                // The text string (e.g., "Platform 3")
        int value;                                      // Cached width (e.g., 85 pixels)
        uint64_t timestamp;                             // When it was last accessed
        bool valid = false;                             // Is this entry in use?
    };
    
    std::array<Entry, 256> cache_;                      // Fixed-size array (no malloc!)
    uint64_t counter_ = 0;                              // Monotonic timestamp counter
    
    size_t hash(const std::string& key) const;          // Hash function for string to array index
    size_t findLRU() const;
    
public:
    struct Stats {                                      // Cache statistics
            size_t used_entries = 0;
            size_t total_entries = 0;
            double fill_ratio = 0.0;
        };
        
    Stats getStats() const;                             // Get cache statistics
    
    int get(const std::string& key);                    // Get cached value, returns -1 if not found
    void put(const std::string& key, int value);        // Cache a new value
    void clear();
    
};

/**
 * FontCache - A utility class to cache character widths for faster text width calculations
 * This avoids repeated calls to Font::CharacterWidth which improves performance
 */
class FontCache {
private:
    std::array<int, 256> char_widths;
    const Font* font_ptr = nullptr;
    int baseline;
    int height;
    bool font_loaded = false;
    static const size_t MAX_CACHE_SIZE = 1000;
    //mutable std::unordered_map<std::string, int> string_width_cache;
    mutable FixedLRUCache string_width_cache_;
    
public:
    FontCache() = default;
    
    /**
     * Set the font and cache all character widths
     * @param font The font to use for width calculations
     */
    void setFont(const Font& font);
    
    /**
     * Get the width of a single character
     * @param c The character to get the width for
     * @return The width of the character
     * @throws std::out_of_range if the character is outside the cached range
     */
    int getCharWidth(char c) const;
    
    /**
     * Calculate the width of a complete text string
     * @param text The text to calculate the width for
     * @return The total width of the text
     */
    int getTextWidth(const std::string& text) const;
    
    /**
     * Return the font basline (x size)
     * @return The font baseline
     */
    int getBaseline();
    
    /**
     * Return the font height (y size)
     * @return The font height
     */
    int getheight();
    
    /**
     * Return true if a font is cached
     * @return The font height
     */
    bool isloaded();
    
    /**
     * Print the caching stats
     */
    void printCacheStats() const;
    
};

/**
 * DisplayText - A class to handle text display properties and operations
 * Stores text content, position, width, and provides operators for manipulation
 */
class DisplayText {
private:
    std::string buffer;
    
public:
    std::string text;
    int width = 0;
    int x_position = 0;
    int y_position = 0;
    uint64_t data_version = 0;
    
    // Default constructor
    DisplayText() = default;
    
    /**
     * Constructor with parameters
     * @param t Text content
     * @param w Width of the text (in pixels)
     * @param x X-position for rendering
     * @param y Y-position for rendering
     * @param v Data version for tracking updates
     */
    DisplayText(const std::string& t, int w = 0, int x = 0, int y = 0, uint64_t v = 0);
    
    /**
     * Set text content and calculate its width
     * @param newText The new text content
     * @param fontsizes FontCache to use for width calculation
     */
    void setTextAndWidth(const std::string& newText, const FontCache& fontsizes);
    
    /**
     * Calculate and set the width based on current text content
     * @param fontsizes FontCache to use for width calculation
     */
    void setWidth(const FontCache& fontsizes);
    
    /**
     * Return true if the string is empty
     * @return bool  - result of empty()
     */
    bool empty();
    
    /**
     * Prints the internal parameters to cout
     * @param str  when printing the data
     */
    void dump(const std::string& str);
    
    /**
     * Prints the internal parameters and text to cout
     * @param str  when printing the data
     */
    void fulldump(const std::string& str);
    
    /**
     * Assignment operator for string
     * @param str String to assign to text content
     * @return Reference to this object
     */
    DisplayText& operator=(const std::string& str);
    
    /**
     * Assignment operator for int (to set x_position)
     * @param pos Position value to assign
     * @return Reference to this object
     */
    DisplayText& operator=(int pos);
    
    /**
     * String concatenation operator
     * @param str String to append or assign
     * @return Reference to this object
     */
    DisplayText& operator<<(const std::string& str);
    
    /**
     * String literal concatenation operator
     * @param str String literal to append or assign
     * @return Reference to this object
     */
    DisplayText& operator<<(const char* str);
    
    /**
     * Integer concatenation operator
     * @param value Integer to convert and append
     * @return Reference to this object
     */
    DisplayText& operator<<(int value);
    
    /**
     * Generic concatenation operator for any streamable type
     * @param value Value to convert and append
     * @return Reference to this object
     */
    template<typename T>
    DisplayText& operator<<(const T& value) {
        try {
            std::ostringstream oss;
            oss << value;
            return *this << oss.str();  // Convert to string and reuse
        } catch (const std::exception& e) {
            DEBUG_PRINT("Error in DisplayText::operator<<: " << e.what());
            throw std::runtime_error("Error in DisplayText::operator<<: " + std::string(e.what()));
        }
    }
    
    /**
     * Explicit conversion operator to int - returns x_position
     * Made explicit to prevent accidental conversions
     * @return The x_position value
     */
    explicit operator int() const;
    
    /**
     * Get x position (safer alternative to int conversion)
     * @return The x_position value
     */
    int getXPosition() const;
    
    /**
     * Prefix increment operator
     * @return Reference to this object after incrementing
     */
    DisplayText& operator++();
     
    /**
     * Postfix increment operator
     * @return Copy of this object before incrementing
     */
    DisplayText operator++(int);
     
    /**
     * Prefix decrement operator
     * @return Reference to this object after decrementing
     */
    DisplayText& operator--();
     
    /**
     * Postfix decrement operator
     * @return Copy of this object before decrementing
     */
    DisplayText operator--(int);
    
    /**
     * Reset function to quickly set all properties to defaults
     */
    void reset();
    
    // Comparison operators
    bool operator<(const DisplayText& other) const;
    bool operator>(const DisplayText& other) const;
    bool operator<=(const DisplayText& other) const;
    bool operator>=(const DisplayText& other) const;
    bool operator==(const DisplayText& other) const;
    bool operator!=(const DisplayText& other) const;
    
    // Friend operators
    friend std::ostream& operator<<(std::ostream& os, const DisplayText& dt);
    friend std::istream& operator>>(std::istream& stream, DisplayText& input);
    friend DisplayText operator+(const DisplayText& dt, int offset);
    friend DisplayText operator+(int offset, const DisplayText& dt);
    friend DisplayText operator-(const DisplayText& dt, int offset);
};

// Comparison operators between DisplayText and int
bool operator<(const DisplayText& lhs, const int& rhs);
bool operator<(const int& lhs, const DisplayText& rhs);
bool operator>(const DisplayText& lhs, const int& rhs);
bool operator>(const int& lhs, const DisplayText& rhs);
bool operator<=(const DisplayText& lhs, const int& rhs);
bool operator<=(const int& lhs, const DisplayText& rhs);
bool operator>=(const DisplayText& lhs, const int& rhs);
bool operator>=(const int& lhs, const DisplayText& rhs);
bool operator==(const DisplayText& lhs, const int& rhs);
bool operator==(const int& lhs, const DisplayText& rhs);
bool operator!=(const DisplayText& lhs, const int& rhs);
bool operator!=(const int& lhs, const DisplayText& rhs);

// Output and input stream operators
std::ostream& operator<<(std::ostream& os, const DisplayText& dt);
std::istream& operator>>(std::istream& stream, DisplayText& input);

// Arithmetic operators
DisplayText operator+(const DisplayText& dt, int offset);
DisplayText operator+(int offset, const DisplayText& dt);
DisplayText operator-(const DisplayText& dt, int offset);

#endif // DISPLAY_TEXT_H

