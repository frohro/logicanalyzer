#!/bin/bash
# Build script for pico2pico master and slave firmware

set -e  # Exit on error

# Check for PICO_SDK_PATH
if [ -z "$PICO_SDK_PATH" ]; then
    echo "ERROR: PICO_SDK_PATH not set"
    echo "Please run: export PICO_SDK_PATH=/path/to/pico-sdk"
    exit 1
fi

echo "Using Pico SDK: $PICO_SDK_PATH"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Build master (clock output)
echo ""
echo "=== Building MASTER (clock output) ==="
mkdir -p build_master
cd build_master
cmake .. -D PICO_BOARD=pico
cmake --build .
cd ..
echo "Master built: build_master/pico2pico_clock.uf2"

# Build slave (clock input)
echo ""
echo "=== Building SLAVE (clock input) ==="
cp CMakeLists.txt CMakeLists_master_backup.txt
cp CMakeLists_slave.txt CMakeLists.txt
mkdir -p build_slave
cd build_slave
cmake .. -D PICO_BOARD=pico
cmake --build .
cd ..
cp CMakeLists_master_backup.txt CMakeLists.txt
rm CMakeLists_master_backup.txt
echo "Slave built: build_slave/pico2pico_slave.uf2"

echo ""
echo "=== BUILD COMPLETE ==="
echo "Master firmware: $(pwd)/build_master/pico2pico_clock.uf2"
echo "Slave firmware:  $(pwd)/build_slave/pico2pico_slave.uf2"
echo ""
echo "To flash:"
echo "  Master: cp build_master/pico2pico_clock.uf2 /media/\$USER/RPI-RP2/"
echo "  Slave:  cp build_slave/pico2pico_slave.uf2 /media/\$USER/RPI-RP2/"
