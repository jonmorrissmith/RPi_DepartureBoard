//
//  matrix_driver.cpp
//  Matrix_Driver
//
//  Created by Jon Morris-Smith on 12/06/2025.
//

#include "matrix_driver.h"

MatrixDriver::MatrixDriver(const Config& configuration):

// Configuration
config(configuration),

// Set white and black
white(255, 255, 255),
black(0, 0, 0),
matrix_configured(false)
{
    // Load and cache the font
    if (!font.LoadFont(config.get("fontPath").c_str())) {
        throw std::runtime_error("Font loading failed for: " + config.get("fontPath"));
    }
    font_cache.setFont(font);
    font_baseline = font_cache.getBaseline();
    font_height = font_cache.getheight();
    
        
    // Trigger a refresh of all content
    whole_display_refresh.triggerRefresh();
    
    matrix_parameters = config.getMatrixOptions();
    initialiseMatrix();
}

MatrixDriver::~MatrixDriver(){
    delete the_matrix;
    DEBUG_PRINT("[Matrix_Driver] Display matrix destroyed");
}


void MatrixDriver::initialiseMatrix() {
    DEBUG_PRINT("[Matrix_Driver] Starting matrix initialization");
    
    RGBMatrix::Options matrix_options;
    RuntimeOptions runtime_opt;
    
    try {
        DEBUG_PRINT("[Matrix_Driver] Configuring matrix options...");
        configureMatrixOptions(matrix_options);
        
        DEBUG_PRINT("[Matrix_Driver] Configuring runtime options...");
        configureRuntimeOptions(runtime_opt);
        
        debugPrintMatrixOptions(matrix_options, runtime_opt);
        
        the_matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
        
        if (the_matrix == nullptr) {
            DEBUG_PRINT("[Matrix_Driver] Matrix creation returned nullptr");
            throw std::runtime_error("Could not create matrix");
        }
        
        if (!font_cache.isloaded()){
            DEBUG_PRINT("[Matrix_Driver] Font not loaded.");
            throw std::runtime_error("Matrix not useable without a font!");
        }
        
        // Cache matrix parameters
        matrix_width = the_matrix->width();
        matrix_height = the_matrix->height();
        canvas = the_matrix->CreateFrameCanvas();
        
        // matrix configured
        matrix_configured = true;
        DEBUG_PRINT("[Matrix_Driver] " << matrix_width << " x " << matrix_height << " Matrix initialised successfully");
        
        // Configure the rows
        configureFirstRow();
        configureSecondRow();
        configureThirdRow();
        configureFourthRow();
        
        // Initialise the last clock update time
        the_clock.last_clock_update_time = 0;
        
        // Trigger display refresh
        whole_display_refresh.triggerRefresh();
        
        DEBUG_PRINT("[Matrix_Driver] Completed matrix initialization");
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error refreshing matrix" << e.what() << std::endl;
    }
}
    
void MatrixDriver::configureMatrixOptions(RGBMatrix::Options& options ) const {
    // Basic matrix configuration
    options.rows = matrix_parameters.matrixrows;
    options.cols = matrix_parameters.matrixcols;
    options.chain_length = matrix_parameters.matrixchain_length;
    options.parallel = matrix_parameters.matrixparallel;
    
    // Hardware mapping - convert std::string to const char*
    options.hardware_mapping = matrix_parameters.matrixhardware_mapping.c_str();
    
    // Set multiplexing
    options.multiplexing = matrix_parameters.led_multiplexing;
    
    // Handle pixel mapper if set
    if (!matrix_parameters.led_pixel_mapper.empty()) {
        options.pixel_mapper_config = matrix_parameters.led_pixel_mapper.c_str();
    } else {
        options.pixel_mapper_config = nullptr;
    }
    
    // Display quality settings
    options.pwm_bits = matrix_parameters.led_pwm_bits;
    options.brightness = matrix_parameters.led_brightness;
    options.scan_mode = matrix_parameters.led_scan_mode;
    options.row_address_type = matrix_parameters.led_row_addr_type;
    
    // Display behavior settings
    options.show_refresh_rate = matrix_parameters.led_show_refresh;
    options.limit_refresh_rate_hz = matrix_parameters.led_limit_refresh;
    
    // Color settings
    options.inverse_colors = matrix_parameters.led_inverse;
    
    // RGB sequence
    if (matrix_parameters.led_rgb_sequence.length() == 3) {
        options.led_rgb_sequence = matrix_parameters.led_rgb_sequence.c_str();
    } else {
        DEBUG_PRINT("[Matrix_Driver] Warning: led-rgb-sequence must be exactly 3 characters. Using default 'RGB'.");
        options.led_rgb_sequence = "RGB";
    }
    
    // Advanced PWM settings
    options.pwm_lsb_nanoseconds = matrix_parameters.led_pwm_lsb_nanoseconds;
    options.pwm_dither_bits = matrix_parameters.led_pwm_dither_bits;
    options.disable_hardware_pulsing = matrix_parameters.led_no_hardware_pulse;
    
    // Handle panel type if set
    if (!matrix_parameters.led_panel_type.empty()) {
        options.panel_type = matrix_parameters.led_panel_type.c_str();
    } else {
        options.panel_type = nullptr;
    }
}

void MatrixDriver::configureRuntimeOptions(RuntimeOptions& runtime_opt) const {
    runtime_opt.gpio_slowdown = matrix_parameters.gpio_slowdown;
    runtime_opt.daemon = matrix_parameters.led_daemon;
}

void MatrixDriver::clearArea(int x_origin, int y_origin, int x_size, int y_size) {
    int x,y;
    
    //DEBUG_PRINT("clearing a " << x_size << " x " << y_size << " area with origin at (" << x_origin <<"," << y_origin <<")");
    
    for (x = x_origin; x < x_size; x++) {
        for ( y = y_origin; y < y_size; y++) {
            if (y >= 0 && y < matrix_height && x >= 0 && x < matrix_width) {
                canvas->SetPixel(x, y, black.r, black.g, black.b);
            }
        }
    }
}

void MatrixDriver::render(){
    try {
        auto current_time = std::chrono::steady_clock::now();
        
        if (whole_display_refresh.needsRender()) {
            
            first_row_config.refresh_state.triggerRefresh();                                                               // trigger first-row refresh
            third_row_config.refresh_state.triggerRefresh();
            debugPrintRefreshState("[Matrix_Driver] Whole display refresh", whole_display_refresh.render_state);
            if (!matrix_configured){
                throw std::runtime_error("[Matrix_Driver] Matrix not configured! No rendering possible. ");
            }
            
            canvas->Clear();
            
            whole_display_refresh.completePass();
        }
        
        renderFirstRow();
        checkFirstRowStateTransition(current_time);
        
        renderSecondRow();
        updateScrollPositions(current_time);
        
        renderThirdRow();
        checkThirdRowStateTransition(current_time);
        
        renderFourthRow();
        checkFourthRowStateTransition(current_time);
        
        updateClockDisplay(current_time);
        
        canvas = the_matrix->SwapOnVSync(canvas);
        
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error rendering the matrix" << e.what() << std::endl;
    }
}

void MatrixDriver::stop(){
    matrix_configured = false;
}


// First Row configuration, update and display

void MatrixDriver::configureFirstRow(){
 
    first_row_config.y_position = config.getInt("first_line_y");                                // y_location of the first row
    first_row_config.refresh_state.triggerRefresh();                                            // Trigger a refresh
    first_row_config.ETD_coach_refresh_seconds = config.getInt("ETD_coach_refresh_seconds");    // Interval between ETD|Coach toggles
    first_row_config.ETDCoach_state = ETD;                                                      // Displaying ETD or Coach (initialise as 'ETD'
    first_row_config.last_first_row_toggle = std::chrono::steady_clock::now();                  // When did the last Coach|ETD toggle happen
    
    first_row_content.destination.x_position = 0;
    first_row_content.estimated_depature_time.x_position = 0;
    first_row_content.coaches.x_position = 0;

    first_row_config.configured = true;
    first_row_content.api_version = -1;
    
    DEBUG_PRINT("[Matrix_Driver] [First row initialised] Destination: (" << first_row_content.destination.x_position << "," << first_row_config.y_position << ") " <<
                "ETD (x is set dynamically): (" << first_row_content.estimated_depature_time.x_position << "," << first_row_config.y_position << "). " <<
                "Coach (x is set dynamically): (" << first_row_content.coaches.x_position  << "," << first_row_config.y_position << "). " <<
                " ETD|Coach interval: " << first_row_config.ETD_coach_refresh_seconds << " (s).");
}

void MatrixDriver::updateFirstRow(const first_row_data &new_first_row){
    try {
        if(new_first_row.api_version < first_row_content.api_version) {
            throw std::runtime_error("New first row data has an API version less than the last update! ");
        }
        if(new_first_row.api_version == first_row_content.api_version){
            return;
        } else {
            first_row_content.platform = new_first_row.platform;
            first_row_content.destination = new_first_row.destination;
            
            first_row_content.scheduled_departure_time = new_first_row.scheduled_departure_time;
            
            first_row_content.estimated_depature_time = new_first_row.estimated_depature_time;
            first_row_content.estimated_depature_time.setWidth(font_cache);
            first_row_content.estimated_depature_time.x_position = matrix_width - first_row_content.estimated_depature_time.width;
            
            
            first_row_content.coach_info_available = new_first_row.coach_info_available;
            if (first_row_content.coach_info_available){
                first_row_content.coaches = new_first_row.coaches.text + " Coaches";
            } else {
                first_row_content.coaches = "";
            }
            first_row_content.coaches.setWidth(font_cache);
            first_row_content.coaches.x_position = matrix_width - first_row_content.coaches.width;
            first_row_content.api_version = new_first_row.api_version;
            if(debug_mode){
                std::cerr << "   [Matrix_Driver] ==> First Row content post-update" <<std::endl;
                std::cerr << "   [Matrix_Driver] y_position: " << first_row_config.y_position <<std::endl;
                std::cerr << "   [Matrix_Driver] coach_info_available: " << first_row_content.coach_info_available <<std::endl;
                first_row_content.destination.fulldump("[Matrix_Driver] Destination");
                first_row_content.estimated_depature_time.fulldump("[Matrix_Driver] ETD Display");
                first_row_content.coaches.fulldump("[Matrix_Driver] Coaches Display");
                std::cerr << "   [Matrix_Driver] api version: " << first_row_content.api_version <<std::endl;
            }
            
            first_row_config.refresh_state.triggerRefresh();
            
            first_row_content.api_version = new_first_row.api_version;
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error updating the first row" << e.what() << std::endl;
    }
}

void MatrixDriver::renderFirstRow(){
    try {
        
        if(first_row_config.refresh_state.needsRender()) {                                                                                          // if a render is required
            // DEBUG_PRINT("Refreshing 1st row");

            clearArea(0, first_row_config.y_position - font_baseline, matrix_width, first_row_config.y_position + font_height - font_baseline);     // clear the row
            rgb_matrix::DrawText(canvas, font, first_row_content.destination.x_position, first_row_config.y_position, white, first_row_content.destination.text.c_str());
            
            if (first_row_config.ETDCoach_state == ETD) {
                rgb_matrix::DrawText(canvas, font, first_row_content.estimated_depature_time.x_position, first_row_config.y_position, white, first_row_content.estimated_depature_time.text.c_str());
            } else {
                rgb_matrix::DrawText(canvas, font, first_row_content.coaches.x_position, first_row_config.y_position, white, first_row_content.coaches.text.c_str());
            }
            
            first_row_config.refresh_state.completePass();                                                                                          // mark the render as complete
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error rendering the first row" << e.what() << std::endl;
    }
}

void MatrixDriver::checkFirstRowStateTransition(const std::chrono::steady_clock::time_point& now){
    //auto now = std::chrono::steady_clock::now();
    
    // For ETD/Coach, toggle based on timer
    if (now - first_row_config.last_first_row_toggle >= std::chrono::seconds(first_row_config.ETD_coach_refresh_seconds)) {
        transitionFirstRowState();
        first_row_config.last_first_row_toggle = now;
    }
}
    
void MatrixDriver::transitionFirstRowState(){
    // Simply toggle between ETD and Number of Coaches
    if(first_row_content.coach_info_available == true) {
        first_row_config.ETDCoach_state = (first_row_config.ETDCoach_state == COACHES ? ETD : COACHES);
    } else {
        first_row_config.ETDCoach_state = ETD;
    }
    first_row_config.refresh_state.triggerRefresh();
    //DEBUG_PRINT("ETD|Coach transition - refresh set to: " << first_row_config.refresh);
}



// Second row configuration, update and display

void MatrixDriver::configureSecondRow(){
    
    second_row_config.y_position = config.getInt("second_line_y");
    second_row_config.calling_point_slowdown = config.getInt("calling_point_slowdown");
    second_row_config.last_second_row_scroll_move = std::chrono::steady_clock::now();                                   // When did the last move happen to scroll the second-row text
    second_row_config.calling_at_text.setTextAndWidth("Calling at:", font_cache);
    second_row_config.space_for_calling_points = matrix_width - second_row_config.calling_at_text.width ;
    second_row_config.second_row_state = CALLING_POINTS;
    second_row_config.scroll_calling_points = true;
    
    second_row_content.calling_points.x_position = matrix_width;
    second_row_content.service_message.x_position = matrix_width;
 
    second_row_config.configured = true;
    second_row_content.api_version = -1;
    
    DEBUG_PRINT("[Matrix_Driver] Second row initialised. y position: " << second_row_config.y_position << ". calling_point_slowdown: " << second_row_config.calling_point_slowdown <<
                ". space_for_calling_points: " << second_row_config.space_for_calling_points << ". scroll_calling_points (bool): " << second_row_config.scroll_calling_points);
}

void MatrixDriver::updateSecondRow(const second_row_data &new_second_row){
    try {
        if(new_second_row.api_version < second_row_content.api_version) {
            throw std::runtime_error("New second row data has an API version less than the last update! ");
        }
        if(new_second_row.api_version == second_row_content.api_version){
            return;
        } else {
            //second_row_content.calling_points.text = new_second_row.calling_points.text;                               // Copy the text, not the entire structure as the latter resets the x position of the scroll
            second_row_content.calling_points.text = new_second_row.calling_points.text;
            second_row_content.has_calling_points = new_second_row.has_calling_points;
            second_row_content.service_message.text = new_second_row.service_message.text;
            second_row_content.calling_points.setWidth(font_cache);
            second_row_content.service_message.setWidth(font_cache);
            if (second_row_content.calling_points.width < (matrix_width - second_row_config.space_for_calling_points)) {
                second_row_config.scroll_calling_points = false;
            } else {
                second_row_config.scroll_calling_points = true;
            }
            second_row_content.api_version = new_second_row.api_version;
            
            if(debug_mode){
                std::cerr << "   [Matrix_Driver] ==> Second Row content post-update" <<std::endl;
                std::cerr << "   [Matrix_Driver] y position: " << second_row_config.y_position << std::endl;
                std::cerr << "   [Matrix_Driver] scroll calling points: " << second_row_config.scroll_calling_points << std::endl;
                std::cerr << "   [Matrix Driver] has calling points: " << second_row_content.has_calling_points << std::endl;
                second_row_content.calling_points.fulldump("[Matrix_Driver] Calling Points");
                second_row_content.service_message.fulldump("[Matrix Driver] Service Message");
                std::cerr << "   [Matrix_Driver] api version: " << second_row_content.api_version <<std::endl;
            }
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error updating the second row" << e.what() << std::endl;
    }
}

void MatrixDriver::renderSecondRow(){
    try {
       
        clearArea(0, second_row_config.y_position - font_baseline, matrix_width, second_row_config.y_position + font_height - font_baseline);                                               // Clear row
        if (second_row_config.second_row_state == CALLING_POINTS && second_row_content.has_calling_points) {
            rgb_matrix::DrawText(canvas, font, second_row_content.calling_points.x_position, second_row_config.y_position, white, second_row_content.calling_points.text.c_str());          // Write the calling points

            clearArea(0, second_row_config.y_position - font_baseline, second_row_config.calling_at_text.width, second_row_config.y_position + font_height - font_baseline);                // Clear area for "Calling at:" text
            rgb_matrix::DrawText(canvas, font, 0, second_row_config.y_position, white, second_row_config.calling_at_text.text.c_str());                                                     // Write "Calling at:" text
        } else {
            rgb_matrix::DrawText(canvas, font, second_row_content.service_message.x_position, second_row_config.y_position, white, second_row_content.service_message.text.c_str());        // Write the service message
        }
        
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error rendering the second row" << e.what() << std::endl;
    }
}

void MatrixDriver::updateScrollPositions(const std::chrono::steady_clock::time_point& now){

    if (second_row_config.second_row_state == CALLING_POINTS && second_row_content.has_calling_points) {
        if (now - second_row_config.last_second_row_scroll_move >= std::chrono::microseconds(second_row_config.calling_point_slowdown)) {       // Calling-point scroll
            if (second_row_config.scroll_calling_points) {
                second_row_content.calling_points -- ;
                if (second_row_content.calling_points.x_position < -second_row_content.calling_points.width) {                                  // When we get to the end of a scroll, change to displaying the Service Message.
                    second_row_content.service_message.x_position = matrix_width;
                    second_row_content.calling_points.x_position = matrix_width;
                    second_row_config.second_row_state = SERVICE_MESSAGE;
                }
            } else {
                second_row_content.calling_points.x_position = second_row_config.calling_at_text.width + 2;
            }
            second_row_config.last_second_row_scroll_move = now;
        }
    } else {
        if (now - second_row_config.last_second_row_scroll_move >= std::chrono::microseconds(second_row_config.calling_point_slowdown)) {       // Service Message scroll
            second_row_content.service_message -- ;
            if (second_row_content.service_message.x_position < -second_row_content.service_message.width) {                                    // When we get to the end of a scroll, change to displaying the Calling Points.
                second_row_content.calling_points.x_position = matrix_width;
                second_row_content.service_message.x_position = matrix_width;
                second_row_config.second_row_state = CALLING_POINTS;
            }
            second_row_config.last_second_row_scroll_move = now;
        }
    }
    
    if (now - fourth_row_config.last_nrcc_message_move >= std::chrono::microseconds(fourth_row_config.nrcc_message_slowdown)) {                 // NRCC message scroll
        fourth_row_content.message--;
        if (fourth_row_content.message.x_position < -fourth_row_content.message.width) {
            fourth_row_content.message.x_position = matrix_width;
            fourth_row_config.message_scroll_complete = true;                                                                                    // Required to enable the toggle to Location (if set)
        }
        fourth_row_config.last_nrcc_message_move = now;
    }
}



// Third row configuration, update and display

void MatrixDriver::configureThirdRow(){
    third_row_config.y_position = config.getInt("third_line_y");
    third_row_config.refresh_state.triggerRefresh();
    third_row_config.third_line_refresh_seconds = config.getInt("third_line_refresh_seconds");
    third_row_config.third_row_state = SECOND_TRAIN;
    third_row_config.last_third_row_toggle = std::chrono::steady_clock::now();
    third_row_config.scroll_in = config.getBool("third_line_scroll_in");
    
    third_row_content.second_departure.x_position = 0;
    third_row_content.second_departure_estimated_departure_time.x_position = 0;
    third_row_content.third_departure.x_position = 0;
    third_row_content.third_departure_estimated_departure_time.x_position = 0;

    third_row_config.configured = true;
    third_row_content.api_version = -1;
    
    DEBUG_PRINT("[Matrix_Driver] [Third row initialised] 2nd departure.(" << third_row_content.second_departure.x_position << "," << third_row_config.y_position << ") " <<
                "3rd departure (" << third_row_content.third_departure.x_position << "," << third_row_config.y_position << ") " <<
                "2nd departure (" << third_row_content.second_departure_estimated_departure_time.x_position << "," << third_row_config.y_position << "). " <<
                "3rd departure (" << third_row_content.third_departure_estimated_departure_time.x_position << "," << third_row_config.y_position << "). " <<
                "Refresh interval: " << third_row_config.third_line_refresh_seconds  << " (s)" <<
                "Scroll-in transition flag: " << third_row_config.scroll_in);
}

void MatrixDriver::updateThirdRow(const third_row_data &new_third_row){
    try {
        if(new_third_row.api_version < third_row_content.api_version) {
            throw std::runtime_error("New third row data has an API version less than the last update! ");
        }
        if(new_third_row.api_version == third_row_content.api_version){
            return;
        } else {
            third_row_content.second_departure = new_third_row.second_departure;
            third_row_content.second_departure.setWidth(font_cache);
            
            third_row_content.second_departure_estimated_departure_time = new_third_row.second_departure_estimated_departure_time;
            third_row_content.second_departure_estimated_departure_time.setWidth(font_cache);
            third_row_content.second_departure_estimated_departure_time.x_position = matrix_width - third_row_content.second_departure_estimated_departure_time.width;
            
            third_row_content.third_departure = new_third_row.third_departure;
            third_row_content.third_departure.setWidth(font_cache);
            
            third_row_content.third_departure_estimated_departure_time = new_third_row.third_departure_estimated_departure_time;
            third_row_content.third_departure_estimated_departure_time.setWidth(font_cache);
            third_row_content.third_departure_estimated_departure_time.x_position = matrix_width - third_row_content.third_departure_estimated_departure_time.width;
            
            third_row_content.api_version = new_third_row.api_version;
            
            third_row_config.refresh_state.triggerRefresh();
            
            if(debug_mode){
                std::cerr << "   [Matrix_Driver] ==> Third Row content post-update" <<std::endl;
                std::cerr << "   [Matrix_Driver] y_position: " << third_row_config.y_position <<std::endl;
                third_row_content.second_departure.fulldump("[Matrix_Driver] 2nd Departure");
                third_row_content.second_departure_estimated_departure_time.fulldump("[Matrix_Driver] 2nd Departure ETD");
                third_row_content.third_departure.fulldump("[Matrix_Driver] 3rd Departure");
                third_row_content.third_departure_estimated_departure_time.fulldump("[Matrix_Driver] 3rd Departure ETD");
                std::cerr << "   [Matrix_Driver] api version: " << third_row_content.api_version <<std::endl;
            }
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error updating the third row" << e.what() << std::endl;
    }
}


// Uncoment this section if you want the 2nd/3rd departures to scroll in from the right!
// Not used at the moment as changes in CPU utilisation (other scrolling, API refresh) can make scroll speed inconsistent.
// ===> Begin section

void MatrixDriver::renderThirdRow(){
    try {
        
        if(third_row_config.refresh_state.needsRender()) {                                                                                              // if a render is required....
            if (third_row_content.second_departure.x_position > 0){                                                                                     // if we're scrolling a transition
                
                clearArea(0, third_row_config.y_position - font_baseline, matrix_width, third_row_config.y_position + font_height - font_baseline);     // clear the row
                
                if (third_row_config.third_row_state == SECOND_TRAIN) {
                    //2nd departure scrolling
                    rgb_matrix::DrawText(canvas, font, third_row_content.second_departure.x_position, third_row_config.y_position, white, third_row_content.second_departure.text.c_str());
                } else {
                    //3rd departure scrolling
                    rgb_matrix::DrawText(canvas, font, third_row_content.third_departure.x_position, third_row_config.y_position, white, third_row_content.third_departure.text.c_str());
                }
                third_row_content.second_departure.x_position--;
                third_row_content.third_departure.x_position--;
            } else {
                clearArea(0, third_row_config.y_position - font_baseline, matrix_width, third_row_config.y_position + font_height - font_baseline);     // clear the row
                
                if (third_row_config.third_row_state == SECOND_TRAIN) {
                    //2nd departure left justified - ETD right justified
                    rgb_matrix::DrawText(canvas, font, third_row_content.second_departure.x_position, third_row_config.y_position, white, third_row_content.second_departure.text.c_str());
                    rgb_matrix::DrawText(canvas, font, third_row_content.second_departure_estimated_departure_time.x_position, third_row_config.y_position, white, third_row_content.second_departure_estimated_departure_time.text.c_str());
                } else {
                    //3rd departure left justified - ETD right justified
                    rgb_matrix::DrawText(canvas, font, third_row_content.third_departure.x_position, third_row_config.y_position, white, third_row_content.third_departure.text.c_str());
                    rgb_matrix::DrawText(canvas, font, third_row_content.third_departure_estimated_departure_time.x_position, third_row_config.y_position, white, third_row_content.third_departure_estimated_departure_time.text.c_str());
                }
                
                third_row_config.refresh_state.completePass();                                                                                          // complete the render
            }
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error rendering the third row" << e.what() << std::endl;
    }
}

void MatrixDriver::checkThirdRowStateTransition(const std::chrono::steady_clock::time_point& now){
    
    if (now - third_row_config.last_third_row_toggle >= std::chrono::seconds(third_row_config.third_line_refresh_seconds)) {
        transitionThirdRowState();
        third_row_config.last_third_row_toggle = now;
    }
}

void MatrixDriver::transitionThirdRowState(){
    third_row_config.third_row_state = (third_row_config.third_row_state == SECOND_TRAIN) ? THIRD_TRAIN : SECOND_TRAIN;
    
    third_row_config.refresh_state.triggerRefresh();                                                                                            // Trigger a refresh of the third row
    if(third_row_config.scroll_in) {
        third_row_content.second_departure.x_position = matrix_width;
        third_row_content.third_departure.x_position = matrix_width;
    }
}
// ===> End section


// Uncoment this section if you want the 2nd/3rd departures simply switch from one to the other.
// If you want these to fly in from the right then comment out the section below and use the code above instead.
// ===> Begin section
/*
void MatrixDriver::renderThirdRow(){
    try {
        
        if(third_row_config.refresh_state.needsRender()) {                                                                                          // if a render is required....
            //DEBUG_PRINT("Refreshing 3rd row");
            clearArea(0, third_row_config.y_position - font_baseline, matrix_width, third_row_config.y_position + font_height - font_baseline);     // clear the row
            
            if (third_row_config.third_row_state == SECOND_TRAIN) {
                //2nd departure left justified - ETD right justified
                rgb_matrix::DrawText(canvas, font, third_row_content.second_departure.x_position, third_row_config.y_position, white, third_row_content.second_departure.text.c_str());
                rgb_matrix::DrawText(canvas, font, third_row_content.second_departure_estimated_departure_time.x_position, third_row_config.y_position, white, third_row_content.second_departure_estimated_departure_time.text.c_str());
            } else {
                //3rd departure left justified - ETD right justified
                rgb_matrix::DrawText(canvas, font, third_row_content.third_departure.x_position, third_row_config.y_position, white, third_row_content.third_departure.text.c_str());
                rgb_matrix::DrawText(canvas, font, third_row_content.third_departure_estimated_departure_time.x_position, third_row_config.y_position, white, third_row_content.third_departure_estimated_departure_time.text.c_str());
            }
            third_row_config.refresh_state.completePass();                                                                                          // complete the render
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error rendering the third row" << e.what() << std::endl;
    }
}

void MatrixDriver::checkThirdRowStateTransition(const std::chrono::steady_clock::time_point& now){
    
    if (now - third_row_config.last_third_row_toggle >= std::chrono::seconds(third_row_config.third_line_refresh_seconds)) {
        transitionThirdRowState();
        third_row_config.last_third_row_toggle = now;
    }
}

void MatrixDriver::transitionThirdRowState(){
    third_row_config.third_row_state = (third_row_config.third_row_state == SECOND_TRAIN) ? THIRD_TRAIN : SECOND_TRAIN;
    third_row_config.refresh_state.triggerRefresh();                                                                                            // Trigger a refresh of the third row
}
 */
// ===> End section


// Fourth row configuration, update and display

void MatrixDriver::configureFourthRow(){
    fourth_row_config.y_position = config.getInt("fourth_line_y");
    fourth_row_config.message_scroll_complete = false;
    fourth_row_config.refresh_state.triggerRefresh();
    fourth_row_config.fourth_line_refresh_seconds = config.getInt("Message_Refresh_interval");
    fourth_row_config.fourth_row_state = LOCATION;
    fourth_row_config.show_messages = config.getBool("ShowMessages");
    fourth_row_config.last_fourth_row_toggle = std::chrono::steady_clock::now();
    fourth_row_config.configured = true;
    fourth_row_config.nrcc_message_slowdown = config.getInt("nrcc_message_slowdown");
    fourth_row_config.last_nrcc_message_move = std::chrono::steady_clock::now();                                                                // When did the last move happen to scroll the nrcc message
    
    fourth_row_content.message.x_position = matrix_width;
    
    fourth_row_content.has_message = false;
    fourth_row_content.api_version = -1;
    
    DEBUG_PRINT("[Matrix_Driver] [Fourth row initialised] Message locn. (" << fourth_row_content.message.x_position<< "," << fourth_row_config.y_position << ") " <<
                "Has message: " << fourth_row_content.has_message << ". Show messages: " << fourth_row_config.show_messages << ". nrcc_message_slowdown: " << fourth_row_config.nrcc_message_slowdown <<
                ". Message refresh interval: " << fourth_row_config.fourth_line_refresh_seconds << " (s)");
}

void MatrixDriver::updateFourthRow(const fourth_row_data &new_forth_row){
    
    try {
        if(new_forth_row.api_version < fourth_row_content.api_version) {
            throw std::runtime_error("New fourth row data has an API version less than the last update! ");
        }
        if(new_forth_row.api_version == fourth_row_content.api_version){
            return;
        } else {
            fourth_row_content.location = new_forth_row.location;
            fourth_row_content.location.setWidth(font_cache);
            fourth_row_content.location.x_position = (matrix_width - fourth_row_content.location.width)/2;
            
            fourth_row_content.message = new_forth_row.message;
            fourth_row_content.message.setWidth(font_cache);
            if(fourth_row_content.message.text.empty()) {
                fourth_row_content.has_message = false;
            } else {
                fourth_row_content.has_message = true;
            }
            fourth_row_content.api_version = new_forth_row.api_version;
            
            fourth_row_config.refresh_state.triggerRefresh();
            
            if(debug_mode){
                std::cerr << "   [Matrix_Driver] ==> Fourth Row content post-update" <<std::endl;
                std::cerr << "   [Matrix_Driver] y_position: " << fourth_row_config.y_position <<std::endl;
                fourth_row_content.location.fulldump("[Matrix_Driver] Location");
                std::cerr << "   [Matrix_Driver] Has message: " << fourth_row_content.has_message << std::endl;
                fourth_row_content.message.fulldump("[Matrix_Driver] Message");
                std::cerr << "   [Matrix_Driver] api version: " << fourth_row_content.api_version <<std::endl;
            }
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error updating the fourth row" << e.what() << std::endl;
    }
}

void MatrixDriver::renderFourthRow(){
    try {
        if (fourth_row_config.fourth_row_state == LOCATION){                                                                                            // Display the Location on the 4th row
            if(fourth_row_config.refresh_state.needsRender()) {                                                                                         // If a render is required....
                // DEBUG_PRINT("Refreshing 4th row (location)");
                clearArea(0, fourth_row_config.y_position - font_baseline, matrix_width, fourth_row_config.y_position + font_height - font_baseline);   // clear the row
                rgb_matrix::DrawText(canvas, font, fourth_row_content.location.x_position, fourth_row_config.y_position, white, fourth_row_content.location.text.c_str());
                
                fourth_row_config.refresh_state.completePass();                                                                                         // flag the pass as complete.
            }
        } else {                                                                                                                                        // Scroll the message
            clearArea(0, fourth_row_config.y_position - font_baseline, matrix_width, fourth_row_config.y_position + font_height - font_baseline);       // Clear the row
            
            rgb_matrix::DrawText(canvas, font, fourth_row_content.message.x_position, fourth_row_config.y_position, white, fourth_row_content.message.text.c_str());
            if (fourth_row_content.message.x_position < 0) {
                rgb_matrix::DrawText(canvas, font, fourth_row_content.message.x_position + matrix_width + fourth_row_content.message.width, fourth_row_config.y_position, white, fourth_row_content.message.text.c_str());
            }
            
        }
    } catch(const std::exception& e) {
        std::cerr << "[Matrix_Driver] Error rendering the first row" << e.what() << std::endl;
    }
}

void MatrixDriver::checkFourthRowStateTransition(const std::chrono::steady_clock::time_point& now)
{
    //auto now = std::chrono::steady_clock::now();
    bool should_toggle = false;
    
    if (!fourth_row_config.show_messages || !fourth_row_content.has_message) {      // If messages are disabled or there aren't any, always show the location
        fourth_row_config.fourth_row_state = LOCATION;
        return;
    }
    
    if (fourth_row_config.fourth_row_state == MESSAGE) {                            // If we're showing a message, only toggle when scrolling is complete
        //if (fourth_row_config.message_scroll_complete && now - fourth_row_config.last_fourth_row_toggle >= std::chrono::seconds(fourth_row_config.fourth_line_refresh_seconds)) {
        if (fourth_row_config.message_scroll_complete) {                            // Changed the condition here so that the transition happens as soon as the scroll has completed
            should_toggle = true;
        }
    } else {                                                                        // For location, toggle based on timer
        if (now - fourth_row_config.last_fourth_row_toggle >= std::chrono::seconds(fourth_row_config.fourth_line_refresh_seconds)) {
            should_toggle = true;
        }
    }
    
    if (should_toggle) {
        transitionFourthRowState();
        fourth_row_config.last_fourth_row_toggle = now;
    }
}

void MatrixDriver::transitionFourthRowState(){
    if (fourth_row_content.has_message) {                               // If there's a message then toggle between location and message
        if (fourth_row_config.fourth_row_state == LOCATION) {
            fourth_row_config.fourth_row_state = MESSAGE;
            
            fourth_row_content.message.x_position = matrix_width;       // Reset message scroll position and completion flag
            fourth_row_config.message_scroll_complete = false;
        } else {
            fourth_row_config.fourth_row_state = LOCATION;              // MESSAGE - so switch to LOCATION
            fourth_row_config.refresh_state.triggerRefresh();
        }
    } else {                                                            // No message
        fourth_row_config.fourth_row_state = LOCATION;
    }
}

// Clock configuration and rendering

void MatrixDriver::updateClockDisplay(const std::chrono::steady_clock::time_point &current_time) {
    
    auto system_time = std::chrono::system_clock::now();
    auto current_time_t = std::chrono::system_clock::to_time_t(system_time);
    
    if (current_time_t != the_clock.last_clock_update_time) {
        auto tm = std::localtime(&current_time_t);
        
        std::snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
        
        the_clock.width = font_cache.getTextWidth(time_buffer);                                  // Cache width calculation (expensive operation)
        the_clock.x_position = matrix_width - the_clock.width;
        
        the_clock.last_clock_update_time = current_time_t;
        the_clock.refresh_state.triggerRefresh();
    }
    
    clearArea(the_clock.x_position - 2, fourth_row_config.y_position - font_baseline, matrix_width, fourth_row_config.y_position + font_height - font_baseline);
    rgb_matrix::DrawText(canvas, font, the_clock.x_position, fourth_row_config.y_position, white, time_buffer);      // Always draw (very fast operation)
}

// Debugging methods to display data-structures

void MatrixDriver::debugPrintFirstRowData(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- First Row Data --" << std::endl;
        first_row_content.platform.fulldump("Platform");
        first_row_content.destination.fulldump("Destination");
        first_row_content.scheduled_departure_time.fulldump("STD");
        first_row_content.estimated_depature_time.fulldump("ETD");
        first_row_content.coaches.fulldump("Coaches");
        std::cerr << "coach_info_available: " << first_row_content.coach_info_available << std::endl;
        std::cerr << "api_version: " << first_row_content.api_version << std::endl;
        std::cerr << "(Font Cache Object: is the Font cache loaded: " <<font_cache.isloaded() << ")" << std::endl;
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintFirstRowConfig(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- First Row Config  --" << std::endl;
        std::cerr << "y_position: " << first_row_config.y_position << std::endl;
        std::cerr << "ETDCoach_state: " << first_row_config.ETDCoach_state << std::endl;
        std::cerr << "ETD_coach_refresh_seconds: " << first_row_config.ETD_coach_refresh_seconds << std::endl;
        std::cerr << "ETD_x_position (from content as it's dyamic): " << first_row_content.estimated_depature_time.x_position << std::endl;
        std::cerr << "Coach_x_position (from content as it's dyamic): " << first_row_content.coaches.x_position << std::endl;
        //std::cerr << "last_first_row_toggle: " << first_row_config.last_first_row_toggle << std::endl;
        std::cerr << "configured: " << first_row_config.configured << std::endl;
        debugPrintRefreshState("First row", first_row_config.refresh_state.render_state);
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintSecondRowData(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- Second Row Data  --" << std::endl;
        second_row_content.calling_points.fulldump("Calling Points");
        second_row_content.service_message.fulldump("Service Message");
        std::cerr << "api_version: " << second_row_content.api_version<< std::endl;
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintSecondRowConfig(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- Second Row Config  --" << std::endl;
        std::cerr << "y_position: " << second_row_config.y_position<< std::endl;
        std::cerr << "calling_point_slowdown: " << second_row_config.calling_point_slowdown << std::endl;
        std::cerr << "second_row_state" << second_row_config.second_row_state << std::endl;
        std::cerr << "calling_at_text: " << second_row_config.calling_at_text<< std::endl;
        std::cerr << "space_for_calling_points: " << second_row_config.space_for_calling_points<< std::endl;
        std::cerr << "scroll_calling_points: " << second_row_config.scroll_calling_points<< std::endl;
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintThirdRowData(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- Third Row Data  --" << std::endl;
        third_row_content.second_departure.fulldump("2nd Departure");
        third_row_content.second_departure_estimated_departure_time.fulldump("2nd Departure ETD");
        third_row_content.third_departure.fulldump("3rd Departure");
        third_row_content.third_departure.fulldump("3rd Departure ETD");
        std::cerr << "api_version: " << third_row_content.api_version<< std::endl;
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintThirdRowConfig(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- Third Row Config  --" << std::endl;
        std::cerr << "y_position: " << third_row_config.y_position<< std::endl;
        std::cerr << "third_row_state: " << third_row_config.third_row_state<< std::endl;
        std::cerr << "third_line_refresh_seconds: " << third_row_config.third_line_refresh_seconds<< std::endl;
        std::cerr << "configured: " << third_row_config.configured<< std::endl;
        debugPrintRefreshState("Third row", third_row_config.refresh_state.render_state);
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintFourthRowData(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- Fourth Row Data  --" << std::endl;
        fourth_row_content.location.fulldump("Location");
        fourth_row_content.message.fulldump("Message");
        std::cerr << "api_version: " << fourth_row_content.api_version<< std::endl;
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintFourthRowConfig(){
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "-- Fourth Row Config  --" << std::endl;
        std::cerr << "y_position: " << fourth_row_config.y_position<< std::endl;
        std::cerr << "nrcc_message_slowdown: " << fourth_row_config.nrcc_message_slowdown << std::endl;
        std::cerr << "message_scroll_complete: " << fourth_row_config.message_scroll_complete<< std::endl;
        std::cerr << "fourth_row_state: " << fourth_row_config.fourth_row_state<< std::endl;
        std::cerr << "fourth_line_refresh_seconds: " << fourth_row_config.fourth_line_refresh_seconds<< std::endl;
        std::cerr << "configured: " << fourth_row_config.configured<< std::endl;
        debugPrintRefreshState("Fourth row", fourth_row_config.refresh_state.render_state);
        std::cerr << "[Matrix_Driver]" <<std::endl;
    }
}

void MatrixDriver::debugPrintMatrixOptions(const RGBMatrix::Options &matrix_options, const RuntimeOptions &runtime_opt) const   {
    if(debug_mode) {
        std::cerr << "[Matrix_Driver]" << std::endl;
        std::cerr << "----------------------------"<< std::endl;
        std::cerr << "Matrix Options - section 1"<< std::endl;
        std::cerr << "matrix_options.rows: " << matrix_options.rows       << " from config matrixrows: " << matrix_parameters.matrixrows << " In config file: " << config.getIntWithDefault("matrixrows", 64) << std::endl;
        std::cerr << "matrix_options.cols: " << matrix_options.cols       <<" from config matrixcols: " << matrix_parameters.matrixcols <<" In config file: " << config.getIntWithDefault("matrixcols", 128) << std::endl;
        std::cerr << "matrix_options.chain_length: " << matrix_options.chain_length       <<" from config matrixchain_length: " << matrix_parameters.matrixchain_length <<" In config file: " << config.getIntWithDefault("matrixchain_length", 3) << std::endl;
        std::cerr << "matrix_options.parallel: " << matrix_options.parallel       <<" from config matrixparallel: " << matrix_parameters.matrixparallel <<  " In config file: " << config.getIntWithDefault("matrixparallel", 1) << std::endl;
        if (matrix_options.hardware_mapping != nullptr) {
            std::cerr << "matrix_options.hardware_mapping: " << matrix_options.hardware_mapping       <<" from config matrixhardware_mapping: " << matrix_parameters.matrixhardware_mapping <<" In config file: " << config.get("matrixhardware_mapping") << std::endl;
        } else {
            std::cerr << "matrix_options.hardware_mapping: <not set>"<< std::endl;
        }
        std::cerr << "matrix_options.multiplexing:  " << matrix_options.multiplexing       << " from config led_multiplexing: " << matrix_parameters.led_multiplexing <<" In config file: " << config.getIntWithDefault("led-multiplexing", 0) << std::endl;
        if (matrix_options.pixel_mapper_config != nullptr) {
            std::cerr << "matrix_options.pixel_mapper_config: " << matrix_options.pixel_mapper_config       << " from config led_pixel_mapper: " << matrix_parameters.led_pixel_mapper <<" In config file: " << config.getStringWithDefault("led-pixel-mapper", "") << std::endl;
        } else {
            std::cerr << "matrix_options.pixel_mapper_config: <not set>"<< std::endl;
        }
        std::cerr << "Matrix Options - section 2"<< std::endl;
        std::cerr << "matrix_options.pwm_bits: " << matrix_options.pwm_bits       << " from config led_pwm_bits: " << matrix_parameters.led_pwm_bits <<" In config file: " << config.getIntWithDefault("led-pwm-bits", 11) << std::endl;
        std::cerr << "matrix_options.brightness: " << matrix_options.brightness       <<" from config led_brightness: " << matrix_parameters.led_brightness <<" In config file: " << config.getIntWithDefault("led-brightness", 100) << std::endl;
        std::cerr << "matrix_options.scan_mode: " << matrix_options.scan_mode       <<" from config led_scan_mode: " << matrix_parameters.led_scan_mode <<" In config file: " << config.getIntWithDefault("led-scan-mode", 0) << std::endl;
        std::cerr <<  "matrix_options.row_address_type: " << matrix_options.row_address_type       <<" from config led_row_addr_type: " << matrix_parameters.led_row_addr_type <<" In config file: " << config.getIntWithDefault("led-row-addr-type", 0) << std::endl;
        std::cerr << "matrix_options.show_refresh_rate: " << matrix_options.show_refresh_rate       <<" from config led_show_refresh: " << matrix_parameters.led_show_refresh <<" In config file: " << config.getBoolWithDefault("led-show-refresh", false) << std::endl;
        std::cerr << "matrix_options.limit_refresh_rate_hz: "<< matrix_options.limit_refresh_rate_hz <<" from config led_limit_refresh: " << matrix_parameters.led_limit_refresh <<" In config file: " << config.getIntWithDefault("led-limit-refresh", 0) << std::endl;
        std::cerr << "Matrix Options - section 3"<< std::endl;
        std::cerr << "matrix_options.inverse_colors: " << matrix_options.inverse_colors       <<" from config led_inverse: " << matrix_parameters.led_inverse <<" In config file: " << config.getBoolWithDefault("led-inverse", false) << std::endl;
        if (matrix_options.led_rgb_sequence != nullptr) {
            std::cerr << "matrix_options.led_rgb_sequence: " << matrix_options.led_rgb_sequence       <<" from config led_rgb_sequence: " << matrix_parameters.led_rgb_sequence <<" In config file: " << config.getStringWithDefault("led-rgb-sequence", "RGB") << std::endl;
        } else {
            std::cerr << "matrix_options.led_rgb_sequence: <not set>"<< std::endl;
        }
        std::cerr << "matrix_options.pwm_lsb_nanoseconds: " << matrix_options.pwm_lsb_nanoseconds       <<" from config led_pwm_lsb_nanoseconds: " << matrix_parameters.led_pwm_lsb_nanoseconds <<" In config file: " << config.getIntWithDefault("led-pwm-lsb-nanoseconds", 130) << std::endl;
        std::cerr << "matrix_options.pwm_dither_bits: " << matrix_options.pwm_dither_bits       <<" from config led_pwm_dither_bits: " << matrix_parameters.led_pwm_dither_bits <<" In config file: " << config.getIntWithDefault("led-pwm-dither-bits", 0) << std::endl;
        std::cerr << "matrix_options.disable_hardware_pulsing: " << matrix_options.disable_hardware_pulsing       <<" from config led_no_hardware_pulse: " << matrix_parameters.led_no_hardware_pulse <<" In config file: " << config.getBoolWithDefault("led-no-hardware-pulse", false)<< std::endl;
        std::cerr << "Matrix Options - section 4"<< std::endl;
        if (matrix_options.panel_type != nullptr) {
            std::cerr << "matrix_options.panel_type: " << matrix_options.panel_type       <<" from config led_panel_type: " << matrix_parameters.led_panel_type <<" In config file: " << config.getStringWithDefault("led-panel-type", "") << std::endl;
        } else {
            std::cerr << "matrix_options.panel_type: <not set>"<< std::endl;
        }
        std::cerr << "runtime_opt.gpio_slowdown: " << runtime_opt.gpio_slowdown << " from config gpio_slowdown: " << matrix_parameters.gpio_slowdown <<" In config file: " << config.getIntWithDefault("gpio_slowdown", 1) << std::endl;
        std::cerr << "runtime_opt.daemon" << runtime_opt.daemon << " from config led_daemon: " << matrix_parameters.led_daemon << " In config file: " << config.getBoolWithDefault("led-daemon", false)<< std::endl;
        
        std::cerr << "[Matrix_Driver] -------------------" <<std::endl;
    }
}

void MatrixDriver::debugPrintRefreshState(const std::string content, const RenderState &render_state) const {
    if(debug_mode){
        std::cerr << "[Matrix_Driver] Refresh State: " << std::endl;
        if (render_state  == RenderState::FIRST_PASS){
            std::cerr << content << ": render state: FIRST_PASS" << std::endl;
        }
        if (render_state  == RenderState::SECOND_PASS){
            std::cerr << content << ": render state: SECOND_PASS" << std::endl;
        }
        if (render_state  == RenderState::IDLE){
            std::cerr << content << ": render state: IDLE" << std::endl;
        }
        std::cerr << "[Matrix_Driver] End Refresh State: " << std::endl;
    }
}




