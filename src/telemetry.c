#include "telemetry.h" // Inclusão do cabeçalho de telemetria

#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <string.h> // Inclusão da biblioteca de manipulação de strings
#include <stdbool.h> // Inclusão da biblioteca para tipos booleanos
#include <stdint.h> // Inclusão da biblioteca para tipos inteiros de largura fixa

#include "config.h" // Inclusão do cabeçalho de configuração
#include "types.h" // Inclusão do cabeçalho de tipos personalizados
#include "utils.h" // Inclusão do cabeçalho de utilitários
#include "fsm.h" // Inclusão do cabeçalho de máquina de estados finitos

#include "pico/cyw43_arch.h" // Inclusão do cabeçalho da arquitetura CYW43
#include "lwip/apps/mqtt.h" // Inclusão do cabeçalho para MQTT
#include "lwip/dns.h" // Inclusão do cabeçalho para DNS
#include "lwip/ip_addr.h" // Inclusão do cabeçalho para endereços IP
#include "lwip/err.h" // Inclusão do cabeçalho para erros

extern float get_current_setpoint_c(void); // Declaração da função externa para obter o setpoint atual

// Enumeração para os estados
typedef enum
{
    TM_STATE_OFF = 0, // Estado desligado
    TM_STATE_WIFI_CONNECTING, // Estado de conexão Wi-Fi
    TM_STATE_WIFI_CONNECTED, // Estado conectado ao Wi-Fi
    TM_STATE_DNS_RESOLVING, // Estado de resolução de DNS
    TM_STATE_MQTT_CONNECTING, // Estado de conexão MQTT
    TM_STATE_MQTT_CONNECTED // Estado conectado ao MQTT
} telemetry_state_t;

typedef struct
{
    bool initialized; // Indica se a telemetria foi inicializada
    bool wifi_connected; // Indica se está conectado ao Wi-Fi
    bool dns_resolved; // Indica se o DNS foi resolvido
    bool dns_in_progress; // Indica se a resolução de DNS está em andamento
    bool mqtt_connected; // Indica se está conectado ao MQTT
    bool mqtt_connecting; // Indica se está tentando conectar ao MQTT

    telemetry_state_t state; // Estado atual da telemetria

    uint32_t next_wifi_retry_ms; // Tempo para a próxima tentativa de conexão Wi-Fi
    uint32_t next_dns_retry_ms; // Tempo para a próxima tentativa de resolução de DNS
    uint32_t next_mqtt_retry_ms; // Tempo para a próxima tentativa de conexão MQTT
    uint32_t next_publish_ms; // Tempo para a próxima publicação

    ip_addr_t broker_ip; // Endereço IP do broker MQTT
    mqtt_client_t *mqtt_client; // Ponteiro para o cliente MQTT
    struct mqtt_connect_client_info_t mqtt_info; // Informações de conexão do cliente MQTT
} telemetry_ctx_t;

static telemetry_ctx_t g_tm = {0}; // Inicialização do contexto de telemetria

extern uint32_t g_now_ms; // Variável externa para o tempo atual em milissegundos
extern system_state_t g_state; // Estado do sistema
extern control_mode_t g_mode; // Modo de controle
extern float g_temperature_c; // Temperatura atual em graus Celsius
extern bool g_lid_open; // Indica se a tampa está aberta

// Declarações de funções estáticas
static void telemetry_reset_after_link_loss(void); // Reseta o estado após perda de conexão
static void telemetry_start_wifi_connect(void); // Inicia a conexão Wi-Fi
static void telemetry_start_dns_resolve(void); // Inicia a resolução de DNS
static void telemetry_start_mqtt_connect(void); // Inicia a conexão MQTT
static void telemetry_publish(void); // Publica dados
static void telemetry_build_payload(char *buf, size_t len); // Constrói a carga útil para publicação

// Callbacks para eventos de DNS e MQTT
static void telemetry_dns_found_cb(const char *hostname, const ip_addr_t *ipaddr, void *arg); // Callback para DNS encontrado
static void telemetry_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status); // Callback para conexão MQTT
static void telemetry_mqtt_pub_cb(void *arg, err_t err); // Callback para publicação MQTT

// Funções para conversão de estado
static const char *telemetry_state_str(system_state_t state); // Converte estado do sistema para string
static const char *tm_state_to_string(telemetry_state_t state); // Converte estado de telemetria para string

// Função que converte o estado do sistema para uma string
static const char *telemetry_state_str(system_state_t state)
{
    switch (state)
    {
        case NORMAL:     return "NORMAL"; // Retorna "NORMAL" se o estado for NORMAL
        case ATENCAO:    return "ATENCAO"; // Retorna "ATENCAO" se o estado for ATENCAO
        case EMERGENCIA: return "EMERGENCIA"; // Retorna "EMERGENCIA" se o estado for EMERGENCIA
        default:         return "DESCONHECIDO"; // Retorna "DESCONHECIDO" para estados desconhecidos
    }
}

// Função que converte o estado de telemetria para uma string
static const char *tm_state_to_string(telemetry_state_t state)
{
    switch (state)
    {
        case TM_STATE_OFF:             return "OFF"; // Retorna "OFF" se o estado for desligado
        case TM_STATE_WIFI_CONNECTING: return "WIFI_CONNECTING"; // Retorna "WIFI_CONNECTING" se estiver conectando ao Wi-Fi
        case TM_STATE_WIFI_CONNECTED:  return "WIFI_CONNECTED"; // Retorna "WIFI_CONNECTED" se estiver conectado ao Wi-Fi
        case TM_STATE_DNS_RESOLVING:   return "DNS_RESOLVING"; // Retorna "DNS_RESOLVING" se estiver resolvendo DNS
        case TM_STATE_MQTT_CONNECTING: return "MQTT_CONNECTING"; // Retorna "MQTT_CONNECTING" se estiver conectando ao MQTT
        case TM_STATE_MQTT_CONNECTED:  return "MQTT_CONNECTED"; // Retorna "MQTT_CONNECTED" se estiver conectado ao MQTT
        default:                       return "UNKNOWN"; // Retorna "UNKNOWN" para estados desconhecidos
    }
}

// Callback para publicação MQTT
static void telemetry_mqtt_pub_cb(void *arg, err_t err)
{
    (void)arg; // Ignora o argumento

#if DEBUG_SERIAL_ENABLE
    if (err == ERR_OK) // Verifica se a publicação foi bem-sucedida
    {
        DBG_PRINTF("[TM] publish ok\n"); // Imprime mensagem de sucesso
    }
    else
    {
        DBG_PRINTF("[TM] publish falhou err=%d\n", err); // Imprime mensagem de erro
    }
#endif

    if (err != ERR_OK) // Se houve erro na publicação
    {
        g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
        g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado
        g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT
    }
}

// Callback para conexão MQTT
static void telemetry_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    (void)client; // Ignora o argumento
    (void)arg; // Ignora o argumento

    if (status == MQTT_CONNECT_ACCEPTED) // Verifica se a conexão foi aceita
    {
        g_tm.mqtt_connected = true; // Atualiza estado de conexão MQTT
        g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
        g_tm.state = TM_STATE_MQTT_CONNECTED; // Atualiza estado para MQTT conectado
        g_tm.next_publish_ms = g_now_ms; // Atualiza tempo para próxima publicação

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] MQTT conectado\n"); // Imprime mensagem de sucesso
#endif
    }
    else // Se a conexão não foi aceita
    {
        g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
        g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado
        g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] MQTT connect falhou status=%d\n", status); // Imprime mensagem de erro
#endif
    }
}

// Callback para DNS encontrado
static void telemetry_dns_found_cb(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    (void)hostname; // Ignora o argumento
    (void)arg; // Ignora o argumento

    g_tm.dns_in_progress = false; // Atualiza estado de resolução de DNS

    if (ipaddr != NULL) // Se o endereço IP foi resolvido
    {
        g_tm.broker_ip = *ipaddr; // Atualiza endereço IP do broker
        g_tm.dns_resolved = true; // Atualiza estado de resolução de DNS
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] DNS resolvido: %s\n", ipaddr_ntoa(ipaddr)); // Imprime endereço IP resolvido
#endif
    }
    else // Se o endereço IP não foi resolvido
    {
        g_tm.dns_resolved = false; // Atualiza estado de resolução de DNS
        g_tm.next_dns_retry_ms = g_now_ms + TM_DNS_RETRY_MS; // Agenda próxima tentativa de resolução de DNS
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] DNS falhou\n"); // Imprime mensagem de erro
#endif
    }
}

// Reseta o estado após perda de conexão
static void telemetry_reset_after_link_loss(void)
{
    if (g_tm.mqtt_client && mqtt_client_is_connected(g_tm.mqtt_client)) // Se o cliente MQTT está conectado
    {
        cyw43_arch_lwip_begin(); // Inicia a arquitetura LwIP
        mqtt_disconnect(g_tm.mqtt_client); // Desconecta o cliente MQTT
        cyw43_arch_lwip_end(); // Finaliza a arquitetura LwIP
    }

    g_tm.wifi_connected = false; // Atualiza estado de conexão Wi-Fi
    g_tm.dns_resolved = false; // Atualiza estado de resolução de DNS
    g_tm.dns_in_progress = false; // Atualiza estado de resolução de DNS
    g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
    g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
    g_tm.state = TM_STATE_OFF; // Atualiza estado para desligado

    g_tm.next_wifi_retry_ms = g_now_ms + TM_WIFI_RETRY_MS; // Agenda próxima tentativa de conexão Wi-Fi
    g_tm.next_dns_retry_ms = g_now_ms + TM_DNS_RETRY_MS; // Agenda próxima tentativa de resolução de DNS
    g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT

#if DEBUG_SERIAL_ENABLE
    DBG_PRINTF("[TM] link perdido, agendando reconexao\n"); // Imprime mensagem de perda de conexão
#endif
}

// Inicia a conexão Wi-Fi
static void telemetry_start_wifi_connect(void)
{
    int rc = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, WIFI_AUTH); // Tenta conectar ao Wi-Fi de forma assíncrona

    if (rc == 0) // Se a conexão foi bem-sucedida
    {
        g_tm.state = TM_STATE_WIFI_CONNECTING; // Atualiza estado para conectando ao Wi-Fi
        g_tm.next_wifi_retry_ms = g_now_ms + TM_WIFI_RETRY_MS; // Agenda próxima tentativa de conexão Wi-Fi

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] iniciando Wi-Fi async\n"); // Imprime mensagem de início de conexão Wi-Fi
#endif
    }
    else // Se houve erro na conexão
    {
        g_tm.next_wifi_retry_ms = g_now_ms + TM_WIFI_RETRY_MS; // Agenda próxima tentativa de conexão Wi-Fi

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] falha ao iniciar Wi-Fi rc=%d\n", rc); // Imprime mensagem de erro
#endif
    }
}

// Inicia a resolução de DNS
static void telemetry_start_dns_resolve(void)
{
    cyw43_arch_lwip_begin(); // Inicia a arquitetura LwIP
    err_t err = dns_gethostbyname(TB_HOST, &g_tm.broker_ip, telemetry_dns_found_cb, NULL); // Tenta resolver o nome do host
    cyw43_arch_lwip_end(); // Finaliza a arquitetura LwIP

    if (err == ERR_OK) // Se a resolução foi bem-sucedida
    {
        g_tm.dns_resolved = true; // Atualiza estado de resolução de DNS
        g_tm.dns_in_progress = false; // Atualiza estado de resolução de DNS
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] DNS imediato: %s\n", ipaddr_ntoa(&g_tm.broker_ip)); // Imprime endereço IP resolvido
#endif
    }
    else if (err == ERR_INPROGRESS) // Se a resolução está em andamento
    {
        g_tm.dns_in_progress = true; // Atualiza estado de resolução de DNS
        g_tm.state = TM_STATE_DNS_RESOLVING; // Atualiza estado para resolvendo DNS

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] DNS em andamento\n"); // Imprime mensagem de resolução em andamento
#endif
    }
    else // Se houve erro na resolução
    {
        g_tm.dns_resolved = false; // Atualiza estado de resolução de DNS
        g_tm.dns_in_progress = false; // Atualiza estado de resolução de DNS
        g_tm.next_dns_retry_ms = g_now_ms + TM_DNS_RETRY_MS; // Agenda próxima tentativa de resolução de DNS
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] erro ao iniciar DNS err=%d\n", err); // Imprime mensagem de erro
#endif
    }
}

// Inicia a conexão MQTT
static void telemetry_start_mqtt_connect(void)
{
    if (!g_tm.mqtt_client) // Se o cliente MQTT não foi criado
    {
        g_tm.mqtt_client = mqtt_client_new(); // Cria um novo cliente MQTT

        if (!g_tm.mqtt_client) // Se a criação do cliente falhou
        {
#if DEBUG_SERIAL_ENABLE
            DBG_PRINTF("[TM] mqtt_client_new falhou\n"); // Imprime mensagem de erro
#endif
            g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT
            return; // Retorna da função
        }
    }

    if (mqtt_client_is_connected(g_tm.mqtt_client)) // Se o cliente MQTT já está conectado
    {
        g_tm.mqtt_connected = true; // Atualiza estado de conexão MQTT
        g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
        g_tm.state = TM_STATE_MQTT_CONNECTED; // Atualiza estado para MQTT conectado
        return; // Retorna da função
    }

    memset(&g_tm.mqtt_info, 0, sizeof(g_tm.mqtt_info)); // Limpa a estrutura de informações do cliente MQTT
    g_tm.mqtt_info.client_id = TB_CLIENT_ID; // Define o ID do cliente MQTT
    g_tm.mqtt_info.client_user = TB_ACCESS_TOKEN; // Define o token de acesso do cliente MQTT
    g_tm.mqtt_info.client_pass = NULL; // Define a senha do cliente MQTT como nula
    g_tm.mqtt_info.keep_alive = TM_MQTT_KEEP_ALIVE_S; // Define o tempo de keep-alive do cliente MQTT

    cyw43_arch_lwip_begin(); // Inicia a arquitetura LwIP
    err_t err = mqtt_client_connect( // Tenta conectar ao broker MQTT
        g_tm.mqtt_client,
        &g_tm.broker_ip,
        TB_PORT,
        telemetry_mqtt_connection_cb,
        NULL,
        &g_tm.mqtt_info
    );
    cyw43_arch_lwip_end(); // Finaliza a arquitetura LwIP

    if (err == ERR_OK) // Se a conexão foi bem-sucedida
    {
        g_tm.mqtt_connecting = true; // Atualiza estado de conexão MQTT
        g_tm.state = TM_STATE_MQTT_CONNECTING; // Atualiza estado para conectando ao MQTT

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] iniciando MQTT\n"); // Imprime mensagem de início de conexão MQTT
#endif
    }
    else // Se houve erro na conexão
    {
        g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
        g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] erro ao iniciar MQTT err=%d\n", err); // Imprime mensagem de erro
#endif
    }
}

// Constrói a carga útil para publicação
static void telemetry_build_payload(char *buf, size_t len)
{
    temp_alert_t temp_alert = get_current_temp_alert(); // Obtém o alerta de temperatura atual
    bool alarm_active = (g_state != NORMAL) || g_lid_open; // Verifica se o alarme está ativo

    snprintf( // Formata a carga útil em JSON
        buf,
        len,
        "{\"timestamp_ms\":%lu,"
        "\"estado\":\"%s\","
        "\"modo\":\"%s\","
        "\"temperatura_c\":%.2f,"
        "\"setpoint_c\":%.2f,"
        "\"tampa_aberta\":%s,"
        "\"temp_alerta\":\"%s\","
        "\"lid_alerta\":\"%s\","
        "\"alarme_ativo\":%s}",
        (unsigned long)g_now_ms, // Timestamp atual
        telemetry_state_str(g_state), // Estado do sistema
        mode_to_string(g_mode), // Modo de operação
        g_temperature_c, // Temperatura atual
        get_current_setpoint_c(), // Setpoint atual
        g_lid_open ? "true" : "false", // Indica se a tampa está aberta
        temp_alert_to_string(temp_alert), // Alerta de temperatura
        g_lid_open ? "ABERTA" : "FECHADA", // Indica se a tampa está aberta ou fechada
        alarm_active ? "true" : "false" // Indica se o alarme está ativo
    );
}

// Publica dados
static void telemetry_publish(void)
{
    if (!g_tm.mqtt_client || !g_tm.mqtt_connected) // Se o cliente MQTT não está disponível ou não está conectado
    {
        return; // Retorna da função
    }

    static char payload[384]; // Buffer para a carga útil
    telemetry_build_payload(payload, sizeof(payload)); // Constrói a carga útil

    cyw43_arch_lwip_begin(); // Inicia a arquitetura LwIP
    err_t err = mqtt_publish( // Tenta publicar a carga útil
        g_tm.mqtt_client,
        TB_TOPIC_TELEMETRY, // Tópico de telemetria
        payload, // Carga útil
        strlen(payload), // Comprimento da carga útil
        TM_MQTT_QOS, // Qualidade de serviço
        TM_MQTT_RETAIN, // Retenção da mensagem
        telemetry_mqtt_pub_cb, // Callback para publicação
        NULL // Argumento para o callback
    );
    cyw43_arch_lwip_end(); // Finaliza a arquitetura LwIP

    if (err != ERR_OK) // Se houve erro na publicação
    {
        g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
        g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
        g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT
        g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] publish start falhou err=%d\n", err); // Imprime mensagem de erro
#endif
    }
    else // Se a publicação foi bem-sucedida
    {
#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] TX: %s\n", payload); // Imprime a carga útil enviada
#endif
    }
}

// Inicializa a telemetria
void telemetry_init(void)
{
    if (cyw43_arch_init()) // Tenta inicializar a arquitetura CYW43
    {
#if DEBUG_SERIAL_ENABLE
        DBG_PRINTF("[TM] cyw43 init falhou\n"); // Imprime mensagem de erro
#endif
        g_tm.initialized = false; // Atualiza estado de inicialização
        return; // Retorna da função
    }

    cyw43_arch_enable_sta_mode(); // Habilita o modo STA da arquitetura CYW43

    g_tm.initialized = true; // Atualiza estado de inicialização
    g_tm.wifi_connected = false; // Atualiza estado de conexão Wi-Fi
    g_tm.dns_resolved = false; // Atualiza estado de resolução de DNS
    g_tm.dns_in_progress = false; // Atualiza estado de resolução de DNS
    g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
    g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
    g_tm.state = TM_STATE_OFF; // Atualiza estado para desligado

    g_tm.next_wifi_retry_ms = g_now_ms; // Inicializa tempo para próxima tentativa de conexão Wi-Fi
    g_tm.next_dns_retry_ms = g_now_ms; // Inicializa tempo para próxima tentativa de resolução de DNS
    g_tm.next_mqtt_retry_ms = g_now_ms; // Inicializa tempo para próxima tentativa de conexão MQTT
    g_tm.next_publish_ms = g_now_ms + TM_PUBLISH_PERIOD_MS; // Inicializa tempo para próxima publicação

    g_tm.mqtt_client = mqtt_client_new(); // Cria um novo cliente MQTT

#if DEBUG_SERIAL_ENABLE
    DBG_PRINTF("[TM] telemetria inicializada\n"); // Imprime mensagem de inicialização
    DBG_PRINTF("[TM] estado=%s\n", tm_state_to_string(g_tm.state)); // Imprime estado inicial
#endif
}

// Atualiza o estado da telemetria
void telemetry_update(void)
{
    if (!g_tm.initialized) // Se a telemetria não foi inicializada
    {
        return; // Retorna da função
    }

    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); // Verifica o status da conexão

    if (link_status == CYW43_LINK_UP) // Se a conexão está ativa
    {
        if (!g_tm.wifi_connected) // Se não está conectado ao Wi-Fi
        {
            g_tm.wifi_connected = true; // Atualiza estado de conexão Wi-Fi
            g_tm.dns_resolved = false; // Atualiza estado de resolução de DNS
            g_tm.dns_in_progress = false; // Atualiza estado de resolução de DNS
            g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
            g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
            g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado
            g_tm.next_dns_retry_ms = g_now_ms; // Inicializa tempo para próxima tentativa de resolução de DNS
            g_tm.next_mqtt_retry_ms = g_now_ms; // Inicializa tempo para próxima tentativa de conexão MQTT

#if DEBUG_SERIAL_ENABLE
            DBG_PRINTF("[TM] Wi-Fi conectado\n"); // Imprime mensagem de conexão Wi-Fi
#endif
        }
    }
    else // Se a conexão não está ativa
    {
        if (g_tm.wifi_connected || g_tm.mqtt_connected || g_tm.dns_resolved || g_tm.mqtt_connecting) // Se qualquer conexão está ativa
        {
            telemetry_reset_after_link_loss(); // Reseta o estado após perda de conexão
        }

        if (g_now_ms >= g_tm.next_wifi_retry_ms) // Se é hora de tentar reconectar ao Wi-Fi
        {
            telemetry_start_wifi_connect(); // Inicia a conexão Wi-Fi
        }

        return; // Retorna da função
    }

    if (!g_tm.dns_resolved && !g_tm.dns_in_progress && g_now_ms >= g_tm.next_dns_retry_ms) // Se o DNS não foi resolvido e não está em andamento
    {
        telemetry_start_dns_resolve(); // Inicia a resolução de DNS
        return; // Retorna da função
    }

    if (g_tm.dns_resolved && // Se o DNS foi resolvido
        !g_tm.mqtt_connected && // Se não está conectado ao MQTT
        !g_tm.mqtt_connecting && // Se não está tentando conectar ao MQTT
        g_now_ms >= g_tm.next_mqtt_retry_ms) // Se é hora de tentar conectar ao MQTT
    {
        telemetry_start_mqtt_connect(); // Inicia a conexão MQTT
        return; // Retorna da função
    }

    if (g_tm.mqtt_connected) // Se está conectado ao MQTT
    {
        if (!mqtt_client_is_connected(g_tm.mqtt_client)) // Se o cliente MQTT não está conectado
        {
            g_tm.mqtt_connected = false; // Atualiza estado de conexão MQTT
            g_tm.mqtt_connecting = false; // Atualiza estado de conexão MQTT
            g_tm.next_mqtt_retry_ms = g_now_ms + TM_MQTT_RETRY_MS; // Agenda próxima tentativa de conexão MQTT
            g_tm.state = TM_STATE_WIFI_CONNECTED; // Atualiza estado para Wi-Fi conectado

#if DEBUG_SERIAL_ENABLE
            DBG_PRINTF("[TM] MQTT caiu\n"); // Imprime mensagem de desconexão MQTT
#endif
            return; // Retorna da função
        }

        if (g_now_ms >= g_tm.next_publish_ms) // Se é hora de publicar dados
        {
            g_tm.next_publish_ms = g_now_ms + TM_PUBLISH_PERIOD_MS; // Atualiza tempo para próxima publicação
            telemetry_publish(); // Publica dados
        }
    }
}
