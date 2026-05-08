/* ============================================================
 * Projet : Gestion d'une salle de travail commune
 * MCU    : STM32F401VE  @  84 MHz (HSE 8 MHz + PLL)
 * Partie : 1 & 2  —  LCD 16x2 + LM35 + Ventilateur
 *
 * CORRECTION : VREF_MV = 5000 (LM35 alimenté en 5V dans Proteus)
 *
 * BROCHAGE :
 *   LCD RS  -> PB0
 *   LCD RW  -> GND
 *   LCD EN  -> PB1
 *   LCD D4  -> PB4
 *   LCD D5  -> PB5
 *   LCD D6  -> PB6
 *   LCD D7  -> PB7
 *   LM35 VOUT -> PA0  (ADC1_IN0)
 *   LM35 VCC  -> +5V
 *   LM35 GND  -> GND
 *   Ventil.   -> PE1
 * ============================================================ */

#include "main.h"
#include <stdio.h>
#include <string.h>

/* -- Handle ADC -------------------------------------------- */
ADC_HandleTypeDef hadc1;

/* -- Seuils température (°C) ------------------------------- */
#define TEMP_MIN   18.0f
#define TEMP_MAX   25.0f

/* -- VREF = 5000 mV car LM35 alimenté en 5V dans Proteus --- */
#define VREF_MV    5000u
#define ADC_RES    4095u

/* -- Prototypes -------------------------------------------- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);

void     LCD_Init(void);
void     LCD_SendCmd(uint8_t cmd);
void     LCD_SendData(uint8_t data);
void     LCD_SendNibble(uint8_t nibble);
void     LCD_EnablePulse(void);
void     LCD_SetCursor(uint8_t row, uint8_t col);
void     LCD_Print(char *str);
void     LCD_Clear(void);

uint16_t ADC_Read(void);
float    ADC_ToTemperature(uint16_t raw);

void     Ventilateur_ON(void);
void     Ventilateur_OFF(void);

/* -- Variable état ventilateur ----------------------------- */
uint8_t ventilateur_actif = 0;

/* ----------------------------------------------------------- */
/*                        M A I N                             */
/* ----------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();

    /* Initialisation LCD */
    LCD_Init();

    /* -- Message de bienvenue (3 secondes) ------------------- */
    LCD_Clear();
    LCD_SetCursor(0, 1);
    LCD_Print("  Bienvenue !");
    LCD_SetCursor(1, 0);
    LCD_Print(" Salle Commune ");
    HAL_Delay(3000);
    LCD_Clear();

    /* -- Boucle principale ------------------------------------ */
    while (1)
    {
        /* 1. Lire ADC et convertir en température */
        uint16_t rawADC = ADC_Read();
        float    tempC  = ADC_ToTemperature(rawADC);

        /* 2. Gestion ventilateur avec hystérésis */
        if (tempC >= TEMP_MAX)
        {
            Ventilateur_ON();
            ventilateur_actif = 1;
        }
        else if (tempC <= TEMP_MIN)
        {
            Ventilateur_OFF();
            ventilateur_actif = 0;
        }
        /* Entre 18°C et 25°C ? on maintient l'état actuel */

        /* 3. Construire les lignes LCD */
        char ligne1[17];
        char ligne2[17];

        snprintf(ligne1, sizeof(ligne1), "Temp: %.1f C    ", tempC);

        if (ventilateur_actif)
            snprintf(ligne2, sizeof(ligne2), "Ventil:  ON     ");
        else
            snprintf(ligne2, sizeof(ligne2), "Ventil:  OFF    ");

        /* 4. Afficher sur LCD */
        LCD_SetCursor(0, 0);
        LCD_Print(ligne1);
        LCD_SetCursor(1, 0);
        LCD_Print(ligne2);

        HAL_Delay(1000);   /* Rafraîchissement toutes les 1s */
    }
}

/* ----------------------------------------------------------- */
/*                   F O N C T I O N S   L C D               */
/* ----------------------------------------------------------- */

void LCD_SendNibble(uint8_t nibble)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LCD_EnablePulse(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);    /* EN = 1 */
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);  /* EN = 0 */
    HAL_Delay(1);
}

void LCD_SendCmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);  /* RS = 0 */
    LCD_SendNibble(cmd >> 4);     /* Nibble haut */
    LCD_EnablePulse();
    LCD_SendNibble(cmd & 0x0F);  /* Nibble bas  */
    LCD_EnablePulse();
    HAL_Delay(2);
}

void LCD_SendData(uint8_t data)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);    /* RS = 1 */
    LCD_SendNibble(data >> 4);
    LCD_EnablePulse();
    LCD_SendNibble(data & 0x0F);
    LCD_EnablePulse();
    HAL_Delay(1);
}

void LCD_Init(void)
{
    HAL_Delay(50);

    /* Séquence reset HD44780 */
    LCD_SendNibble(0x03); LCD_EnablePulse(); HAL_Delay(5);
    LCD_SendNibble(0x03); LCD_EnablePulse(); HAL_Delay(1);
    LCD_SendNibble(0x03); LCD_EnablePulse(); HAL_Delay(1);
    LCD_SendNibble(0x02); LCD_EnablePulse(); HAL_Delay(1);

    LCD_SendCmd(0x28);   /* 4 bits, 2 lignes, 5x8        */
    LCD_SendCmd(0x0C);   /* Display ON, curseur OFF       */
    LCD_SendCmd(0x06);   /* Incrément automatique         */
    LCD_SendCmd(0x01);   /* Effacement écran              */
    HAL_Delay(2);
}

void LCD_Clear(void)
{
    LCD_SendCmd(0x01);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_SendCmd(addr);
}

void LCD_Print(char *str)
{
    while (*str)
        LCD_SendData((uint8_t)(*str++));
}

/* ----------------------------------------------------------- */
/*                   F O N C T I O N S   A D C               */
/* ----------------------------------------------------------- */

uint16_t ADC_Read(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

/*
 * Formule LM35 avec alimentation 5V :
 *
 *   Vout (mV) = (raw / 4095) x 5000
 *   Temp (°C) = Vout / 10
 *
 * Exemple : LM35 à 80°C
 *   Vout = 800 mV
 *   raw  = (800 / 5000) x 4095 = 655
 *   Temp = ((655/4095) x 5000) / 10 = 80.0 °C ?
 */
float ADC_ToTemperature(uint16_t raw)
{
    float voltage_mV  = ((float)raw / (float)ADC_RES) * (float)VREF_MV;
    float temperature = voltage_mV / 10.0f;
    return temperature;
}

/* ----------------------------------------------------------- */
/*                   V E N T I L A T E U R                   */
/* ----------------------------------------------------------- */

void Ventilateur_ON(void)
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
}

void Ventilateur_OFF(void)
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
}

/* ----------------------------------------------------------- */
/*           C O N F I G U R A T I O N   H A L               */
/* ----------------------------------------------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSE 8 MHz ? PLL ? SYSCLK 84 MHz */
    RCC_OscInitStruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState        = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState    = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM        = 8;
    RCC_OscInitStruct.PLL.PLLN        = 336;
    RCC_OscInitStruct.PLL.PLLP        = RCC_PLLP_DIV4;   /* 84 MHz */
    RCC_OscInitStruct.PLL.PLLQ        = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK  = 84 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;      /* PCLK1 = 42 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;      /* PCLK2 = 84 MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    /* PA0 = ADC1_IN0 ? LM35 */
    sConfig.Channel      = ADC_CHANNEL_0;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Activer les horloges */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* PA0 : Analogique ? LM35 VOUT */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB0(RS) PB1(EN) PB4(D4) PB5(D5) PB6(D6) PB7(D7) ? LCD */
    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1
                          | GPIO_PIN_4 | GPIO_PIN_5
                          | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PE1 : Ventilateur */
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* État initial = tout à 0 */
    HAL_GPIO_WritePin(GPIOB,
        GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7,
        GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
}
