#!/bin/bash
#
# SPDX-FileCopyrightText: 2016 The CyanogenMod Project
# SPDX-FileCopyrightText: 2017-2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

set -e

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."

HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

ONLY_COMMON=
ONLY_TARGET=
KANG=
SECTION=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        --only-common)
            ONLY_COMMON=true
            ;;
        --only-target)
            ONLY_TARGET=true
            ;;
        -n | --no-cleanup)
            CLEAN_VENDOR=false
            ;;
        -k | --kang)
            KANG="--kang"
            ;;
        -s | --section)
            SECTION="${2}"
            shift
            CLEAN_VENDOR=false
            ;;
        *)
            SRC="${1}"
            ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    SRC="adb"
fi

function blob_fixup() {
    # Patch all binaries for libstagefright_foundation AND libaudioroute
    shopt -s globstar
    case "${1}" in
        vendor/bin/** | vendor/**/*.so)
            if readelf -d "$2" 2>/dev/null | grep -q 'libstagefright_foundation.so'; then
                "${PATCHELF}" --replace-needed "libstagefright_foundation.so" "libstagefright_foundation-v33.so" "${2}"
            fi
            
            if readelf -d "$2" 2>/dev/null | grep -q 'libaudioroute.so'; then
                "${PATCHELF}" --replace-needed "libaudioroute.so" "libaudioroute-v34.so" "${2}"
            fi
            ;;
    esac

    case "${1}" in
        vendor/bin/hw/android.hardware.security.keymint-service-qti | vendor/lib64/libqtikeymint.so)
            "${PATCHELF}" --replace-needed "android.hardware.security.keymint-V1-ndk_platform.so" "android.hardware.security.keymint-V1-ndk.so" "${2}"
            "${PATCHELF}" --replace-needed "android.hardware.security.secureclock-V1-ndk_platform.so" "android.hardware.security.secureclock-V1-ndk.so" "${2}"
            "${PATCHELF}" --replace-needed "android.hardware.security.sharedsecret-V1-ndk_platform.so" "android.hardware.security.sharedsecret-V1-ndk.so" "${2}"
            "${PATCHELF}" --add-needed "android.hardware.security.rkp-V1-ndk.so" "${2}"
            ;;
        vendor/bin/hw/vendor.qti.hardware.display.composer-service)
            "${PATCHELF}" --remove-needed "libutils.so" "${2}"
            "${PATCHELF}" --add-needed "libutils-v32.so" "${2}"
            "${PATCHELF}" --add-needed "libutils-shim.so" "${2}"
            ;;
        vendor/etc/camera/*_motiontuning.xml)
            sed -i 's/xml=version/xml\ version/g' "${2}"
            ;;
        vendor/etc/camera/pureView_parameter.xml)
            sed -i "s/=\([0-9]\+\)>/=\"\1\">/g" "${2}"
            ;;
        vendor/etc/media_codecs_dolby_audio.xml)
            sed -i "/<MediaCodec name=\"c2\.dolby\.ac4\.decoder/,/<\/MediaCodec>/d" "${2}"
            sed -i "/software-codec/d" "${2}"
            ;;
        vendor/etc/media_codecs*.xml)
            sed -Ei "/media_codecs_(google_audio|google_c2|google_telephony|vendor_audio)/d" "${2}"
            sed -i "/media_codecs_with_dolby/d" "${2}"
            sed -i "/<MediaCodec name=\"c2\.dolby\./,/<\/MediaCodec>/d" "${2}"
            ;;
        vendor/lib64/libcamximageformatutils.so)
            "${PATCHELF}" --replace-needed "vendor.qti.hardware.display.config-V2-ndk_platform.so" "vendor.qti.hardware.display.config-V2-ndk.so" "${2}"
            ;;
        vendor/lib64/libTrueSight.so | vendor/lib64/libalLDC.so | vendor/lib64/libalhLDC.so)
            "${PATCHELF}" --clear-symbol-version "AHardwareBuffer_allocate" "${2}"
            "${PATCHELF}" --clear-symbol-version "AHardwareBuffer_describe" "${2}"
            "${PATCHELF}" --clear-symbol-version "AHardwareBuffer_lock" "${2}"
            "${PATCHELF}" --clear-symbol-version "AHardwareBuffer_lockPlanes" "${2}"
            "${PATCHELF}" --clear-symbol-version "AHardwareBuffer_release" "${2}"
            "${PATCHELF}" --clear-symbol-version "AHardwareBuffer_unlock" "${2}"
            ;;
        vendor/lib64/libgf_hal.so)
            sed -i "s/\[%s\] openat: %s xiaomi_sysfs_fd,failed:\[fingerdown\]/\[%s\] openat: xiaomi_sysfs_fd,failed:\[fingerdown\]\x00\x00\x00/g" "${2}"
            ;;
        vendor/lib64/libwvhidl.so)
            "${PATCHELF}" --add-needed "libcrypto_shim.so" "${2}"
            ;;
        vendor/lib64/vendor.libdpmframework.so)
            "${PATCHELF}" --add-needed "libhidlbase_shim.so" "${2}"
            ;;
        vendor/lib64/hw/com.qti.chi.override.so | vendor/lib64/libcamxcommonutils.so | vendor/lib64/libmialgoengine.so)
            "${PATCHELF}" --add-needed "libprocessgroup_shim.so" "$2"
            ;;
    esac
}

if [ -z "${ONLY_TARGET}" ]; then
    # Initialize the helper for common device
    setup_vendor "${DEVICE_COMMON}" "${VENDOR_COMMON:-$VENDOR}" "${ANDROID_ROOT}" true "${CLEAN_VENDOR}"

    extract "${MY_DIR}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"
fi

if [ -z "${ONLY_COMMON}" ] && [ -s "${MY_DIR}/../../${VENDOR}/${DEVICE}/proprietary-files.txt" ]; then
    # Reinitialize the helper for device
    source "${MY_DIR}/../../${VENDOR}/${DEVICE}/extract-files.sh"
    setup_vendor "${DEVICE}" "${VENDOR}" "${ANDROID_ROOT}" false "${CLEAN_VENDOR}"

    extract "${MY_DIR}/../../${VENDOR}/${DEVICE}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"
fi

"${MY_DIR}/setup-makefiles.sh"
