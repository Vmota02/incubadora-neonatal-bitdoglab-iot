#include "utils.h" // Inclusão do cabeçalho de utilitários
#include "config.h" // Inclusão do cabeçalho de configuração

// Função que converte o estado do sistema em uma string
const char *state_to_string(system_state_t state)
{
    switch (state) // Início da estrutura de controle switch
    {
        case NORMAL: return "NORMAL"; // Retorna "NORMAL" se o estado for NORMAL
        case ATENCAO: return "ATENCAO"; // Retorna "ATENCAO" se o estado for ATENCAO
        case EMERGENCIA: return "EMERGENCIA"; // Retorna "EMERGENCIA" se o estado for EMERGENCIA
        default: return "DESCONHECIDO"; // Retorna "DESCONHECIDO" para estados não reconhecidos
    }
}

// Função que converte o estado do sistema em uma string curta
const char *state_short_string(system_state_t state)
{
    switch (state) // Início da estrutura de controle switch
    {
        case NORMAL: return "NOR"; // Retorna "NOR" se o estado for NORMAL
        case ATENCAO: return "ATN"; // Retorna "ATN" se o estado for ATENCAO
        case EMERGENCIA: return "EMG"; // Retorna "EMG" se o estado for EMERGENCIA
        default: return "UNK"; // Retorna "UNK" para estados não reconhecidos
    }
}

// Função que converte o modo de controle em uma string
const char *mode_to_string(control_mode_t mode)
{
    switch (mode) // Início da estrutura de controle switch
    {
        case MODO_AR: return "AR"; // Retorna "AR" se o modo for MODO_AR
        case MODO_PELE: return "PELE"; // Retorna "PELE" se o modo for MODO_PELE
        default: return "DESCONHECIDO"; // Retorna "DESCONHECIDO" para modos não reconhecidos
    }
}

// Função que converte o alerta de temperatura em uma string
const char *temp_alert_to_string(temp_alert_t alert)
{
    switch (alert) // Início da estrutura de controle switch
    {
        case TEMP_OK: return "OK"; // Retorna "OK" se o alerta for TEMP_OK
        case TEMP_ATENCAO: return "ATENCAO"; // Retorna "ATENCAO" se o alerta for TEMP_ATENCAO
        case TEMP_EMERGENCIA: return "EMERGENCIA"; // Retorna "EMERGENCIA" se o alerta for TEMP_EMERGENCIA
        default: return "DESCONHECIDO"; // Retorna "DESCONHECIDO" para alertas não reconhecidos
    }
}

// Função que converte a razão de transição em uma string
const char *transition_reason_to_string(transition_reason_t reason)
{
    switch (reason) // Início da estrutura de controle switch
    {
        case TRANSITION_BOOT: return "BOOT"; // Retorna "BOOT" se a razão for TRANSITION_BOOT
        case TRANSITION_TEMP_OK: return "TEMP_OK"; // Retorna "TEMP_OK" se a razão for TRANSITION_TEMP_OK
        case TRANSITION_TEMP_ATENCAO: return "TEMP_ATENCAO"; // Retorna "TEMP_ATENCAO" se a razão for TRANSITION_TEMP_ATENCAO
        case TRANSITION_TEMP_EMERGENCIA: return "TEMP_EMERGENCIA"; // Retorna "TEMP_EMERGENCIA" se a razão for TRANSITION_TEMP_EMERGENCIA
        case TRANSITION_LID_OPEN: return "LID_OPEN"; // Retorna "LID_OPEN" se a razão for TRANSITION_LID_OPEN
        case TRANSITION_LID_CLOSED: return "LID_CLOSED"; // Retorna "LID_CLOSED" se a razão for TRANSITION_LID_CLOSED
        case TRANSITION_MODE_CHANGED: return "MODE_CHANGED"; // Retorna "MODE_CHANGED" se a razão for TRANSITION_MODE_CHANGED
        case TRANSITION_NONE:
        default: return "NONE"; // Retorna "NONE" para razões não reconhecidas
    }
}

// Função que converte o estado do heartbeat em uma string
const char *heartbeat_to_string(bool hb_on)
{
    return hb_on ? "*" : " "; // Retorna "*" se hb_on for verdadeiro, caso contrário retorna um espaço
}

// Função que converte o estado da tampa em uma string curta
const char *lid_short_string(bool lid_open)
{
    return lid_open ? "ABERTA" : "FECH"; // Retorna "ABERTA" se a tampa estiver aberta, caso contrário retorna "FECH"
}

// Função que limita um valor entre um mínimo e um máximo
float clampf_local(float value, float minv, float maxv)
{
    if (value < minv) return minv; // Retorna minv se o valor for menor que minv
    if (value > maxv) return maxv; // Retorna maxv se o valor for maior que maxv
    return value; // Retorna o valor se estiver dentro dos limites
}

// Função que aplica um filtro EMA (Média Móvel Exponencial)
float ema_filter(float input, float previous, float alpha)
{
    return alpha * input + (1.0f - alpha) * previous; // Retorna o valor filtrado
}

// Função que converte um valor ADC em temperatura em Celsius
float adc_to_temperature_c(float adc_value)
{
    const float adc_min = 0.0f; // Valor mínimo do ADC
    const float adc_max = 4095.0f; // Valor máximo do ADC

    float normalized = (adc_value - adc_min) / (adc_max - adc_min); // Normaliza o valor ADC
    normalized = clampf_local(normalized, 0.0f, 1.0f); // Limita o valor normalizado entre 0 e 1

    return TEMP_SIM_MIN_C + normalized * (TEMP_SIM_MAX_C - TEMP_SIM_MIN_C); // Retorna a temperatura em Celsius
}
