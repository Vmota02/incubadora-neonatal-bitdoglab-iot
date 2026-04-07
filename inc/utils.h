#ifndef UTILS_H // Verifica se o cabeçalho não foi incluído anteriormente
#define UTILS_H // Define o cabeçalho para evitar inclusões múltiplas

#include <stdbool.h> // Inclui a biblioteca para tipos booleanos

#include "types.h" // Inclui o cabeçalho de tipos personalizados

// Função que converte o estado do sistema em uma string
const char *state_to_string(system_state_t state); 
// Função que converte o estado do sistema em uma string curta
const char *state_short_string(system_state_t state); 
// Função que converte o modo de controle em uma string
const char *mode_to_string(control_mode_t mode); 
// Função que converte o alerta de temperatura em uma string
const char *temp_alert_to_string(temp_alert_t alert); 
// Função que converte a razão de transição em uma string
const char *transition_reason_to_string(transition_reason_t reason); 
// Função que converte o estado do heartbeat em uma string
const char *heartbeat_to_string(bool hb_on); 
// Função que converte o estado da tampa em uma string curta
const char *lid_short_string(bool lid_open); 

// Função que limita um valor entre um mínimo e um máximo
float clampf_local(float value, float minv, float maxv); 
// Função que aplica um filtro EMA (Exponential Moving Average)
float ema_filter(float input, float previous, float alpha); 
// Função que converte um valor ADC em temperatura em Celsius
float adc_to_temperature_c(float adc_value); 

#endif // UTILS_H // Fim da definição do cabeçalho
