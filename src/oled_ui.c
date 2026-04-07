#include "oled_ui.h" // Inclusão do cabeçalho para a interface do OLED

#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <string.h> // Inclusão da biblioteca para manipulação de strings
#include <math.h> // Inclusão da biblioteca matemática

#include "pico/stdlib.h" // Inclusão da biblioteca padrão para o microcontrolador Pico
#include "hardware/i2c.h" // Inclusão da biblioteca para comunicação I2C

#include "config.h" // Inclusão do cabeçalho de configuração
#include "types.h" // Inclusão do cabeçalho de tipos personalizados
#include "utils.h" // Inclusão do cabeçalho de funções utilitárias

#include "ssd1306.h" // Inclusão do cabeçalho para controle do display SSD1306

// =====================================================
// Estado global 
// =====================================================
extern uint32_t g_now_ms; // Variável externa que armazena o tempo atual em milissegundos
extern system_state_t g_state; // Variável externa que representa o estado do sistema
extern control_mode_t g_mode; // Variável externa que representa o modo de controle
extern float g_temperature_c; // Variável externa que armazena a temperatura em graus Celsius
extern bool g_lid_open; // Variável externa que indica se a tampa está aberta
extern uint16_t g_adc_raw; // Variável externa que armazena o valor bruto do ADC
extern float g_adc_filtered; // Variável externa que armazena o valor filtrado do ADC

extern float get_current_setpoint_c(void); // Declaração da função que retorna o ponto de ajuste atual em graus Celsius

// =====================================================
// Variáveis
// =====================================================
static struct render_area g_frame_area; // Estrutura que define a área de renderização do display
static uint8_t g_oled_buffer[ssd1306_buffer_length]; // Buffer para armazenar os dados a serem exibidos no OLED
static uint32_t g_oled_next_refresh_ms = 0; // Variável que armazena o próximo tempo de atualização do display
static uint32_t g_oled_next_heartbeat_ms = 0; // Variável que armazena o próximo tempo de "heartbeat"
static bool g_oled_dirty = true; // Flag que indica se o conteúdo do OLED foi alterado
static bool g_oled_heartbeat_on = false; // Flag que indica se o "heartbeat" está ativo
static bool g_oled_splash_active = true; // Flag que indica se a tela de inicialização está ativa
static uint32_t g_oled_splash_until_ms = 0; // Variável que armazena o tempo até o qual a tela de inicialização deve ser exibida

// Cache de valores para evitar renderização desnecessária (piscar tela)
static float g_oled_last_temp_c = -999.0f; // Última temperatura exibida no OLED
static uint16_t g_oled_last_adc_raw = 0xFFFF; // Último valor bruto do ADC exibido
static float g_oled_last_adc_filtered = -999.0f; // Último valor filtrado do ADC exibido
static system_state_t g_oled_last_state = (system_state_t)-1; // Último estado do sistema exibido
static control_mode_t g_oled_last_mode = (control_mode_t)-1; // Último modo de controle exibido
static bool g_oled_last_lid_open = false; // Último estado da tampa exibido

// =====================================================
// Funções
// =====================================================

void oled_init_display(void) // Função para inicializar o display OLED
{
    i2c_init(i2c1, ssd1306_i2c_clock * 1000); // Inicializa a comunicação I2C com a frequência especificada
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C); // Configura o pino SDA para função I2C
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C); // Configura o pino SCL para função I2C
    gpio_pull_up(OLED_SDA_PIN); // Ativa o resistor de pull-up no pino SDA
    gpio_pull_up(OLED_SCL_PIN); // Ativa o resistor de pull-up no pino SCL

    ssd1306_init(); // Inicializa o display SSD1306

    g_frame_area.start_column = 0; // Define a coluna inicial da área de renderização
    g_frame_area.end_column = ssd1306_width - 1; // Define a coluna final da área de renderização
    g_frame_area.start_page = 0; // Define a página inicial da área de renderização
    g_frame_area.end_page = ssd1306_n_pages - 1; // Define a página final da área de renderização
    calculate_render_area_buffer_length(&g_frame_area); // Calcula o comprimento do buffer da área de renderização

    memset(g_oled_buffer, 0, ssd1306_buffer_length); // Limpa o buffer do OLED
    render_on_display(g_oled_buffer, &g_frame_area); // Renderiza o conteúdo do buffer no display

    g_oled_next_refresh_ms = g_now_ms; // Define o próximo tempo de atualização como o tempo atual
    g_oled_next_heartbeat_ms = g_now_ms + OLED_HEARTBEAT_MS; // Define o próximo tempo de "heartbeat"
    g_oled_splash_until_ms = g_now_ms + OLED_SPLASH_MS; // Define o tempo até o qual a tela de inicialização deve ser exibida
    g_oled_splash_active = true; // Ativa a tela de inicialização
    g_oled_dirty = true; // Marca o OLED como sujo para renderização
}

void oled_mark_dirty(void) // Função para marcar o OLED como sujo
{
    g_oled_dirty = true; // Define a flag de sujo como verdadeira
}

static void oled_render_boot_splash(void) // Função para renderizar a tela de inicialização
{
    memset(g_oled_buffer, 0, ssd1306_buffer_length); // Limpa o buffer do OLED
    ssd1306_draw_string(g_oled_buffer, 18, 16, "EMBARCATECH"); // Desenha a string "EMBARCATECH" no buffer
    ssd1306_draw_line(g_oled_buffer, 12, 30, 116, 30, true); // Desenha uma linha no buffer
    ssd1306_draw_string(g_oled_buffer, 24, 42, "BitDogLab"); // Desenha a string "BitDogLab" no buffer
    ssd1306_draw_string(g_oled_buffer, 20, 54, "Incubadora"); // Desenha a string "Incubadora" no buffer
    render_on_display(g_oled_buffer, &g_frame_area); // Renderiza o conteúdo do buffer no display
}

static void oled_render_dashboard(void) // Função para renderizar o painel de controle
{
    char line[32]; // Buffer para armazenar uma linha de texto
    char mode_str[8]; // Buffer para armazenar a representação do modo
    char state_str[8]; // Buffer para armazenar a representação do estado
    char hb_str[2]; // Buffer para armazenar a representação do "heartbeat"
    float setpoint_c = get_current_setpoint_c(); // Obtém o ponto de ajuste atual

    memset(g_oled_buffer, 0, ssd1306_buffer_length); // Limpa o buffer do OLED

    snprintf(mode_str, sizeof(mode_str), "%s", mode_to_string(g_mode)); // Converte o modo atual em string
    snprintf(state_str, sizeof(state_str), "%s", state_short_string(g_state)); // Converte o estado atual em string
    snprintf(hb_str, sizeof(hb_str), "%s", heartbeat_to_string(g_oled_heartbeat_on)); // Converte o estado do "heartbeat" em string

    ssd1306_draw_string(g_oled_buffer, 0, 0, mode_str); // Desenha o modo no buffer
    ssd1306_draw_string(g_oled_buffer, 92, 0, state_str); // Desenha o estado no buffer
    ssd1306_draw_line(g_oled_buffer, 0, 10, 127, 10, true); // Desenha uma linha separadora no buffer

    ssd1306_draw_string(g_oled_buffer, 0, 16, "TEMP"); // Desenha o rótulo "TEMP" no buffer
    snprintf(line, sizeof(line), "%.2f C", g_temperature_c); // Formata a temperatura atual em string
    ssd1306_draw_string(g_oled_buffer, 24, 28, line); // Desenha a temperatura no buffer

    ssd1306_draw_line(g_oled_buffer, 0, 42, 127, 42, true); // Desenha uma linha separadora no buffer

    snprintf(line, sizeof(line), "SET %.2fC", setpoint_c); // Formata o ponto de ajuste em string
    ssd1306_draw_string(g_oled_buffer, 0, 48, line); // Desenha o ponto de ajuste no buffer

    snprintf(line, sizeof(line), "TAMPA %s", lid_short_string(g_lid_open)); // Formata o estado da tampa em string
    ssd1306_draw_string(g_oled_buffer, 0, 56, line); // Desenha o estado da tampa no buffer

    ssd1306_draw_string(g_oled_buffer, 120, 56, hb_str); // Desenha o estado do "heartbeat" no buffer
    render_on_display(g_oled_buffer, &g_frame_area); // Renderiza o conteúdo do buffer no display

    g_oled_last_temp_c = g_temperature_c; // Atualiza a última temperatura exibida
    g_oled_last_adc_raw = g_adc_raw; // Atualiza o último valor bruto do ADC exibido
    g_oled_last_adc_filtered = g_adc_filtered; // Atualiza o último valor filtrado do ADC exibido
    g_oled_last_state = g_state; // Atualiza o último estado exibido
    g_oled_last_mode = g_mode; // Atualiza o último modo exibido
    g_oled_last_lid_open = g_lid_open; // Atualiza o último estado da tampa exibido
}

void oled_update_if_needed(void) // Função para atualizar o OLED se necessário
{
    bool content_changed = false; // Variável que indica se o conteúdo foi alterado

    if (g_oled_splash_active) // Verifica se a tela de inicialização está ativa
    {
        if (g_now_ms < g_oled_splash_until_ms) // Verifica se ainda está dentro do tempo de exibição da tela de inicialização
        {
            if (g_oled_dirty) // Verifica se o OLED foi marcado como sujo
            {
                oled_render_boot_splash(); // Renderiza a tela de inicialização
                g_oled_dirty = false; // Marca o OLED como limpo
            }
            return; // Retorna da função
        }
        else // Se o tempo de exibição da tela de inicialização acabou
        {
            g_oled_splash_active = false; // Desativa a tela de inicialização
            g_oled_dirty = true; // Marca o OLED como sujo para renderização
        }
    }

    if (g_now_ms >= g_oled_next_heartbeat_ms) // Verifica se é hora de atualizar o "heartbeat"
    {
        g_oled_next_heartbeat_ms = g_now_ms + OLED_HEARTBEAT_MS; // Atualiza o próximo tempo de "heartbeat"
        g_oled_heartbeat_on = !g_oled_heartbeat_on; // Alterna o estado do "heartbeat"
        g_oled_dirty = true; // Marca o OLED como sujo para renderização
    }

    // Verifica se houve alterações nos dados que precisam ser exibidos
    if (fabsf(g_temperature_c - g_oled_last_temp_c) >= OLED_TEMP_EPSILON_C) content_changed = true; // Verifica se a temperatura mudou
    if (g_adc_raw != g_oled_last_adc_raw) content_changed = true; // Verifica se o valor bruto do ADC mudou
    if (fabsf(g_adc_filtered - g_oled_last_adc_filtered) >= 1.0f) content_changed = true; // Verifica se o valor filtrado do ADC mudou
    if (g_state != g_oled_last_state) content_changed = true; // Verifica se o estado do sistema mudou
    if (g_mode != g_oled_last_mode) content_changed = true; // Verifica se o modo de controle mudou
    if (g_lid_open != g_oled_last_lid_open) content_changed = true; // Verifica se o estado da tampa mudou

    if (content_changed) g_oled_dirty = true; // Se houve alteração, marca o OLED como sujo
    if (!g_oled_dirty) return; // Se o OLED não está sujo, retorna da função
    if (g_now_ms < g_oled_next_refresh_ms) return; // Se ainda não é hora de atualizar, retorna da função

    g_oled_next_refresh_ms = g_now_ms + OLED_REFRESH_MS; // Atualiza o próximo tempo de atualização
    oled_render_dashboard(); // Renderiza o painel de controle
    g_oled_dirty = false; // Marca o OLED como limpo
}