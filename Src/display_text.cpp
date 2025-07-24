// Train Display - an RGB matrix departure board for the Raspberry Pi
//
// Display text implementation file
//
// Jon Morris Smith - April 2025
// Version 1.0
// Instructions, fixes and issues at https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board
//

#include "display_text.h"
#include <algorithm>
#include <utility>

//-------------------------------------------------------------------------
// LRU implementation
//-------------------------------------------------------------------------

// Simple but effective hash for font cache keys
size_t FixedLRUCache::hash(const std::string& key) const {
    
    size_t hash = 5381;
    for (char c : key) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash % cache_.size();
}

// Find least recently used entry
size_t FixedLRUCache::findLRU() const {
    size_t lru_index = 0;
    uint64_t oldest_time = UINT64_MAX;
    
    for (size_t i = 0; i < cache_.size(); ++i) {
        if (!cache_[i].valid) {
            return i;  // Found empty slot
        }
        if (cache_[i].timestamp < oldest_time) {
            oldest_time = cache_[i].timestamp;
            lru_index = i;
        }
    }
    return lru_index;
}

// Get cached value, returns -1 if not found
int FixedLRUCache::get(const std::string& key) {
    size_t index = hash(key);
    
    // Linear probing for collision resolution
    for (size_t i = 0; i < cache_.size(); ++i) {
        size_t probe_index = (index + i) % cache_.size();
        Entry& entry = cache_[probe_index];
        
        if (!entry.valid) {
            break;  // Not found
        }
        
        if (entry.key == key) {
            // Found! Update timestamp and return
            entry.timestamp = ++counter_;
            return entry.value;
        }
    }
    
    return -1;  // Not found
}

// Cache a new value
void FixedLRUCache::put(const std::string& key, int value) {
    size_t index = hash(key);
    
    // Try to find existing entry or empty slot
    for (size_t i = 0; i < cache_.size(); ++i) {
        size_t probe_index = (index + i) % cache_.size();
        Entry& entry = cache_[probe_index];
        
        if (!entry.valid || entry.key == key) {
            // Found empty slot or updating existing
            entry.key = key;
            entry.value = value;
            entry.timestamp = ++counter_;
            entry.valid = true;
            return;
        }
    }
    
    // Cache full, evict LRU entry
    size_t lru_index = findLRU();
    Entry& entry = cache_[lru_index];
    entry.key = key;
    entry.value = value;
    entry.timestamp = ++counter_;
    entry.valid = true;
}

// Return Stats
FixedLRUCache::Stats FixedLRUCache::getStats() const {
    Stats stats;
    stats.total_entries = cache_.size();
    
    for (const auto& entry : cache_) {
        if (entry.valid) {
            stats.used_entries++;
        }
    }
    
    stats.fill_ratio = (double)stats.used_entries / stats.total_entries;
    return stats;
}

// Clear
void FixedLRUCache::clear() {
    for (auto& entry : cache_) {
        entry.valid = false;
    }
    counter_ = 0;
}

//-------------------------------------------------------------------------
// FontCache implementation
//-------------------------------------------------------------------------

void FontCache::setFont(const Font& font) {
    try {
        font_ptr = &font;
        // Cache character widths
        for (int i = 0; i < 256; i++) {
            char_widths[i] = font.CharacterWidth(static_cast<char>(i));
        }
        // Cache font baseline
        baseline = font.baseline();
        height = font.height();
        font_loaded = true;
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in FontCache::setFont: " << e.what());
        throw std::runtime_error("Error initializing font cache: " + std::string(e.what()));
    }
}

int FontCache::getCharWidth(char c) const {
    try {
        unsigned char idx = static_cast<unsigned char>(c);
        return char_widths[idx];
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in FontCache::getCharWidth: " << e.what());
        throw std::out_of_range("Character out of range in FontCache::getCharWidth");
    }
}

int FontCache::getTextWidth(const std::string& text) const {
    try {
        /*
         auto it = string_width_cache.find(text);
         if (it != string_width_cache.end()) {
         return it->second;
         }
         
         int width = 0;
         for (const char& c : text) {
         width += getCharWidth(c);
         }
         
         if (string_width_cache.size() >= MAX_CACHE_SIZE) {              // Limit cache size
         string_width_cache.clear();                                 // Simple eviction strategy
         }
         
         string_width_cache[text] = width;
         return width;
         */
        
        int cached_width = string_width_cache_.get(text);               // Try cache first
        if (cached_width != -1) {
            return cached_width;  // Cache hit!
        }
        
        int width = 0;                                                  // Cache miss - calculate width
        for (const char& c : text) {
            width += getCharWidth(c);
        }
        
        string_width_cache_.put(text, width);                           // Cache the result
        return width;
        
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in FontCache::getTextWidth: " << e.what());
        throw std::runtime_error("Error calculating text width: " + std::string(e.what()));
    }
}

int FontCache::getBaseline(){
    return baseline;
}

int FontCache::getheight() {
    return height;
}

bool FontCache::isloaded(){
    return font_loaded;
}

void FontCache::printCacheStats() const {
    auto stats = string_width_cache_.getStats();
    DEBUG_PRINT("Font cache: " << stats.used_entries << "/"
               << stats.total_entries << " entries ("
               << (int)(stats.fill_ratio * 100) << "% full)");
}


//-------------------------------------------------------------------------
// DisplayText implementation
//-------------------------------------------------------------------------

DisplayText::DisplayText(const std::string& t, int w, int x, int y, uint64_t v)
    : text(t), width(w), x_position(x), y_position(y), data_version(v) {}

void DisplayText::setTextAndWidth(const std::string& newText, const FontCache& fontsizes) {
    try {
        if (text == newText) return;
        
        buffer.clear();
        buffer.reserve(newText.size() + 50);  // Reserve extra space
        buffer = newText;
        text = buffer;
        width = fontsizes.getTextWidth(text);
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in DisplayText::setTextAndWidth: " << e.what());
        throw std::runtime_error("Error setting text and width: " + std::string(e.what()));
    }
}

void DisplayText::setWidth(const FontCache& fontsizes) {
    try {
        width = fontsizes.getTextWidth(text);
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in DisplayText::setWidth: " << e.what());
        throw std::runtime_error("Error setting width: " + std::string(e.what()));
    }
}

bool DisplayText::empty() {
    return text.empty();
}

DisplayText& DisplayText::operator=(const std::string& str) {
    try {
        text = str;
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in DisplayText::operator=: " << e.what());
        throw std::runtime_error("Error assigning string to DisplayText: " + std::string(e.what()));
    }
    return *this;
}

DisplayText& DisplayText::operator=(int pos) {
    x_position = pos;
    return *this;
}

DisplayText& DisplayText::operator<<(const std::string& str) {
    try {
        // More efficient string concatenation
        if (text.empty()) {
            text = str;  // Direct assignment for first string
        } else {
            // Reserve space to avoid multiple reallocations
            text.reserve(text.size() + str.size());
            text.append(str);  // More efficient than concatenation operator
        }
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in DisplayText::operator<<: " << e.what());
        throw std::runtime_error("Error in string concatenation: " + std::string(e.what()));
    }
    return *this;
}

DisplayText& DisplayText::operator<<(const char* str) {
    try {
        return *this << std::string(str);  // Reuse string version
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in DisplayText::operator<<(const char*): " << e.what());
        throw std::runtime_error("Error in string literal concatenation: " + std::string(e.what()));
    }
}

DisplayText& DisplayText::operator<<(int value) {
    try {
        return *this << std::to_string(value);  // Convert to string and reuse
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in DisplayText::operator<<(int): " << e.what());
        throw std::runtime_error("Error in integer concatenation: " + std::string(e.what()));
    }
}

DisplayText::operator int() const {
    return x_position;
}

int DisplayText::getXPosition() const {
    return x_position;
}

DisplayText& DisplayText::operator++() {
    ++x_position;
    return *this;
}

DisplayText DisplayText::operator++(int) {
    DisplayText old = *this;
    ++(*this);
    return old;
}

DisplayText& DisplayText::operator--() {
    --x_position;
    return *this;
}

DisplayText DisplayText::operator--(int) {
    DisplayText old = *this;
    --(*this);
    return old;
}

void DisplayText::reset() {
    text.clear();
    width = 0;
    x_position = 0;
    y_position = 0;
    data_version = 0;
}

void DisplayText::dump(const std::string &name) {
    std::cout << "    [Display Text] Name: " << name;
    std::cout << ". Width: " << width;
    std::cout << ", x_position: " << x_position;
    std::cout << ", y_position: " << y_position;
    std::cout << ", data_version: " << data_version << "." << std::endl;
}

void DisplayText::fulldump(const std::string &name) {
    std::cout << "   [Display Text] Name: " << name;
    std::cout << ". text: " << text;
    std::cout << ", Width: " << width;
    std::cout << ", x_position: " << x_position;
    std::cout << ", y_position: " << y_position;
    std::cout << ", data_version: " << data_version << "." << std::endl;
}

bool DisplayText::operator<(const DisplayText& other) const {
    return x_position < other.x_position;
}

bool DisplayText::operator>(const DisplayText& other) const {
    return x_position > other.x_position;
}

bool DisplayText::operator<=(const DisplayText& other) const {
    return x_position <= other.x_position;
}

bool DisplayText::operator>=(const DisplayText& other) const {
    return x_position >= other.x_position;
}

bool DisplayText::operator==(const DisplayText& other) const {
    return x_position == other.x_position;
}

bool DisplayText::operator!=(const DisplayText& other) const {
    return x_position != other.x_position;
}

//-------------------------------------------------------------------------
// Global operator implementations
//-------------------------------------------------------------------------

// Comparison operators between DisplayText and int
bool operator<(const DisplayText& lhs, const int& rhs) {
    return lhs.getXPosition() < rhs;
}

bool operator<(const int& lhs, const DisplayText& rhs) {
    return lhs < rhs.getXPosition();
}

bool operator>(const DisplayText& lhs, const int& rhs) {
    return lhs.getXPosition() > rhs;
}

bool operator>(const int& lhs, const DisplayText& rhs) {
    return lhs > rhs.getXPosition();
}

bool operator<=(const DisplayText& lhs, const int& rhs) {
    return lhs.getXPosition() <= rhs;
}

bool operator<=(const int& lhs, const DisplayText& rhs) {
    return lhs <= rhs.getXPosition();
}

bool operator>=(const DisplayText& lhs, const int& rhs) {
    return lhs.getXPosition() >= rhs;
}

bool operator>=(const int& lhs, const DisplayText& rhs) {
    return lhs >= rhs.getXPosition();
}

bool operator==(const DisplayText& lhs, const int& rhs) {
    return lhs.getXPosition() == rhs;
}

bool operator==(const int& lhs, const DisplayText& rhs) {
    return lhs == rhs.getXPosition();
}

bool operator!=(const DisplayText& lhs, const int& rhs) {
    return lhs.getXPosition() != rhs;
}

bool operator!=(const int& lhs, const DisplayText& rhs) {
    return lhs != rhs.getXPosition();
}

// Output stream operator
std::ostream& operator<<(std::ostream& os, const DisplayText& dt) {
    try {
        os << dt.text;
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in operator<< for DisplayText: " << e.what());
        throw std::runtime_error("Error in stream output: " + std::string(e.what()));
    }
    return os;
}

// Input stream operator
std::istream& operator>>(std::istream& stream, DisplayText& input) {
    try {
        stream >> input.text;
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in operator>> for DisplayText: " << e.what());
        throw std::runtime_error("Error in stream input: " + std::string(e.what()));
    }
    return stream;
}

// Addition operators
DisplayText operator+(const DisplayText& dt, int offset) {
    DisplayText result = dt;
    result.x_position += offset;
    return result;
}

DisplayText operator+(int offset, const DisplayText& dt) {
    return dt + offset;
}

// Subtraction operator
DisplayText operator-(const DisplayText& dt, int offset) {
    DisplayText result = dt;
    result.x_position -= offset;
    return result;
}
