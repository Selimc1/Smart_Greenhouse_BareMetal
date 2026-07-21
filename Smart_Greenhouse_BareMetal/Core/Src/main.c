#include "stm32f1xx.h"
#include <stdio.h>

// ==========================================
// 1. DELAY FUNCTIONS (SYSTICK)
// ==========================================
void Delay_us(uint32_t us) {
    SysTick->LOAD = (8 * us) - 1; // 1 microsecond for 8MHz core clock
    SysTick->VAL = 0;
    SysTick->CTRL = 5; // Start SysTick (Internal Clock)
    while (!(SysTick->CTRL & 0x10000)); // Wait for the flag
    SysTick->CTRL = 0; // Stop SysTick
}

void Delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) Delay_us(1000);
}

// ==========================================
// 2. UART (SERIAL COMMUNICATION)
// ==========================================
void UART1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;

    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |= (0xB << 4);  // PA9 TX: Alternate Function Push-Pull

    USART1->BRR = 0x0341;      // 9600 Baud Rate @ 8MHz
    USART1->CR1 |= USART_CR1_TE | USART_CR1_UE; // Enable TX and UART
}

void UART_Print(char *str) {
    while (*str) {
        while (!(USART1->SR & USART_SR_TXE)); // Wait until TX buffer is empty
        USART1->DR = *str++;
    }
}

// ==========================================
// 3. PWM (MOTOR CONTROL - TIM2 CH3)
// ==========================================
void TIM2_PWM_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(0xF << 8);
    GPIOA->CRL |= (0xB << 8);  // PA2: Alternate Function Push-Pull (L293D EN1)

    TIM2->PSC = 79;  // Prescaler: 100kHz clock
    TIM2->ARR = 999; // Auto-Reload: 100Hz PWM Frequency

    TIM2->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2; // PWM Mode 1 for CH3
    TIM2->CCER |= TIM_CCER_CC3E; // Enable CH3 Output
    TIM2->CR1 |= TIM_CR1_CEN;    // Start Timer 2

    TIM2->CCR3 = 0; // Initial fan speed is 0
}

// ==========================================
// 4. EXTI (SECURITY BUTTON - PB10)
// ==========================================
void EXTI_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    GPIOB->CRH &= ~(0xF << 8);
    GPIOB->CRH |= (0x8 << 8);  // PB10: Input with Pull-up/Pull-down
    GPIOB->ODR |= (1 << 10);   // Enable internal Pull-up

    AFIO->EXTICR[2] &= ~(0xF << 8);
    AFIO->EXTICR[2] |= (0x1 << 8); // Map EXTI10 to PB10

    EXTI->IMR |= (1 << 10);  // Unmask EXTI10
    EXTI->FTSR |= (1 << 10); // Trigger on Falling Edge
    NVIC_EnableIRQ(EXTI15_10_IRQn); // Enable EXTI interrupt in NVIC
}

void EXTI15_10_IRQHandler(void) {
    if (EXTI->PR & (1 << 10)) {
        EXTI->PR |= (1 << 10); // Clear pending bit

        TIM2->CCR3 = 0; // Emergency: Stop the fan immediately
        UART_Print("\r\n!!! ALARM: UNAUTHORIZED DOOR OPENING !!!\r\n");
    }
}

// ==========================================
// 5. DHT11 - SYSTICK TIMER LOGIC
// ==========================================
uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity) {
    uint8_t data[5] = {0};
    uint32_t count = 0;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // Set PA1 to Open-Drain Output
    GPIOA->CRL &= ~(0xF << 4);
    GPIOA->CRL |= (0x7 << 4);

    // 1. Send Start Signal (20ms LOW)
    GPIOA->ODR &= ~(1 << 1);
    Delay_ms(20);

    // 2. Release Line and Listen
    GPIOA->ODR |= (1 << 1);
    Delay_us(30);

    // 3. Wait for Sensor Response
    count = 0; while((GPIOA->IDR & (1 << 1)))  { if(++count > 500000) return 11; }
    count = 0; while(!(GPIOA->IDR & (1 << 1))) { if(++count > 500000) return 12; }
    count = 0; while((GPIOA->IDR & (1 << 1)))  { if(++count > 500000) return 13; }

    // 4. Read 40-Bit Data
    for (int i = 0; i < 5; i++) {
        for (int j = 7; j >= 0; j--) {
            // Skip the initial 50us LOW signal for each bit
            count = 0; while(!(GPIOA->IDR & (1 << 1))) { if(++count > 500000) return 14; }

            // Pin is HIGH! Wait exactly 40 microseconds
            Delay_us(40);

            // If pin is still HIGH after 40us, the bit is '1'
            if (GPIOA->IDR & (1 << 1)) {
                data[i] |= (1 << j);
                // Wait for the remaining HIGH duration to finish
                count = 0; while((GPIOA->IDR & (1 << 1))) { if(++count > 500000) return 15; }
            }
        }
    }

    // 5. Checksum and Fake Zero Validation
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) == data[4]) {
        // Drop invalid fully-zero readings
        if (data[0] == 0 && data[2] == 0 && data[4] == 0) return 17;

        *humidity = data[0];
        *temperature = data[2];
        return 1; // Success
    }

    return 16; // Checksum Error
}

// ==========================================
// 6. MAIN LOOP
// ==========================================
int main(void) {
    UART1_Init();
    TIM2_PWM_Init();
    EXTI_Init();

    Delay_ms(1000); // System boot delay
    UART_Print("\r\n======================================\r\n");
    UART_Print("STM32 Smart Greenhouse System Started...\r\n");
    UART_Print("Hardware: Bare-Metal Register Level\r\n");
    UART_Print("======================================\r\n\n");

    uint8_t temp = 0, hum = 0;
    char buffer[100];

    while(1) {
        uint8_t status = DHT11_Read(&temp, &hum);

        if (status == 1) {
            // Map humidity to fan speed (PWM duty cycle)
            uint32_t fan_speed = hum * 10;
            if(fan_speed > 999) fan_speed = 999;
            TIM2->CCR3 = fan_speed;

            sprintf(buffer, "Temp: %d C | Hum: %% %d | Fan Pwr: %% %d\r\n", temp, hum, hum);
            UART_Print(buffer);
        } else {
            sprintf(buffer, "[ERROR] DHT11 Read Issue! (Error Code: %d)\r\n", status);
            UART_Print(buffer);
        }

        Delay_ms(2000); // 2-second polling interval
    }
}
