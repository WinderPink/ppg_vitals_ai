// app_bt_event.c - Bluetooth stack event handler
#include "sl_bt_api.h"
#include "app_assert.h"
#include "../app.h"
#include "internal.h"
#include "ead.h"
#include "ble_log.h"
#include "sl_sleeptimer.h"

static struct sl_bt_ead_key_material_s key_material;
static struct sl_bt_ead_nonce_s nonce;
static psa_key_id_t key_id = PSA_KEY_ID_NULL;
static uint8_t index;
static uint8_t connection_handle = SL_BT_INVALID_CONNECTION_HANDLE;
static uint8_t pairing_state = 0;
static bd_addr random_address;
uint8_t advertising_set_handle; 

static void handle_external_signal(uint32_t signals)
{
  sl_status_t sc;

  // ---- Periodic address rotation ----
  if ((signals & (1 << PERIODIC_TIMER_CALLBACK)) || signals == PERIODIC_TIMER_CALLBACK) {
    sl_bt_advertiser_stop(advertising_set_handle);

    sc = sl_bt_advertiser_set_random_address(advertising_set_handle,
                                             sl_bt_gap_random_resolvable_address,
                                             (bd_addr){ 0 },
                                             &random_address);
    app_assert_status(sc);

    sl_bt_ead_randomizer_update(&nonce);
    construct_advertisement_payload(&key_material, &nonce, &index);
    sl_bt_extended_advertiser_set_data(advertising_set_handle, index, advertisement_buffer);

    sc = sl_bt_extended_advertiser_start(advertising_set_handle,
                                         sl_bt_extended_advertiser_connectable,
                                         0);
    app_assert_status(sc);
    return;
  }

  // ---- Payload refresh ----
  if ((signals & (1 << PAYLOAD_UPDATE_TIMER_CALLBACK)) || signals == PAYLOAD_UPDATE_TIMER_CALLBACK) {
    construct_advertisement_payload(&key_material, &nonce, &index);
    sl_bt_extended_advertiser_set_data(advertising_set_handle, index, advertisement_buffer);
    return;
  }

  if (pairing_state == 0) {
    return;
  }

  // ---- Oneshot timer timeout ----
  if (signals == ONESHOT_BTN_TIMER_CALLBACK) {
    btn_count = 0;
    if (connection_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
      sl_bt_sm_passkey_confirm(connection_handle, 0);
    }
    return;
  }

  // ---- Button press ----
  if (signals == CONFIRM_BTN) {
    btn_count++;
    log_safe("btn_count = %d\r\n", btn_count);

    if (btn_count >= 2) {
      if (connection_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
        sc = sl_bt_sm_passkey_confirm(connection_handle, 1);
        app_assert_status(sc);
        log_safe("[OK] Bonding accepted by user\r\n");
      }
      btn_count = 0;
      sl_sleeptimer_stop_timer(&oneshot_btn_timer_handle);
    } else if (btn_count == 1) {
      sc = sl_sleeptimer_start_timer_ms(&oneshot_btn_timer_handle,
                                        BTN_CONFIRM_PERIOD_MS,
                                        oneshot_sleeptimer_callback,
                                        (void *)NULL, 0, 0);
      app_assert_status(sc);
    }
  }
}

void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {

    case sl_bt_evt_system_boot_id:
      pairing_state = 0;
      log_safe("boot\r\n");

      sl_bt_sm_delete_bondings();
      log_safe("[OK] Old bondings deleted\r\n");

      sc = sl_sleeptimer_start_periodic_timer_ms(&payload_timer_handle,
                                                 PAYLOAD_UPDATE_PERIOD_MS,
                                                 payload_sleeptimer_callback,
                                                 (void *)NULL, 0, 0);
      app_assert_status(sc);
      log_safe("[OK] Payload-update timer started (%d ms)\r\n", PAYLOAD_UPDATE_PERIOD_MS);

      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);

      sc = sl_bt_sm_configure(SL_BT_SM_CONFIGURATION_MITM_REQUIRED
                              | SL_BT_SM_CONFIGURATION_BONDING_REQUIRED
                              | SL_BT_SM_CONFIGURATION_SC_ONLY
                              | SL_BT_SM_CONFIGURATION_BONDING_REQUEST_REQUIRED,
                              sl_bt_sm_io_capability_displayyesno);
      app_assert_status(sc);
      log_safe("[OK] Security manager configured (MITM + SC-only bonding)\r\n");

      initialize_and_store_key_material(&key_material, &key_id);
      sl_bt_ead_session_init(&key_material, NULL, &nonce);

      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      sc = sl_bt_advertiser_set_random_address(advertising_set_handle,
                                               sl_bt_gap_random_resolvable_address,
                                               (bd_addr){ 0 },
                                               &random_address);
      app_assert_status(sc);

      construct_advertisement_payload(&key_material, &nonce, &index);

      sc = sl_bt_extended_advertiser_set_data(advertising_set_handle, index, advertisement_buffer);
      app_assert_status(sc);

      sc = sl_bt_advertiser_set_timing(advertising_set_handle, 160, 160, 0, 0);
      app_assert_status(sc);

      sc = sl_bt_extended_advertiser_start(advertising_set_handle,
                                           sl_bt_extended_advertiser_connectable,
                                           0);
      app_assert_status(sc);
      log_safe("started advertisement\r\n");
      break;

    case sl_bt_evt_connection_opened_id:
      log_safe("connection opened (handle=%d)\r\n",
               evt->data.evt_connection_opened.connection);
      sl_sleeptimer_stop_timer(&periodic_timer_handle);
      connection_handle = evt->data.evt_connection_opened.connection;
      break;

    case sl_bt_evt_connection_parameters_id:
      switch (evt->data.evt_connection_parameters.security_mode) {
        case sl_bt_connection_mode1_level1: log_safe("No Security\r\n"); break;
        case sl_bt_connection_mode1_level2: log_safe("Unauthenticated pairing with encryption\r\n"); break;
        case sl_bt_connection_mode1_level3: log_safe("Authenticated pairing with encryption\r\n"); break;
        case sl_bt_connection_mode1_level4: log_safe("Authenticated Secure Connections pairing with encryption\r\n"); break;
        default: log_safe("Unknown security mode\r\n"); break;
      }
      break;

    case sl_bt_evt_connection_closed_id:
      log_safe("closed connection reason: 0x%04X\r\n",
               evt->data.evt_connection_closed.reason);
      connection_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      pairing_state = 0;
      btn_count = 0;

      sc = sl_sleeptimer_start_periodic_timer_ms(&periodic_timer_handle,
                                                 ADDRESS_CHANGE_PERIOD_MS,
                                                 periodic_sleeptimer_callback,
                                                 (void *)NULL, 0, 0);
      app_assert_status(sc);

      sc = sl_bt_extended_advertiser_start(advertising_set_handle,
                                           sl_bt_extended_advertiser_connectable,
                                           0);
      app_assert_status(sc);
      log_safe("[OK] Re-started advertisement after disconnect\r\n");
      break;

    case sl_bt_evt_sm_confirm_passkey_id:
      pairing_state = 1;
      log_safe("The passkey is: %06lu\r\n",
               (unsigned long)evt->data.evt_sm_confirm_passkey.passkey);

#if AUTO_ACCEPT_PASSKEY
      if (connection_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
        sc = sl_bt_sm_passkey_confirm(connection_handle, 1);
        app_assert_status(sc);
        log_safe("[DEBUG] Auto-accepted passkey\r\n");
      }
#else
      log_safe("Please press btn0 twice within %d ms to accept bonding\r\n",
               BTN_CONFIRM_PERIOD_MS);
#endif
      break;

    case sl_bt_evt_sm_confirm_bonding_id:
      log_safe("New bonding request\r\n");
      sc = sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_bonded_id:
      log_safe("[OK] device bonded successfully\r\n");
      pairing_state = 0;
      btn_count = 0;
      break;

    case sl_bt_evt_sm_bonding_failed_id:
      log_safe("[ERR] bonding failed, reason 0x%04X\r\n",
               evt->data.evt_sm_bonding_failed.reason);
      pairing_state = 0;
      btn_count = 0;
      sl_bt_sm_delete_bondings();
      break;

    case sl_bt_evt_system_external_signal_id:
      handle_external_signal(evt->data.evt_system_external_signal.extsignals);
      break;

    default:
      break;
  }
}
