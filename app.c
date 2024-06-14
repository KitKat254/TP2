/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "sl_status.h"
#include "gatt_db.h"
#include "sl_bt_api.h"
#include "sl_sleeptimer.h"
#include "temperature.h"
#include "sl_simple_led_instances.h"

#define TEMPERATURE_TIMER_SIGNAL (1<<0)


uint8_t connection;
uint16_t characteristic;

int temperature;
int ev_temp;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
sl_sleeptimer_timer_handle_t handle;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  sl_sensor_rht_init();
  app_log_info("reset%s\n", __FUNCTION__);
  sl_simple_led_init_instances();
}
/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);
      sl_bt_evt_gatt_server_user_read_request_id;

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("nouvelle connexion%s\n", __FUNCTION__);

      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      app_log_info("fin connexion%s\n", __FUNCTION__);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      sl_sensor_rht_deinit();
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_gatt_server_characteristic_status_id:
      switch (evt->data.evt_gatt_server_characteristic_status.status_flags) {
                       case sl_bt_gatt_server_client_config:
                         if (evt->data.evt_gatt_server_characteristic_status.client_config_flags & 0x1) {
                           app_log_info("Cas ou on active les notifs\n");
                           app_log_info("Notify %s\n", __FUNCTION__);
                           app_log_info("Client Config Flags: 0x%02x\n", evt->data.evt_gatt_server_characteristic_status.client_config_flags);
                           sl_sleeptimer_start_periodic_timer_ms(&handle, 1000, callback, NULL, 0, 0);

                           connection = evt-> data.evt_gatt_server_characteristic_status.connection;
                           characteristic=evt->data.evt_gatt_server_characteristic_status.characteristic;

                           sl_led_led0.turn_on(sl_led_led0.context);
                         }

                         else if(evt->data.evt_gatt_server_characteristic_status.client_config_flags & 0x2) {}
                         else {
                           app_log_info("Cas ou desactive les notifs\n");
                           sl_sleeptimer_stop_timer(&handle);
                           sl_led_led0.turn_off(sl_led_led0.context);
                         }
                           break;

                           default:
                               app_log_info("%s: default\n", __FUNCTION__);
                               break;
             }
             break;
             case sl_bt_evt_system_external_signal_id :
               switch(evt->data.evt_system_external_signal.extsignals) {
                 case TEMPERATURE_TIMER_SIGNAL :
                   temperature = temp();
                   sl_bt_gatt_server_send_notification(connection, characteristic, sizeof(int), &temperature);
                   break;
               }
               break;

               case sl_bt_evt_gatt_server_user_write_request_id :
                 app_log_info("%d \n",evt->data.evt_gatt_server_user_write_request.att_opcode);
                 sl_bt_gatt_server_send_user_write_response(evt->data.evt_gatt_server_user_write_request.connection,
                                                            evt->data.evt_gatt_server_user_write_request.characteristic,0);
                 break;

     case sl_bt_evt_gatt_server_user_read_request_id:
      switch(evt->data.evt_gatt_server_user_read_request.characteristic){
        case gattdb_temperature_0:
          uint16_t len;
          int temperature = temp()/100;
          int ev_temp = temp();
          app_log_info("%s: %dC \n", __FUNCTION__,temperature);
          sl_bt_gatt_server_send_user_read_response(evt->data.handle,
                                                    evt->data.evt_gatt_server_user_read_request.characteristic,
                                                    0,
                                                    sizeof (ev_temp),
                                                    (uint8_t*)&ev_temp,
                                                    (uint16_t*) &len);
       break;
        default:
              app_log_info("%s: default\n", __FUNCTION__);
              break;
      }

              // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

