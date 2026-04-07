#ifndef TYPES_H // Verifica se o arquivo TYPES_H já foi incluído
#define TYPES_H // Define o arquivo TYPES_H para evitar inclusões múltiplas

#include <stdbool.h> // Inclui a biblioteca para tipos booleanos
#include <stdint.h> // Inclui a biblioteca para tipos inteiros de tamanho fixo

// Enumeração que representa os estados do sistema
typedef enum
{
    NORMAL = 0, // Estado normal do sistema
    ATENCAO,    // Estado de atenção, indicando uma condição que requer monitoramento
    EMERGENCIA   // Estado de emergência, indicando uma condição crítica
} system_state_t;

// Enumeração que representa os modos de controle
typedef enum
{
    MODO_AR = 0, // Modo de operação de ar
    MODO_PELE    // Modo de operação de pele
} control_mode_t;

// Enumeração que representa os alertas de temperatura
typedef enum
{
    TEMP_OK = 0, // Temperatura dentro dos limites normais
    TEMP_ATENCAO, // Temperatura que requer atenção
    TEMP_EMERGENCIA // Temperatura em nível crítico
} temp_alert_t;

// Enumeração que representa as razões de transição do sistema
typedef enum
{
    TRANSITION_NONE = 0, // Sem transição
    TRANSITION_BOOT, // Transição de inicialização
    TRANSITION_TEMP_OK, // Transição para temperatura normal
    TRANSITION_TEMP_ATENCAO, // Transição para temperatura de atenção
    TRANSITION_TEMP_EMERGENCIA, // Transição para temperatura de emergência
    TRANSITION_LID_OPEN, // Transição quando a tampa é aberta
    TRANSITION_LID_CLOSED, // Transição quando a tampa é fechada
    TRANSITION_MODE_CHANGED // Transição quando o modo é alterado
} transition_reason_t;

// Estrutura que representa um instantâneo de depuração
typedef struct
{
    uint32_t ts_ms; // Timestamp em milissegundos
    uint16_t adc_raw; // Valor bruto do ADC
    float adc_filtered; // Valor filtrado do ADC
    float temp_c; // Temperatura em graus Celsius
    system_state_t state; // Estado atual do sistema
    control_mode_t mode; // Modo de controle atual
    transition_reason_t last_transition_reason; // Última razão de transição
    bool lid_open; // Indica se a tampa está aberta
    bool button_a_pressed; // Indica se o botão A foi pressionado
} debug_snapshot_t;

// Estrutura que representa um pixel RGB
typedef struct
{
    uint8_t G; // Componente verde do pixel
    uint8_t R; // Componente vermelho do pixel
    uint8_t B; // Componente azul do pixel
} rgb_pixel_t;

// Estrutura que representa o contexto da matriz RGB
typedef struct
{
    bool initialized; // Indica se a matriz RGB foi inicializada
    system_state_t last_state; // Último estado do sistema
} rgb_matrix_ctx_t;

// Estrutura que representa o contexto do buzzer
typedef struct
{
    bool initialized; // Indica se o buzzer foi inicializado
    bool muted; // Indica se o buzzer está silenciado
    bool output_on; // Indica se a saída do buzzer está ativada
    system_state_t last_state; // Último estado do sistema
    uint32_t next_toggle_ms; // Tempo em milissegundos para a próxima alternância
} buzzer_ctx_t;

#endif // TYPES_H // Fim da proteção contra inclusão múltipla
