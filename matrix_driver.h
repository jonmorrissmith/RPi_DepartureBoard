//
//  Untitled.h
//  Matrix_Driver
//
//  Created by Jon Morris-Smith on 12/06/2025.
//

#ifndef MATRIX_DRIVER_H
#define MATRIX_DRIVER_H

#include <led-matrix.h>
#include <graphics.h>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <tuple>
#include "display_text.h"
#include "config.h"

using namespace rgb_matrix;

// Forward declaration for the debug printing macro
extern bool debug_mode;
#define DEBUG_PRINT(x) if(debug_mode) { std::cerr << x << std::endl; }

class MatrixDriver {
public:
    
    struct first_row_data {                                                         // First row data-structure
        DisplayText platform;
        DisplayText destination;
        DisplayText scheduled_departure_time;
        DisplayText estimated_depature_time;
        bool coach_info_available;
        DisplayText coaches;
        int64_t api_version;
    };
    
    struct second_row_data {                                                        // Second row data-structure
        DisplayText calling_points;
        int64_t api_version;
    };
    
    struct third_row_data {                                                         // Third row data-structure
        DisplayText second_departure;
        DisplayText second_departure_estimated_departure_time;
        DisplayText third_departure;
        DisplayText third_departure_estimated_departure_time;
        int64_t api_version;
    };
    
    struct fourth_row_data {                                                        // Fourth row data-structure
        DisplayText location;
        DisplayText message;
        bool has_message;
        int64_t api_version;
    };

    
    MatrixDriver(const Config& configuration);
    ~MatrixDriver();
    
    void initialiseMatrix();                                                        // Initialise the RGB Matrix
    void render();                                                                  // Render the data into the matrix display
    void stop();                                                                    // Stop the matrix
    
    void updateFirstRow(const first_row_data& new_first_row);                       // Update the first row content
    void updateSecondRow(const second_row_data& new_second_row);                    // Udate the second row content
    void updateThirdRow(const third_row_data& new_third_row);                       // Update the third row content
    void updateFourthRow(const fourth_row_data& new_forth_row);                     // Update the fourth row content
    
    void debugPrintFirstRowData();                                                  // Data-dumps for debugging
    void debugPrintFirstRowConfig();
    void debugPrintSecondRowData();
    void debugPrintSecondRowConfig();
    void debugPrintThirdRowData();
    void debugPrintThirdRowConfig();
    void debugPrintFourthRowData();
    void debugPrintFourthRowConfig();
    void debugPrintMatrixOptions (const RGBMatrix::Options& options,  const RuntimeOptions& runtime_opt) const;
    
private:
    // Temporary variables
    int extract;
    

    // Display components
    Config::matrix_options matrix_parameters;                                       // Matrix parameters
    RGBMatrix* the_matrix;                                                          // Matrix
    FrameCanvas* canvas;                                                            // Matrix canvas for creating content to display
    Font font;                                                                      // Font
    FontCache font_cache;                                                           // Cache of font sizes
    int font_baseline;                                                              // Baseline size of the font
    int font_height;                                                                // Height of the font
    int matrix_width;                                                               // Width of the matrix
    int matrix_height;                                                              // Height of the matrix
    std::atomic<bool> matrix_configured;                                            // Flag to indicate whether the display is running
    const Config& config;                                                           // Configuration object
    
    // Colors
    Color white;
    Color black;

    // Display state
    enum FirstRowState { ETD, COACHES };                                            // Toggle to show the Estimated Time of Departure or Coaches on the 1st line
    enum ThirdRowState { SECOND_TRAIN, THIRD_TRAIN };                               // Toggle to show 2nd or 3rd train on the 3rd line
    enum FourthRowState { LOCATION, MESSAGE };                                      // Toggle to show the Clock alone or the Clock and message on the 4th line
    enum class RenderState { IDLE, FIRST_PASS, SECOND_PASS };                       // Tracking state for 2-pass display refresh
    
    struct Refresh_state {                                                          // 2-pass display refreh struct.
        RenderState render_state;
        void triggerRefresh() { render_state = RenderState::FIRST_PASS; }
        bool needsRender() const { return render_state != RenderState::IDLE; }
        
        void completePass() {                                                       // State transition: 1st pass -> 2nd pass -> idle
            switch (render_state) {
                case RenderState::FIRST_PASS:
                    render_state = RenderState::SECOND_PASS;
                    break;
                case RenderState::SECOND_PASS:
                    render_state = RenderState::IDLE;
                    break;
                case RenderState::IDLE:
                    break;
            }
        }
    };
    
    Refresh_state whole_display_refresh;                                            // Refresh status for the whole display.
    
    struct first_row_configuration {
        int y_position;                                                             // y position
        FirstRowState ETDCoach_state;                                               // First row state: ETD, COACHES
        Refresh_state refresh_state;                                                // Refresh-control (two passes needed for each refresh)
        int ETD_coach_refresh_seconds;                                              // Interval between ETD|Coach toggles
        std::chrono::steady_clock::time_point last_first_row_toggle;                // First row - time of the last ETD|Coach toggle
        bool configured;                                                            // Has the first row been configured?
    };
    
    struct second_row_configuration {
        int y_position;                                                             // y position
        int message_slowdown_sleep;                                                 // ms sleep to slow down the scroll
        DisplayText calling_at_text;                                                // Contains the text - "Calling at:"
        int space_for_calling_points;                                               // How much space there is to display the calling points
        bool scroll_calling_points;                                                 // Yes/No - this is 'true' if the calling points exceed the width of the screen
        bool configured;                                                            // Has the second row been configured?
    };
    
    struct third_row_configuration {
        int y_position;                                                             // y position
        Refresh_state refresh_state;                                                // Refresh-control (two passes needed for each refresh)
        ThirdRowState third_row_state;                                              // Third row state - SECOND_TRAIN, THIRD_TRAIN
        int third_line_refresh_seconds;                                             // Interval between SECOND_TRAIN|THIRD_TRAIN toggles
        std::chrono::steady_clock::time_point last_third_row_toggle;                // Third row - time of the last SECOND_TRAIN|THIRD_TRAIN toggle
        bool configured;                                                            // Has the third row been configured?
    };
    
    struct fourth_row_configuration {
        int y_position;                                                             // y position
        bool show_messages;                                                         // Flag to show messages;
        Refresh_state refresh_state;                                                // Refresh-control (two passes needed for each refresh)
        bool message_scroll_complete;                                               // Yes/No - has the message been shown
        FourthRowState fourth_row_state;                                            // Fourth row state - LOCATION, MESSAGE
        int fourth_line_refresh_seconds;                                            // Interval between LOCATION, MESSAGE toggles
        std::chrono::steady_clock::time_point last_fourth_row_toggle;               // Fourth row - time of the last LOCATION, MESSAGE toggle
        bool configured;                                                            // Has the fourth row been configured?
    };
    
    struct clock_display {
        std::time_t last_clock_update_time;
        int width;
        int x_position;
        Refresh_state refresh_state;
    };
    
    clock_display the_clock;
    char time_buffer[11];

    first_row_configuration first_row_config;                                       // First row configuration - location, states, toggles.
    first_row_data first_row_content;                                               // First row content
    second_row_configuration second_row_config;                                     // Second row configuration - location, states, toggles.
    second_row_data second_row_content;                                             // Second row content
    third_row_configuration third_row_config;                                       // Third row configuration - location, states, toggles.
    third_row_data third_row_content;                                               // Third row content
    fourth_row_configuration fourth_row_config;                                     // Fourth row configuration - location, states, toggles.
    fourth_row_data fourth_row_content;                                             // Fourth row content
    
    //Helper functions
    
    void configureFirstRow();
    void configureSecondRow();
    void configureThirdRow();
    void configureFourthRow();
    
    // Matrix Configuration
    void configureMatrixOptions(RGBMatrix::Options& options) const;
    void configureRuntimeOptions(RuntimeOptions& runtime_opt) const;
    
    // Matrix Rendering
    void clearArea(int x_origin, int y_origin, int x_size, int y_size);             // Clear an area on the matrix
    void renderFirstRow();                                                          // Render the first row
    void renderSecondRow();                                                         // Render the second row
    void renderThirdRow();                                                          // Render the third row
    void renderFourthRow();                                                         // Render the fourth row
    
    void updateScrollPositions();                                                   // Update scrolling positions

    // Toggles for display data
    void checkFirstRowStateTransition(const std::chrono::steady_clock::time_point& now);    // ETD | Coaches
    void checkThirdRowStateTransition(const std::chrono::steady_clock::time_point& now);    // 2nd departure | 3rd departure
    void checkFourthRowStateTransition(const std::chrono::steady_clock::time_point& now);   // Message | Location (which may be blank)
    void transitionFirstRowState();
    void transitionThirdRowState();
    void transitionFourthRowState();

    // Clock display
    void updateClockDisplay(const std::chrono::steady_clock::time_point& current_time);     // Update the clock
    
    // Debugging (private method)
    void debugPrintRefreshState(const std::string content, const RenderState& render_state) const;  // Print the refresh-state
    
};

#endif	
