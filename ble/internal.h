#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "sl_bt_api.h"
#include "sl_bt_ead_core.h"
#include "sl_sleeptimer.h"
#include "psa/crypto.h"
#include "../app.h"

#define BLE_EA_ADV_DATA_LEN           0xBF
#define ADDRESS_CHANGE_PERIOD_MS      20000
#define BTN_CONFIRM_PERIOD_MS         2000
#define PAYLOAD_UPDATE_PERIOD_MS      3000

#define CONFIRM_BTN                   0
#define ONESHOT_BTN_TIMER_CALLBACK    1
#define PERIODIC_TIMER_CALLBACK       2
#define PAYLOAD_UPDATE_TIMER_CALLBACK 3

#define AUTO_ACCEPT_PASSKEY           1

extern uint8_t btn_count;
extern uint8_t advertisement_buffer[BLE_EA_ADV_DATA_LEN];
extern char name[];
extern uint8_t advertising_set_handle;  

/* Timers */
extern sl_sleeptimer_timer_handle_t periodic_timer_handle;
extern sl_sleeptimer_timer_handle_t oneshot_btn_timer_handle;
extern sl_sleeptimer_timer_handle_t payload_timer_handle;

void periodic_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
void oneshot_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
void payload_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);


/* Logging */
void log_safe(const char *fmt, ...);

/* EAD */
sl_status_t initialize_and_store_key_material(sl_bt_ead_key_material_p key_material,
                                              psa_key_id_t *key_id);
sl_status_t construct_advertisement_payload(sl_bt_ead_key_material_p key_material,
                                            sl_bt_ead_nonce_p nonce,
                                            uint8_t *index);

#endif // APP_INTERNAL_H
