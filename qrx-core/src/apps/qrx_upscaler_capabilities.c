/*
 * QRX Upscaler - host capability and hardware profile detection.
 * SPDX-License-Identifier: MIT
 */
#include "apps/qrx_upscaler_capabilities.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int contains_ci(const char *s,const char *needle){
    if(!s||!needle||!*needle)return 0;
    size_t n=strlen(needle);
    for(;*s;s++){
        size_t i=0;
        while(i<n&&s[i]){
            char a=s[i],b=needle[i];
            if(a>='A'&&a<='Z')a=(char)(a-'A'+'a');
            if(b>='A'&&b<='Z')b=(char)(b-'A'+'a');
            if(a!=b)break;
            i++;
        }
        if(i==n)return 1;
    }
    return 0;
}

const char *qrx_ups_hardware_profile_name(QrxUpsHardwareProfile p){
    switch(p){
        case QRX_UPS_HW_APPLE_SILICON:return "apple-silicon";
        case QRX_UPS_HW_RASPBERRY_PI5:return "raspberry-pi-5";
        case QRX_UPS_HW_ODROID_N2_PLUS:return "odroid-n2-plus";
        default:return "generic";
    }
}
const char *qrx_ups_ai_support_name(QrxUpsAiSupport s){
    switch(s){case QRX_UPS_AI_SUPPORTED:return "supported";case QRX_UPS_AI_EXPERIMENTAL:return "experimental";default:return "unavailable";}
}

static uint32_t tile_for_memory(uint64_t bytes,uint32_t low,uint32_t mid,uint32_t high){
    const uint64_t gib=1024ull*1024ull*1024ull;
    if(bytes>=8*gib)return high;
    if(bytes>=4*gib)return mid;
    return low;
}

static void apple_silicon_metadata(const char *model,QrxUpsCapabilities *out){
    const char *gen="unknown",*variant="unknown";
    if(!out)return;
    if(contains_ci(model,"m1"))gen="M1";
    else if(contains_ci(model,"m2"))gen="M2";
    else if(contains_ci(model,"m3"))gen="M3";
    else if(contains_ci(model,"m4"))gen="M4";
    else if(contains_ci(model,"m5"))gen="M5";
    else if(contains_ci(model,"m6"))gen="M6";
    if(contains_ci(model,"ultra"))variant="Ultra";
    else if(contains_ci(model,"max"))variant="Max";
    else if(contains_ci(model,"pro"))variant="Pro";
    else if(strcmp(gen,"unknown")!=0)variant="base";
    snprintf(out->apple_silicon_generation,sizeof(out->apple_silicon_generation),"%s",gen);
    snprintf(out->apple_silicon_variant,sizeof(out->apple_silicon_variant),"%s",variant);
}

static uint32_t apple_threads_hint(const QrxUpsCapabilities *out){
    if(!out)return 4;
    /* Generation is only a tuning hint. RAM/runtime probes remain authoritative. */
    if(!strcmp(out->apple_silicon_generation,"M1"))return 4;
    if(!strcmp(out->apple_silicon_generation,"M2"))return 6;
    if(!strcmp(out->apple_silicon_generation,"M3")||!strcmp(out->apple_silicon_generation,"M4")||
       !strcmp(out->apple_silicon_generation,"M5")||!strcmp(out->apple_silicon_generation,"M6"))return 8;
    return 4;
}

int qrx_ups_capabilities_classify(const char *device_model,const char *os,const char *arch,
                                  uint64_t memory_bytes,int vulkan_loader_present,
                                  int render_device_present,QrxUpsCapabilities *out){
    if(!out)return -1;
    memset(out,0,sizeof(*out));out->version=1;out->memory_bytes=memory_bytes;out->classical_available=1;
    snprintf(out->device_model,sizeof(out->device_model),"%s",device_model&&*device_model?device_model:"Unknown device");
    snprintf(out->os,sizeof(out->os),"%s",os&&*os?os:"unknown");
    snprintf(out->arch,sizeof(out->arch),"%s",arch&&*arch?arch:"unknown");
    out->vulkan_loader_present=vulkan_loader_present?1:0;out->render_device_present=render_device_present?1:0;
    out->hardware_profile=QRX_UPS_HW_GENERIC;out->recommended_tile=256;out->recommended_threads=2;
    snprintf(out->vulkan_mode,sizeof(out->vulkan_mode),"%s",vulkan_loader_present?(render_device_present?"native":"loader-only"):"unavailable");
    snprintf(out->accelerator,sizeof(out->accelerator),"CPU / Classical");

    if(contains_ci(out->device_model,"odroid-n2")||contains_ci(out->device_model,"odroid n2")){
        out->hardware_profile=QRX_UPS_HW_ODROID_N2_PLUS;
        out->ai_support=QRX_UPS_AI_EXPERIMENTAL;
        out->recommended_tile=tile_for_memory(memory_bytes,64,128,256);
        out->recommended_threads=2;
        out->ai_runtime_ready=(out->vulkan_loader_present&&out->render_device_present)?1:0;
        snprintf(out->accelerator,sizeof(out->accelerator),"Mali-G52 / PanVK");
        snprintf(out->status,sizeof(out->status),out->ai_runtime_ready?
                 "Mali-G52 / PanVK detected: AI Vulkan path is experimental; classical fallback remains available.":
                 "ODROID-N2+ detected: Mali-G52 is Vulkan-capable, but a usable PanVK/Vulkan render device was not found; using classical fallback.");
        return 0;
    }
    if(contains_ci(out->device_model,"raspberry pi 5")){
        out->hardware_profile=QRX_UPS_HW_RASPBERRY_PI5;
        out->ai_support=QRX_UPS_AI_SUPPORTED;
        out->recommended_tile=tile_for_memory(memory_bytes,64,128,256);
        out->recommended_threads=3;
        out->ai_runtime_ready=(out->vulkan_loader_present&&out->render_device_present)?1:0;
        snprintf(out->accelerator,sizeof(out->accelerator),"VideoCore VII / V3DV Vulkan");
        snprintf(out->status,sizeof(out->status),out->ai_runtime_ready?
                 "Raspberry Pi 5 Vulkan path ready; low-memory tiling enabled.":
                 "Raspberry Pi 5 detected, but Vulkan runtime/render device is unavailable; using classical fallback.");
        return 0;
    }
    if((contains_ci(out->os,"mac")||contains_ci(out->os,"darwin"))&&
       (contains_ci(out->arch,"arm64")||contains_ci(out->arch,"aarch64"))){
        out->hardware_profile=QRX_UPS_HW_APPLE_SILICON;
        out->ai_support=QRX_UPS_AI_SUPPORTED;
        out->native_metal_present=1;
        out->metal_version_major=4; /* Highest QRX Metal target; runtime probing remains authoritative. */
        out->recommended_tile=tile_for_memory(memory_bytes,128,256,512);
        apple_silicon_metadata(out->device_model,out);
        out->recommended_threads=apple_threads_hint(out);
        snprintf(out->accelerator,sizeof(out->accelerator),"Apple GPU / Metal");
        snprintf(out->vulkan_mode,sizeof(out->vulkan_mode),"%s",out->vulkan_loader_present?"moltenvk":"unavailable");
        /* macOS does not expose native Vulkan. QRX uses Vulkan through MoltenVK for the NCNN path. */
        out->ai_runtime_ready=out->vulkan_loader_present?1:0;
        snprintf(out->status,sizeof(out->status),out->ai_runtime_ready?
                 "Apple Silicon profile ready; generation is metadata only. NCNN uses Vulkan through MoltenVK, while native Metal remains available for QRX Metal backends.":
                 "Apple Silicon profile ready, but the MoltenVK/Vulkan runtime is unavailable; using classical fallback until the AI runtime is present.");
        return 0;
    }

    if(out->vulkan_loader_present&&out->render_device_present){
        out->ai_support=QRX_UPS_AI_SUPPORTED;out->ai_runtime_ready=1;
        out->recommended_tile=tile_for_memory(memory_bytes,128,256,512);
        snprintf(out->accelerator,sizeof(out->accelerator),"Vulkan GPU");
        snprintf(out->status,sizeof(out->status),"Generic native Vulkan device available; AI runtime may be used after model/runtime validation.");
    }else{
        out->ai_support=QRX_UPS_AI_UNAVAILABLE;out->ai_runtime_ready=0;
        snprintf(out->status,sizeof(out->status),"No usable Vulkan AI path detected; classical upscaling remains available.");
    }
    return 0;
}

static uint64_t host_memory(void){
#if defined(__linux__)
    struct sysinfo si;if(sysinfo(&si)==0)return (uint64_t)si.totalram*(uint64_t)si.mem_unit;
#elif defined(__APPLE__)
    uint64_t n=0;size_t z=sizeof(n);if(sysctlbyname("hw.memsize",&n,&z,NULL,0)==0)return n;
#elif defined(_WIN32)
    MEMORYSTATUSEX s;memset(&s,0,sizeof(s));s.dwLength=sizeof(s);if(GlobalMemoryStatusEx(&s))return (uint64_t)s.ullTotalPhys;
#endif
    return 0;
}
static void host_model(char *out,size_t cap){
    if(!out||!cap)return;out[0]=0;
#if defined(__linux__)
    const char*paths[]={"/proc/device-tree/model","/sys/firmware/devicetree/base/model"};
    for(size_t i=0;i<sizeof(paths)/sizeof(paths[0]);i++){FILE*f=fopen(paths[i],"rb");if(!f)continue;size_t n=fread(out,1,cap-1,f);fclose(f);if(n){while(n&&((unsigned char)out[n-1]==0||out[n-1]=='\n'||out[n-1]=='\r'))n--;out[n]=0;return;}}
#elif defined(__APPLE__)
    size_t z=cap;if(sysctlbyname("machdep.cpu.brand_string",out,&z,NULL,0)==0&&out[0]){out[cap-1]=0;return;}
    z=cap;if(sysctlbyname("hw.model",out,&z,NULL,0)==0){out[cap-1]=0;return;}
#elif defined(_WIN32)
    snprintf(out,cap,"Windows PC");return;
#endif
    snprintf(out,cap,"Unknown device");
}
static int have_vulkan_loader(void){
#if defined(__linux__)
    void*h=dlopen("libvulkan.so.1",RTLD_LAZY|RTLD_LOCAL);if(!h)h=dlopen("libvulkan.so",RTLD_LAZY|RTLD_LOCAL);if(h){dlclose(h);return 1;}return 0;
#elif defined(__APPLE__)
    const char*libs[]={"libvulkan.1.dylib","libvulkan.dylib","libMoltenVK.dylib","/usr/local/lib/libMoltenVK.dylib","/opt/homebrew/lib/libMoltenVK.dylib"};
    for(size_t i=0;i<sizeof(libs)/sizeof(libs[0]);i++){void*h=dlopen(libs[i],RTLD_LAZY|RTLD_LOCAL);if(h){dlclose(h);return 1;}}return 0;
#elif defined(_WIN32)
    HMODULE h=LoadLibraryA("vulkan-1.dll");if(h){FreeLibrary(h);return 1;}return 0;
#else
    return 0;
#endif
}
static int have_render_device(void){
#if defined(__linux__)
    char p[64];for(int i=128;i<144;i++){snprintf(p,sizeof(p),"/dev/dri/renderD%d",i);if(access(p,R_OK|W_OK)==0||access(p,R_OK)==0)return 1;}return 0;
#elif defined(__APPLE__)
    return 1;
#elif defined(_WIN32)
    return 1;
#else
    return 0;
#endif
}
int qrx_ups_capabilities_probe(QrxUpsCapabilities *out){
    char model[160];host_model(model,sizeof(model));
#if defined(__APPLE__)
    const char*os="macOS";
#elif defined(_WIN32)
    const char*os="Windows";
#elif defined(__linux__)
    const char*os="Linux";
#else
    const char*os="Unknown";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    const char*arch="arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    const char*arch="x86_64";
#elif defined(__arm__) || defined(_M_ARM)
    const char*arch="arm";
#else
    const char*arch="unknown";
#endif
    return qrx_ups_capabilities_classify(model,os,arch,host_memory(),have_vulkan_loader(),have_render_device(),out);
}
