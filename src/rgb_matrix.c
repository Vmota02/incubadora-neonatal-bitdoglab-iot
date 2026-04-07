#include "rgb_matrix.h" // Inclusão do cabeçalho para a matriz RGB

#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <stdbool.h> // Inclusão da biblioteca para tipos booleanos
#include <stdint.h> // Inclusão da biblioteca para tipos inteiros de tamanho fixo

#include "pico/stdlib.h" // Inclusão da biblioteca padrão do Pico
#include "hardware/pio.h" // Inclusão da biblioteca para manipulação do PIO

#include "config.h" // Inclusão do cabeçalho de configuração
#include "types.h" // Inclusão do cabeçalho de tipos
#include "utils.h" // Inclusão do cabeçalho de utilitários

// O cabeçalho gerado pelo compilador do arquivo .pio
#include "ws2818b.pio.h" // Inclusão do cabeçalho para o programa PIO WS2818B

// =====================================================
// Estado global 
// =====================================================
extern uint32_t g_now_ms; // Declaração da variável global que armazena o tempo atual em milissegundos
extern system_state_t g_state; // Declaração da variável global que armazena o estado do sistema

// =====================================================
// Variáveis
// =====================================================
static PIO g_rgb_pio = pio0; // Inicialização do PIO utilizado para controle da matriz RGB
static uint g_rgb_sm = 0; // Inicialização do estado da máquina do PIO
static rgb_pixel_t g_rgb_pixels[RGB_MATRIX_LED_COUNT]; // Array para armazenar os pixels RGB da matriz

static rgb_matrix_ctx_t g_rgb_ctx = { // Estrutura de contexto da matriz RGB
    .initialized = false, // Indica se a matriz RGB foi inicializada
    .last_state = (system_state_t)-1 // Armazena o último estado do sistema
};

// =====================================================
// Funções 
// =====================================================
static void rgb_matrix_put_pixel(uint index, uint8_t r, uint8_t g, uint8_t b) // Função para definir a cor de um pixel específico
{
    if (index >= RGB_MATRIX_LED_COUNT) return; // Verifica se o índice está dentro dos limites

    g_rgb_pixels[index].R = r; // Define o valor do Led vermelho
    g_rgb_pixels[index].G = g; // Define o valor do Led verde
    g_rgb_pixels[index].B = b; // Define o valor do Led azul
}

static void rgb_matrix_show(void) // Função para exibir a matriz RGB
{
    for (uint i = 0; i < RGB_MATRIX_LED_COUNT; i++) // Itera sobre todos os pixels da matriz
    {
        pio_sm_put_blocking(g_rgb_pio, g_rgb_sm, g_rgb_pixels[i].G); // Envia o valor verde para o PIO
        pio_sm_put_blocking(g_rgb_pio, g_rgb_sm, g_rgb_pixels[i].R); // Envia o valor vermelho para o PIO
        pio_sm_put_blocking(g_rgb_pio, g_rgb_sm, g_rgb_pixels[i].B); // Envia o valor azul para o PIO
    }

    sleep_us(100); // Aguarda 100 microssegundos para estabilização
}

static void rgb_matrix_set_all(uint8_t r, uint8_t g, uint8_t b) // Função para definir a cor de todos os pixels
{
    for (uint i = 0; i < RGB_MATRIX_LED_COUNT; i++) // Itera sobre todos os pixels da matriz
    {
        rgb_matrix_put_pixel(i, r, g, b); // Define a cor do pixel atual
    }

    rgb_matrix_show(); // Exibe a matriz RGB com as novas cores
}

static void rgb_matrix_clear(void) // Função para limpar a matriz RGB
{
    rgb_matrix_set_all(0, 0, 0); // Define todos os pixels como apagados (preto)
}

static void rgb_matrix_apply_state_pattern(system_state_t state) // Função para aplicar o padrão de estado
{
    g_rgb_ctx.last_state = state; // Atualiza o último estado com o estado atual
}

static void rgb_matrix_render_current_frame(void) // Função para renderizar o quadro atual da matriz RGB
{
    switch (g_state) // Verifica o estado atual do sistema
    {
        case NORMAL: // Estado normal
            rgb_matrix_set_all(RGB_NORMAL_R, RGB_NORMAL_G, RGB_NORMAL_B); // Define a cor correspondente ao estado normal
            break;

        case ATENCAO: // Estado de atenção
            rgb_matrix_set_all(RGB_ATT_R, RGB_ATT_G, RGB_ATT_B); // Define a cor correspondente ao estado de atenção
            break;

        case EMERGENCIA: // Estado de emergência
        default: // Caso padrão
            rgb_matrix_set_all(RGB_EMG_R, RGB_EMG_G, RGB_EMG_B); // Define a cor correspondente ao estado de emergência
            break;
    }
}

// =====================================================
// Interface
// =====================================================
void rgb_matrix_init(void) // Função para inicializar a matriz RGB
{
    int claimed_sm = pio_claim_unused_sm(pio0, false); // Tenta reivindicar uma máquina de estado não utilizada
    if (claimed_sm >= 0) g_rgb_pio = pio0; // Se bem-sucedido, usa pio0
    else g_rgb_pio = pio1; // Caso contrário, usa pio1

    claimed_sm = pio_claim_unused_sm(g_rgb_pio, true); // Reivindica uma máquina de estado não utilizada no PIO selecionado
    g_rgb_sm = (uint)claimed_sm; // Armazena o número da máquina de estado

    uint offset = pio_add_program(g_rgb_pio, &ws2818b_program); // Adiciona o programa WS2818B ao PIO
    ws2818b_program_init(g_rgb_pio, g_rgb_sm, offset, RGB_MATRIX_PIN, 800000.0f); // Inicializa o programa WS2818B

    for (uint i = 0; i < RGB_MATRIX_LED_COUNT; i++) // Itera sobre todos os pixels da matriz
    {
        g_rgb_pixels[i].R = 0; // Inicializa o Led vermelho como 0
        g_rgb_pixels[i].G = 0; // Inicializa o Led verde como 0
        g_rgb_pixels[i].B = 0; // Inicializa o Led azul como 0
    }

    g_rgb_ctx.initialized = true; // Marca a matriz RGB como inicializada
    rgb_matrix_apply_state_pattern(g_state); // Aplica o padrão de estado atual
    rgb_matrix_render_current_frame(); // Renderiza o quadro atual da matriz RGB

#if DEBUG_SERIAL_ENABLE // Se a depuração serial estiver habilitada
    DBG_PRINTF("[RGB] init ok | pin=%d | leds=%d | sm=%u\n", RGB_MATRIX_PIN, RGB_MATRIX_LED_COUNT, g_rgb_sm); // Imprime informações de inicialização
#endif
}

void rgb_matrix_update(void) // Função para atualizar a matriz RGB
{
    if (!g_rgb_ctx.initialized) return; // Retorna se a matriz RGB não estiver inicializada

    if (g_state != g_rgb_ctx.last_state) // Verifica se o estado atual é diferente do último estado
    {
        rgb_matrix_apply_state_pattern(g_state); // Aplica o padrão de estado atual
        rgb_matrix_render_current_frame(); // Renderiza o quadro atual da matriz RGB

#if DEBUG_SERIAL_ENABLE // Se a depuração serial estiver habilitada
        DBG_PRINTF("[RGB] state=%s | cor fixa atualizada em %lu ms\n", // Imprime informações sobre a atualização do estado
                   state_to_string(g_state), // Converte o estado atual para string
                   (unsigned long)g_now_ms); // Imprime o tempo atual em milissegundos
#endif
    }
}