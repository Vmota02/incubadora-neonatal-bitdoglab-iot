#ifndef CONFIG_H // Verifica se CONFIG_H não foi definido
#define CONFIG_H // Define CONFIG_H para evitar inclusões múltiplas

#include <stdbool.h> // Inclui a biblioteca para tipos booleanos

#include "pico/cyw43_arch.h" // Inclui a biblioteca para suporte à arquitetura CYW43

// =====================================================
// Mapeamento de hardware
// =====================================================
#define BUTTON_A_PIN            5 // Define o pino do botão A como 5
#define BUTTON_B_PIN            6 // Define o pino do botão B como 6
#define JOYSTICK_VRY_PIN        26 // Define o pino do eixo vertical do joystick como 26
#define JOYSTICK_ADC_CH         0 // Define o canal ADC do joystick como 0

// =====================================================
// Matriz RGB WS2818B / NeoPixel
// =====================================================
#define RGB_MATRIX_PIN          7 // Define o pino da matriz RGB como 7
#define RGB_MATRIX_LED_COUNT    25 // Define a quantidade de LEDs na matriz RGB como 25

#define RGB_NORMAL_R            0 // Define o valor do componente vermelho normal como 0
#define RGB_NORMAL_G            2 // Define o valor do componente verde normal como 2
#define RGB_NORMAL_B            0 // Define o valor do componente azul normal como 0

#define RGB_ATT_R               2 // Define o valor do componente vermelho de atenção como 2
#define RGB_ATT_G               2 // Define o valor do componente verde de atenção como 2
#define RGB_ATT_B               0 // Define o valor do componente azul de atenção como 0

#define RGB_EMG_R               2 // Define o valor do componente vermelho de emergência como 2
#define RGB_EMG_G               0 // Define o valor do componente verde de emergência como 0
#define RGB_EMG_B               0 // Define o valor do componente azul de emergência como 0

// =====================================================
// Configuração Wi-Fi / ThingsBoard
// =====================================================
#define WIFI_SSID               "Define o SSID da rede Wi-Fi" // Define o SSID da rede Wi-Fi
#define WIFI_PASSWORD           "Define a senha da rede Wi-Fi" // Define a senha da rede Wi-Fi
#define WIFI_AUTH               CYW43_AUTH_WPA2_AES_PSK // Define o método de autenticação Wi-Fi

#define TB_HOST                 "mqtt.thingsboard.cloud" // Define o host do ThingsBoard
#define TB_PORT                 1883 // Define a porta do ThingsBoard
#define TB_ACCESS_TOKEN         "Define o token de acesso do ThingsBoard" // Define o token de acesso do ThingsBoard
#define TB_CLIENT_ID            "Define o ID do cliente do ThingsBoard" // Define o ID do cliente do ThingsBoard
#define TB_TOPIC_TELEMETRY      "v1/devices/me/telemetry" // Define o tópico de telemetria do ThingsBoard

#define TM_PUBLISH_PERIOD_MS    10000u // Define o período de publicação em milissegundos
#define TM_WIFI_RETRY_MS        5000u // Define o tempo de espera para nova tentativa de conexão Wi-Fi
#define TM_DNS_RETRY_MS         4000u // Define o tempo de espera para nova tentativa de resolução DNS
#define TM_MQTT_RETRY_MS        3000u // Define o tempo de espera para nova tentativa de conexão MQTT
#define TM_MQTT_KEEP_ALIVE_S    30u // Define o intervalo de keep-alive MQTT em segundos
#define TM_MQTT_QOS             0u // Define o nível de QoS para MQTT
#define TM_MQTT_RETAIN          0u // Define se a mensagem deve ser retida no MQTT

// =====================================================
// Buzzer
// =====================================================
#define BUZZER_PIN              21 // Define o pino do buzzer como 21
#define BUZZER_ENABLE           1 // Define se o buzzer está habilitado
#define BUZZER_MUTE_DEFAULT     false // Define o estado padrão do buzzer como mudo

#define BUZZER_PWM_WRAP         1000u // Define o valor máximo do PWM para o buzzer
#define BUZZER_DUTY_LEVEL       (BUZZER_PWM_WRAP / 2u) // Define o nível de duty cycle do buzzer

#define BUZZER_ATT_FREQ_HZ      1800u // Define a frequência de atenção do buzzer em Hz
#define BUZZER_ATT_ON_MS        120u // Define o tempo que o buzzer ficará ligado em milissegundos
#define BUZZER_ATT_OFF_MS       900u // Define o tempo que o buzzer ficará desligado em milissegundos

#define BUZZER_EMG_FREQ_HZ      2800u // Define a frequência de emergência do buzzer em Hz
#define BUZZER_EMG_ON_MS        120u // Define o tempo que o buzzer de emergência ficará ligado em milissegundos
#define BUZZER_EMG_OFF_MS       120u // Define o tempo que o buzzer de emergência ficará desligado em milissegundos

// =====================================================
// Temporização lógica
// =====================================================
#define INPUT_PERIOD_MS         20u // Define o período de entrada em milissegundos
#define DEBOUNCE_MS             30u // Define o tempo de debounce em milissegundos
#define DOUBLE_CLICK_MS         400u // Define o tempo para considerar um clique duplo em milissegundos
#define LID_EMERGENCY_MS        5000u // Define o tempo de emergência para a tampa em milissegundos
#define REPORT_PERIOD_MS        10000u // Define o período de relatório em milissegundos

// =====================================================
// Faixa simulada da temperatura contínua
// =====================================================
#define TEMP_SIM_MIN_C          34.0f // Define a temperatura mínima simulada em graus Celsius
#define TEMP_SIM_MAX_C          39.5f // Define a temperatura máxima simulada em graus Celsius
#define TEMP_START_C            35.0f // Define a temperatura inicial em graus Celsius

// =====================================================
// Filtro exponencial EMA
// =====================================================
#define EMA_ALPHA               0.12f // Define o fator de suavização do filtro exponencial

// =====================================================
// Critérios do modo PELE
// =====================================================
#define SKIN_NORMAL_MIN_C       36.5f // Define a temperatura mínima normal da pele em graus Celsius
#define SKIN_NORMAL_MAX_C       37.5f // Define a temperatura máxima normal da pele em graus Celsius
#define SKIN_ATT_LOW_C          36.0f // Define a temperatura baixa de atenção da pele em graus Celsius
#define SKIN_EMG_LOW_C          36.0f // Define a temperatura baixa de emergência da pele em graus Celsius
#define SKIN_EMG_HIGH_C         38.0f // Define a temperatura alta de emergência da pele em graus Celsius
#define SKIN_SETPOINT_C         37.0f // Define o ponto de ajuste da temperatura da pele em graus Celsius
#define SKIN_DEV_ATT_C          0.5f // Define a variação de atenção da temperatura da pele em graus Celsius

// =====================================================
// Critérios do modo AR
// =====================================================
#define AIR_SETPOINT_C          36.0f // Define o ponto de ajuste da temperatura do ar em graus Celsius
#define AIR_DEV_ATT_C           1.5f // Define a variação de atenção da temperatura do ar em graus Celsius
#define AIR_EMG_HIGH_C          38.0f // Define a temperatura alta de emergência do ar em graus Celsius

// =====================================================
// OLED SSD1306 - BitDogLab
// =====================================================
#define OLED_SDA_PIN            14 // Define o pino SDA do OLED como 14
#define OLED_SCL_PIN            15 // Define o pino SCL do OLED como 15
#define OLED_REFRESH_MS         200u // Define o intervalo de atualização do OLED em milissegundos
#define OLED_HEARTBEAT_MS       500u // Define o intervalo de heartbeat do OLED em milissegundos
#define OLED_TEMP_EPSILON_C     0.05f // Define a tolerância de temperatura do OLED em graus Celsius
#define OLED_SPLASH_MS          1200u // Define o tempo de splash do OLED em milissegundos

// =====================================================
// Debug serial
// =====================================================
#define DEBUG_SERIAL_ENABLE     1 // Define se a depuração serial está habilitada
#define DEBUG_PERIOD_MS         1000u // Define o período de depuração em milissegundos

#if DEBUG_SERIAL_ENABLE // Verifica se a depuração serial está habilitada
#define DBG_PRINTF(...)         printf(__VA_ARGS__) // Define a macro para impressão de depuração
#else
#define DBG_PRINTF(...)         ((void)0) // Define a macro vazia se a depuração não estiver habilitada
#endif

#endif // CONFIG_H // Finaliza a inclusão condicional de CONFIG_H
