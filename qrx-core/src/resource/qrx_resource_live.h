#pragma once
#include "resource/qrx_resource_dashboard.h"
#ifdef __cplusplus
extern "C" {
#endif
int qrx_resource_live_snapshot(const char *chain_dir,size_t privacy_min_providers,
                               QrxResourceDashboardSnapshot *out,
                               QrxStorageAtlasCell **cells_out,size_t *cell_count_out);
#ifdef __cplusplus
}
#endif
