#ifndef FSM_H // Verifica se o cabeçalho FSM_H já foi incluído
#define FSM_H // Define o cabeçalho FSM_H para evitar inclusões múltiplas

#include "types.h" // Inclui o arquivo de tipos personalizados

void update_fsm(void); // Declara a função que atualiza a máquina de estados
temp_alert_t get_current_temp_alert(void); // Declara a função que obtém o nível de alerta térmico

#endif // FSM_H // Finaliza a diretiva de pré-processamento
