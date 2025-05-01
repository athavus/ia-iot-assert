#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BUTTON GPIO_NUM_26

#define LED1 GPIO_NUM_16
#define LED2 GPIO_NUM_4
#define LED3 GPIO_NUM_0
#define LED4 GPIO_NUM_2

int i = 0;

void app_main() {

    // Configuração dos LEDs como saída
    gpio_config_t io_conf_leds = {
        .pin_bit_mask = (1ULL << LED1) | (1ULL << LED2) | (1ULL << LED3) | (1ULL << LED4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_leds);

    // Configuração do botão como entrada
    gpio_config_t io_conf_button = {
        .pin_bit_mask = (1ULL << BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0, 
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_button);

  
  while (true) {
    int button = gpio_get_level(BUTTON);
     
     if (button == 0) {
      gpio_set_level(LED1, 0);
      gpio_set_level(LED2, 0);
      gpio_set_level(LED3, 0);
      gpio_set_level(LED4, 0);

      i++;
      if ( i > 15) {
        i = 0;
      }

      vTaskDelay(pdMS_TO_TICKS(500)); 
  } else {
      gpio_set_level(LED1, (i >> 0) & 1);
      gpio_set_level(LED2, (i >> 1) & 1);
      gpio_set_level(LED3, (i >> 2) & 1);
      gpio_set_level(LED4, (i >> 3) & 1);

      vTaskDelay(pdMS_TO_TICKS(500));
    }
    printf("ESTADO DECIMAL DOS LEDS: %d\n", i);
  }

}