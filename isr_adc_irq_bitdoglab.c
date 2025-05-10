#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "pico/time.h"

// --- PINOS ---
// Pinos do Joystick
#define JOY_X_ADC_CHANNEL 1     // ADC1 = GPIO27 (Pico pin 32) - Eixo X
#define JOY_Y_ADC_CHANNEL 0     // ADC0 = GPIO26 (Pico pin 31) - Eixo Y
#define LED_AZUL_GPIO 12

// --- CALIBRAÇÃO E ZONA MORTA ---
// ✅ Requisito atendido: Ajustar o valor de limite para calibrar o joystick
#define CALIBRATION_SAMPLES 50  // Número de amostras para calcular o centro do joystick na calibração
#define DEADZONE_Y 150          // Zona morta para o eixo Y (valores dentro de centro_y +/- DEADZONE_Y são ignorados)
#define DEADZONE_X 150          // Zona morta para o eixo X
volatile uint16_t centro_y = 2048; // Valor central calibrado para o eixo Y (ADC lê de 0 a 4095)
volatile uint16_t centro_x = 2048; // Valor central calibrado para o eixo X

// --- PISCAR PROPORCIONAL (controlado pelo Eixo Y) ---
// ✅ Requisito atendido: Fazer o LED piscar proporcionalmente à posição do joystick
#define MIN_BLINK_DELAY_MS 50   // Delay mínimo entre piscadas (LED pisca mais rápido)
#define MAX_BLINK_DELAY_MS 500  // Delay máximo entre piscadas (LED pisca mais devagar)
volatile int current_blink_delay_ms = MAX_BLINK_DELAY_MS; // Delay atual, ajustado pela posição do joystick
volatile bool led_blinking_active = false; // Flag que indica se o LED deve estar piscando
struct repeating_timer blinky_timer; // Estrutura do timer para controlar o piscar
volatile bool led_is_on_in_blink_cycle = false; // Estado do LED (ligado/desligado) durante um ciclo de piscar

// --- DETECÇÃO DE MOVIMENTO RÁPIDO ---
// ✅ Requisito atendido: Implementar detecção de movimentos rápidos (parcialmente aqui, com a constante)
#define FAST_MOVE_THRESHOLD 250 // Limiar da diferença entre leituras ADC para considerar um movimento rápido
volatile uint16_t last_adc_y_value = 0; // Última leitura ADC do eixo Y, para calcular variação
volatile uint16_t last_adc_x_value = 0; // Última leitura ADC do eixo X

// --- ESTADO DO SISTEMA ---
volatile bool acionado_y = false; // Flag: true se o eixo Y está fora da zona morta
volatile bool acionado_x = false; // Flag: true se o eixo X está fora da zona morta

#define ADC_RANGE 4095 // Valor máximo de leitura do ADC (12 bits)

// Protótipos das funções
void adc_irq_handler();
bool blink_led_callback(struct repeating_timer *t);
void calibrar_joystick();
void init_led();


// Função para inicializar o pino do LED Azul
void init_led() {
    gpio_init(LED_AZUL_GPIO);
    gpio_set_dir(LED_AZUL_GPIO, GPIO_OUT);
    gpio_put(LED_AZUL_GPIO, 0); // Começa com o LED Azul desligado
}

// Função para calibrar a posição central do joystick
void calibrar_joystick() {
    // ✅ Requisito atendido: Ajustar o valor de limite para calibrar o joystick (implementação principal)
    printf("Iniciando calibração do joystick. Mantenha-o em repouso...\n");
    uint32_t sum_y = 0; // Acumulador para leituras do eixo Y
    uint32_t sum_x = 0; // Acumulador para leituras do eixo X

    // Calibra Eixo Y (ADC0 = GPIO26)
    adc_select_input(JOY_Y_ADC_CHANNEL); // Seleciona o canal ADC do eixo Y
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        sum_y += adc_read(); // Lê o valor do ADC e soma
        sleep_ms(10);        // Pequena pausa entre leituras
    }
    centro_y = sum_y / CALIBRATION_SAMPLES; // Calcula a média para definir o centro
    last_adc_y_value = centro_y;           // Inicializa o último valor lido com o valor central

    // Calibra Eixo X (ADC1 = GPIO27)
    adc_select_input(JOY_X_ADC_CHANNEL); // Seleciona o canal ADC do eixo X
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        sum_x += adc_read();
        sleep_ms(10);
    }
    centro_x = sum_x / CALIBRATION_SAMPLES;
    last_adc_x_value = centro_x;

    printf("Calibração concluída: Centro Y(G26)=%d, Centro X(G27)=%d\n", centro_y, centro_x);
}

// Callback do timer para fazer o LED piscar
bool blink_led_callback(struct repeating_timer *t) {
    // ✅ Requisito atendido: Fazer o LED piscar proporcionalmente à posição do joystick (lógica de ligar/desligar)
    if (led_blinking_active) {
        // Alterna o estado do LED
        led_is_on_in_blink_cycle = !led_is_on_in_blink_cycle;
        if (led_is_on_in_blink_cycle) {
            gpio_put(LED_AZUL_GPIO, 1); // Liga o LED Azul
        } else {
            gpio_put(LED_AZUL_GPIO, 0); // Desliga o LED Azul
        }
    } else {
        // Se o piscar não estiver ativo, garante que o LED esteja desligado
        gpio_put(LED_AZUL_GPIO, 0);
    }
    return true; // Mantém o timer rodando
}

// Handler da Interrupção do ADC (chamado quando há novos dados do ADC)
void adc_irq_handler() {
    uint16_t valor = adc_fifo_get(); // Pega o valor convertido do ADC
    uint8_t channel_read = adc_get_selected_input(); // Verifica qual canal ADC foi lido

    // Processamento para o Eixo Y
    if (channel_read == JOY_Y_ADC_CHANNEL) {
        // ✅ Requisito atendido: Implementar detecção de movimentos rápidos (variações abruptas de valor) - para Eixo Y
        if (abs(valor - last_adc_y_value) > FAST_MOVE_THRESHOLD) {
            printf("Movimento RÁPIDO no eixo Y! Delta: %d\n", abs(valor - last_adc_y_value));
        }
        last_adc_y_value = valor; // Atualiza o último valor lido para o eixo Y
        int delta_y = valor - centro_y; // Calcula a diferença da posição atual para o centro calibrado

        // Verifica se o joystick saiu da zona morta do eixo Y
        // ✅ Requisito atendido: Ajustar o valor de limite para calibrar o joystick (uso da deadzone e centro)
        if (abs(delta_y) > DEADZONE_Y) {
            if (!acionado_y) {
                acionado_y = true; // Marca que o eixo Y está ativo
            }

            // ✅ Requisito atendido: Fazer o LED piscar proporcionalmente à posição do joystick (cálculo do delay)
            // Calcula o delay do piscar: quanto maior o delta, menor o delay (pisca mais rápido)
            int effective_delta = abs(delta_y) - DEADZONE_Y;
            int max_movable_delta = (ADC_RANGE / 2) - DEADZONE_Y; // Máximo deslocamento útil fora da deadzone
            if (effective_delta > max_movable_delta) effective_delta = max_movable_delta;
            if (max_movable_delta <= 0) max_movable_delta = 1; // Evita divisão por zero

            // Mapeia o delta para a faixa de delays (MIN_BLINK_DELAY_MS a MAX_BLINK_DELAY_MS)
            current_blink_delay_ms = MAX_BLINK_DELAY_MS - (((long)effective_delta * (MAX_BLINK_DELAY_MS - MIN_BLINK_DELAY_MS)) / max_movable_delta);

            // Garante que o delay esteja dentro dos limites definidos
            if (current_blink_delay_ms < MIN_BLINK_DELAY_MS) current_blink_delay_ms = MIN_BLINK_DELAY_MS;
            if (current_blink_delay_ms > MAX_BLINK_DELAY_MS) current_blink_delay_ms = MAX_BLINK_DELAY_MS;

            // Gerencia o timer de piscar
            if (!led_blinking_active) { // Se o LED não estava piscando, inicia o piscar
                led_blinking_active = true;
                led_is_on_in_blink_cycle = true; // Faz o LED acender no primeiro ciclo
                gpio_put(LED_AZUL_GPIO, 1);      // Liga o LED Azul imediatamente

                if (blinky_timer.alarm_id != 0) cancel_repeating_timer(&blinky_timer); // Cancela timer antigo, se houver
                // Adiciona (ou reinicia) o timer com o novo delay calculado
                if (!add_repeating_timer_ms(current_blink_delay_ms, blink_led_callback, NULL, &blinky_timer)) {
                    printf("Erro ao adicionar timer para piscar Y!\n");
                }
            } else { // Se o LED já estava piscando, apenas ajusta o delay do timer
                if (blinky_timer.alarm_id != 0) cancel_repeating_timer(&blinky_timer);
                if (!add_repeating_timer_ms(current_blink_delay_ms, blink_led_callback, NULL, &blinky_timer)) {
                    printf("Erro ao re-adicionar timer para piscar Y!\n");
                }
            }
            // Descomente para depuração:
            // printf("Y(G26):%4d |DYa:%4d |BlkDelay:%3d ms\n", valor, delta_y, current_blink_delay_ms);

        } else { // Joystick está dentro da zona morta do eixo Y
            if (acionado_y) {
                acionado_y = false; // Marca que o eixo Y não está mais ativo
            }
            // Se o LED estava piscando, para o piscar e desliga o LED
            if (led_blinking_active) {
                if (blinky_timer.alarm_id != 0) cancel_repeating_timer(&blinky_timer);
                blinky_timer.alarm_id = 0; // Marca timer como cancelado/inválido
                led_blinking_active = false;
                led_is_on_in_blink_cycle = false;
                gpio_put(LED_AZUL_GPIO, 0); // Desliga o LED Azul
            }
        }

    // Processamento para o Eixo X
    // ✅ Requisito atendido: Implementar ação para outro eixo do joystick (Eixo X)
    } else if (channel_read == JOY_X_ADC_CHANNEL) {
        // ✅ Requisito atendido: Implementar detecção de movimentos rápidos (variações abruptas de valor) - para Eixo X
        if (abs(valor - last_adc_x_value) > FAST_MOVE_THRESHOLD) {
            printf("Movimento RÁPIDO no eixo X! Delta: %d\n", abs(valor - last_adc_x_value));
        }
        last_adc_x_value = valor; // Atualiza o último valor lido para o eixo X
        int delta_x = valor - centro_x; // Calcula a diferença da posição atual para o centro

        // Verifica se o joystick saiu da zona morta do eixo X
        if (abs(delta_x) > DEADZONE_X) {
            if (!acionado_x) {
                acionado_x = true; // Marca que o eixo X está ativo
                // Ação para o eixo X: Apenas imprimir uma mensagem por enquanto
                printf("Eixo X ACIONADO! Valor: %d, Delta: %d\n", valor, delta_x);
            }
            // Descomente para depuração:
            // printf("X(G27):%4d |DXa:%4d\n", valor, delta_x);
        } else { // Joystick está dentro da zona morta do eixo X
            if (acionado_x) {
                acionado_x = false; // Marca que o eixo X não está mais ativo
                printf("Eixo X DESATIVADO. Valor: %d\n", valor);
            }
        }
    }
}

// Função principal
int main() {
    stdio_init_all(); // Inicializa todas as stdio disponíveis (USB e/ou UART)
    sleep_ms(2000); // Pausa para permitir que o terminal serial se conecte

    printf("--- Sistema Joystick com LED Azul (GPIO On/Off) ---\n");
    printf("Eixo Y (Pisca LED AZUL): GPIO26 (ADC0)\n");
    printf("Eixo X: GPIO27 (ADC1)\n");
    printf("LED Azul: GPIO%d\n", LED_AZUL_GPIO);

    init_led(); // Inicializa o LED Azul

    // Inicializa o hardware ADC
    adc_init();
    adc_gpio_init(26); // Habilita ADC no GPIO26 (ADC0) para Eixo Y
    adc_gpio_init(27); // Habilita ADC no GPIO27 (ADC1) para Eixo X

    calibrar_joystick(); // Executa a rotina de calibração

    // Configura o FIFO do ADC
    adc_fifo_setup(
        true,  // Habilita FIFO
        false, // Não usa DMA data request
        1,     // Gera IRQ com 1 amostra no FIFO
        false, // Não usa erro no bit de threshold
        false  // Sem bit de erro no FIFO
    );
    adc_set_round_robin(0); // Desabilita leitura round-robin de canais ADC (selecionamos manualmente)

    // Configura a interrupção do ADC
    irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_irq_handler); // Define a função que trata a IRQ
    adc_irq_set_enabled(true); // Habilita a fonte de IRQ específica do ADC (FIFO não vazio)
    irq_set_enabled(ADC_IRQ_FIFO, true); // Habilita a IRQ no controlador de interrupções do processador (NVIC)

    printf("Sistema pronto. Movimente o joystick.\n");

    // Configuração inicial para leitura ADC
    adc_select_input(JOY_Y_ADC_CHANNEL); // Começa lendo o eixo Y
    adc_run(true); // Habilita o ADC para rodar em modo de conversão livre (free-running)

    uint8_t current_main_loop_channel = JOY_Y_ADC_CHANNEL; // Canal ADC que o loop principal está focando

    // Loop principal infinito
    while (true) {
        // ✅ Requisito atendido: Implementar ação para outro eixo do joystick (lógica de alternar canais)
        // Para ler ambos os eixos, o ADC é parado, o canal é trocado,
        // o FIFO é limpo (para evitar ler dados antigos do canal anterior),
        // e o ADC é reiniciado no novo canal.
        adc_run(false); // Para o ADC temporariamente para mudar o canal

        // Alterna entre canal Y e X
        if (current_main_loop_channel == JOY_Y_ADC_CHANNEL) {
            current_main_loop_channel = JOY_X_ADC_CHANNEL;
        } else {
            current_main_loop_channel = JOY_Y_ADC_CHANNEL;
        }
        adc_select_input(current_main_loop_channel); // Seleciona o novo canal
        adc_fifo_drain(); // Limpa o FIFO de amostras antigas do canal anterior
        adc_run(true);  // Reinicia o ADC no novo canal selecionado

        // Pequeno delay para permitir que o ADC realize algumas conversões no canal atual
        // e para que a IRQ seja processada antes de trocar de canal novamente.
        // O tempo total para ler ambos os eixos será aproximadamente 2 * este delay.
        // Ex: 20ms aqui -> cada eixo é lido por 20ms, total ~40ms por ciclo completo X+Y (~25Hz por eixo).
        sleep_ms(20);
    }
    return 0; // Esta linha nunca é alcançada
}