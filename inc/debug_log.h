#ifndef DEBUG_LOG_H // Verifica se DEBUG_LOG_H não foi definido
#define DEBUG_LOG_H // Define DEBUG_LOG_H para evitar inclusões múltiplas

#include "config.h" // Inclui o arquivo de configuração
#include "types.h" // Inclui definições de tipos

#if DEBUG_SERIAL_ENABLE // Verifica se a funcionalidade de debug está habilitada

// Globais de debug expostas para a FSM e outros módulos
extern transition_reason_t g_last_transition_reason; // Razão da última transição
extern system_state_t g_last_transition_from; // Estado do sistema antes da última transição
extern system_state_t g_last_transition_to; // Estado do sistema após a última transição

void debug_log_init(void); // Função para inicializar o log de debug
void debug_log_periodic_update(void); // Função para atualizar periodicamente o log de debug
void debug_log_report_update(void); // Função para atualizar o relatório de log de debug
void debug_log_event(const char *tag, transition_reason_t reason); // Função para registrar um evento de log de debug

#else // Caso o debug esteja desativado

// Stubs vazios caso o debug esteja desativado
#define debug_log_init() // Definição vazia para a função de inicialização
#define debug_log_periodic_update() // Definição vazia para a função de atualização periódica
#define debug_log_report_update() // Definição vazia para a função de atualização de relatório
#define debug_log_event(tag, reason) // Definição vazia para a função de registro de evento

#endif // DEBUG_SERIAL_ENABLE // Fim da verificação de habilitação do debug

#endif // DEBUG_LOG_H // Fim da verificação de inclusão múltipla
