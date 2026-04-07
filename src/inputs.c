#include "inputs.h" // Inclusão do cabeçalho para definições de entrada

#include <stdbool.h> // Inclusão da biblioteca para tipos booleanos
#include <stdint.h>  // Inclusão da biblioteca para tipos inteiros de tamanho fixo

#include "config.h"  // Inclusão do cabeçalho de configuração
#include "types.h"   // Inclusão do cabeçalho de tipos personalizados
#include "utils.h"   // Inclusão do cabeçalho de utilitários

#include "hardware/adc.h" // Inclusão do cabeçalho para controle do ADC
#include "hardware/gpio.h" // Inclusão do cabeçalho para controle de GPIO
#include "pico/stdlib.h"   // Inclusão da biblioteca padrão do Pico

// Estado global compartilhado
extern uint32_t g_now_ms; // Variável externa para o tempo atual em milissegundos
extern control_mode_t g_mode; // Variável externa para o modo de controle

extern uint32_t g_lid_open_since_ms; // Variável externa para o tempo em que a tampa foi aberta
extern bool g_lid_open; // Variável externa que indica se a tampa está aberta

extern bool g_evt_lid_open; // Evento externo que indica que a tampa foi aberta
extern bool g_evt_lid_closed; // Evento externo que indica que a tampa foi fechada
extern bool g_evt_mode_changed; // Evento externo que indica que o modo foi alterado

extern uint16_t g_adc_raw; // Variável externa para o valor bruto do ADC
extern float g_adc_filtered; // Variável externa para o valor filtrado do ADC
extern float g_temperature_c; // Variável externa para a temperatura em graus Celsius
extern bool g_adc_filter_initialized; // Variável externa que indica se o filtro do ADC foi inicializado

// Dependência atual do OLED
extern void oled_mark_dirty(void); // Função externa para marcar o OLED como sujo

// Variáveis locais exclusivas para Debounce e lógica de cliques nas interrupções
static uint32_t last_btn_a_irq_time = 0; // Tempo da última interrupção do botão A
static uint32_t last_btn_b_irq_time = 0; // Tempo da última interrupção do botão B
static uint32_t g_btn_a_first_click_ms = 0; // Tempo do primeiro clique do botão A

// ============================================================================
// CALLBACK DE INTERRUPÇÃO (Hardware IRQ)
// ============================================================================
static void gpio_irq_handler(uint gpio, uint32_t events) // Manipulador de interrupção para GPIO
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time()); // Obtém o tempo atual em milissegundos desde o boot

    if (gpio == BUTTON_A_PIN) // Verifica se a interrupção foi gerada pelo botão A
    {
        if (current_time - last_btn_a_irq_time > DEBOUNCE_MS) // Verifica se o tempo de debounce foi respeitado
        {
            last_btn_a_irq_time = current_time; // Atualiza o tempo da última interrupção

            if (!g_lid_open) // Se a tampa não estiver aberta
            {
                g_lid_open = true; // Abre a tampa
                g_lid_open_since_ms = current_time; // Registra o tempo em que a tampa foi aberta
                g_evt_lid_open = true; // Marca o evento de abertura da tampa
                g_btn_a_first_click_ms = current_time; // Registra o tempo do primeiro clique
                oled_mark_dirty(); // Atualiza a tela OLED
            }
            else // Se a tampa já estiver aberta
            {
                if (current_time - g_btn_a_first_click_ms <= DOUBLE_CLICK_MS) // Verifica se é um clique duplo
                {
                    g_lid_open = false; // Fecha a tampa
                    g_evt_lid_closed = true; // Marca o evento de fechamento da tampa
                    oled_mark_dirty(); // Atualiza a tela OLED
                }
                g_btn_a_first_click_ms = current_time; // Atualiza o tempo do primeiro clique
            }
        }
    }
    else if (gpio == BUTTON_B_PIN) // Verifica se a interrupção foi gerada pelo botão B
    {
        if (current_time - last_btn_b_irq_time > DEBOUNCE_MS) // Verifica se o tempo de debounce foi respeitado
        {
            last_btn_b_irq_time = current_time; // Atualiza o tempo da última interrupção
            g_mode = (g_mode == MODO_AR) ? MODO_PELE : MODO_AR; // Alterna entre os modos
            g_evt_mode_changed = true; // Marca o evento de mudança de modo
            oled_mark_dirty(); // Atualiza a tela OLED
        }
    }
}

// ============================================================================
// INICIALIZAÇÃO DAS INTERRUPÇÕES
// ============================================================================
void inputs_init(void) // Função para inicializar as entradas
{
    gpio_init(BUTTON_A_PIN); // Inicializa o GPIO do botão A
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN); // Define a direção do botão A como entrada
    gpio_pull_up(BUTTON_A_PIN); // Ativa o resistor pull-up para o botão A

    gpio_init(BUTTON_B_PIN); // Inicializa o GPIO do botão B
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN); // Define a direção do botão B como entrada
    gpio_pull_up(BUTTON_B_PIN); // Ativa o resistor pull-up para o botão B

    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Habilita a interrupção para o botão A com callback
    gpio_set_irq_enabled(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true); // Habilita a interrupção para o botão B
}

// ============================================================================
// LEITURA DO ADC
// ============================================================================
static void process_joystick_temperature(void) // Função para processar a temperatura do joystick
{
    g_adc_raw = adc_read(); // Lê o valor bruto do ADC

    if (!g_adc_filter_initialized) // Verifica se o filtro do ADC não foi inicializado
    {
        g_adc_filtered = (float)g_adc_raw; // Inicializa o valor filtrado com o valor bruto
        g_adc_filter_initialized = true; // Marca o filtro como inicializado
    }
    else // Se o filtro já estiver inicializado
    {
        g_adc_filtered = ema_filter((float)g_adc_raw, g_adc_filtered, EMA_ALPHA); // Aplica o filtro EMA
    }

    g_temperature_c = adc_to_temperature_c(g_adc_filtered); // Converte o valor filtrado do ADC para temperatura em Celsius
    g_temperature_c = clampf_local(g_temperature_c, TEMP_SIM_MIN_C, TEMP_SIM_MAX_C); // Limita a temperatura dentro dos valores mínimos e máximos
}

void read_inputs(void) // Função para ler as entradas
{
    process_joystick_temperature(); // Chama a função para processar a temperatura do joystick
}
