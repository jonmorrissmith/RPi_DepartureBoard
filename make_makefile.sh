#!/bin/bash

# Architecture-Aware Makefile Generator for Raspberry Pi Train Departure Board
# Automatically detects CPU architecture and sets optimal compiler flags

set -e  # Exit on any error

echo "🚂 Train Departure Board - Makefile Generator"
echo "=============================================="

# Detect architecture
ARCH=$(uname -m)
echo "Detected architecture: $ARCH"

# Initialize compiler flags
#BASE_CXXFLAGS="-O3 -Wall -Wextra -I/home/display/rpi-rgb-led-matrix/include -I\$(SRCDIR)"
BASE_CXXFLAGS="-O3 -I/home/display/rpi-rgb-led-matrix/include -I\$(SRCDIR)"
ARCH_FLAGS=""
#CXX_STANDARD="-std=c++11"  # Default to C++11 for compatibility
CXX_STANDARD=""  

# Function to detect GCC version
get_gcc_version() {
    gcc -dumpversion | cut -d. -f1
}

# Function to check if CPU supports NEON/Advanced SIMD
check_neon_support() {
    local arch=$(uname -m)
    
    # ARM64 always has Advanced SIMD (successor to NEON)
    if [ "$arch" = "aarch64" ]; then
        return 0  # ARM64 always has Advanced SIMD
    fi
    
    # For ARM32, check /proc/cpuinfo for NEON
    if [ -f /proc/cpuinfo ]; then
        if grep -q -E "(neon|asimd)" /proc/cpuinfo; then
            return 0  # NEON/ASIMD supported
        fi
    fi
    return 1  # NEON not supported or unknown
}

# Function to detect specific Pi model for optimization
detect_pi_model() {
    if [ -f /proc/device-tree/model ]; then
        local model=$(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')
        echo "$model"
    elif [ -f /sys/firmware/devicetree/base/model ]; then
        local model=$(cat /sys/firmware/devicetree/base/model 2>/dev/null | tr -d '\0')
        echo "$model"
    else
        echo "Unknown Pi Model"
    fi
}

echo "Pi Model: $(detect_pi_model)"
echo "GCC Version: $(get_gcc_version)"

# Set architecture-specific flags
case "$ARCH" in
    armv6l)
        echo "📟 Configuring for ARM v6 (Raspberry Pi Zero, Pi 1)"
        ARCH_FLAGS="-march=armv6+fp -mfpu=vfp -mfloat-abi=hard"
        echo "⚠️  Note: NEON not available on ARMv6 - using VFP optimizations"
        ;;
        
    armv7l)
        echo "🔧 Configuring for ARM v7 (Raspberry Pi 2, Pi 3 in 32-bit mode)"
        if check_neon_support; then
            ARCH_FLAGS="-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -ftree-vectorize"
            echo "✅ NEON SIMD support enabled!"
        else
            ARCH_FLAGS="-march=armv7-a -mfpu=vfpv4 -mfloat-abi=hard"
            echo "⚠️  NEON not detected - using VFPv4 optimizations"
        fi
        ;;
        
    aarch64)
        echo "🚀 Configuring for ARM64 (Raspberry Pi 3/4/5 in 64-bit mode)"
        
        # Detect specific Pi model for targeted optimization
        pi_model=$(detect_pi_model)
        case "$pi_model" in
            *"Raspberry Pi 5"*)
                echo "🔥 Raspberry Pi 5 detected - using Cortex-A76 optimizations"
                ARCH_FLAGS="-march=armv8.2-a+fp16+rcpc+dotprod -mtune=cortex-a76 -ftree-vectorize"
                #CXX_STANDARD="-std=c++17"  # Pi 5 can handle newer standards
                ;;
            *"Raspberry Pi 4"*)
                echo "⚡ Raspberry Pi 4 detected - using Cortex-A72 optimizations"
                ARCH_FLAGS="-march=armv8-a+crc -mtune=cortex-a72 -ftree-vectorize"
                #CXX_STANDARD="-std=c++14"  # Pi 4 can handle C++14
                ;;
            *"Raspberry Pi 3"*)
                echo "🎯 Raspberry Pi 3 detected - using Cortex-A53 optimizations"
                ARCH_FLAGS="-march=armv8-a -mtune=cortex-a53 -ftree-vectorize"
                ;;
            *)
                echo "🔧 Generic ARM64 optimization"
                ARCH_FLAGS="-march=armv8-a -ftree-vectorize"
                ;;
        esac
        echo "✅ Advanced SIMD (NEON successor) support enabled!"
        ;;
        
    x86_64)
        echo "💻 Configuring for x86_64 (development/testing)"
        ARCH_FLAGS="-march=native -mtune=native"
        echo "✅ Using native x86_64 optimizations (no NEON)"
        ;;
        
    i686|i386)
        echo "💻 Configuring for x86 32-bit (development/testing)"
        ARCH_FLAGS="-march=native -mtune=native"
        echo "✅ Using native x86 optimizations (no NEON)"
        ;;
        
    *)
        echo "❓ Unknown architecture: $ARCH"
        echo "   Using generic optimizations"
        ARCH_FLAGS="-O3"
        ;;
esac

# Add additional performance flags
PERF_FLAGS="-ffast-math -funroll-loops -fomit-frame-pointer"

# Combine all flags
FINAL_CXXFLAGS="$CXX_STANDARD $BASE_CXXFLAGS $ARCH_FLAGS $PERF_FLAGS"

echo ""
echo "📋 Generated compiler flags:"
echo "   $FINAL_CXXFLAGS"
echo ""

# Create the optimized Makefile
cat > Makefile << EOF
# Auto-generated Makefile for Train Departure Board
# Architecture: $ARCH
# Generated: $(date)
# Pi Model: $(detect_pi_model)

# Directories
SRCDIR = Src
OBJDIR = Obj

# Compiler and flags
CXX = g++
CXXFLAGS = $FINAL_CXXFLAGS
LDFLAGS = -L/home/display/rpi-rgb-led-matrix/lib
LDLIBS = -lrgbmatrix -lcurl -lpthread

# Target executable
TARGET = departureboard

# Source files (in Src directory)
SOURCES = \$(SRCDIR)/API_client.cpp \\
          \$(SRCDIR)/config.cpp \\
          \$(SRCDIR)/departure_board.cpp \\
          \$(SRCDIR)/departureboard.cpp \\
          \$(SRCDIR)/display_text.cpp \\
          \$(SRCDIR)/HTML_processor.cpp \\
          \$(SRCDIR)/time_utls.cpp \\
          \$(SRCDIR)/train_service_parser.cpp \\
          \$(SRCDIR)/matrix_driver.cpp 

# Object files (maintained in separate directory)
OBJECTS = \$(patsubst \$(SRCDIR)/%.cpp,\$(OBJDIR)/%.o,\$(SOURCES))

# Ensure obj directory exists
\$(shell mkdir -p \$(OBJDIR))

# Architecture info target
arch-info:
	@echo "Architecture: $ARCH"
	@echo "Compiler flags: \$(CXXFLAGS)"
	@echo "Pi Model: $(detect_pi_model)"
	@echo "GCC Version: \$(shell gcc --version | head -n1)"
	@echo "NEON/ASIMD Support: $(check_neon_support && echo "Yes" || echo "No")"

# Main rules
all: \$(TARGET)

\$(TARGET): \$(OBJECTS)
	@echo "🔗 Linking \$@..."
	\$(CXX) \$(LDFLAGS) -o \$@ \$^ \$(LDLIBS)
	@echo "✅ Build complete!"
	@echo ""
	@echo "🚂 Train Departure Board built for $ARCH"
	@echo "   Optimizations: $(echo $ARCH_FLAGS | sed 's/-/ /g')"

# Pattern rule for object files
\$(OBJDIR)/%.o: \$(SRCDIR)/%.cpp
	@echo "🔨 Compiling \$<..."
	\$(CXX) \$(CXXFLAGS) -c \$< -o \$@

# Development targets
debug: CXXFLAGS += -g -DDEBUG -O1
debug: clean \$(TARGET)

profile: CXXFLAGS += -pg -g
profile: LDFLAGS += -pg
profile: clean \$(TARGET)

# Performance testing target
benchmark: \$(TARGET)
	@echo "🏃 Running basic performance test..."
	@time ./\$(TARGET) --test-mode 2>/dev/null || echo "Performance test requires implementation"

# Clean rule
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f \$(TARGET) parser_test \$(OBJDIR)/*.o
	rmdir \$(OBJDIR) 2>/dev/null || true
	@echo "✅ Clean complete!"

# Install dependencies (Raspberry Pi specific)
install-deps:
	@echo "📦 Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y build-essential libcurl4-openssl-dev
	@echo "✅ Dependencies installed!"

# Show optimization report
opt-report:
	@echo "🎯 Optimization Report"
	@echo "====================="
	@echo "Architecture: $ARCH"
	@echo "Compiler: \$(shell \$(CXX) --version | head -n1)"
	@echo "Flags: \$(CXXFLAGS)"
	@objdump -f \$(TARGET) 2>/dev/null | grep "file format" || echo "Build target first with 'make'"

# Phony targets
.PHONY: all clean debug profile arch-info benchmark install-deps opt-report

# Help target
help:
	@echo "🚂 Train Departure Board - Build System"
	@echo "========================================"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build optimized release version"
	@echo "  debug        - Build debug version with symbols"
	@echo "  profile      - Build with profiling support"
	@echo "  clean        - Remove build artifacts"
	@echo "  arch-info    - Show architecture and compiler info"
	@echo "  benchmark    - Run basic performance test"
	@echo "  install-deps - Install required dependencies"
	@echo "  opt-report   - Show optimization details"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Architecture: $ARCH"
	@echo "Optimizations: Active"
EOF

echo "✅ Makefile created successfully!"
echo ""
echo "🎯 Optimization Summary:"
echo "   • Architecture: $ARCH"
echo "   • NEON SIMD: $(check_neon_support && echo "Enabled" || echo "Not available")"
echo "   • Vectorization: Enabled"
echo "   • Pi-specific tuning: Enabled"
echo ""
echo "🚀 Quick start:"
echo "   make arch-info    # Show detailed architecture info"
echo "   make             # Build optimized version"
echo "   make help        # Show all available targets"
echo ""

# Optional: Show what optimizations are active
if check_neon_support; then
    echo "🔥 Your Raspberry Pi supports Advanced SIMD instructions!"
    echo "   The HTML processor will automatically use vectorized operations"
    echo "   for improved performance on longer strings."
else
    echo "ℹ️  NEON SIMD not available on this architecture."
    echo "   The code will use optimized scalar operations instead."
fi

echo ""
echo "🎉 Ready to build your optimized train departure board!"

