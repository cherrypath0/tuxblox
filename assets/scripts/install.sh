#!/bin/bash

echo "This CLI is deprecated!"
exit 0

INSTALLERVERSION="0.0-INDEV"
SPIN_CHARS='|/—\'
SPIN_PID=""

start_spin() {
    local msg="$1"
    (
        local i=0
        while true; do
            printf "\r[  %s  ] %s" "${SPIN_CHARS:$((i % 4)):1}" "$msg"
            i=$((i + 1))
            sleep 0.1
        done
    ) &
    SPIN_PID=$!
    disown "$SPIN_PID" 2>/dev/null
}

stop_spin() {
    if [ -n "$SPIN_PID" ]; then
        kill "$SPIN_PID" 2>/dev/null
        wait "$SPIN_PID" 2>/dev/null
        SPIN_PID=""
    fi
}

status_ok() {
    local msg="$1"
    stop_spin
    printf "\r[  \e[32mOK\e[0m  ] %s\n" "$msg"
}

status_err() {
    local msg="$1"
    stop_spin
    printf "\r[  \e[31mERR\e[0m  ] %s\n" "$msg"
}

status_warn() {
    local msg="$1"
    stop_spin
    printf "\r[  \e[33mWARN\e[0m  ] %s\n" "$msg"
}

detail() {
    echo "          $1"
}

echo "Running TuxBlox installer version $INSTALLERVERSION"
echo ""

echo " == User Permissions == "
start_spin "Checking user"
USER_IS_ROOT=$([ "$(id -u)" -eq 0 ] && echo 1 || echo 0)

if [ "$USER_IS_ROOT" -eq 1 ]; then
    status_err "Checking user"
    detail "Running this script as root is dangerous, do not run as root!"
    echo ""
    echo "Aborting installation!"
    exit 1
else
    status_ok "Checking user"
fi

start_spin "Checking if TuxBlox is already installed"

IS_UPDATED=0
if tuxblox --version > /dev/null 2>&1 || [ -d "$HOME/.tuxblox" ]; then
    if tuxblox --version > /dev/null 2>&1; then
        status_warn "Checking if TuxBlox is already installed"
        detail "TuxBlox is already installed. The script will update it."
        IS_UPDATED=1
    elif [ -d "$HOME/.tuxblox" ]; then
        status_err "Checking if TuxBlox is already installed"
        detail "TuxBlox directory exists, but the installation is partial or corrupt"
        detail "Possible solution: Run this command: rm -rf $HOME/.tuxblox"
        echo ""
        echo "Aborting installation!"
        exit 1
    fi
else
    status_ok "Checking if TuxBlox is already installed"
fi

MIN_RAM_GB=6
REC_RAM_GB=8

MIN_STORAGE_GB=60
REC_STORAGE_GB=70

MIN_CPU_CORES=2
REC_CPU_CORES=4

FAIL=0

echo ""
echo " == Checking Compatibility == "
start_spin "Checking memory"
TOTAL_RAM_KB=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
TOTAL_RAM_GB=$(( (TOTAL_RAM_KB + 1048575) / 1048576 ))

if [ "$TOTAL_RAM_GB" -lt "$MIN_RAM_GB" ]; then
    status_err "Checking memory"
    detail "Memory requirement not met (minimum ${MIN_RAM_GB}GB, detected ${TOTAL_RAM_GB}GB)"
    FAIL=1
elif [ "$TOTAL_RAM_GB" -lt "$REC_RAM_GB" ]; then
    status_warn "Checking memory"
    detail "${REC_RAM_GB}GB RAM is recommended, but ${TOTAL_RAM_GB}GB was detected"
else
    status_ok "Checking memory"
fi

start_spin "Checking storage"

INSTALL_PATH="${HOME}"

AVAILABLE_STORAGE_KB=$(df -Pk "$INSTALL_PATH" 2>/dev/null | awk 'NR==2 {print $4}')
AVAILABLE_STORAGE_GB=$(( AVAILABLE_STORAGE_KB / 1048576 ))

SOURCE_DEV=$(df -P "$INSTALL_PATH" 2>/dev/null | awk 'NR==2 {print $1}')
SOURCE_NAME=$(basename "$SOURCE_DEV" 2>/dev/null)
PKNAME=$(lsblk -no PKNAME "$SOURCE_DEV" 2>/dev/null | head -n1)
[ -z "$PKNAME" ] && PKNAME="$SOURCE_NAME"

ROTA="1"
if [ -n "$PKNAME" ] && [ -r "/sys/block/${PKNAME}/queue/rotational" ]; then
    ROTA=$(cat "/sys/block/${PKNAME}/queue/rotational")
fi

if [ "$AVAILABLE_STORAGE_GB" -lt "$MIN_STORAGE_GB" ]; then
    status_err "Checking storage"
    detail "Storage requirement not met (minimum ${MIN_STORAGE_GB}GB, detected ${AVAILABLE_STORAGE_GB}GB free on ${INSTALL_PATH})"
    FAIL=1
else
    NEEDS_WARN=0
    if [ "$AVAILABLE_STORAGE_GB" -lt "$REC_STORAGE_GB" ]; then
        NEEDS_WARN=1
    fi
    if [ "$ROTA" = "1" ]; then
        NEEDS_WARN=1
    fi

    if [ "$NEEDS_WARN" -eq 1 ]; then
        status_warn "Checking storage"
        if [ "$AVAILABLE_STORAGE_GB" -lt "$REC_STORAGE_GB" ]; then
            detail "${REC_STORAGE_GB}GB Storage is recommended, but ${AVAILABLE_STORAGE_GB}GB was detected"
        fi
        if [ "$ROTA" = "1" ]; then
            detail "SSD recommended for better experience (${SOURCE_NAME:-detected drive} is an HDD)"
        fi
    else
        status_ok "Checking storage"
    fi
fi

start_spin "Checking processor"
CPU_CORES=$(nproc --all 2>/dev/null || grep -c ^processor /proc/cpuinfo)

if [ "$CPU_CORES" -lt "$MIN_CPU_CORES" ]; then
    status_err "Checking processor"
    detail "Processor requirement not met (minimum ${MIN_CPU_CORES} cores, detected ${CPU_CORES})"
    FAIL=1
elif [ "$CPU_CORES" -lt "$REC_CPU_CORES" ]; then
    status_warn "Checking processor"
    detail "${REC_CPU_CORES} CPU cores is recommended, but ${CPU_CORES} was detected"
else
    status_ok "Checking processor"
fi

start_spin "Checking IOMMU support"

IOMMU_OK=0

if [ -r /proc/cmdline ]; then
    CMDLINE=$(cat /proc/cmdline)
else
    CMDLINE=""
fi

IOMMU_DMESG=$(dmesg 2>/dev/null | grep -iE "iommu|dmar|amd-vi")

IOMMU_GROUPS_PRESENT=0
if [ -d /sys/kernel/iommu_groups ] && [ "$(ls -A /sys/kernel/iommu_groups 2>/dev/null)" ]; then
    IOMMU_GROUPS_PRESENT=1
fi

if [ "$IOMMU_GROUPS_PRESENT" -eq 1 ]; then
    IOMMU_OK=1
    status_ok "Checking IOMMU support"
elif echo "$CMDLINE" | grep -qiE "intel_iommu=on|amd_iommu=on|iommu=pt"; then
    IOMMU_OK=1
    status_warn "Checking IOMMU support"
    detail "IOMMU appears enabled via kernel cmdline, but no IOMMU groups found yet (reboot may be required)"
else
    status_err "Checking IOMMU support"
    if [ -n "$IOMMU_DMESG" ]; then
        detail "IOMMU/DMAR references found in dmesg, but no active IOMMU groups detected"
    else
        detail "No IOMMU support detected"
    fi
    detail "Possible solution: Enable VT-d (Intel) or AMD-Vi (AMD) in BIOS/UEFI, and add"
    detail "'intel_iommu=on' or 'amd_iommu=on' to your kernel boot parameters."
    FAIL=1
fi

start_spin "Checking GPU / VFIO passthrough"

GPU_OK=0

if ! command -v lspci >/dev/null 2>&1; then
    status_err "Checking GPU / VFIO passthrough"
    detail "'lspci' not found, cannot detect GPU (install pciutils)"
    FAIL=1
else
    GPU_LIST=$(lspci -nn | grep -iE "vga|3d controller|display controller")

    if [ -z "$GPU_LIST" ]; then
        status_err "Checking GPU / VFIO passthrough"
        detail "No GPU detected"
        FAIL=1
    else
        VFIO_MODULE_OK=0
        if lsmod 2>/dev/null | grep -q "^vfio_pci"; then
            VFIO_MODULE_OK=1
        elif modinfo vfio-pci >/dev/null 2>&1; then
            VFIO_MODULE_OK=1
        fi

        if [ "$VFIO_MODULE_OK" -eq 1 ] && [ "$IOMMU_OK" -eq 1 ]; then
            GPU_OK=1
            status_ok "Checking GPU / VFIO passthrough"
        elif [ "$VFIO_MODULE_OK" -eq 0 ]; then
            status_err "Checking GPU / VFIO passthrough"
            detail "vfio-pci kernel module not found/loaded (required for VFIO passthrough)"
            FAIL=1
        else
            status_err "Checking GPU / VFIO passthrough"
            detail "GPU found, but IOMMU is not active, so VFIO passthrough is not currently possible"
            FAIL=1
        fi
    fi
fi

if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
    echo ""
    echo "ERROR: Neither 'curl' nor 'wget' was found. Please install one of them to download TuxBlox."
    exit 1
fi

echo ""
if [ "$FAIL" -ne 0 ]; then
    echo "ERROR: Requirements not met!"
    echo "Aborting installation!"
    exit 1
fi

echo " == Installing TuxBlox == "

start_spin "Creating the TuxBlox directory structure"
if ! mkdir -p ~/.tuxblox/logs ~/.tuxblox/config ~/.tuxblox/tmp ~/.tuxblox/iso "$HOME/.local/bin" 2>/dev/null; then
    status_err "Creating the TuxBlox directory structure"
    detail "Failed to create directories!"
    echo ""
    echo "Aborting installation!"
    exit 1
fi
status_ok "Creating the TuxBlox directory structure"

if [ "$IS_UPDATED" -eq 1 ]; then
    ACTION_MSG="Updating TuxBlox CLI"
else
    ACTION_MSG="Downloading TuxBlox CLI"
fi

start_spin "$ACTION_MSG"

BIN_URL="https://tuxblox.net/files/tuxblox-cli"
TARGET_DIR="$HOME/.local/bin"
TARGET_FILE="$TARGET_DIR/tuxblox"

rm -f "$TARGET_FILE"

if command -v curl >/dev/null 2>&1; then
    curl -sSL -o "$TARGET_FILE" "$BIN_URL"
else
    wget -q -O "$TARGET_FILE" "$BIN_URL"
fi

if [ ! -s "$TARGET_FILE" ]; then
    status_err "$ACTION_MSG"
    detail "Failed to download the binary from $BIN_URL"
    echo ""
    echo "Aborting installation!"
    exit 1
fi

chmod +x "$TARGET_FILE"
status_ok "$ACTION_MSG"

echo ""
if [ "$IS_UPDATED" -eq 1 ]; then
    echo "Finished updating TuxBlox!"
else
    echo "Finished installing TuxBlox!"
fi

if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
    echo ""
    echo "NOTE: '$HOME/.local/bin' is not in your current PATH variable."
    echo "To run 'tuxblox' from anywhere, add this line to your ~/.bashrc or ~/.zshrc file:"
    echo "export PATH=\"\$HOME/.local/bin:\$PATH\""
    echo "Then restart your terminal or run: source ~/.bashrc"
fi

exit 0