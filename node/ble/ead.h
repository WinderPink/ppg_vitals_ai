#ifndef APP_EAD_H
#define APP_EAD_H

#include "sl_bt_api.h"
#include "sl_bt_ead_core.h"
#include "psa/crypto.h"

sl_status_t initialize_and_store_key_material(sl_bt_ead_key_material_p key_material,
                                              psa_key_id_t *key_id);
sl_status_t construct_advertisement_payload(sl_bt_ead_key_material_p key_material,
                                            sl_bt_ead_nonce_p nonce,
                                            uint8_t *index);

#endif
