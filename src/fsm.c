#include "fsm.h"  // Inclusão do cabeçalho da máquina de estados finitos

#include <stdbool.h>  // Inclusão da biblioteca para tipos booleanos
#include <stdint.h>   // Inclusão da biblioteca para tipos inteiros de tamanho fixo
#include <math.h>     // Inclusão da biblioteca matemática

#include "config.h"   // Inclusão do cabeçalho de configuração
#include "types.h"    // Inclusão do cabeçalho de tipos personalizados
#include "oled_ui.h"  // Inclusão do cabeçalho da interface OLED

// Estado global compartilhado
extern system_state_t g_state;  // Declaração do estado do sistema
extern control_mode_t g_mode;    // Declaração do modo de controle

extern uint32_t g_now_ms;        // Declaração do tempo atual em milissegundos
extern uint32_t g_lid_open_since_ms;  // Declaração do tempo desde que a tampa está aberta

extern float g_temperature_c;    // Declaração da temperatura em graus Celsius
extern bool g_lid_open;          // Declaração do estado da tampa (aberta ou fechada)

extern bool g_evt_lid_open;      // Declaração do evento de abertura da tampa
extern bool g_evt_lid_closed;    // Declaração do evento de fechamento da tampa
extern bool g_evt_mode_changed;   // Declaração do evento de mudança de modo

#if DEBUG_SERIAL_ENABLE
extern transition_reason_t g_last_transition_reason;  // Declaração da razão da última transição
extern system_state_t g_last_transition_from;         // Declaração do estado anterior à última transição
extern system_state_t g_last_transition_to;           // Declaração do estado posterior à última transição

extern void debug_log_event(const char *tag, transition_reason_t reason);  // Declaração da função de log de eventos
#endif

// =====================================================
// Regras térmicas
// =====================================================

// Função que determina o alerta de temperatura da pele
static temp_alert_t get_skin_alert(float temp_c)
{
    // Verifica se a temperatura está fora dos limites de emergência
    if (temp_c < SKIN_EMG_LOW_C || temp_c > SKIN_EMG_HIGH_C)
        return TEMP_EMERGENCIA;  // Retorna alerta de emergência

    // Verifica se a temperatura está fora dos limites de atenção
    if (temp_c < SKIN_ATT_LOW_C ||
        temp_c < SKIN_NORMAL_MIN_C ||
        temp_c > SKIN_NORMAL_MAX_C ||
        (temp_c < SKIN_EMG_HIGH_C && fabsf(temp_c - SKIN_SETPOINT_C) >= SKIN_DEV_ATT_C))
        return TEMP_ATENCAO;  // Retorna alerta de atenção

    return TEMP_OK;  // Retorna estado normal
}

// Função que determina o alerta de temperatura do ar
static temp_alert_t get_air_alert(float temp_c)
{
    // Verifica se a temperatura do ar está acima do limite de emergência
    if (temp_c >= AIR_EMG_HIGH_C)
        return TEMP_EMERGENCIA;  // Retorna alerta de emergência

    // Verifica se a temperatura do ar está fora do limite de atenção
    if (fabsf(temp_c - AIR_SETPOINT_C) >= AIR_DEV_ATT_C)
        return TEMP_ATENCAO;  // Retorna alerta de atenção

    return TEMP_OK;  // Retorna estado normal
}

// =====================================================
// Interface
// =====================================================

// Função que obtém o alerta de temperatura atual
temp_alert_t get_current_temp_alert(void)
{
    // Verifica o modo de operação e chama a função apropriada para obter o alerta
    if (g_mode == MODO_PELE)
        return get_skin_alert(g_temperature_c);  // Retorna alerta da pele

    return get_air_alert(g_temperature_c);  // Retorna alerta do ar
}

// Função que atualiza a máquina de estados
void update_fsm(void)
{
    temp_alert_t temp_alert = get_current_temp_alert();  // Obtém o alerta de temperatura atual
    system_state_t prev_state = g_state;  // Armazena o estado anterior
    system_state_t next_state = NORMAL;    // Inicializa o próximo estado como normal

#if DEBUG_SERIAL_ENABLE
    transition_reason_t transition_reason = TRANSITION_NONE;  // Inicializa a razão da transição
#endif

    // Prioridade: emergência > tampa > atenção > normal
    if (temp_alert == TEMP_EMERGENCIA)
    {
        next_state = EMERGENCIA;  // Define o próximo estado como emergência
#if DEBUG_SERIAL_ENABLE
        transition_reason = TRANSITION_TEMP_EMERGENCIA;  // Registra a razão da transição
#endif
    }
    else if (g_lid_open)
    {
        next_state = ATENCAO;  // Define o próximo estado como atenção
#if DEBUG_SERIAL_ENABLE
        transition_reason = TRANSITION_LID_OPEN;  // Registra a razão da transição
#endif
    }
    else if (temp_alert == TEMP_ATENCAO)
    {
        next_state = ATENCAO;  // Define o próximo estado como atenção
#if DEBUG_SERIAL_ENABLE
        transition_reason = TRANSITION_TEMP_ATENCAO;  // Registra a razão da transição
#endif
    }
    else
    {
        next_state = NORMAL;  // Define o próximo estado como normal
#if DEBUG_SERIAL_ENABLE
        transition_reason = TRANSITION_TEMP_OK;  // Registra a razão da transição
#endif
    }

    if (g_lid_open && (g_now_ms - g_lid_open_since_ms >= LID_EMERGENCY_MS))
    {
        // futuro
    }

#if DEBUG_SERIAL_ENABLE
    // Verifica eventos de mudança de modo e atualiza a razão da transição
    if (g_evt_mode_changed && next_state != prev_state)
        transition_reason = TRANSITION_MODE_CHANGED;
    else if (g_evt_lid_closed && next_state != prev_state)
        transition_reason = TRANSITION_LID_CLOSED;
    else if (g_evt_lid_open && next_state != prev_state && temp_alert != TEMP_EMERGENCIA)
        transition_reason = TRANSITION_LID_OPEN;
#endif

    g_state = next_state;  // Atualiza o estado global

    // Verifica se houve mudança de estado
    if (g_state != prev_state)
    {
        oled_mark_dirty();  // Marca a interface OLED como suja para atualização

#if DEBUG_SERIAL_ENABLE
        g_last_transition_from = prev_state;  // Armazena o estado anterior da transição
        g_last_transition_to = g_state;        // Armazena o estado atual da transição
        g_last_transition_reason = transition_reason;  // Armazena a razão da transição

        debug_log_event("FSM_TRANSITION", transition_reason);  // Registra o evento de transição
#endif
    }

#if DEBUG_SERIAL_ENABLE
    // Registra eventos de abertura e fechamento da tampa
    if (g_evt_lid_open)
        debug_log_event("LID_OPEN", TRANSITION_LID_OPEN);

    if (g_evt_lid_closed)
        debug_log_event("LID_CLOSED", TRANSITION_LID_CLOSED);

    if (g_evt_mode_changed)
        debug_log_event("MODE_CHANGED", TRANSITION_MODE_CHANGED);
#endif

    // Reseta os eventos de tampa
    g_evt_lid_open = false;  // Reseta o evento de abertura da tampa
    g_evt_lid_closed = false;  // Reseta o evento de fechamento da tampa
    
}