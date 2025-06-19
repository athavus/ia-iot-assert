// Inclusões das bibliotecas padrão C
#include <stdio.h>
#include <stdbool.h>

// Inclusões do FreeRTOS para controle de tarefas e semáforos
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"

// Bibliotecas específicas do ESP32
#include "esp_system.h"
#include <esp_err.h>
#include "driver/gpio.h"

// Biblioteca para sensor ultrassônico
#include "ultrasonic.h"
#include "ultrasonic.c"

// Definições dos pinos dos LEDs
#define PINO_LED_VERMELHO GPIO_NUM_19
#define PINO_LED_AMARELO GPIO_NUM_18  
#define PINO_LED_VERDE GPIO_NUM_5

// Máscara para configuração dos pinos de saída
#define MASCARA_PINOS_SAIDA ((1ULL<<PINO_LED_VERMELHO) | (1ULL<<PINO_LED_AMARELO) | (1ULL<<PINO_LED_VERDE))

// Enumeração para os estados do semáforo
typedef enum {
    ESTADO_VERMELHO,
    ESTADO_AMARELO,
    ESTADO_VERDE
} cores_semaforo_t;

// Configurações do sensor ultrassônico
#define PINO_ECHO 14
#define PINO_TRIGGER 12
#define DISTANCIA_MAXIMA_CM 400

// Variáveis globais
static TaskHandle_t handle_tarefa_semaforo = NULL;
static cores_semaforo_t estado_atual = ESTADO_VERMELHO;

// Função para inicializar os LEDs
void configurar_leds() {
    gpio_config_t config_led = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = MASCARA_PINOS_SAIDA,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&config_led);
}

// Função para atualizar o estado dos LEDs
int alternar_estado_semaforo(cores_semaforo_t *estado_ptr) {
    switch (*estado_ptr) {
        case ESTADO_VERMELHO:
            printf("Amarelo apagou, ");
            gpio_set_level(PINO_LED_VERMELHO, 1);
            gpio_set_level(PINO_LED_AMARELO, 0);
            gpio_set_level(PINO_LED_VERDE, 0);
            *estado_ptr = ESTADO_VERDE;
            return 5;
            
        case ESTADO_VERDE:
            printf("Vermelho apagou, ");
            gpio_set_level(PINO_LED_VERMELHO, 0);
            gpio_set_level(PINO_LED_AMARELO, 0);
            gpio_set_level(PINO_LED_VERDE, 1);
            *estado_ptr = ESTADO_AMARELO;
            return 10;
            
        case ESTADO_AMARELO:
        default:
            printf("Verde apagou, ");
            gpio_set_level(PINO_LED_VERMELHO, 0);
            gpio_set_level(PINO_LED_AMARELO, 1);
            gpio_set_level(PINO_LED_VERDE, 0);
            *estado_ptr = ESTADO_VERMELHO;
            return 1;
    }
}

// Tarefa para controlar a sequência do semáforo
void tarefa_controle_semaforo(void *parametros) {
    for (;;) {
        printf("Estado atual: %d\n", estado_atual);
        int tempo_espera = alternar_estado_semaforo(&estado_atual);
        vTaskDelay(pdMS_TO_TICKS(1000 * tempo_espera));
    }
}

// Função para processar erros do sensor
void processar_erro_sensor(esp_err_t codigo_erro) {
    printf("Erro %d: ", codigo_erro);
    switch (codigo_erro) {
        case ESP_ERR_ULTRASONIC_PING:
            printf("Não foi possível fazer ping (dispositivo em estado inválido)\n");
            break;
        case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
            printf("Timeout no ping (nenhum dispositivo encontrado)\n");
            break;
        case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
            printf("Timeout no echo (distância muito grande)\n");
            break;
        default:
            printf("%s\n", esp_err_to_name(codigo_erro));
    }
}

// Tarefa para monitoramento do sensor ultrassônico
void tarefa_monitoramento_sensor(void *parametros) {
    float distancia_medida;
    bool modo_alerta_ativo = false;
    
    // Configuração do sensor
    ultrasonic_sensor_t sensor_distancia = {
        .trigger_pin = PINO_TRIGGER,
        .echo_pin = PINO_ECHO
    };
    
    ultrasonic_init(&sensor_distancia);
    
    while (true) {
        esp_err_t resultado = ultrasonic_measure(&sensor_distancia, DISTANCIA_MAXIMA_CM, &distancia_medida);
        
        if (resultado == ESP_OK) {
            printf("Distância: %0.04f m\n", distancia_medida);
            
            // Verifica se objeto está próximo (menos de 1 metro)
            if (distancia_medida <= 1.0 && !modo_alerta_ativo) {
                // Ativa modo de alerta - LED vermelho fixo
                gpio_set_level(PINO_LED_VERMELHO, 1);
                gpio_set_level(PINO_LED_AMARELO, 0);
                gpio_set_level(PINO_LED_VERDE, 0);
                
                vTaskSuspend(handle_tarefa_semaforo);
                estado_atual = ESTADO_VERMELHO;
                modo_alerta_ativo = true;
            }
            // Verifica se objeto se afastou (mais de 1 metro)
            else if (distancia_medida > 1.0 && modo_alerta_ativo) {
                vTaskResume(handle_tarefa_semaforo);
                modo_alerta_ativo = false;
            }
        } 
        else {
            processar_erro_sensor(resultado);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Função principal da aplicação
void app_main() {
    // Inicialização dos componentes
    configurar_leds();
    
    // Criação das tarefas
    xTaskCreate(
        tarefa_monitoramento_sensor, 
        "MonitorSensor", 
        configMINIMAL_STACK_SIZE * 3, 
        NULL, 
        5, 
        NULL
    );
    
    xTaskCreate(
        tarefa_controle_semaforo, 
        "ControleSemaforo", 
        2048, 
        NULL, 
        5, 
        &handle_tarefa_semaforo
    );
}