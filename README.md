# Incubadora Neonatal – BitDogLab / Raspberry Pi Pico W

Projeto de firmware modular para simulação de uma **incubadora neonatal** usando a placa **BitDogLab** baseada no **Raspberry Pi Pico W**, com interface local (OLED, matriz RGB, buzzer, botões) e **telemetria MQTT para o ThingsBoard Cloud**.

> Aviso: este projeto é **educacional** e simula o comportamento de uma incubadora. Não deve ser utilizado em nenhum contexto médico real.

---

## Visão geral

O sistema lê um valor analógico do joystick vertical da BitDogLab e o converte em uma **temperatura simulada** em graus Celsius, dentro de uma faixa configurável. A partir dessa temperatura:

- Aplica regras diferentes para os modos **AR** e **PELE**.
- Classifica a condição como **NORMAL**, **ATENÇÃO** ou **EMERGÊNCIA**.
- Atualiza a **máquina de estados finita (FSM)**.
- Reflete o estado em:
  - **Display OLED SSD1306** (dashboard com modo, estado, temperatura, setpoint e tampa).
  - **Matriz RGB WS2818B 5×5** (cor fixa por estado).
  - **Buzzer** (padrões de beep para ATENÇÃO e EMERGÊNCIA).
  - **Logs seriais de debug** periódicos e de eventos.
- Envia periodicamente a telemetria via **Wi‑Fi + MQTT** para o **ThingsBoard Cloud**.

---

## Hardware utilizado

Projeto desenvolvido para a placa **BitDogLab** com Pico W:

- **MCU**: Raspberry Pi Pico W (RP2040 + CYW43).
- **Joystick analógico** (eixo vertical conectado ao ADC0 em `GPIO 26`).
- **Botão A** (`GPIO 5`): abre / fecha a tampa da incubadora (clique simples/duplo).
- **Botão B** (`GPIO 6`): alterna entre os modos **AR** e **PELE**.
- **Matriz RGB WS2818B / NeoPixel 5×5** em `GPIO 7`.
- **Buzzer** em `GPIO 21` (PWM).
- **OLED SSD1306 I2C** no barramento `i2c1`:
  - SDA: `GPIO 14`.
  - SCL: `GPIO 15`.

Todos os pinos e constantes de hardware são definidos em `config.h`.

---

## Estrutura do projeto

Arquivos principais (diretório raiz + `src/` e `inc/`):

- `main.c` – loop principal, integração de todos os módulos.
- `config.h` – mapeamento de pinos, constantes de temporização, limites térmicos e credenciais de Wi‑Fi/ThingsBoard.
- `types.h` – tipos globais: estados do sistema, modos, alertas de temperatura, contextos de buzzer/matriz/debug.
- `utils.c/.h` – funções auxiliares (conversões, filtros, strings de estado, clamp, EMA, conversão ADC→temperatura, etc.).
- `inputs.c/.h` – leitura do joystick, filtragem exponencial, cálculo da temperatura e gerenciamento das interrupções dos botões.
- `fsm.c/.h` – regras térmicas e máquina de estados (NORMAL / ATENÇÃO / EMERGÊNCIA).
- `oled_ui.c/.h` – inicialização e renderização do dashboard no SSD1306 (splash e tela principal).
- `rgb_matrix.c/.h` – controle da matriz WS2818B via PIO, com cor fixa por estado.
- `buzzer.c/.h` – controle do buzzer via PWM, com padrões de beep por estado.
- `telemetry.c/.h` – estado da telemetria, conexão Wi‑Fi, DNS, MQTT e publicação periódica.
- `debug_log.c/.h` – logs seriais periódicos e relatórios de estado/alertas.
- `ssd1306_*.c/.h` – driver do display SSD1306 e fontes bitmap.
- `lwipopts.h` – configurações do lwIP para uso com MQTT.
- `pio/ws2818b.pio` – programa PIO para dirigir a matriz WS2818B.
- `CMakeLists.txt` / `pico_sdk_import.cmake` – configuração de build com Pico SDK.

---

## Estados, modos e alertas

Definições em `types.h`:

- **Estados do sistema (`system_state_t`)**:
  - `NORMAL`
  - `ATENCAO`
  - `EMERGENCIA`
- **Modos de controle (`control_mode_t`)**:
  - `MODO_AR` – controle baseado em temperatura de ar.
  - `MODO_PELE` – controle baseado em temperatura de pele.
- **Alertas de temperatura (`temp_alert_t`)**:
  - `TEMP_OK`
  - `TEMP_ATENCAO`
  - `TEMP_EMERGENCIA`

A temperatura simulada é obtida a partir do ADC (0–4095) e mapeada linearmente para a faixa configurada (`TEMP_SIM_MIN_C` até `TEMP_SIM_MAX_C`). Em seguida, é aplicado um filtro EMA com ganho `EMA_ALPHA` para suavizar a leitura.

### Critérios térmicos – modo PELE

Configuração em `config.h`:

- Faixa normal: `SKIN_NORMAL_MIN_C` a `SKIN_NORMAL_MAX_C`.
- Atenção: desvios maiores que `SKIN_DEV_ATT_C` do `SKIN_SETPOINT_C` ou fora da faixa normal.
- Emergência: temperatura abaixo de `SKIN_EMG_LOW_C` ou acima de `SKIN_EMG_HIGH_C`.

### Critérios térmicos – modo AR

- Setpoint: `AIR_SETPOINT_C`.
- Atenção:  
  - quando \(|T - AIR_SETPOINT_C| ≥ AIR_DEV_ATT_C\).
- Emergência:
  - quando `T ≥ AIR_EMG_HIGH_C`.

A função `get_current_temp_alert()` escolhe automaticamente entre regras de AR ou PELE conforme o modo atual.

---

## Máquina de estados (FSM)

O módulo `fsm.c` implementa a lógica principal de estado do sistema.

Prioridade das condições:

1. **Emergência de temperatura** (`TEMP_EMERGENCIA`).
2. **Tampa aberta**.
3. **Temperatura em atenção** (`TEMP_ATENCAO`).
4. Caso contrário: **NORMAL**.

Transições consideram tanto o alerta de temperatura quanto eventos discretos:

- Mudança de modo (botão B).
- Abertura/fechamento da tampa (botão A + lógica de clique duplo).
- Futuramente, pode ser usada a permanência da tampa aberta por mais que `LID_EMERGENCY_MS`.

Cada transição relevante é registrada no log de debug com a razão (`transition_reason_t`).

---

## Entradas: joystick e botões

### Joystick / temperatura simulada

Implementado em `inputs.c`:

- Lê o **ADC** em `JOYSTICK_VRY_PIN` (`ADC0`).
- Aplica filtro **EMA** com ganho `EMA_ALPHA`.
- Converte para temperatura usando `adc_to_temperature_c()`.
- Faz clamp da temperatura para `[TEMP_SIM_MIN_C, TEMP_SIM_MAX_C]`.

### Botão A – tampa da incubadora

- Clique com tampa fechada:
  - Marca `g_lid_open = true`.
  - Registra timestamp de abertura.
  - Dispara evento `g_evt_lid_open`.
- Clique com tampa já aberta:
  - Se o segundo clique ocorrer dentro de `DOUBLE_CLICK_MS`, considera **clique duplo**:
    - Fecha a tampa (`g_lid_open = false`).
    - Dispara evento `g_evt_lid_closed`.
- Toda mudança invalida o cache do OLED, forçando atualização.

### Botão B – modo de controle

- Alterna entre `MODO_AR` e `MODO_PELE`.
- Seta `g_evt_mode_changed` para informar a FSM e a UI.

Ambos os botões usam **interrupção por borda de descida** com **debounce por tempo** (`DEBOUNCE_MS`).

---

## Saídas: OLED, matriz RGB e buzzer

### Display OLED SSD1306

Implementado em `oled_ui.c` com driver em `ssd1306_*.c`:

- Usa `i2c1` com pinos configurados em `config.h`.
- Renderiza duas telas:
  - **Splash de boot** com texto “EMBARCATECH / BitDogLab / Incubadora”.
  - **Dashboard principal** com:
    - Linha superior: modo (`AR`/`PELE`), estado (`NOR`/`ATN`/`EMG`) e separador.
    - Área central: temperatura atual formatada com duas casas decimais.
    - Linha inferior: setpoint atual, estado da tampa (“ABERTA” / “FECH”) e um “heartbeat” piscando.
- Atualização periódica controlada por `OLED_REFRESH_MS`.
- Heartbeat alterna a cada `OLED_HEARTBEAT_MS`.
- Para evitar flicker, mantém um **cache dos últimos valores** exibidos e só redesenha quando algo muda além de um epsilon (`OLED_TEMP_EPSILON_C`, troca de estado, modo, tampa, etc.).

### Matriz RGB 5×5 (WS2818B / NeoPixel)

Implementado em `rgb_matrix.c` usando PIO (`ws2818b.pio`):

- Cada pixel é representado por `rgb_pixel_t { G, R, B }`.
- Um frame é enviado para a matriz usando `pio_sm_put_blocking`.
- Cores por estado (configuráveis em `config.h`):
  - `RGB_NORMAL_*` – cor para estado NORMAL.
  - `RGB_ATT_*` – cor para estado ATENÇÃO.
  - `RGB_EMG_*` – cor para estado EMERGÊNCIA.
- A matriz só é atualizada quando o estado global muda, reduzindo tráfego de PIO e consumo.

### Buzzer

Implementado em `buzzer.c` com PWM:

- Usa `BUZZER_PIN` como saída PWM.
- Frequência e duty definidos por `BUZZER_PWM_WRAP`, `BUZZER_DUTY_LEVEL` e funções auxiliares.
- Comportamento por estado:
  - `NORMAL`: buzzer desligado.
  - `ATENCAO`: beep intermitente com frequência `BUZZER_ATT_FREQ_HZ`, tempos `BUZZER_ATT_ON_MS` / `BUZZER_ATT_OFF_MS`.
  - `EMERGENCIA`: beep mais rápido/urgente com `BUZZER_EMG_FREQ_HZ`, tempos `BUZZER_EMG_ON_MS` / `BUZZER_EMG_OFF_MS`.
- Pode ser globalmente habilitado/desabilitado via `BUZZER_ENABLE` e silenciado em runtime via `buzzer_set_muted()`.

---

## Telemetria Wi‑Fi + MQTT (ThingsBoard)

Toda a telemetria é implementada em `telemetry.c` usando:

- **CYW43 + lwIP** em modo *threadsafe background* (`pico_cyw43_arch_lwip_threadsafe_background`).
- **MQTT** via `pico_lwip_mqtt`.

### Parâmetros de configuração (config.h)

Editar conforme o ambiente:

- Wi‑Fi:
  - `WIFI_SSID`
  - `WIFI_PASSWORD`
  - `WIFI_AUTH`
- ThingsBoard Cloud:
  - `TB_HOST` (ex.: `mqtt.thingsboard.cloud`).
  - `TB_PORT` (ex.: `1883`).
  - `TB_ACCESS_TOKEN` (token do dispositivo no ThingsBoard).
  - `TB_CLIENT_ID` (string para identificar o cliente MQTT).
  - `TB_TOPIC_TELEMETRY` (padrão do ThingsBoard: `v1/devices/me/telemetry`).
- Temporizações:
  - `TM_PUBLISH_PERIOD_MS` – período entre publicações.
  - `TM_WIFI_RETRY_MS`, `TM_DNS_RETRY_MS`, `TM_MQTT_RETRY_MS` – tempos de re‑tentativa.
  - `TM_MQTT_KEEP_ALIVE_S`, `TM_MQTT_QOS`, `TM_MQTT_RETAIN`.

### Máquina de estados da telemetria

`telemetry.c` mantém um contexto (`telemetry_ctx_t`) com flags de Wi‑Fi/DNS/MQTT, endereços e timers. Os estados principais são:

- `TM_STATE_OFF`
- `TM_STATE_WIFI_CONNECTING`
- `TM_STATE_WIFI_CONNECTED`
- `TM_STATE_DNS_RESOLVING`
- `TM_STATE_MQTT_CONNECTING`
- `TM_STATE_MQTT_CONNECTED`

Fluxo típico em `telemetry_update()`:

1. Verifica status do link Wi‑Fi (`cyw43_tcpip_link_status`).
2. Se link caiu e havia conexões ativas, chama `telemetry_reset_after_link_loss()` e agenda novas tentativas.
3. Se Wi‑Fi ok e DNS ainda não resolvido, chama `telemetry_start_dns_resolve()`.
4. Quando DNS se resolve, tenta conectar ao broker com `telemetry_start_mqtt_connect()`.
5. Em `TM_STATE_MQTT_CONNECTED`, publica telemetria a cada `TM_PUBLISH_PERIOD_MS` via `telemetry_publish()`.

### Payload da telemetria

A função `telemetry_build_payload()` monta um JSON compacto com os principais dados do sistema:

- Timestamp em milissegundos desde o boot.
- Estado (`NORMAL`, `ATENCAO`, `EMERGENCIA`).
- Modo (`AR` ou `PELE`).
- Temperatura atual (float, em °C).
- Setpoint atual (em °C).
- Flag de tampa aberta (booleana e descrição textual).
- Alerta de temperatura (`OK`, `ATENCAO`, `EMERGENCIA`).
- Flag de “alarme ativo” combinando estado e tampa.

O payload é enviado para o tópico configurado (`TB_TOPIC_TELEMETRY`) usando `mqtt_publish()`.

---

## Logs de debug e relatórios

`debug_log.c` oferece duas camadas de logs seriais (via `DBG_PRINTF`):

- **Periodic logs** (`debug_log_periodic_update`):
  - Snapshots com timestamp, ADC bruto/filtrado, temperatura, estado, modo, última transição, tampa e botão A.
- **Reports** (`debug_log_report_update`):
  - Resumo em intervalo maior (`REPORT_PERIOD_MS`) com mensagens adicionais:
    - Aviso de tampa aberta.
    - Avisos para temperaturas em ATENÇÃO ou EMERGÊNCIA, diferenciando AR/PELE.
- **Eventos** (`debug_log_event`):
  - Chamado em transições de FSM, boot e outros eventos importantes.

Os logs podem ser desabilitados em build definindo `DEBUG_SERIAL_ENABLE` como `0` em `config.h`.

---

## Fluxo principal (main.c)

`main.c` mantém o loop principal do firmware:

1. `init_system()`:
   - Inicializa stdio, ADC e estado global de tempo.
   - Faz uma primeira leitura do ADC e converte para temperatura.
   - Inicializa módulos: `inputs_init`, `oled_init_display`, `rgb_matrix_init`, `buzzer_init`, `telemetry_init`, `debug_log_init`.
2. Loop infinito:
   - Atualiza `g_now_ms`.
   - Chama `cyw43_arch_poll()` para manter a pilha de rede.
   - Chama `telemetry_update()` para avançar a máquina de telemetria.
   - A cada `INPUT_PERIOD_MS`:
     - `read_inputs()` – atualiza temperatura e eventos de botões.
     - `update_fsm()` – recalcula o estado do sistema.
     - `apply_outputs()` – atualiza OLED, RGB, buzzer e logs.
   - `tight_loop_contents()` para manter o loop eficiente.

`apply_outputs()` centraliza as atualizações de saída, respeitando os eventos de borda (ex.: `g_evt_mode_changed`).

---

## Adaptações e extensões

Algumas ideias para evoluir o projeto:

- **Telemetria local**: adicionar servidor HTTP ou WebSocket local em vez (ou além) do ThingsBoard.
- **Controle fechado de temperatura**: integrar um atuador (ex.: resistência simulada) e implementar controle PID.
- **Histórico na tela**: adicionar gráficos simples no OLED para histórico recente de temperatura/estado.
- **Perfis de paciente**: presets de limites térmicos diferentes por “perfil” selecionável no UI.

---
