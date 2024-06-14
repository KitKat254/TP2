#include "em_common.h"
#include "app_assert.h"
#include "app.h"
#include "sl_sensor_rht.h"

#define TEMPERATURE_TIMER_SIGNAL (1<<0)

uint32_t temp(void){

  uint32_t rh;
  int32_t t;

  sl_sensor_rht_init();
  sl_sensor_rht_get(&rh,&t);
  sl_sensor_rht_deinit();

  return t/10;
}

void callback(sl_sleeptimer_timer_handle_t *handle, void *callback_data) {
  handle = handle;
  callback_data = callback_data;
  static int timer_step = 0;
  timer_step++;
  app_log_info("Timer step %d\n", timer_step);
  sl_bt_external_signal(TEMPERATURE_TIMER_SIGNAL);
}
