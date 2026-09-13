/*
 * QRX Upscaler - host capability and hardware profile detection.
 * SPDX-License-Identifier: MIT
 */
#ifndef QRX_UPSCALER_CAPABILITIES_H
#define QRX_UPSCALER_CAPABILITIES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QRX_UPS_HW_GENERIC = 0,
    /* One compatibility profile for every Apple Silicon generation.
     * Generation/variant are metadata used only for automatic tuning. */
    QRX_UPS_HW_APPLE_SILICON = 1,
    /* Keep stable numeric values for existing serialized/ABI consumers. */
    QRX_UPS_HW_RASPBERRY_PI5 = 6,
    QRX_UPS_HW_ODROID_N2_PLUS = 7
} QrxUpsHardwareProfile;

typedef enum {
    QRX_UPS_AI_UNAVAILABLE = 0,
    QRX_UPS_AI_SUPPORTED = 1,
    QRX_UPS_AI_EXPERIMENTAL = 2
} QrxUpsAiSupport;

typedef struct {
    uint32_t version;
    QrxUpsHardwareProfile hardware_profile;
    QrxUpsAiSupport ai_support;
    uint64_t memory_bytes;
    uint32_t recommended_tile;
    uint32_t recommended_threads;
    uint8_t vulkan_loader_present;
    uint8_t render_device_present;
    uint8_t native_metal_present;
    uint8_t metal_version_major;
    uint8_t ai_runtime_ready;       /* host Vulkan path ready; model/runner is validated separately */
    uint8_t classical_available;
    char vulkan_mode[32];           /* native, moltenvk, unavailable */
    char accelerator[64];           /* Metal / Vulkan / CPU summary */
    char os[32];
    char arch[32];
    char device_model[160];
    char apple_silicon_generation[16]; /* e.g. M1, M2, M3, M4; metadata only */
    char apple_silicon_variant[24];    /* base, Pro, Max, Ultra, unknown */
    char status[256];
} QrxUpsCapabilities;

/* Pure classifier used by tests and by the live host probe. */
int qrx_ups_capabilities_classify(const char *device_model,
                                  const char *os,
                                  const char *arch,
                                  uint64_t memory_bytes,
                                  int vulkan_loader_present,
                                  int render_device_present,
                                  QrxUpsCapabilities *out);

/* Probe the current host without requiring Vulkan headers or a Vulkan SDK. */
int qrx_ups_capabilities_probe(QrxUpsCapabilities *out);

const char *qrx_ups_hardware_profile_name(QrxUpsHardwareProfile p);
const char *qrx_ups_ai_support_name(QrxUpsAiSupport s);

#ifdef __cplusplus
}
#endif
#endif
