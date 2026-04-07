#include "debug_log.h" // Inclusão do cabeçalho para funções de log de depuração

#if DEBUG_SERIAL_ENABLE // Verifica se a depuração serial está habilitada

#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <stdint.h> // Inclusão da biblioteca para tipos de dados inteiros com tamanho fixo
#include <stdbool.h> // Inclusão da biblioteca para o tipo booleano

#include "utils.h" // Inclusão de utilitários personalizados
#include "fsm.h" // Inclusão do cabeçalho para a máquina de estados

// =====================================================
// Estado global
// =====================================================
extern uint32_t g_now_ms; // Declaração da variável global que armazena o tempo atual em milissegundos
extern system_state_t g_state; // Declaração da variável global que armazena o estado do sistema
extern control_mode_t g_mode; // Declaração da variável global que armazena o modo de controle
extern float g_temperature_c; // Declaração da variável global que armazena a temperatura em graus Celsius
extern bool g_lid_open; // Declaração da variável global que indica se a tampa está aberta
extern uint16_t g_adc_raw; // Declaração da variável global que armazena o valor bruto do ADC
extern float g_adc_filtered; // Declaração da variável global que armazena o valor filtrado do ADC
extern bool g_btn_a_stable; // Declaração da variável global que indica se o botão A está estável

// =====================================================
// Variáveis
// =====================================================
transition_reason_t g_last_transition_reason = TRANSITION_BOOT; // Razão da última transição, inicializada como BOOT
system_state_t g_last_transition_from = NORMAL; // Estado da última transição de onde veio, inicializado como NORMAL
system_state_t g_last_transition_to = NORMAL; // Estado da última transição para onde foi, inicializado como NORMAL
static uint32_t g_debug_next_ms = 0; // Variável local que armazena o próximo tempo de depuração em milissegundos
static uint32_t g_next_report_ms = 0; // Variável local que armazena o próximo tempo de relatório em milissegundos
static debug_snapshot_t g_debug_snapshot = {0}; // Estrutura que armazena um instantâneo de depuração, inicializada com zero

// =====================================================
// Funções
// =====================================================
static void debug_capture_snapshot(void) // Função que captura o estado atual do sistema
{
    g_debug_snapshot.ts_ms = g_now_ms; // Armazena o timestamp atual no instantâneo de depuração
    g_debug_snapshot.adc_raw = g_adc_raw; // Armazena o valor bruto do ADC no instantâneo
    g_debug_snapshot.adc_filtered = g_adc_filtered; // Armazena o valor filtrado do ADC no instantâneo
    g_debug_snapshot.temp_c = g_temperature_c; // Armazena a temperatura atual no instantâneo
    g_debug_snapshot.state = g_state; // Armazena o estado atual do sistema no instantâneo
    g_debug_snapshot.mode = g_mode; // Armazena o modo atual de controle no instantâneo
    g_debug_snapshot.last_transition_reason = g_last_transition_reason; // Armazena a razão da última transição no instantâneo
    g_debug_snapshot.lid_open = g_lid_open; // Armazena o estado da tampa no instantâneo
    g_debug_snapshot.button_a_pressed = g_btn_a_stable; // Armazena o estado do botão A no instantâneo
}

// =====================================================
// Interface
// =====================================================
void debug_log_init(void) // Função que inicializa o log de depuração
{
    g_debug_next_ms = g_now_ms + DEBUG_PERIOD_MS; // Define o próximo tempo de depuração
    g_next_report_ms = g_now_ms + REPORT_PERIOD_MS; // Define o próximo tempo de relatório
    
    g_last_transition_reason = TRANSITION_BOOT; // Inicializa a razão da última transição como BOOT
    g_last_transition_from = g_state; // Inicializa o estado da última transição de onde veio
    g_last_transition_to = g_state; // Inicializa o estado da última transição para onde foi
    
    debug_log_event("BOOT", TRANSITION_BOOT); // Registra o evento de inicialização
}

void debug_log_periodic_update(void) // Função que atualiza o log de depuração periodicamente
{
    if (g_now_ms >= g_debug_next_ms) // Verifica se é hora de atualizar o log
    {
        g_debug_next_ms = g_now_ms + DEBUG_PERIOD_MS; // Atualiza o próximo tempo de depuração
        debug_capture_snapshot(); // Captura o estado atual do sistema

        DBG_PRINTF( // Imprime os dados de depuração formatados
            "[DBG][PERIODIC] t=%lu ms | adc_raw=%u | adc_filt=%.1f | temp=%.2f C | state=%s | mode=%s | last_transition=%s | lid=%s | btnA=%s\n",
            (unsigned long)g_debug_snapshot.ts_ms, // Timestamp
            g_debug_snapshot.adc_raw, // Valor bruto do ADC
            g_debug_snapshot.adc_filtered, // Valor filtrado do ADC
            g_debug_snapshot.temp_c, // Temperatura
            state_to_string(g_debug_snapshot.state), // Estado do sistema como string
            mode_to_string(g_debug_snapshot.mode), // Modo de controle como string
            transition_reason_to_string(g_debug_snapshot.last_transition_reason), // Razão da última transição como string
            g_debug_snapshot.lid_open ? "OPEN" : "CLOSED", // Estado da tampa
            g_debug_snapshot.button_a_pressed ? "PRESSED" : "RELEASED" // Estado do botão A
        );
    }
}

void debug_log_report_update(void) // Função que atualiza o relatório de depuração
{
    if (g_now_ms >= g_next_report_ms) // Verifica se é hora de atualizar o relatório
    {
        g_next_report_ms = g_now_ms + REPORT_PERIOD_MS; // Atualiza o próximo tempo de relatório

        temp_alert_t temp_alert = get_current_temp_alert(); // Obtém o alerta de temperatura atual

        DBG_PRINTF( // Imprime os dados do relatório formatados
            "[REPORT] t=%lu ms | mode=%s | state=%s | adc_raw=%u | adc_filt=%.1f | temp=%.2f C | lid=%s | last_transition=%s\n",
            (unsigned long)g_now_ms, // Timestamp
            mode_to_string(g_mode), // Modo de controle como string
            state_to_string(g_state), // Estado do sistema como string
            g_adc_raw, // Valor bruto do ADC
            g_adc_filtered, // Valor filtrado do ADC
            g_temperature_c, // Temperatura
            g_lid_open ? "OPEN" : "CLOSED", // Estado da tampa
            transition_reason_to_string(g_last_transition_reason) // Razão da última transição como string
        );

        if (g_lid_open) // Verifica se a tampa está aberta
            DBG_PRINTF("[REPORT] AVISO Tampa aberta.\n"); // Emite um aviso se a tampa estiver aberta

        if (temp_alert == TEMP_ATENCAO) // Verifica se o alerta de temperatura é de atenção
        {
            if (g_mode == MODO_PELE) // Verifica se o modo é de pele
                DBG_PRINTF("[REPORT] AVISO Temperatura de pele em ATENCAO.\n"); // Emite um aviso específico
            else
                DBG_PRINTF("[REPORT] AVISO Temperatura do ar em ATENCAO.\n"); // Emite um aviso específico
        }
        else if (temp_alert == TEMP_EMERGENCIA) // Verifica se o alerta de temperatura é de emergência
        {
            if (g_mode == MODO_PELE) // Verifica se o modo é de pele
                DBG_PRINTF("[REPORT] AVISO Temperatura de pele em EMERGENCIA.\n"); // Emite um aviso específico
            else
                DBG_PRINTF("[REPORT] AVISO Temperatura do ar em EMERGENCIA.\n"); // Emite um aviso específico
        }
    }
}

void debug_log_event(const char *tag, transition_reason_t reason) // Função que registra um evento de depuração
{
    debug_capture_snapshot(); // Captura o estado atual do sistema

    DBG_PRINTF( // Imprime os dados do evento formatados
        "[DBG][EVENT][%s] t=%lu ms | reason=%s | state=%s | mode=%s | lid=%s | adc_raw=%u | adc_filt=%.1f | temp=%.2f C\n",
        tag, // Tag do evento
        (unsigned long)g_debug_snapshot.ts_ms, // Timestamp
        transition_reason_to_string(reason), // Razão do evento como string
        state_to_string(g_debug_snapshot.state), // Estado do sistema como string
        mode_to_string(g_debug_snapshot.mode), // Modo de controle como string
        g_debug_snapshot.lid_open ? "OPEN" : "CLOSED", // Estado da tampa
        g_debug_snapshot.adc_raw, // Valor bruto do ADC
        g_debug_snapshot.adc_filtered, // Valor filtrado do ADC
        g_debug_snapshot.temp_c // Temperatura
    );
}

#endif // Fim da verificação de habilitação da depuração serial
