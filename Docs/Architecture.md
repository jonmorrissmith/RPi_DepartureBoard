# Software Architecture Summary

I'll expand on this in due course, but wanted to provide a high-level view of what's going on here!

# Dependencies

* [Hzeller RPi HUB75 driver library](https://github.com/hzeller/rpi-rgb-led-matrix)
* [JSON for Modern C++](https://json.nlohmann.me/)
* [Rail Data Marketplace](https://raildata.org.uk/)
* Curl

# High Level Architecture

API calls to Rail Data Marketplace pull JSON data based on parameters in the configuration file.

This is a large dataset - the parser creates a cached index to ease navigation.

The data-extracts required for display are handled by the display-manager. 

Parsed data is passed to the matrix driver by the display-manager after each API refresh.

The matrix-driver caches content and fonts to minimises the number of display-refreshes.

The Python UI allows editing of the configuration file and (re)starts of the main departure board process.

# Configuration

*config.h|cpp*

Configuration parameters and their default values are defined in the header file.

Parameters are read from the config file and over-ride the default values. Settings are cached and functions are available to read string/int/bool with an optional default value.

A struct is made available for matrix-configuration to avoid repeated cache-reads when setting up the display.

# API calls

*API_client.h|cpp*

Parameters are passed to the client in a struct containing the API keys and debug flags/directories.

Two API calls are available - both return JSON data into a string.
* Delay/Cancellation reasons - reference data to translate reason-codes to text
* Staff Departure Board data - parameter is the CRS station code for the departure board

Departure-board data is versioned - version can be queried.

API calls for Staff Departure Board have a timestamp parameter - current time is used

# Parser

*train_service_parser.h|cpp*

The most critical element - navigating and caching a large and complex data-set.

The other option is to use Darwin... I'm not convinced the extra effort would provide significant benefit for a project of this type.

I've tried to optimise, cache and separate static from dynamic data as much as possible to minimise the cycles required to refresh the data.

The parser uses the API data-version for cache/refresh optimisation and is thread-safe. A 'null service' is identified with an ID of 999.

## JSON structure

Documentation is available on the Rail Data Marketplace - copy from July 2025: [LDBSVWS Documentation](https://github.com/jonmorrissmith/RPi_DepartureBoard/blob/Documentation/Docs/LDBSVWS_Documentation.pdf)

## External Parser Data Structures 

The payload contains far more information that is required for a departure board.  The following data-structures are defined based on the frequency of use and application.

### Basic Service Information

Often used and key to the departure board.
```
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
```
### Additional Service Information

Infrequently used data
```
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
```

### Location Information

Location data - can be a calling point or the station the data is created for
```
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
```

### Calling Point Information

Used for previous (current location) and subsequent (stopping points) location information
```
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
```

## How the Parser Works ###

### Key data elements ###

* The index of Services in the JSON (Service Index) - this is the order in which they appear in the JSON
* An unordered-map of the Service Index and the TrainID (the unique identifier for each Service)
* The ServiceSequence structure which holds the essential information needed to order Services
```
    struct ServiceSequence {
        bool std_specified;       // Is there an STD?
        time_t std;               // Scheduled Departure Time
        bool etd_specified;       // Is there an ETD?
        time_t etd;               // Estimated Departure Time
        time_t departure_time;    // Departure time - std or etd (if specified)
        std::string platform;     // Platform
        std::string trainid;      // Train Identifier
        uint64_t api_version;     // Version of the API data used
    };
```

### Initialisation ###

* Cancel/Delay reson-codes loaded
* Set the maximum number of services in the JSON
* Set the number of services to be cached
* Set the platform if specified (you need a process-restart to change this)

### Meta Data ###

The number of Services in the JSON and any Network Rail messages for the location.

This is refreshed after each API data-load.

### Cache Prefetch ###
Triggered when new JSON data is available from the API.

New Services are added to the map and their ServiceSequence data populated.  The 'departure time' is set to the scheduled departure time or, if present (when a Service is delayed), the estimated departure time.

Cached Basic/Additional static data is retained for existing Services - dynamic data such as estimated departure times, calling points, delays and so forth are flagged for refresh.

### Cache Hydration ###
Triggered when new JSON data is available from the API.

The next three departures are calculated - either across all platforms or for a specific platform if specified.

Noted that terminus stations are a special-case where arriving Services are in the JSON but have no subsequent destination.

Dynamic Basic Service data is updated for the three Services (Static data only if not aleady cached).

A Service is flagged as 'Delayed' if an Estimated Time of Departure is flagged as available and it's later than the Scheduled Time.  If an Estimated Time of Departure is flagged and one isn't set then the word 'Delayed' is used instead of the estimated time.

Noted that lazy-loading is used for all Additional and Basic Service data.

### Calling Points ###
Lazy-loaded - data is set to 'stale' after each API data-load

The 'ExtractCallingPoints' method finds all subsequent calling points and stores them along with the Estimated Time of Departure for each.  The method ignores stations/locations in the JSON which a Service will pass and not stop at.

Noted that both the list of calling points and the list of calling points with the Estimated Time of Departure are stored.

### Service Location ###
Lazy-loaded - data is set to 'stale' after each API data-load

The 'ExtractCallingPoints' method finds all previous calling points and walks across them to find the last calling point with an 'Actual' time of departure.

The Service is located between that calling point and the next calling point in the JSON.

# Matrix Driver #
*matrix_driver.h|cpp*

To optimise performance the matrix driver only updates areas where content has changed - a clear of the whole display is a special-case.

Data is passed to the Matrix Driver in these data-structures:
```
    struct first_row_data {                                                         
        DisplayText platform;
        DisplayText destination;
        DisplayText scheduled_departure_time;
        DisplayText estimated_depature_time;
        bool coach_info_available;
        DisplayText coaches;
        int64_t api_version;
    };
    
    struct second_row_data {                                                        
        DisplayText calling_points;
        DisplayText service_message;
        bool has_calling_points;
        int64_t api_version;
    };
    
    struct third_row_data {                                                      
        DisplayText second_departure;
        DisplayText second_departure_estimated_departure_time;
        DisplayText third_departure;
        DisplayText third_departure_estimated_departure_time;
        int64_t api_version;
    };
    
    struct fourth_row_data {                                                       
        DisplayText location;
        DisplayText message;
        bool has_message;
        int64_t api_version;
    };
```

A display refresh requires two passes to complete - a Refresh_State structure is used to count the passes.

Note that the main loop for rendering the display is outside of the Matrix Driver.

### Initialisation

The driver initialises the matrix, handles the display of the four rows of data and displays the current time.

Matrix configuration is provided in a structure passed from the Config class.

The font is cached on initialisation and the width of each character held in a map.

Each row is configured and cleared

### First Row

Displays the platform, scheduled time, estimated time, destiation and configuration of the first departure.

There is a toggle to alternate between the number of coaches and the status (On Time, ETD, Delayed, Cancelled)

## Second Row

Scrolls the subsequent calling points, service operator and location of the first departure.

Scroll speed is set in the configuration file.

## Third Row

Toggles between the second and third departure, showing platform, scheduled time, estimated time and destiation

## Forth Row

Toggles between the location of the departure board and a scroll of Network Rail messages (if available).

Scroll speed is set in the configuration file.

The current time is displayed in the bottom right

# Departure Board Driver #
*departure_board.h|cpp*

This is the glue which pulls together the components above.

The driver initialises the API, Parser and the Matrix Driver and drives the main-loop.

Data is refreshed periodically via the API client, which is done as a forked process to avoid a performance hit - the amount of data can lead to calls taken multiple seconds to complete.

The data is fed into the parser, results pulled and sent to the Matrix Driver for display.  What is sent depends on the configuration set.












