#include "buzzer.h" // Inclusão do cabeçalho do módulo buzzer

#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <stdbool.h> // Inclusão da biblioteca para tipos booleanos
#include <stdint.h> // Inclusão da biblioteca para tipos inteiros de tamanho fixo

#include "pico/stdlib.h" // Inclusão da biblioteca padrão do Pico
#include "hardware/pwm.h" // Inclusão da biblioteca para controle de PWM
#include "hardware/clocks.h" // Inclusão da biblioteca para controle de clocks

#include "config.h" // Inclusão do cabeçalho de configuração
#include "types.h" // Inclusão do cabeçalho de tipos

// =====================================================
// Estado global 
// =====================================================
extern uint32_t g_now_ms; // Declaração da variável global que armazena o tempo atual em milissegundos
extern system_state_t g_state; // Declaração da variável global que armazena o estado do sistema

// =====================================================
// Variáveis
// =====================================================
static uint g_buzzer_slice = 0; // Variável que armazena o slice do PWM do buzzer
static uint g_buzzer_chan = 0; // Variável que armazena o canal do PWM do buzzer

static buzzer_ctx_t g_buzzer_ctx = {
    .initialized = false, // Indica se o buzzer foi inicializado
    .muted = BUZZER_MUTE_DEFAULT, // Indica se o buzzer está mudo
    .output_on = false, // Indica se a saída do buzzer está ativada
    .last_state = (system_state_t)-1, // Armazena o último estado do sistema
    .next_toggle_ms = 0 // Armazena o próximo tempo de alternância em milissegundos
};

// =====================================================
// Funções
// =====================================================
static void buzzer_apply_frequency(uint32_t freq_hz) // Função que aplica a frequência ao buzzer
{
    uint32_t sys_hz = clock_get_hz(clk_sys); // Obtém a frequência do sistema
    float clkdiv = (float)sys_hz / ((float)freq_hz * (float)(BUZZER_PWM_WRAP + 1u)); // Calcula o divisor de clock

    if (clkdiv < 1.0f) clkdiv = 1.0f; // Garante que o divisor não seja menor que 1
    else if (clkdiv > 255.0f) clkdiv = 255.0f; // Garante que o divisor não seja maior que 255

    pwm_set_clkdiv(g_buzzer_slice, clkdiv); // Define o divisor de clock do PWM
    pwm_set_wrap(g_buzzer_slice, BUZZER_PWM_WRAP); // Define o wrap do PWM
}

static void buzzer_output_off(void) // Função que desativa a saída do buzzer
{
    pwm_set_chan_level(g_buzzer_slice, g_buzzer_chan, 0); // Define o nível do canal do PWM como 0
    g_buzzer_ctx.output_on = false; // Atualiza o estado da saída do buzzer
}

static void buzzer_output_on(uint32_t freq_hz) // Função que ativa a saída do buzzer com uma frequência específica
{
    buzzer_apply_frequency(freq_hz); // Aplica a frequência ao buzzer
    pwm_set_chan_level(g_buzzer_slice, g_buzzer_chan, BUZZER_DUTY_LEVEL); // Define o nível do canal do PWM
    g_buzzer_ctx.output_on = true; // Atualiza o estado da saída do buzzer
}

// =====================================================
// Interface
// =====================================================
void buzzer_set_muted(bool muted) // Função que define o estado mudo do buzzer
{
    g_buzzer_ctx.muted = muted; // Atualiza o estado mudo do buzzer

    if (g_buzzer_ctx.initialized && muted) // Verifica se o buzzer está inicializado e se deve ser mudo
        buzzer_output_off(); // Desativa a saída do buzzer
}

void buzzer_init(void) // Função que inicializa o buzzer
{
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM); // Configura o pino do buzzer para função PWM
    g_buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN); // Obtém o slice do PWM associado ao pino do buzzer
    g_buzzer_chan = pwm_gpio_to_channel(BUZZER_PIN); // Obtém o canal do PWM associado ao pino do buzzer

    pwm_set_wrap(g_buzzer_slice, BUZZER_PWM_WRAP); // Define o wrap do PWM
    pwm_set_chan_level(g_buzzer_slice, g_buzzer_chan, 0); // Define o nível do canal do PWM como 0
    pwm_set_enabled(g_buzzer_slice, true); // Habilita o PWM

    g_buzzer_ctx.initialized = true; // Marca o buzzer como inicializado
    g_buzzer_ctx.muted = BUZZER_MUTE_DEFAULT; // Define o estado mudo padrão
    g_buzzer_ctx.output_on = false; // Define a saída do buzzer como desativada
    g_buzzer_ctx.last_state = (system_state_t)-1; // Inicializa o último estado do sistema
    g_buzzer_ctx.next_toggle_ms = g_now_ms; // Inicializa o próximo tempo de alternância

#if DEBUG_SERIAL_ENABLE
    printf("[BUZZER] init ok | pin=%d | slice=%u | chan=%u | muted=%s\n",
               BUZZER_PIN, // Pino do buzzer
               g_buzzer_slice, // Slice do PWM
               g_buzzer_chan, // Canal do PWM
               g_buzzer_ctx.muted ? "YES" : "NO"); // Estado mudo do buzzer
#endif
}

void buzzer_update(void) // Função que atualiza o estado do buzzer
{
    if (!BUZZER_ENABLE) // Verifica se o buzzer está habilitado
    {
        if (g_buzzer_ctx.initialized) buzzer_output_off(); // Desativa a saída se o buzzer estiver inicializado
        return; // Retorna da função
    }

    if (!g_buzzer_ctx.initialized) return; // Retorna se o buzzer não estiver inicializado

    if (g_buzzer_ctx.muted) // Verifica se o buzzer está mudo
    {
        buzzer_output_off(); // Desativa a saída do buzzer
        g_buzzer_ctx.last_state = g_state; // Atualiza o último estado do sistema
        g_buzzer_ctx.next_toggle_ms = g_now_ms; // Atualiza o próximo tempo de alternância
        return; // Retorna da função
    }

    if (g_state != g_buzzer_ctx.last_state) // Verifica se o estado do sistema mudou
    {
        g_buzzer_ctx.last_state = g_state; // Atualiza o último estado do sistema
        g_buzzer_ctx.next_toggle_ms = g_now_ms; // Atualiza o próximo tempo de alternância
        g_buzzer_ctx.output_on = false; // Atualiza o estado da saída do buzzer
        buzzer_output_off(); // Desativa a saída do buzzer
    }

    switch (g_state) // Verifica o estado atual do sistema
    {
        case NORMAL: // Estado normal
            buzzer_output_off(); // Desativa a saída do buzzer
            break;

        case ATENCAO: // Estado de atenção
            if (g_now_ms < g_buzzer_ctx.next_toggle_ms) return; // Retorna se o tempo atual for menor que o próximo tempo de alternância

            if (g_buzzer_ctx.output_on) // Verifica se a saída do buzzer está ativada
            {
                buzzer_output_off(); // Desativa a saída do buzzer
                g_buzzer_ctx.next_toggle_ms = g_now_ms + BUZZER_ATT_OFF_MS; // Atualiza o próximo tempo de alternância
            }
            else // Se a saída do buzzer não estiver ativada
            {
                buzzer_output_on(BUZZER_ATT_FREQ_HZ); // Ativa a saída do buzzer com a frequência de atenção
                g_buzzer_ctx.next_toggle_ms = g_now_ms + BUZZER_ATT_ON_MS; // Atualiza o próximo tempo de alternância
            }
            break;

        case EMERGENCIA: // Estado de emergência
        default: // Caso padrão
            if (g_now_ms < g_buzzer_ctx.next_toggle_ms) return; // Retorna se o tempo atual for menor que o próximo tempo de alternância

            if (g_buzzer_ctx.output_on) // Verifica se a saída do buzzer está ativada
            {
                buzzer_output_off(); // Desativa a saída do buzzer
                g_buzzer_ctx.next_toggle_ms = g_now_ms + BUZZER_EMG_OFF_MS; // Atualiza o próximo tempo de alternância
            }
            else // Se a saída do buzzer não estiver ativada
            {
                buzzer_output_on(BUZZER_EMG_FREQ_HZ); // Ativa a saída do buzzer com a frequência de emergência
                g_buzzer_ctx.next_toggle_ms = g_now_ms + BUZZER_EMG_ON_MS; // Atualiza o próximo tempo de alternância
            }
            break;
    }
}