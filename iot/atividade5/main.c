// ======================================================================================
// Projeto: Sensor Ultrassônico com MQTT via Wi-Fi (ESP-IDF)
// Autor: Ronaldo Urquiza
// Descrição: Lê dados de distância de um sensor HC-SR04, 
//            envia periodicamente via MQTT para um broker público.
// ======================================================================================

// =============================== INCLUDES E DEFINIÇÕES ===============================
#include <stdio.h>               // Entrada/saída padrão
#include <string.h>              // Manipulação de strings
#include <stdbool.h>             // Tipo booleano

#include "freertos/FreeRTOS.h"   // Kernel FreeRTOS
#include "freertos/task.h"       // Manipulação de tarefas
#include "freertos/event_groups.h" // Grupo de eventos
#include "freertos/semphr.h"     // Semáforo binário

#include "esp_wifi.h"            // Gerenciamento de Wi-Fi
#include "esp_event.h"           // Manipulação de eventos (IP, Wi-Fi)
#include "esp_log.h"             // Log de depuração
#include "esp_system.h"          // Sistema e reinicialização
#include "nvs_flash.h"           // Memória flash não-volátil

#include "mqtt_client.h"         // Cliente MQTT

#include "driver/gpio.h"         // GPIOs gerais

// Biblioteca para sensor ultrassônico
#include "ultrasonic.h"
#include "ultrasonic.c"

// Configurações do sensor ultrassônico
#define PINO_TRIGGER 12           // GPIO 2 - Pino TRIG do HC-SR04
#define PINO_ECHO 14              // GPIO 4 - Pino ECHO do HC-SR04
#define DISTANCIA_MAXIMA_CM 400  // Distância máxima de medição

#define WIFI_SSID "Wokwi-GUEST"  // Nome da rede Wi-Fi
#define WIFI_PASS ""             // Senha da rede Wi-Fi (vazia)
#define CONNECTED_BIT BIT0       // Bit indicador de conexão Wi-Fi

// =============================== VARIÁVEIS GLOBAIS ===================================
static EventGroupHandle_t wifi_event_group;    // Grupo de eventos do Wi-Fi
static const char *TAG = "LOG_MQTT";            // TAG para logs
SemaphoreHandle_t xSemaphore;                  // Semáforo binário
esp_mqtt_client_handle_t client;               // Cliente MQTT
static float global_distance = 0.0;            // Distância global inicial

// =============================== HANDLER DE EVENTOS WI-FI ============================
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

// =============================== INICIALIZAÇÃO DO WI-FI ==============================
static void initialise_wifi(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// =============================== HANDLER DE EVENTOS MQTT =============================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("Mensagem recebida: %.*s\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

// =============================== INICIALIZAÇÃO MQTT ==================================
static void mqtt_initialize(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://test.mosquitto.org",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// =============================== FUNÇÃO PARA PROCESSAR ERROS DO SENSOR ===============
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

// =============================== LEITURA DE DISTÂNCIA ================================
float distanciaGlobal = 0.0;

// Task do FreeRTOS que roda continuamente e lê o sensor ultrassônico
void generate_data(void *pvParam) {
    // Configuração do sensor ultrassônico
    ultrasonic_sensor_t sensor_distancia = {
        .trigger_pin = PINO_TRIGGER,
        .echo_pin = PINO_ECHO
    };
    
    // Inicializa o sensor
    ultrasonic_init(&sensor_distancia);

    while (1) {
        esp_err_t resultado = ultrasonic_measure(&sensor_distancia, DISTANCIA_MAXIMA_CM, &distanciaGlobal);
        
        if (resultado == ESP_OK) {
            // Converte para centímetros para facilitar a visualização
            float distancia_cm = distanciaGlobal * 100;
            
            printf("Distância: %.2f cm\n", distancia_cm);
            
            // Salva o valor na variável global (em cm)
            global_distance = distancia_cm;
            
            if (xSemaphore != NULL) {
                // Libera o semáforo sinalizando que há novo valor disponível
                xSemaphoreGive(xSemaphore);
            }
        } else {
            processar_erro_sensor(resultado);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 segundo antes de repetir a leitura
    }
}

// =============================== ENVIO DE MENSAGEM VIA MQTT ===========================
char tx_buffer[128];

// Task responsável por enviar a distância via MQTT quando ela estiver pronta
void send_messages(void *pvParam) {
    while (1) {
        if (xSemaphore != NULL && xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(3000))) {
            // Copia o valor atual da distância e formata como string
            float value = global_distance;
            sprintf(tx_buffer, "%.2f", value);

            printf("Enviando distância: %s cm\n", tx_buffer);

            // Publica a mensagem MQTT no tópico de distância
            esp_mqtt_client_publish(client, "topic/distancia/sensor", tx_buffer, 0, 0, 0);

            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

// =============================== FUNÇÃO PRINCIPAL =====================================
void app_main() {
    // Inicializa a NVS (memória flash não volátil)
    ESP_ERROR_CHECK(nvs_flash_init());

    // Inicializa o Wi-Fi e espera conexão
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    // Inicializa o MQTT
    mqtt_initialize();

    // Cria o semáforo binário para sincronizar as tasks
    xSemaphore = xSemaphoreCreateBinary();

    // Cria as tasks
    xTaskCreate(generate_data, "TaskSensor", 4096, NULL, 6, NULL);     // Task do sensor
    xTaskCreate(send_messages, "TaskMQTT", 2048, NULL, 4, NULL);       // Task do MQTT
}