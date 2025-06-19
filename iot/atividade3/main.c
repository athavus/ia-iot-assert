#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Definição dos pinos
typedef enum {
    BUTTON_BACK = GPIO_NUM_17,
    BUTTON_GO = GPIO_NUM_19
} ButtonPins;

typedef enum {
    LED_1 = GPIO_NUM_2,
    LED_2 = GPIO_NUM_15,
    LED_3 = GPIO_NUM_13,
    LED_4 = GPIO_NUM_14,
    LED_5 = GPIO_NUM_27,
    LED_6 = GPIO_NUM_25,
    LED_7 = GPIO_NUM_33,
    LED_8 = GPIO_NUM_26,
    LED_COUNT = 8
} LedPins;

// Variável compartilhada para direção
volatile int direction = 0; // 0 = direita, 1 = esquerda

// Handler para o botão "IR"
static void IRAM_ATTR handle_go_button(void* arg) {
    direction = 0; // Move para direita
}

// Handler para o botão "VOLTAR"
static void IRAM_ATTR handle_back_button(void* arg) {
    direction = 1; // Move para esquerda
}

// Configuração dos GPIOs
void configure_gpios(void) {
    // Configuração dos LEDs
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_1) | (1ULL << LED_2) | (1ULL << LED_3) | 
                         (1ULL << LED_4) | (1ULL << LED_5) | (1ULL << LED_6) | 
                         (1ULL << LED_7) | (1ULL << LED_8),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_config);

    // Configuração dos botões
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_BACK) | (1ULL << BUTTON_GO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&button_config);
}

// Atualiza os LEDs baseado no valor atual
void update_leds(uint8_t value) {
    const gpio_num_t leds[LED_COUNT] = {LED_1, LED_2, LED_3, LED_4, LED_5, LED_6, LED_7, LED_8};
    
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_set_level(leds[i], (value >> i) & 1);
    }
}

void app_main() {
    configure_gpios();
    
    // Configura interrupções
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GO, handle_go_button, NULL);
    gpio_isr_handler_add(BUTTON_BACK, handle_back_button, NULL);

    uint8_t current_value = 0b00000001; // Começa com o primeiro LED aceso
    
    while (1) {
        update_leds(current_value);
        printf("Valor atual: 0x%02X\n", current_value);

        // Atualiza o valor baseado na direção
        if (direction == 0) {
            current_value = (current_value << 1);
            if (current_value == 0) current_value = 0b00000001;
        } else {
            current_value = (current_value >> 1);
            if (current_value == 0) current_value = 0b10000000;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}