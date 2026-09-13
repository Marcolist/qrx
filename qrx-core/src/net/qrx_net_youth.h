#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define QRX_YOUTH_PROFILE_ADULT 0u
#define QRX_YOUTH_PROFILE_TEEN  1u
#define QRX_YOUTH_PROFILE_CHILD 2u

#define QRX_YOUTH_CAT_GENERAL   (1u<<0)
#define QRX_YOUTH_CAT_VIOLENCE  (1u<<1)
#define QRX_YOUTH_CAT_GAMBLING  (1u<<2)
#define QRX_YOUTH_CAT_ADULT     (1u<<3)
#define QRX_YOUTH_CAT_DRUGS     (1u<<4)
#define QRX_YOUTH_CAT_FINANCE   (1u<<5)
#define QRX_YOUTH_CAT_SOCIAL    (1u<<6)
#define QRX_YOUTH_CAT_UNKNOWN   (1u<<31)

#define QRX_YOUTH_CAP_DAPP          (1u<<0)
#define QRX_YOUTH_CAP_PAYMENT       (1u<<1)
#define QRX_YOUTH_CAP_SIGNATURE     (1u<<2)
#define QRX_YOUTH_CAP_SPONSORED_ADS (1u<<3)
#define QRX_YOUTH_CAP_VIEWER_REWARD (1u<<4)
#define QRX_YOUTH_CAP_CAMERA        (1u<<5)
#define QRX_YOUTH_CAP_MIC           (1u<<6)
#define QRX_YOUTH_CAP_LOCATION      (1u<<7)
#define QRX_YOUTH_CAP_CLIPBOARD     (1u<<8)

#define QRX_YOUTH_MAX_DOMAINS 32u
#define QRX_YOUTH_POLICY_VERSION 1u
#define QRX_YOUTH_PBKDF2_ITERS 200000u

typedef struct {
    uint32_t version;
    uint32_t profile;
    uint32_t max_age_rating;
    uint32_t blocked_categories;
    uint32_t allowed_capabilities;
    uint64_t payment_limit_atoms;
    uint32_t session_minutes;
    uint32_t unrated_blocked;
    uint32_t ads_blocked;
    uint32_t viewer_rewards_blocked;
    uint32_t allow_count;
    uint32_t block_count;
    char allow_domains[QRX_YOUTH_MAX_DOMAINS][254];
    char block_domains[QRX_YOUTH_MAX_DOMAINS][254];
} QrxYouthPolicy;

typedef struct {
    uint32_t min_age;
    uint32_t categories;
    uint32_t rated;
} QrxYouthContentRating;

typedef enum {
    QRX_YOUTH_ALLOW = 0,
    QRX_YOUTH_BLOCK_DOMAIN = 1,
    QRX_YOUTH_BLOCK_UNRATED = 2,
    QRX_YOUTH_BLOCK_AGE = 3,
    QRX_YOUTH_BLOCK_CATEGORY = 4,
    QRX_YOUTH_BLOCK_CAPABILITY = 5,
    QRX_YOUTH_BLOCK_PAYMENT_LIMIT = 6,
    QRX_YOUTH_BLOCK_ADS = 7,
    QRX_YOUTH_BLOCK_VIEWER_REWARD = 8
} QrxYouthDecision;

void qrx_youth_policy_default(uint32_t profile,QrxYouthPolicy *out);
int qrx_youth_policy_validate(const QrxYouthPolicy *p);
int qrx_youth_policy_add_allow(QrxYouthPolicy *p,const char *domain);
int qrx_youth_policy_add_block(QrxYouthPolicy *p,const char *domain);
QrxYouthDecision qrx_youth_evaluate_navigation(const QrxYouthPolicy *p,const char *domain,const QrxYouthContentRating *rating);
QrxYouthDecision qrx_youth_evaluate_capability(const QrxYouthPolicy *p,uint32_t capability,uint64_t payment_atoms);
int qrx_youth_policy_save_encrypted(const char *path,const QrxYouthPolicy *p,const char *guardian_pin);
int qrx_youth_policy_load_encrypted(const char *path,QrxYouthPolicy *out,const char *guardian_pin);
#ifdef __cplusplus
}
#endif
