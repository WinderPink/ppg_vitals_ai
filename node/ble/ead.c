// app_ead.c
#include "sl_common.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "../app.h"
#include "internal.h"
#include "ead.h"
#include "ble_log.h"
#include <stdio.h>
#include <string.h>
#include "sl_memory_manager.h"
#include "gatt_db.h"

uint8_t advertisement_buffer[BLE_EA_ADV_DATA_LEN];
char name[] = "Encrypted Advertiser";

sl_status_t initialize_and_store_key_material(sl_bt_ead_key_material_p key_material,
                                              psa_key_id_t *key_id)
{
  sl_status_t sc = SL_STATUS_FAIL;
  sl_bt_ead_session_key_t session_key;
  sl_bt_ead_iv_t initialization_vector;

  if (psa_generate_random(session_key, SL_BT_EAD_SESSION_KEY_SIZE) != PSA_SUCCESS
      || psa_generate_random(initialization_vector, SL_BT_EAD_IV_SIZE) != PSA_SUCCESS) {
    log_safe("[ERR] Failed to generate session key / IV\r\n");
    return sc;
  }

  memcpy(key_material->key, session_key, SL_BT_EAD_SESSION_KEY_SIZE);
  memcpy(key_material->iv, initialization_vector, SL_BT_EAD_IV_SIZE);

  char key_str[3 * SL_BT_EAD_SESSION_KEY_SIZE + 1];
  char iv_str[3 * SL_BT_EAD_IV_SIZE + 1];
  size_t pos = 0;

  for (uint8_t i = 0; i < SL_BT_EAD_SESSION_KEY_SIZE; i++) {
    pos += snprintf(key_str + pos, sizeof(key_str) - pos, "%02X:", key_material->key[i]);
  }
  if (pos > 0) key_str[pos - 1] = '\0';

  pos = 0;
  for (uint8_t i = 0; i < SL_BT_EAD_IV_SIZE; i++) {
    pos += snprintf(iv_str + pos, sizeof(iv_str) - pos, "%02X:", key_material->iv[i]);
  }
  if (pos > 0) iv_str[pos - 1] = '\0';

  log_safe("session key: %s\r\n", key_str);
  log_safe("initialization vector: %s\r\n", iv_str);

  sc = sl_bt_gatt_server_write_attribute_value(gattdb_Encrypted_Data_Key_Material,
                                               0,
                                               SL_BT_EAD_KEY_MATERIAL_SIZE,
                                               (uint8_t *)key_material);
  app_assert_status(sc);
  log_safe("[OK] Key material written to GATT attribute\r\n");

  sc = sl_bt_ead_store_key(PSA_KEY_USAGE_ENCRYPT,
                           PSA_KEY_LIFETIME_VOLATILE,
                           key_material,
                           key_id);
  app_assert_status(sc);
  log_safe("[OK] Key material stored via PSA crypto (key_id=0x%08lx)\r\n",
           (unsigned long)*key_id);

  return sc;
}

sl_status_t construct_advertisement_payload(sl_bt_ead_key_material_p key_material,
                                            sl_bt_ead_nonce_p nonce,
                                            uint8_t *index)
{
  sl_status_t sc = SL_STATUS_FAIL;
  *index = 0;
  health_data_t local_health_payload;

  if (g_health_payload_mutex != NULL) {
    if (xSemaphoreTake(g_health_payload_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      memcpy(&local_health_payload, &health_payload, sizeof(health_data_t));
      xSemaphoreGive(g_health_payload_mutex);
    } else {
      memset(&local_health_payload, 0, sizeof(health_data_t));
      log_safe("[WARN] health_payload mutex timeout\r\n");
    }
  } else {
    memcpy(&local_health_payload, &health_payload, sizeof(health_data_t));
  }

  // 1. Flags
  advertisement_buffer[(*index)++] = 0x02;
  advertisement_buffer[(*index)++] = 0x01;
  advertisement_buffer[(*index)++] = 0x06;

  // 2. Complete Local Name
  advertisement_buffer[(*index)++] = (uint8_t)(strlen(name) + 1);
  advertisement_buffer[(*index)++] = 0x09;
  memcpy(advertisement_buffer + *index, name, strlen(name));
  *index += (uint8_t)strlen(name);

  // 3. Encrypted health data
  static uint8_t health_data_buf[BLE_EA_ADV_DATA_LEN];
  size_t health_data_len = 2 + sizeof(health_data_t);
  sl_bt_ead_mic_t message_integrity_check;

  health_data_buf[0] = (uint8_t)(sizeof(health_data_t) + 1);
  health_data_buf[1] = 0x16;
  memcpy(health_data_buf + 2, &local_health_payload, sizeof(health_data_t));

  sc = sl_bt_ead_encrypt(key_material, nonce, health_data_len,
                         health_data_buf, message_integrity_check);
  app_assert_status(sc);

  sl_bt_ead_ad_structure_p encrypted_ad_structure =
      (sl_bt_ead_ad_structure_p)sl_malloc(sizeof(struct sl_bt_ead_ad_structure_s));
  app_assert(encrypted_ad_structure != NULL, "sl_malloc failed");

  uint8_t encrypted_data_length = BLE_EA_ADV_DATA_LEN;

  encrypted_ad_structure->length     = (uint8_t)health_data_len;
  encrypted_ad_structure->ad_type    = SL_BT_ENCRYPTED_DATA_AD_TYPE;
  encrypted_ad_structure->ad_data    = health_data_buf;
  encrypted_ad_structure->randomizer = &(nonce->randomizer);
  encrypted_ad_structure->mic        = &message_integrity_check;

  sc = sl_bt_ead_pack_ad_data(encrypted_ad_structure,
                              &encrypted_data_length,
                              advertisement_buffer + *index);
  app_assert_status(sc);

  sl_free(encrypted_ad_structure);
  (*index) += encrypted_data_length;

  return sc;
}
