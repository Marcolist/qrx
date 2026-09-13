#include "apps/qrx_upscaler_capabilities.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    QrxUpsCapabilities c;
    assert(!qrx_ups_capabilities_classify("Hardkernel ODROID-N2Plus","Linux","arm64",4ull<<30,1,1,&c));
    assert(c.hardware_profile==QRX_UPS_HW_ODROID_N2_PLUS);
    assert(c.ai_support==QRX_UPS_AI_EXPERIMENTAL);
    assert(c.ai_runtime_ready==1);
    assert(c.recommended_tile==128);
    assert(c.classical_available==1);
    assert(strstr(c.status,"experimental"));

    assert(!qrx_ups_capabilities_classify("Hardkernel ODROID-N2Plus","Linux","arm64",2ull<<30,1,0,&c));
    assert(c.hardware_profile==QRX_UPS_HW_ODROID_N2_PLUS);
    assert(c.ai_support==QRX_UPS_AI_EXPERIMENTAL);
    assert(c.ai_runtime_ready==0);
    assert(c.recommended_tile==64);

    assert(!qrx_ups_capabilities_classify("Raspberry Pi 5 Model B Rev 1.0","Linux","arm64",8ull<<30,1,1,&c));
    assert(c.hardware_profile==QRX_UPS_HW_RASPBERRY_PI5);
    assert(c.ai_support==QRX_UPS_AI_SUPPORTED);
    assert(c.ai_runtime_ready==1);
    assert(c.recommended_tile==256);

    assert(!qrx_ups_capabilities_classify("Apple M1","macOS","arm64",8ull<<30,1,0,&c));
    assert(c.hardware_profile==QRX_UPS_HW_APPLE_SILICON);
    assert(!strcmp(c.apple_silicon_generation,"M1"));
    assert(!strcmp(c.apple_silicon_variant,"base"));
    assert(c.ai_support==QRX_UPS_AI_SUPPORTED);
    assert(c.native_metal_present==1 && c.metal_version_major==4);
    assert(!strcmp(c.vulkan_mode,"moltenvk"));
    assert(c.ai_runtime_ready==1);
    assert(c.recommended_tile==512);

    assert(!qrx_ups_capabilities_classify("Apple M2 Pro","macOS","arm64",16ull<<30,1,0,&c));
    assert(c.hardware_profile==QRX_UPS_HW_APPLE_SILICON);
    assert(!strcmp(c.apple_silicon_generation,"M2"));
    assert(!strcmp(c.apple_silicon_variant,"Pro"));
    assert(c.native_metal_present==1 && c.recommended_threads==6);

    assert(!qrx_ups_capabilities_classify("Apple M3 Max","macOS","arm64",36ull<<30,1,0,&c));
    assert(c.hardware_profile==QRX_UPS_HW_APPLE_SILICON);
    assert(!strcmp(c.apple_silicon_generation,"M3"));
    assert(!strcmp(c.apple_silicon_variant,"Max"));
    assert(c.native_metal_present==1 && c.recommended_threads==8);

    assert(!qrx_ups_capabilities_classify("Apple M4","macOS","arm64",16ull<<30,1,0,&c));
    assert(c.hardware_profile==QRX_UPS_HW_APPLE_SILICON);
    assert(!strcmp(c.apple_silicon_generation,"M4"));
    assert(!strcmp(c.apple_silicon_variant,"base"));
    assert(c.native_metal_present==1 && c.metal_version_major==4);

    puts("PASS: unified Apple Silicon profile preserves M-generation/variant tuning metadata; Pi5 supported and ODROID-N2+ experimental profiles select safe automatic tiles and explicit Vulkan mode");
    return 0;
}
