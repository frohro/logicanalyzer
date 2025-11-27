#!/bin/bash

set -euo pipefail

# Reboot pico2-ice (RP2040/RP2350) into BOOTSEL mode without touching BOOTSEL.
# Flow:
#   1. If already in BOOTSEL, exit successfully immediately.
#   2. If MicroPython (or any firmware with USB REPL) is running, try `mpremote`.
#   3. Fall back to toggling the USB CDC port at 1200 baud (DTR/RTS low).
#   4. Wait for the BOOTSEL mass-storage device (RP2350 or RPI-RP2) to appear.
# Exit status is non-zero only when the BOOTSEL drive never shows up.

BOOTSEL_LABELS=("RP2350" "RPI-RP2" "RP2")
WAIT_SECS=25

info()  { echo -e "[INFO]  $*"; }
warn()  { echo -e "[WARN]  $*"; }
error() { echo -e "[ERROR] $*" >&2; }

detect_bootsel_device() {
    local labels
    labels=$(lsblk -rno LABEL 2>/dev/null || true)
    for label in "${BOOTSEL_LABELS[@]}"; do
        if echo "$labels" | grep -Fxq "$label"; then
            return 0
        fi
        if [ -e "/dev/disk/by-label/$label" ]; then
            return 0
        fi
    done
    return 1
}

detect_bootsel_mount() {
    for label in "${BOOTSEL_LABELS[@]}"; do
        for base in "/media/$USER" "/run/media/$USER"; do
            if [ -d "$base/$label" ]; then
                echo "$base/$label"
                return 0
            fi
        done
    done
    return 1
}

wait_for_bootsel() {
    local timeout="${1:-$WAIT_SECS}"
    for ((i = 0; i < timeout; i++)); do
        if detect_bootsel_device; then
            if mount_point=$(detect_bootsel_mount); then
                info "Detected BOOTSEL mass-storage at $mount_point"
            else
                info "Detected BOOTSEL device (mount pending)"
            fi
            return 0
        fi
        sleep 1
    done
    return 1
}

find_pico_port() {
    for port in /dev/ttyACM* /dev/ttyUSB*; do
        if [ -e "$port" ] && [ -c "$port" ]; then
            local vendor
            vendor=$(udevadm info -q property -n "$port" 2>/dev/null | grep "ID_VENDOR_ID=" | cut -d= -f2 || true)
            if [ "$vendor" = "2e8a" ] || [ "$vendor" = "1209" ]; then
                echo "$port"
                return 0
            fi
        fi
    done
    for port in /dev/ttyACM*; do
        [ -c "$port" ] && { echo "$port"; return 0; }
    done
    return 1
}

attempt_mpremote() {
    command -v mpremote >/dev/null 2>&1 || return 1
    info "Requesting BOOTSEL via mpremote (machine.bootloader)"
    set +e
    mpremote exec "import machine; machine.bootloader()" >/dev/null 2>&1
    local status=$?
    set -e
    # Status will be non-zero because device disconnects; treat as success if command ran
    return 0
}

serial_trigger_python() {
    command -v python3 >/dev/null 2>&1 || return 1
    python3 - "$PICO_PORT" <<'PY'
import serial, sys, time
port = sys.argv[1]
try:
    with serial.Serial(port, 1200, timeout=0.1) as ser:
        ser.dtr = False
        ser.rts = False
        time.sleep(0.15)
except Exception as exc:
    sys.exit(1)
PY
}

serial_trigger_stty() {
    command -v stty >/dev/null 2>&1 || return 1
    stty -F "$PICO_PORT" 1200 cs8 -cstopb -parenb -hupcl -clocal -echo -echoe -echok -echoctl -echoke 2>/dev/null
    sleep 0.2
}

attempt_serial_trigger() {
    info "Looking for Pico USB serial device"
    if ! PICO_PORT=$(find_pico_port); then
        warn "No Pico serial device (ttyACM/ttyUSB) found"
        return 1
    fi
    info "Using $PICO_PORT for 1200-baud reboot"
    if serial_trigger_python || serial_trigger_stty; then
        return 0
    fi
    warn "Could not toggle $PICO_PORT (missing pyserial and stty?)"
    return 1
}

main() {
    if detect_bootsel_device; then
        info "Device already in BOOTSEL mode"
        exit 0
    fi

    if attempt_mpremote; then
        sleep 2  # Give device time to disconnect and re-enumerate
        if wait_for_bootsel 15; then
            info "Device rebooted to BOOTSEL via mpremote"
            exit 0
        fi
        warn "mpremote completed but BOOTSEL device did not appear"
    fi

    if attempt_serial_trigger; then
        info "Waiting for BOOTSEL device after 1200-baud toggle"
        if wait_for_bootsel; then
            info "Device is now in BOOTSEL mode"
            exit 0
        fi
    else
        warn "Serial trigger step could not be performed"
    fi

    error "Unable to enter BOOTSEL automatically. Try power-cycling while holding BOOTSEL."
    exit 1
}

main "$@"
