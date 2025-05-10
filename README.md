# Controle de Joystick com Interrupção ADC, Calibração e LED

Este projeto demonstra técnicas avançadas de leitura de joystick utilizando o Raspberry Pi Pico, incluindo a leitura de valores analógicos via interrupção do ADC, rotinas de calibração automática, implementação de zona morta (deadzone), detecção de variações rápidas de valor e o controle da frequência de piscar de um LED de forma proporcional à posição do joystick, utilizando um timer repetitivo.

## ✨ Requisitos Implementados

Este exemplo de código foca na implementação dos seguintes requisitos:

1.  **Ajustar o valor de limite para calibrar o joystick:** Permite definir o processo e os parâmetros para encontrar a posição central do joystick e estabelecer zonas de inatividade (deadzones).
2.  **Fazer o LED piscar proporcionalmente à posição do joystick:** Controla a frequência com que um LED pisca com base na intensidade do movimento em um dos eixos do joystick.
3.  **Implementar detecção de movimentos rápidos (variações abruptas de valor):** Identifica mudanças súbitas na leitura do ADC em ambos os eixos.
4.  **Implementar ação para outro eixo do joystick:** Processa e reage ao movimento em um eixo diferente do principal (que controla o piscar do LED).

## ✅ Como os Requisitos Foram Atendidos

O código implementa as seguintes funcionalidades para cumprir os requisitos:

1.  **Ajustar o valor de limite para calibrar o joystick:**
    -   A função `calibrar_joystick` realiza uma média de `CALIBRATION_SAMPLES` leituras para determinar os valores `centro_y` e `centro_x` no início.
    -   As constantes `DEADZONE_Y` e `DEADZONE_X` definem a faixa de valores em torno do centro que são considerados "em repouso" para cada eixo.
    -   A lógica principal no `adc_irq_handler` usa esses valores (`centro_y/x` e `DEADZONE_Y/X`) para determinar se um eixo está "acionado" (fora da zona morta).
2.  **Fazer o LED piscar proporcionalmente à posição do joystick:**
    -   As constantes `MIN_BLINK_DELAY_MS` e `MAX_BLINK_DELAY_MS` definem a faixa de velocidade de piscar (delay entre LIGA/DESLIGA).
    -   A lógica dentro do `adc_irq_handler` (no bloco do Eixo Y) calcula `current_blink_delay_ms` com base na distância que a leitura do eixo Y está do `centro_y`, fora da `DEADZONE_Y`. Quanto maior o deslocamento, menor o delay calculado (pisca mais rápido).
    -   Um `repeating_timer` (`blinky_timer`), configurado com o `current_blink_delay_ms` calculado, chama a função `blink_led_callback`.
    -   A função `blink_led_callback` é executada periodicamente pelo timer e simplesmente alterna o estado do pino `LED_AZUL_GPIO` (ligado/desligado), fazendo o LED piscar no ritmo definido pelo timer.
3.  **Implementar detecção de movimentos rápidos (variações abruptas de valor):**
    -   A constante `FAST_MOVE_THRESHOLD` define o limiar de mudança considerado "rápido".
    -   No `adc_irq_handler`, tanto para o Eixo Y quanto para o Eixo X, o valor lido atualmente (`valor`) é comparado com o último valor lido armazenado (`last_adc_y/x_value`). Se a diferença absoluta exceder `FAST_MOVE_THRESHOLD`, uma mensagem é impressa no terminal serial.
4.  **Implementar ação para outro eixo do joystick:**
    -   O código lê e processa ambos os eixos (X e Y). O eixo Y é usado para controlar o piscar do LED.
    -   O eixo X é tratado separadamente no `adc_irq_handler`. Ele também verifica a zona morta (`DEADZONE_X`) e a detecção de movimento rápido (`FAST_MOVE_THRESHOLD`). Quando o eixo X sai ou entra na sua zona morta, uma mensagem específica é impressa no terminal serial. O loop principal garante que o ADC alterne as leituras entre os canais Y e X.

## 🎮 Funcionalidades Principais

-   **Leitura ADC via Interrupção:** O ADC é configurado para operar em modo free-running e gerar uma interrupção (`ADC_IRQ_FIFO`) sempre que uma nova amostra está disponível no FIFO.
-   **Handler de Interrupção do ADC:** Toda a lógica de processamento das leituras do joystick (calibração, zona morta, detecção de movimento rápido, controle do piscar) é executada *dentro* do handler de interrupção, mantendo o loop principal livre.
-   **Calibração Automática:** Uma rotina de inicialização calcula o centro do joystick (valores médios de repouso para X e Y).
-   **Zona Morta (Deadzone):** Ignora pequenos desvios em torno do centro calibrado para evitar acionamentos indesejados por ruído ou imprecisão.
-   **Detecção de Movimento Rápido:** Alerta sobre mudanças súbitas na posição do joystick.
-   **Controle de LED Azul Proporcional:** O LED Azul pisca em uma frequência que aumenta quanto mais longe o joystick (eixo Y) é movido do centro (fora da deadzone).
-   **Timer Repetitivo:** Utiliza `pico/time.h` e um `repeating_timer` para gerar os pulsos de piscar do LED de forma precisa e não bloqueante.
-   **Processamento Independente de Eixos:** Os eixos X e Y são lidos e processados, com o eixo Y controlando o piscar do LED e o eixo X disparando mensagens no serial.
-   **Alternância de Leitura ADC:** O loop principal alterna o canal ADC selecionado para garantir que amostras de ambos os eixos sejam coletadas e processadas.

## 🔌 Diagrama de Conexões (Referência)

| Pino Pico        | Componente        | Detalhe                         |
|------------------|-------------------|---------------------------------|
| GPIO26 (ADC0)    | Eixo Y do Joystick | Entrada analógica (eixo Y)      |
| GPIO27 (ADC1)    | Eixo X do Joystick | Entrada analógica (eixo X)      |
| GPIO12           | LED Azul          | Saída digital para controlar o LED |

*(Certifique-se de usar resistores apropriados em série com os LEDs para limitar a corrente.)*

## ⚙️ Configuração do Sistema

-   **Clock do Sistema**: 125MHz (padrão Pico)
-   **ADC**:
    -   Resolução: 12 bits (0-4095)
    -   Modo: Free-running (conversões contínuas)
    -   Interrupção: `ADC_IRQ_FIFO` habilitada, dispara quando 1 amostra está no FIFO.
    -   Round-Robin: Desabilitado (seleção de canal manual no `main`).
-   **Calibração**:
    -   `CALIBRATION_SAMPLES`: 50 amostras para determinar o centro.
    -   `DEADZONE_Y`/`X`: 150 unidades ADC de cada lado do centro.
-   **Detecção de Movimento Rápido**:
    -   `FAST_MOVE_THRESHOLD`: Diferença > 250 unidades ADC entre leituras consecutivas.
-   **Controle de Piscar (Eixo Y)**:
    -   `MIN_BLINK_DELAY_MS`: 50 ms (pisca mais rápido).
    -   `MAX_BLINK_DELAY_MS`: 500 ms (pisca mais devagar).
    -   Timer: `repeating_timer` do SDK do Pico.
-   **Taxa de Leitura de Eixos:** O loop principal alterna os canais ADC a cada 20ms, resultando em uma taxa de amostragem efetiva de aproximadamente 25Hz por eixo.

## 🧠 Estrutura do Código

-   **Variáveis Voláteis:** Variáveis compartilhadas entre o loop principal e o handler de interrupção (como `centro_y/x`, flags de estado, variáveis do timer) são marcadas como `volatile`.
-   **`init_led()`:** Configura o pino `LED_AZUL_GPIO` como saída digital.
-   **`calibrar_joystick()`:** Realiza a rotina de calibração inicial e define `centro_y` e `centro_x`.
-   **`blink_led_callback()`:** Função chamada pelo `repeating_timer`. Altera o estado do pino do LED Azul.
-   **`adc_irq_handler()`:** O coração do processamento. Chamado a cada nova amostra no FIFO do ADC. Lê o valor, verifica o canal, detecta movimento rápido, e aplica a lógica específica de cada eixo (cálculo do delay de piscar para Y, detecção de deadzone e mensagens para X).
-   **`main()`:** Inicializa stdio, LEDs, ADC (GPIOs, IRQ, FIFO, Round-Robin), executa a calibração, inicia o ADC em modo free-running e entra em um loop infinito que gerencia a alternância entre a leitura dos canais Y e X do ADC.

## ▶️ Modo de Uso

1.  Carregue o firmware no Raspberry Pi Pico.
2.  Abra um monitor serial (como minicom, PuTTY, Thonny) conectado ao Pico na taxa de 115200 baud.
3.  Observe a mensagem de início e aguarde a conclusão da calibração ("Calibração concluída..."). Mantenha o joystick centrado durante a calibração.
4.  Movimente o joystick:
    -   Mova o eixo Y para fora da deadzone: O LED Azul começará a piscar. Mova mais para longe do centro no eixo Y (para cima ou para baixo) para fazer o LED piscar mais rápido. Mova-o de volta para o centro da deadzone para parar o piscar.
    -   Mova o eixo X para fora da deadzone (para a direita ou esquerda): Mensagens como "Eixo X ACIONADO!" e "Eixo X DESATIVADO!" aparecerão no monitor serial.
    -   Movimente o joystick rapidamente em qualquer eixo: Mensagens como "Movimento RÁPIDO no eixo Y/X!" aparecerão no monitor serial.

## Propósito

Este projeto foi desenvolvido com fins estritamente educacionais e aprendizado durante a residência em sistemas embarcados pelo embarcatech
