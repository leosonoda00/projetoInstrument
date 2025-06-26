# Monitoramento de Temperatura de uma composteira com Raspberry Pi Pico e Display OLED 

Este projeto, desenvolvido para as disciplinas de Instrumentação Eletrônica (ECA409) e Sistemas Microcontrolados (ECA407), apresenta o desenvolvimento de um sistema completo para o monitoramento de temperatura em composteiras. A base do sistema é a construção de um sensor de temperatura, que utilizando o diodo 1N4148 como principal elemento. 

O princípio de funcionamento explora a relação linear que existe entre a tensão de polarização direta do diodo e a temperatura ambiente. O hardware é centrado no microcontrolador Raspberry Pi Pico (RP2040), que realiza o condicionamento do sinal e o processamento dos dados, feito em C/C++ utiliza o Pico SDK. Os resultados da medição são exibidos em tempo real em um display OLED para fácil visualização. 

![image](https://github.com/user-attachments/assets/e324176e-713f-46a3-8d78-a148e0cbfb3a) ![image](https://github.com/user-attachments/assets/7b225a1c-8830-499e-8062-d8e8a5066d73)
![image](https://github.com/user-attachments/assets/fd949ad4-a5ea-4863-9156-2e188ff26bcc)
## Funcionalidades

- **Leitura de Temperatura:** Utiliza a variação de tensão em um diodo comum (1N4148) para aferir a temperatura ambiente.
- **Filtro de Média Móvel:** Suaviza as leituras do sensor para fornecer um valor mais estável e preciso.
- **Display OLED:** Exibe a tensão lida e a temperatura (em °C ou °F) em um display OLED de 128x32 pixels.
- **Botão de Interação:** Permite ao usuário alternar a unidade de temperatura entre Celsius (°C) e Fahrenheit (°F) com um simples clique.
- **LED Indicador:** Acende para indicar visualmente que a temperatura está abaixo de um limiar pré-definido (40°C no código).
- **Eficiência Energética:** Utiliza o modo `sleep` (Wait For Interrupt) para minimizar o consumo de energia, "acordando" apenas para realizar leituras ou responder a eventos.

---

## Hardware Necessário

| Componente                | Quantidade | Observações                               |
| :------------------------ | :--------- | :---------------------------------------- |
| Raspberry Pi Pico         | 1          | -           |
| Display OLED 128x32 I2C   | 1          | Modelo com driver SSD1306.                |
| Diodo 1N4148              | 1          | Usado como sensor de temperatura.         |
| Resistor de 10kΩ          | 1          | Resistor limitador de corrente para o diodo.       |
| Botão (Push Button)       | 1          | Para trocar a unidade de medida.          |
| LED (5mm, qualquer cor)   | 1          | Indicador visual.                         |
| Resistor de 330Ω          | 1          | Para limitar a corrente do LED.           |
| Protoboard e Jumpers      | -          | Para montagem do circuito.                |
| Diodo 1N4007 | 1 | Segurança para caso de alimentação ao contrário. |

---

## Diagrama de Conexões

![image](https://github.com/user-attachments/assets/5b9d6de1-605c-472b-aeaf-6e6ac170f104)

### Tabela de Conexões (Baseada Literalmente no Esquemático)

| Componente | Pino no Componente | Pino na Raspberry Pi Pico | Observações |
| :--- | :--- | :--- | :--- |
| **Display OLED** | VCC | 5V (VBUS - Pino 40)| Ligado na alimentação de 5V. |
| | GND | GND - Pino 38 | Terra. |
| | SCL | GP5 (I2C0 SCL) - Pino 7 | Clock do I2C. |
| | SDA | GP4 (I2C0 SDA) - Pino 6 | Dados do I2C. |
| **Sensor (Diodo)** | Ânodo (+) | GP26 (ADC0) - Pino 31 | Conectado ao resistor R1. |
| | Cátodo (-) | GND | |
| **Resistor R1 10kΩ**| Terminal 1 | 5V (VBUS - Pino 40)| Ligado na alimentação de 5V. |
| | Terminal 2 | GP26 (ADC0) - Pino 31 | Ponto de leitura para o sensor. |
| **Botão** | Terminal 1 | GP10 - Pino 14 | |
| | Terminal 2 | GND | |
| **LED** | Ânodo (+) | - | Conectado ao Resistor R2. |
| | Cátodo (-) | GND | |
| **Resistor R2 330Ω**| Terminal 1 | GP11 - Pino 15 | Limita a corrente para o LED. |
| | Terminal 2 | - | Conectado ao Ânodo (+) do LED. |

---

## Software do Projeto

O projeto foi feito no Vscode (Visual Studio Code), com a extensão Raspberry Pi Pico, na linguagem C/C++ em SDK Pico

Nesse projeto foi utilizado o display OLED 128x32, e foram utilizadas algumas bibliotecas e funções dos exemplos dados pela própria extensão Raspberry Pi Pico


`CMakeLists.txt`: Arquivo de configuração do CMake, responsável por definir como o projeto é compilado, listar os arquivos-fonte e vincular as bibliotecas necessárias do Pico SDK, como hardware_i2c e hardware_adc. 

`main.c`: Contém toda a lógica principal do sistema. É responsável pela inicialização dos periféricos (ADC, I2C, GPIO), leitura da temperatura do diodo, aplicação do filtro de média móvel, controle do display OLED e gerenciamento de eventos (botão e timer).

`ssd1306_font.h`: Arquivo de cabeçalho que contém os dados (em formato de array de bytes) da fonte utilizada para desenhar os caracteres alfanuméricos no display OLED.

`raspberry26x32.h`: Arquivo de cabeçalho que armazena os dados do bitmap para uma imagem de 26x32 pixels do logo da Raspberry Pi.

## Funcionamento do Código

O código é estruturado em torno de um loop principal de baixo consumo (`__wfi()`) que é "acordado" por duas interrupções principais:

1.  **Timer Periódico (`adc_timer_callback`):** A cada 500ms, o sistema realiza a leitura da tensão no pino ADC, converte-a para temperatura, atualiza o filtro de média móvel e controla o LED.
2.  **Interrupção de GPIO (`button_isr`):** Ocorre quando o botão é pressionado. A rotina de interrupção apenas sinaliza ao loop principal que a unidade de exibição deve ser trocada, implementando um debounce por software para evitar múltiplos acionamentos.

### Calibração do Sensor

A conversão da tensão lida no diodo para temperatura em Celsius é feita com a seguinte fórmula:

`Temperatura (°C) = (Tensão_ADC - 0.6264) / (-0.0021)`

Esses valores de calibração (`0.6264` e `-0.0021`) não são arbitrários. Eles foram obtidos a partir de dados experimentais documentados no artigo **"Termômetro de Alta Sensibilidade Usando Diodo Semicondutor como Elemento Sensor" (2012)**

---



![image](https://github.com/user-attachments/assets/c70c6e0e-c489-4e90-ba06-1f685e367dfe)


## Referências

- Silva, G. V. et al. (2012). *Termômetro de Alta Sensibilidade Usando Diodo Semicondutor como Elemento Sensor*. CEEL 2012. Disponível em: [https://www.peteletricaufu.com.br/static/ceel/doc/artigos/artigos2012/ceel2012_artigo044_r01.pdf](https://www.peteletricaufu.com.br/static/ceel/doc/artigos/artigos2012/ceel2012_artigo044_r01.pdf)

---
