#ifndef QRX_UPSCALER_AI_H
#define QRX_UPSCALER_AI_H
#include <stddef.h>
#include <stdint.h>
#define QRX_UPS_AI_PATH_MAX 1024
#define QRX_UPS_AI_ID_MAX 96
typedef struct {
    int runtime_installed;
    int runtime_verified;
    int model_x2_installed;
    int model_x4_installed;
    int model_x2_verified;
    int model_x4_verified;
    int ai_ready;
    char backend[48];
    char runtime_path[QRX_UPS_AI_PATH_MAX];
    char model_dir[QRX_UPS_AI_PATH_MAX];
    char model_x2_id[QRX_UPS_AI_ID_MAX];
    char model_x4_id[QRX_UPS_AI_ID_MAX];
    char status[320];
} QrxUpsAiStatus;
int qrx_ups_ai_status(QrxUpsAiStatus *out);
int qrx_ups_ai_run_image(const char *input,const char *output,int scale,unsigned tile,const char *model_id);
int qrx_ups_ai_verify_manifest(const char *manifest_path,char *err,size_t errcap);
#endif
