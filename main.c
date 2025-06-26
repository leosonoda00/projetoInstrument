/**
 * Sistema de Monitoramento de Temperatura com display 128x32
 * 
 * Funcionalidades:
 * - Leitura analógica de temperatura via diodo 1N4148
 * - Filtragem por média móvel para leituras estáveis
 * - Exibição em display OLED 128x32
 * - Troca de unidade (Celsius/Fahrenheit) por botão
 * - LED indicador para temperatura abaixo de 40°C
 * - Eficiência energética com modo sleep
 */


/* 1. BIBLIOTECAS NECESSÁRIAS */

#include <stdio.h>       // Funções padrão de entrada/saída
#include <string.h>      // Manipulação de strings
#include <stdlib.h>      
#include <ctype.h>       // Manipulação de caracteres
#include "pico/stdlib.h" // SDK do Raspberry Pi Pico
#include "pico/binary_info.h" // Metadados para ferramentas
#include "hardware/i2c.h"     // Comunicação I2C
#include "hardware/adc.h"     // Conversor Analógico-Digital
#include "hardware/gpio.h"    // Controle de GPIO
#include "hardware/sync.h"    // Funções de sincronização (inclui __wfi)
#include "ssd1306_font.h"     // Fonte para o display OLED


/* 2. DEFINIÇÕES E CONSTANTES */

// Configurações do display OLED (dadas pelo próprio exemplo da Adafruit)
#define SSD1306_HEIGHT      32      // Altura do display em pixels
#define SSD1306_WIDTH       128     // Largura do display em pixels
#define SSD1306_I2C_ADDR    _u(0x3C) // Endereço I2C do display
#define SSD1306_I2C_CLK     400     // Clock I2C em kHz (padrão)
#define SSD1306_PAGE_HEIGHT _u(8)   // Altura de uma página (padrão)
#define SSD1306_NUM_PAGES   (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN     (SSD1306_NUM_PAGES * SSD1306_WIDTH)

// Configurações do ADC
#define ADC_PIN     26      // GPIO 26 (Canal ADC0)
#define ADC_NUM     0       // Número do canal ADC
#define ADC_VREF    3.3f  // Tensão de referência medida 
#define ADC_RANGE   (1 << 12) // Faixa do ADC (12 bits = 4096 valores)

// Pinos GPIO
#define LED_PIN     11      // GPIO para o LED indicador
#define BUTTON_PIN  10      // GPIO para o botão de troca de unidade

// Configurações da média móvel
#define MOVING_AVG_SIZE 40  // Tamanho da janela para média móvel


/* 3. VARIÁVEIS GLOBAIS */

// Histórico para média móvel
float temp_history[MOVING_AVG_SIZE];   // Buffer circular de temperaturas
int history_index = 0;                 // Índice atual no buffer
bool history_filled = false;           // Flag indicando buffer cheio

// Controle do sistema
volatile bool button_pressed = false;  // Flag para botão pressionado
bool show_fahrenheit = false;          // Unidade de exibição (false=Celsius)
bool update_display = false;           // Flag para atualizar display
float raw_temp = 0.0f;                 // Temperatura bruta (Celsius)
float filtered_temp = 0.0f;            // Temperatura filtrada (Celsius)
float voltage = 0.0f;                  // Tensão lida do ADC


/* 4. ESTRUTURAS DE DADOS */

// Área de renderização para o display OLED (dado pelo próprio exemplo da Adafruit)
struct render_area {
    uint8_t start_col;  
    uint8_t end_col;    
    uint8_t start_page;
    uint8_t end_page;
    int buflen;
};


/* 5. FUNÇÕES DO DISPLAY OLED */ 
// Dadas pelo próprio exemplo da Adafruit

void calc_render_area_buflen(struct render_area *area) {
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

void SSD1306_send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x80, cmd};
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buf, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i=0; i<num; i++)
        SSD1306_send_cmd(buf[i]);
}

void SSD1306_send_buf(uint8_t buf[], int buflen) {
    uint8_t *temp_buf = malloc(buflen + 1);
    temp_buf[0] = 0x40;
    memcpy(temp_buf+1, buf, buflen);
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);
    free(temp_buf);
}

void SSD1306_init() {
    uint8_t cmds[] = {
        0xAE,       // SET_DISP: display off
        0x20, 0x00, // SET_MEM_MODE: horizontal addressing
        0x40,       // SET_DISP_START_LINE: start line 0
        0xA1,       // SET_SEG_REMAP: column 127 mapped to SEG0
        0xA8, 0x1F, // SET_MUX_RATIO: height-1 (31 for 32px display)
        0xC8,       // SET_COM_OUT_DIR: scan from COM[N-1] to COM0
        0xD3, 0x00, // SET_DISP_OFFSET: no offset
        0xDA, 0x02, // SET_COM_PIN_CFG: sequential, disable COM left/right remap (32px height)
        0xD5, 0x80, // SET_DISP_CLK_DIV: div ratio 1, standard freq
        0xD9, 0xF1, // SET_PRECHARGE: Vcc internally generated
        0xDB, 0x30, // SET_VCOM_DESEL: 0.83xVcc
        0x81, 0xFF, // SET_CONTRAST: max contrast
        0xA4,       // SET_ENTIRE_ON: output follows RAM content
        0xA6,       // SET_NORM_DISP: normal display (not inverted)
        0x8D, 0x14, // SET_CHARGE_PUMP: enable, Vcc internally generated
        0xAF        // SET_DISP: display on
    };

    SSD1306_send_cmd_list(cmds, sizeof(cmds)/sizeof(cmds[0]));
}

static inline int GetFontIndex(uint8_t ch) {
    if (ch >= 'A' && ch <='Z') return ch - 'A' + 1;
    else if (ch >= '0' && ch <='9') return ch - '0' + 27;
    else return 0; // Space for unsupported characters
}

static void WriteChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8) return;
    y = y/8;
    ch = toupper(ch);
    int idx = GetFontIndex(ch);
    int fb_idx = y * 128 + x;
    for (int i=0; i<8; i++) buf[fb_idx++] = font[idx * 8 + i];
}

static void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8) return;
    while (*str) WriteChar(buf, x, y, *str++), x+=8;
}

void render(uint8_t *buf, struct render_area *area) {
    uint8_t cmds[] = {
        0x21, area->start_col, area->end_col, // SET_COL_ADDR
        0x22, area->start_page, area->end_page // SET_PAGE_ADDR
    };
    SSD1306_send_cmd_list(cmds, sizeof(cmds)/sizeof(cmds[0]));
    SSD1306_send_buf(buf, area->buflen);
}


/* 6. FUNÇÕES DE PROCESSAMENTO */


// Lê o valor do ADC e converte para tensão
float read_adc_voltage() {
    uint16_t raw = adc_read(); // Lê valor bruto (0-4095)
    return (raw * ADC_VREF) / ADC_RANGE; // Converte para tensão
}

// Conversão da tensão para temperatura em celcius
float voltage_to_temperature(float voltage) {
    return (voltage - 0.6264) / (-0.0021);
}

// Conversão de Celcius para Fahrenheit
float celsius_to_fahrenheit(float celsius) {
    return (celsius * 9.0f / 5.0f) + 32.0f;
}

// Calculadora da média móvel
float moving_average(float new_temp) {
    // 1. Armazena nova temperatura no buffer circular
    temp_history[history_index] = new_temp;
    
    // 2. Atualiza índice (comportamento circular)
    history_index = (history_index + 1) % MOVING_AVG_SIZE;
    
    // 3. Marca quando o buffer está completamente preenchido
    if (history_index == 0) {
        history_filled = true;
    }
    
    // 4. Calcula a média
    float sum = 0;
    int count = history_filled ? MOVING_AVG_SIZE : history_index;
    
    for (int i = 0; i < count; i++) {
        sum += temp_history[i];
    }
    
    return sum / count; // Retorna a média
}


/* 7. INTERRUPÇÕES E CALLBACKS */

// Interrupção e debounce do botão
void button_isr(uint gpio, uint32_t events) {
    // 1. Desabilita temporariamente a interrupção para debounce
    gpio_set_irq_enabled(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, false);
    
    // 2. Marca que o botão foi pressionado
    button_pressed = true;
    
    // 3. Agenda reabilitação da interrupção após 200ms (debounce)
    add_alarm_in_ms(200, (alarm_callback_t)re_enable_button_irq, NULL, false);
}

// Libera a interrupção do botão pós debounce
bool re_enable_button_irq(alarm_id_t id, void *user_data) {
    gpio_set_irq_enabled(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true);
    return false; // Não repetir
}

// Leitura periódica do ADC (sensor) e controle de saídas (OLED e LED)
bool adc_timer_callback(repeating_timer_t *rt) {
    // 1. Lê tensão do ADC
    voltage = read_adc_voltage();
    
    // 2. Converte tensão para temperatura (Celsius)
    raw_temp = voltage_to_temperature(voltage);
    
    // 3. Calcula temperatura filtrada (média móvel)
    filtered_temp = moving_average(raw_temp);
    
    // 4. Controle do LED (acende se temperatura < 40°C)
    gpio_put(LED_PIN, filtered_temp < 40.0f);
    
    // 5. Solicita atualização do display
    update_display = true;
    
    return true; // Mantém o timer ativo
}


/* 8. FUNÇÃO PRINCIPAL */

int main() {
    // Inicializa comunicação serial (para depuração)
    stdio_init_all();
    
    // Verifica se a placa tem suporte a I2C (dado pelo exemplo da própria Adafruit)
#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c / SSD1306_i2d example requires a board with I2C pins
    puts("Default I2C pins were not defined");
#else

    /* 8.1 INICIALIZAÇÕES */
    
    // Configura I2C
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    i2c_init(i2c_default, SSD1306_I2C_CLK * 1000); // Inicializa I2C com clock de 400kHz
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C); // Configura pino SDA
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C); // Configura pino SCL
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN); // Habilita pull-up
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN); // Habilita pull-up

    // Inicializa ADC
    adc_init(); // Habilita o bloco ADC
    adc_gpio_init(ADC_PIN); // Configura GPIO26 como entrada analógica
    adc_select_input(ADC_NUM); // Seleciona o canal ADC0

    // Configura LED
    gpio_init(LED_PIN); // Inicializa pino
    gpio_set_dir(LED_PIN, GPIO_OUT); // Define como saída
    gpio_put(LED_PIN, 0); // Inicia desligado

    // Configura Botão
    gpio_init(BUTTON_PIN); // Inicializa pino
    gpio_set_dir(BUTTON_PIN, GPIO_IN); // Define como entrada
    gpio_pull_up(BUTTON_PIN); // Habilita resistor de pull-up
    // Configura interrupção para borda de descida (botão pressionado)
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);

    // Inicializa display OLED
    SSD1306_init();
    struct render_area frame_area = {
        .start_col = 0, 
        .end_col = SSD1306_WIDTH - 1,
        .start_page = 0,
        .end_page = SSD1306_NUM_PAGES - 1
    };
    calc_render_area_buflen(&frame_area); // Calcula tamanho do buffer

    // Configura timer para leitura periódica do ADC (500ms)
    repeating_timer_t adc_timer;
    add_repeating_timer_ms(500, adc_timer_callback, NULL, &adc_timer);

    /* 8.2 LOOP PRINCIPAL */

    while (1) {
        // 1. Tratamento do botão
        if (button_pressed) {
            button_pressed = false;
            show_fahrenheit = !show_fahrenheit; // Alterna unidade
            update_display = true; // Força atualização do display
        }

        // 2. Atualização do display quando necessário
        if (update_display) {
            update_display = false; // Reseta flag
            
            // Prepara buffer de exibição
            uint8_t buf[SSD1306_BUF_LEN];
            memset(buf, 0, SSD1306_BUF_LEN); // Limpa buffer
            
            // 2.1. Converte unidades se necessário
            float display_temp = filtered_temp;
            if (show_fahrenheit) {
                display_temp = celsius_to_fahrenheit(display_temp);
            }
            
            // 2.2. Formata strings
            char voltage_str[16];
            char temp_str[16];
            sprintf(voltage_str, "%.3f V", voltage);
            sprintf(temp_str, "%.1f %c", display_temp, show_fahrenheit ? 'F' : 'C');
            
            // 2.3. Escreve no buffer
            WriteString(buf, 10, 0, "Tensao:");
            WriteString(buf, 70, 0, voltage_str);
            WriteString(buf, 10, 8, "Temp:");
            WriteString(buf, 70, 8, temp_str);
            
            // 2.4. Atualiza display
            render(buf, &frame_area);
        }

        // 3. Entra em modo de baixo consumo (Wait For Interrupt)
        __wfi(); // Reduz consumo enquanto aguarda eventos
    }
#endif
    return 0;
}

//testando