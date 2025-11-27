#!/bin/bash

# Flash firmware to Raspberry Pi Pico
# Usage: ./flash.sh [firmware_file.uf2]

set -euo pipefail

FIRMWARE="${1:-}"

if [ -z "$FIRMWARE" ]; then
    echo "✗ Usage: $0 <firmware.uf2>"
    exit 1
fi

if [ ! -f "$FIRMWARE" ]; then
    echo "✗ Firmware file '$FIRMWARE' not found."
    exit 1
fi

echo "Flashing $(basename "$FIRMWARE")..."

check_bootsel() {
    for label in "RPI-RP2" "RP2350" "RP2"; do
        if [ -e "/dev/disk/by-label/$label" ]; then
            return 0
        fi
    done
    return 1
}

find_bootsel_mount() {
    for label in "RPI-RP2" "RP2350" "RP2"; do
        for base in "/media/$USER" "/run/media/$USER"; do
            if [ -d "$base/$label" ]; then
                echo "$base/$label"
                return 0
            fi
        done
    done
    return 1
}

# Check if already in BOOTSEL mode
if ! check_bootsel; then
    echo "No device in BOOTSEL mode found."
    echo "Attempting to reboot device into BOOTSEL mode..."
    if ./reboot_bootsel.sh; then
        sleep 2
    else
        echo ""
        echo "Automatic reboot failed. Please manually enter BOOTSEL mode:"
        echo "  1. Unplug the device"
        echo "  2. Hold the BOOTSEL button"
        echo "  3. Plug in the USB cable while holding BOOTSEL"
        echo "  4. Release BOOTSEL"
        echo ""
        read -p "Press Enter when device is in BOOTSEL mode..."
    fi
fi

# Wait for mount point
echo "Waiting for BOOTSEL mount point..."
MOUNT_POINT=""
for i in {1..30}; do
    if MOUNT_POINT=$(find_bootsel_mount); then
        break
    fi
    sleep 1
done

if [ -z "$MOUNT_POINT" ]; then
    echo "✗ BOOTSEL mount point not found after 30 seconds"
    exit 1
fi

echo "✓ Found BOOTSEL at $MOUNT_POINT"
echo "Copying firmware..."
cp "$FIRMWARE" "$MOUNT_POINT/"
echo "✓ Firmware flashed successfully!"
echo "Device will reboot automatically."
