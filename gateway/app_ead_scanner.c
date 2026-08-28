/***************************************************************************//**
 * @file app_ead_scanner.c
 * @brief Cài đặt module quét BLE advertisement, bonding và giải mã EAD.
 *
 * Được tách ra từ app.c gốc (phần EAD Scanner, trước đây thuộc
 * app_encryption_advertising.c đã được gộp chung vào app.c). Toàn bộ logic
 * và các FIX giữ nguyên như bản gốc, chỉ thay đổi cách tổ chức file.
 *
 * ---------------------------------------------------------------------------
 * SỬA LỖI so với bản trước (xem comment "FIX:" bên dưới để tìm nhanh):
 *  1. FIX: Thiếu handler sl_bt_evt_sm_confirm_bonding_id. Với cấu hình
 *     MITM_REQUIRED + SC_ONLY + displayyesno, sau bước Numeric Comparison
 *     (confirm_passkey), stack còn gửi thêm 1 sự kiện riêng yêu cầu xác
 *     nhận TIẾP TỤC bonding (sl_bt_evt_sm_confirm_bonding_id). Nếu sự kiện
 *     này không được confirm bằng sl_bt_sm_bonding_confirm(), quá trình
 *     bonding sẽ treo vĩnh viễn -> đây là nguyên nhân chính khiến log dừng
 *     lại ở "connection handle 1" mà không bao giờ thấy "device bonded
 *     successfully".
 *  2. FIX: Nhánh CONFIRM_BTN (btn_count == 2) trước đây không hủy
 *     oneshot_btn_timer_handle đã khởi động ở lần bấm thứ nhất. Nếu không
 *     hủy, 2 giây sau timer cũ tự bắn ONESHOT_BTN_TIMER_CALLBACK, gọi
 *     sl_bt_sm_passkey_confirm() LẦN THỨ 2 cho phiên đã confirm rồi ->
 *     trả về SL_STATUS_INVALID_STATE (0x0002) -> app_assert_status() fail.
 *     Đã thêm sl_sleeptimer_stop_timer(&oneshot_btn_timer_handle) ở nhánh
 *     accept, cũng như ở sl_bt_evt_sm_bonded_id / sl_bt_evt_sm_bonding_failed_id
 *     để chặn triệt để mọi đường bắn timer trễ.
 *  3. FIX: extract_and_deycrypt() gọi oled_display_health_data() 2 lần liên
 *     tiếp (dư thừa) -> đã bỏ bớt 1 lần gọi.
 * ---------------------------------------------------------------------------
 ******************************************************************************/
#include "app_ead_scanner.h"

#include "sl_common.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_sleeptimer.h"
#include "sl_simple_button_instances.h"
#include "sl_bt_ead_core.h"
#include "psa/crypto_values.h"
#include "psa/crypto.h"

#include "app_oled.h"
#include "app_health_data.h"

#include <stdbool.h>
#include <string.h>

#define ADVERTISEMENT_READ_PERIOD_MS 5000
#define BTN_CONFIRM_PERIOD_MS        2000

#define CONFIRM_BTN                 0
#define ONESHOT_BTN_TIMER_CALLBACK  1
#define PERIODIC_TIMER_CALLBACK     2
#define CLOSE_CONN_TIMER_CALLBACK   3

// GATT Procedure States
#define SERVICE_DISCOVERY          1
#define CHARACTERISTIC_DISCOVERY   2
#define CHARACTERISTIC_READ        3

#define CONNECTION_HOLD_TIME_MS   10000 // Hold duration in milliseconds

// -----------------------------------------------------------------------------
// BLE / EAD globals (từ app_encryption_advertising.c)
// -----------------------------------------------------------------------------
sl_sleeptimer_timer_handle_t conn_close_timer_handle;
sl_sleeptimer_timer_handle_t periodic_timer_handle;
sl_sleeptimer_timer_handle_t oneshot_btn_timer_handle;

uint8_t btn_count = 0;

char remote_name[] = "Encrypted Advertiser";
// Gap service UUID
const uint8_t Gap_service_uuid[] = { 0x00, 0x18 };
// Key material characteristic UUID
const uint8_t key_material_char_uuid[] = { 0x88, 0x2b };

// -----------------------------------------------------------------------------
// Timer callbacks
// -----------------------------------------------------------------------------
void conn_close_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_bt_external_signal(CLOSE_CONN_TIMER_CALLBACK);
}

void oneshot_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_bt_external_signal(ONESHOT_BTN_TIMER_CALLBACK);
}

void periodic_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_bt_external_signal(PERIODIC_TIMER_CALLBACK);
}

void sl_button_on_change(const sl_button_t *handle)
{
  if (handle == &sl_button_btn0 && sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    sl_bt_external_signal(CONFIRM_BTN);
  }
}

// -----------------------------------------------------------------------------
// Advertisement parsing / decryption (logic giữ nguyên như bản gốc)
// -----------------------------------------------------------------------------
sl_status_t find_advertiser_by_local_name(sl_bt_evt_scanner_extended_advertisement_report_t *adv_report)
{
  sl_status_t sc = SL_STATUS_FAIL;
  uint8_t ad_len;
  uint8_t ad_type;
  uint8_t i = 0;
  while (i < adv_report->data.len) {
    ad_len = adv_report->data.data[i];
    ad_type = adv_report->data.data[i + 1];
    if (ad_type == 0x09) {
      if (memcmp(remote_name, &(adv_report->data.data[i + 2]), ad_len - 1) == 0) {
        return SL_STATUS_OK;
      }
    }
    i = i + ad_len + 1;
  }
  return sc;
}

sl_status_t extract_and_deycrypt(sl_bt_evt_scanner_extended_advertisement_report_t *adv_report,
                                 sl_bt_ead_key_material_p key_material,
                                 sl_bt_ead_nonce_p nonce)
{
  sl_status_t sc = SL_STATUS_FAIL;
  uint8_t ad_len;
  uint8_t ad_type;
  uint8_t i = 0;
  struct sl_bt_ead_ad_structure_s advertisement_info;
  sl_bt_ead_randomizer_t randomizer;
  uint8_t encrypted_data_buffer[30];
  sl_bt_ead_mic_t mic;

  app_log("--------------------------------------------------------------------------\n\r");
  app_log("Decrypting\r\n");

  advertisement_info.length = sizeof(encrypted_data_buffer);
  advertisement_info.randomizer = &randomizer;
  advertisement_info.ad_data = encrypted_data_buffer;
  advertisement_info.mic = &mic;

  while (i < adv_report->data.len) {
    ad_len = adv_report->data.data[i];
    ad_type = adv_report->data.data[i + 1];
    if (ad_type == SL_BT_ENCRYPTED_DATA_AD_TYPE) {
      app_log("secret information encrypted:\r\n");
      for (uint8_t j = 0; j < ad_len + 1; j++) {
        app_log("%02X", adv_report->data.data[i + j]);
      }
      sc = sl_bt_ead_unpack_ad_data(&adv_report->data.data[i], &advertisement_info);
      if (sc != 0) {
        app_log("unpacking unsuccessful with rc %08lX\r\n", sc);
      }
      memcpy(nonce->randomizer, advertisement_info.randomizer, SL_BT_EAD_RANDOMIZER_SIZE);
      sc = sl_bt_ead_decrypt(key_material, nonce, (uint8_t *)advertisement_info.mic, advertisement_info.length, advertisement_info.ad_data);
      if (sc != 0) {
        app_log("decrypting unsuccessful with rc %08lX\r\n", sc);
        oled_show_status("DECRYPT FAILED");
      } else {
        app_log("\r\nDecrypted Health Payload Received:\r\n");
        health_data_t *received_health = (health_data_t *)&advertisement_info.ad_data[2];
        app_log("  - Heart Rate:        %d bpm\r\n", received_health->heart_rate);
        app_log("  - Blood Pressure:    %d/%d mmHg\r\n", received_health->systolic, received_health->diastolic);
        app_log("  - Oxygen Saturation: %d%%\r\n", received_health->spo2);

        // Đẩy dữ liệu vừa giải mã lên màn hình OLED
        // FIX: trước đây gọi trùng 2 lần, đã bỏ bớt 1 lần
        oled_display_health_data(received_health);
      }
    }
    i = i + ad_len + 1;
  }
  return sc;
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
void app_ead_scanner_init(void)
{
  sl_status_t sc;

  // BLE / EAD periodic advertisement read timer
  sc = sl_sleeptimer_start_periodic_timer_ms(&periodic_timer_handle, ADVERTISEMENT_READ_PERIOD_MS, periodic_sleeptimer_callback, (void *)NULL, 0, 0);
  app_assert_status(sc);
}

/***************************************************************************//**
 * Bluetooth stack event handler.
 ******************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  static uint8_t connection_handle = SL_BT_INVALID_CONNECTION_HANDLE;
  static uint8_t key_need_update;
  static uint8_t pairing_state;
  static uint32_t gap_service_handle;
  static uint32_t key_material_char_handle;
  static uint8_t Gatt_procedure;
  static struct sl_bt_ead_key_material_s key_material;
  static struct sl_bt_ead_nonce_s nonce;
  static psa_key_id_t key_id = PSA_KEY_ID_NULL;
  static uint8_t decrypt_adv;

  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      pairing_state = 0;
      key_need_update = 1;
      decrypt_adv = 1;
      app_log("boot\r\n");

      sl_bt_sm_delete_bondings();

        // FIX: Tăng supervision timeout lên 10s (mặc định chỉ 1s — quá ngắn để
        // người dùng kịp đọc passkey và bấm nút xác nhận trên cả 2 board trong
        // lúc Numeric Comparison pairing đang diễn ra) -> tránh bị 0x1008
        // (Connection Timeout) giữa chừng pairing.
        sc = sl_bt_connection_set_default_parameters(
              40,    // min_interval = 40 x 1.25ms = 50ms
              80,    // max_interval = 80 x 1.25ms = 100ms
              0,     // latency = 0
              1000,  // timeout = 1000 x 10ms = 10000ms = 10 giây
              0,     // min_ce_length
              0xFFFF // max_ce_length
            );
        app_assert_status(sc);

      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);
      sc = sl_bt_sm_configure(SL_BT_SM_CONFIGURATION_MITM_REQUIRED | SL_BT_SM_CONFIGURATION_BONDING_REQUIRED | SL_BT_SM_CONFIGURATION_SC_ONLY | SL_BT_SM_CONFIGURATION_BONDING_REQUEST_REQUIRED, sl_bt_sm_io_capability_displayyesno);
      app_assert_status(sc);
      sc = sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_passive, 200, 200);
      app_assert_status(sc);
      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_observation);
      app_assert_status(sc);
      break;

    case sl_bt_evt_scanner_extended_advertisement_report_id:
      {
        sl_bt_evt_scanner_extended_advertisement_report_t *adv_report = &evt->data.evt_scanner_extended_advertisement_report;
        if (find_advertiser_by_local_name(adv_report) == 0) {
          if (key_need_update == 1) {
            sc = sl_bt_scanner_stop();
            app_assert_status(sc);
            sc = sl_bt_connection_open(adv_report->address,
                                       adv_report->address_type,
                                       adv_report->primary_phy,
                                       &connection_handle);
            app_assert_status(sc);
          } else {
            sc = extract_and_deycrypt(adv_report, &key_material, &nonce);
            if (sc != 0) {
              app_log("failed to decrypt the message, fetching new key\r\n");
              key_need_update = 1;
            }
          }
        }
      }
      break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      break;

    case sl_bt_evt_connection_opened_id:
      {
        sl_bt_evt_connection_opened_t connection_data = evt->data.evt_connection_opened;
        app_log("connection opened\r\n");
        connection_handle = connection_data.connection;
        if (connection_data.bonding == SL_BT_INVALID_BONDING_HANDLE) {
          sl_bt_sm_increase_security(connection_handle);
        } else {
          app_log("discovering services\r\n");
          Gatt_procedure = SERVICE_DISCOVERY;
          sc = sl_bt_gatt_discover_primary_services_by_uuid(connection_handle, sizeof(Gap_service_uuid), Gap_service_uuid);
          app_assert_status(sc);
        }
      }
      break;

    case sl_bt_evt_connection_closed_id:
      app_log("closed connection reason: 0x%4X\r\n", evt->data.evt_connection_closed.reason);
      connection_handle = SL_BT_INVALID_CONNECTION_HANDLE;
      // FIX-nhắc: hủy timer nút bấm nếu đang chạy dở, phòng khi kết nối
      // đóng đột ngột giữa lúc đang chờ người dùng bấm lần 2
      sl_sleeptimer_stop_timer(&oneshot_btn_timer_handle);
      pairing_state = 0;
      btn_count = 0;
      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_observation);
      app_assert_status(sc);
      break;

    case sl_bt_evt_connection_parameters_id:
      switch (evt->data.evt_connection_parameters.security_mode) {
        case sl_bt_connection_mode1_level1:
          app_log("No Security\r\n");
          break;
        case sl_bt_connection_mode1_level2:
          app_log("Unauthenticated pairing with encryption\r\n");
          break;
        case sl_bt_connection_mode1_level3:
          app_log("Authenticated pairing with encryption\r\n");
          break;
        case sl_bt_connection_mode1_level4:
          app_log("Authenticated Secure Connections pairing with encryption\r\n");
          break;
      }
      break;

    case sl_bt_evt_gatt_service_id:
      app_log("Service discovery using UUID: ");
      gap_service_handle = evt->data.evt_gatt_service.service;
      for (int i = 0; i < evt->data.evt_gatt_service.uuid.len; i++) {
        app_log("%02X", evt->data.evt_gatt_service.uuid.data[i]);
      }
      app_log("\r\nresulted in the handle: %08lX\r\n", gap_service_handle);
      break;

    case sl_bt_evt_gatt_characteristic_id:
      app_log("Characteristic discovery using UUID: ");
      key_material_char_handle = evt->data.evt_gatt_characteristic.characteristic;
      for (int i = 0; i < evt->data.evt_gatt_characteristic.uuid.len; i++) {
        app_log("%02X", evt->data.evt_gatt_characteristic.uuid.data[i]);
      }
      app_log("\r\nresulted in the handle: %04X \r\n", evt->data.evt_gatt_characteristic.characteristic);
      break;

    case sl_bt_evt_gatt_characteristic_value_id:
          memcpy(key_material.key, evt->data.evt_gatt_characteristic_value.value.data, SL_BT_EAD_SESSION_KEY_SIZE);
          memcpy(key_material.iv, evt->data.evt_gatt_characteristic_value.value.data + SL_BT_EAD_SESSION_KEY_SIZE, SL_BT_EAD_IV_SIZE);
          memcpy(nonce.iv, key_material.iv, SL_BT_EAD_IV_SIZE);

          app_log("key material: ");
          for (uint8_t i = 0; i < SL_BT_EAD_SESSION_KEY_SIZE; i++) {
            app_log("%02X:", key_material.key[i]);
          }
          app_log("\r\ninitialization vector: ");
          for (uint8_t i = 0; i < SL_BT_EAD_IV_SIZE; i++) {
            app_log("%02X:", key_material.iv[i]);
          }
          app_log("\r\n");

          sc = sl_bt_ead_store_key(PSA_KEY_USAGE_DECRYPT, PSA_KEY_LIFETIME_VOLATILE, &key_material, &key_id);
          app_assert_status(sc);

          key_need_update = 0;
          oled_show_status("KEY UPDATED");

          // Start a 10-second timer before disconnecting
          app_log("Key material saved. Holding connection open for 10 seconds...\r\n");
          sc = sl_sleeptimer_start_timer_ms(&conn_close_timer_handle,
                                            CONNECTION_HOLD_TIME_MS,
                                            conn_close_timer_callback,
                                            (void *)NULL,
                                            0,
                                            0);
          app_assert_status(sc);
          break;

    case sl_bt_evt_gatt_procedure_completed_id:
      app_log("Gatt procedure result:  0x%04X \r\n", evt->data.evt_gatt_procedure_completed.result);
      if (Gatt_procedure == SERVICE_DISCOVERY) {
        Gatt_procedure = CHARACTERISTIC_DISCOVERY;
        sc = sl_bt_gatt_discover_characteristics_by_uuid(connection_handle, gap_service_handle, sizeof(key_material_char_uuid), key_material_char_uuid);
      } else if (Gatt_procedure == CHARACTERISTIC_DISCOVERY) {
        Gatt_procedure = CHARACTERISTIC_READ;
        sl_bt_sm_increase_security(connection_handle);
        sl_bt_gatt_read_characteristic_value(connection_handle, key_material_char_handle);
      }
      break;

    case sl_bt_evt_sm_confirm_passkey_id:
      pairing_state = 1;
      app_log("The passkey is: %06li\r\n", evt->data.evt_sm_confirm_passkey.passkey);
      app_log("Please press btn0 once to refuse bonding or twice to accept bonding\n\r");
      // sl_bt_sm_passkey_confirm(evt->data.evt_sm_confirm_passkey.connection, 1);
      break;

    // FIX: Handler còn thiếu trong bản trước. Bắt buộc phải confirm sự kiện
    // này (khác với confirm_passkey) thì stack mới tiếp tục hoàn tất
    // bonding. Thiếu handler này là nguyên nhân chính khiến log dừng lại ở
    // "connection handle 1" và không bao giờ tới "device bonded
    // successfully".
    case sl_bt_evt_sm_confirm_bonding_id:
      app_log("New bonding request\r\n");
      sc = sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log("device bonded successfully\r\n");
      pairing_state = 0;
      btn_count = 0;
      // FIX: hủy timer nút bấm còn treo, tránh nó bắn confirm(0) trễ sau
      // khi phiên bonding đã hoàn tất thành công
      sl_sleeptimer_stop_timer(&oneshot_btn_timer_handle);
      sc = sl_bt_connection_close(connection_handle);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_bonding_failed_id:
      app_log("bonding failed, reason 0x%04X\r\n", evt->data.evt_sm_bonding_failed.reason);
      pairing_state = 0;
      btn_count = 0;
      // FIX: hủy timer nút bấm còn treo, tránh nó bắn confirm() trễ sau
      // khi phiên bonding đã kết thúc (thất bại)
      sl_sleeptimer_stop_timer(&oneshot_btn_timer_handle);
      sl_bt_sm_delete_bondings();
      sc = sl_bt_connection_close(connection_handle);
      app_assert_status(sc);
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals == CLOSE_CONN_TIMER_CALLBACK) {
        app_log("10-second hold elapsed. Closing connection now...\r\n");
        if (connection_handle != SL_BT_INVALID_CONNECTION_HANDLE) {
          sl_bt_connection_close(connection_handle);
        }
        break;
      }
      if (evt->data.evt_system_external_signal.extsignals == PERIODIC_TIMER_CALLBACK) {
        decrypt_adv = 1;
        break;
      }
      if (pairing_state == 0) {
        break;
      }
      if (evt->data.evt_system_external_signal.extsignals == ONESHOT_BTN_TIMER_CALLBACK) {
        btn_count = 0;
        sc = sl_bt_sm_passkey_confirm(connection_handle, 0);
        app_assert_status(sc);
        break;
      }
      if (evt->data.evt_system_external_signal.extsignals == CONFIRM_BTN) {
        btn_count++;
        if (btn_count == 2) {
          app_log("connection handle %d\r\n", connection_handle);

          // BẮT BUỘC: hủy timer 2s đang chờ, tránh nó bắn confirm(0) hoặc
          // cho phép confirm bị gọi lại lần 2 -> gây 0x0002
          sl_sleeptimer_stop_timer(&oneshot_btn_timer_handle);

          sc = sl_bt_sm_passkey_confirm(connection_handle, 1);   // <-- dùng connection_handle, KHÔNG dùng evt->data.evt_sm_confirm_passkey.connection
          app_assert_status(sc);
          btn_count = 0;
          pairing_state = 0;
        } else if (btn_count == 1) {
          sc = sl_sleeptimer_start_timer_ms(&oneshot_btn_timer_handle, BTN_CONFIRM_PERIOD_MS,
                                            oneshot_sleeptimer_callback, (void *)NULL, 0, 0);
          app_assert_status(sc);
        }
        break;
      }
      break;
  }
}
