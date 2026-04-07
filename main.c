#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <stdbool.h> // Inclusão da biblioteca para tipos booleanos
#include <stdint.h> // Inclusão da biblioteca para tipos inteiros de tamanho fixo

#include "pico/stdlib.h" // Inclusão da biblioteca padrão do Pico
#include "hardware/adc.h" // Inclusão da biblioteca para controle do ADC
#include "pico/cyw43_arch.h" // Inclusão da biblioteca para arquitetura CYW43

#include "config.h" // Inclusão do arquivo de configuração
#include "types.h" // Inclusão do arquivo de tipos personalizados
#include "utils.h" // Inclusão do arquivo de utilitários
#include "inputs.h" // Inclusão do arquivo de entradas
#include "fsm.h" // Inclusão do arquivo de máquina de estados
#include "telemetry.h" // Inclusão do arquivo de telemetria
#include "oled_ui.h" // Inclusão do arquivo de interface OLED
#include "buzzer.h" // Inclusão do arquivo de controle do buzzer
#include "rgb_matrix.h" // Inclusão do arquivo de controle da matriz RGB
#include "debug_log.h" // Inclusão do arquivo de log de depuração

// =====================================================
// Variáveis globais essenciais (Compartilhadas via extern)
// =====================================================
system_state_t g_state = NORMAL; // Estado do sistema inicializado como NORMAL
control_mode_t g_mode = MODO_AR; // Modo de controle inicializado como MODO_AR

uint32_t g_now_ms = 0; // Variável para armazenar o tempo atual em milissegundos
uint32_t g_next_input_ms = 0; // Variável para armazenar o próximo tempo de entrada em milissegundos
uint32_t g_lid_open_since_ms = 0; // Variável para armazenar o tempo em que a tampa foi aberta

// Variável "fantasma" mantida para o debug_log.c compilar
bool g_btn_a_stable = false; // Variável para controle de estabilidade do botão A

// Entradas e processo
uint16_t g_adc_raw = 0; // Variável para armazenar o valor bruto do ADC
float g_adc_filtered = 0.0f; // Variável para armazenar o valor filtrado do ADC
float g_temperature_c = TEMP_START_C; // Variável para armazenar a temperatura em graus Celsius
bool g_adc_filter_initialized = false; // Flag para verificar se o filtro do ADC foi inicializado
bool g_lid_open = false; // Flag para verificar se a tampa está aberta

// Eventos de borda
bool g_evt_lid_open = false; // Evento para indicar que a tampa foi aberta
bool g_evt_lid_closed = false; // Evento para indicar que a tampa foi fechada
bool g_evt_mode_changed = false; // Evento para indicar que o modo foi alterado

void init_system(void); // Protótipo da função de inicialização do sistema
void apply_outputs(void); // Protótipo da função que aplica as saídas
float get_current_setpoint_c(void); // Protótipo da função que obtém o ponto de ajuste atual em Celsius
float get_current_setpoint_c(void) // Função que retorna o ponto de ajuste atual em Celsius
{
    return (g_mode == MODO_PELE) ? SKIN_SETPOINT_C : AIR_SETPOINT_C; // Retorna o ponto de ajuste baseado no modo atual
}

// =====================================================
// Inicialização
// =====================================================
void init_system(void) // Função que inicializa o sistema
{
    stdio_init_all(); // Inicializa todas as interfaces de entrada e saída

    adc_init(); // Inicializa o ADC
    adc_gpio_init(JOYSTICK_VRY_PIN); // Inicializa o pino do joystick para leitura do ADC
    adc_select_input(JOYSTICK_ADC_CH); // Seleciona o canal do ADC para leitura

    g_now_ms = to_ms_since_boot(get_absolute_time()); // Obtém o tempo atual desde o boot em milissegundos
    g_next_input_ms = g_now_ms; // Inicializa o próximo tempo de entrada com o tempo atual

    g_adc_raw = adc_read(); // Lê o valor bruto do ADC
    g_adc_filtered = (float)g_adc_raw; // Converte o valor bruto para float e armazena
    g_temperature_c = adc_to_temperature_c(g_adc_filtered); // Converte o valor filtrado do ADC para temperatura em Celsius
    g_adc_filter_initialized = true; // Marca o filtro do ADC como inicializado

    // Inicialização dos Módulos Isolados
    inputs_init(); // Inicializa o módulo de entradas
    oled_init_display(); // Inicializa o display OLED
    rgb_matrix_init(); // Inicializa a matriz RGB
    buzzer_init(); // Inicializa o buzzer
    telemetry_init(); // Inicializa o módulo de telemetria
    debug_log_init(); // Inicializa o log de depuração
}

// =====================================================
// Saídas (Atualiza hardware com base no estado atual)
// =====================================================
void apply_outputs(void) // Função que aplica as saídas com base no estado atual
{
    debug_log_periodic_update(); // Atualiza o log de depuração periodicamente
    debug_log_report_update(); // Atualiza o relatório do log de depuração

    if (g_evt_mode_changed) // Verifica se o modo foi alterado
        g_evt_mode_changed = false; // Reseta o evento de mudança de modo

    oled_update_if_needed(); // Atualiza o display OLED se necessário
    rgb_matrix_update(); // Atualiza a matriz RGB
    buzzer_update(); // Atualiza o estado do buzzer
}

// =====================================================
// Loop Principal
// =====================================================
int main(void) // Função principal do programa
{
    init_system(); // Chama a função de inicialização do sistema

    while (true) // Loop infinito
    {
        g_now_ms = to_ms_since_boot(get_absolute_time()); // Atualiza o tempo atual em milissegundos

        cyw43_arch_poll(); // Realiza a verificação da arquitetura CYW43

        telemetry_update(); // Atualiza os dados de telemetria

        if (g_now_ms >= g_next_input_ms) // Verifica se é hora de processar a próxima entrada
        {
            g_next_input_ms = g_now_ms + INPUT_PERIOD_MS; // Atualiza o próximo tempo de entrada

            read_inputs(); // Lê as entradas
            update_fsm(); // Atualiza a máquina de estados
            apply_outputs(); // Aplica as saídas
        }

        tight_loop_contents(); // Mantém o loop apertado para eficiência
    }

    return 0; // Retorna 0 ao final da execução
}
