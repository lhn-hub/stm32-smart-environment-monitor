#ifndef __BSP_PIN_H
#define __BSP_PIN_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* sms-hub M0 configuration. See Docs/M0-static-review.md for evidence.
 * Fixed board: STM32F103C8T6 + Wildfire expansion V1.0.2.
 * PSC/ARR macros below are register values unless named *_PERIOD.
 */

/* Shared GPIO register masks for CubeMX-generated init/deinit code. */
#define BSP_GPIO_PIN_0 GPIO_PIN_0
#define BSP_GPIO_PIN_1 GPIO_PIN_1
#define BSP_GPIO_PIN_2 GPIO_PIN_2
#define BSP_GPIO_PIN_3 GPIO_PIN_3
#define BSP_GPIO_PIN_4 GPIO_PIN_4
#define BSP_GPIO_PIN_5 GPIO_PIN_5
#define BSP_GPIO_PIN_6 GPIO_PIN_6
#define BSP_GPIO_PIN_7 GPIO_PIN_7
#define BSP_GPIO_PIN_8 GPIO_PIN_8
#define BSP_GPIO_PIN_9 GPIO_PIN_9
#define BSP_GPIO_PIN_10 GPIO_PIN_10
#define BSP_GPIO_PIN_11 GPIO_PIN_11
#define BSP_GPIO_PIN_12 GPIO_PIN_12
#define BSP_GPIO_PIN_13 GPIO_PIN_13
#define BSP_GPIO_PIN_14 GPIO_PIN_14
#define BSP_GPIO_PIN_15 GPIO_PIN_15
#define BSP_GPIO_PORT_A GPIOA
#define BSP_GPIO_PORT_B GPIOB
#define BSP_GPIO_PORT_C GPIOC
#define BSP_GPIO_PORT_D GPIOD
#define BSP_US_CLOCK_HZ 1000000u
#define BSP_HAL_TICK_HZ 1000u

/* Clock tree. */
#define BSP_HSE_HZ                 8000000u
#define BSP_LSE_HZ                 32768u
#define BSP_SYSCLK_HZ              72000000u
#define BSP_APB1_HZ                36000000u
#define BSP_APB2_HZ                72000000u
#define BSP_APB1_TIMER_HZ          BSP_SYSCLK_HZ
#define BSP_ADC_CLOCK_HZ           12000000u
#define BSP_USB_CLOCK_HZ           48000000u
#define BSP_PLL_MUL                RCC_PLL_MUL9

/* SWD retained; release PB3/PB4/PA15 with NOJTAG in board init. */
#define BSP_NVIC_GROUP             NVIC_PRIORITYGROUP_4
#define BSP_IRQ_TIM4_PRIO          4u
#define BSP_IRQ_EXTI0_PRIO         5u
#define BSP_IRQ_EXTI3_PRIO         5u
#define BSP_IRQ_EXTI4_PRIO         5u
#define BSP_IRQ_EXTI15_10_PRIO      5u
#define BSP_IRQ_USART3_PRIO        6u
#define BSP_IRQ_USART3_DMA_PRIO    6u
#define BSP_IRQ_ADC_DMA_PRIO       10u
#define BSP_IRQ_SYSTICK_PRIO       15u
#define BSP_IRQ_PENDSV_PRIO        15u
#define BSP_IRQ_SUBPRIO            0u

/* USART1 / CH340. Do not connect the USB Device socket. */
#define BSP_DEBUG_UART_BAUD        115200u
#define BSP_DEBUG_TX_PORT          GPIOA
#define BSP_DEBUG_TX_PIN           GPIO_PIN_9
#define BSP_DEBUG_RX_PORT          GPIOA
#define BSP_DEBUG_RX_PIN           GPIO_PIN_10

/* RGB: TIM2 CH2/3/4, 1 kHz. Active low, confirmed by schematic page 3 and board feedback. */
#define RGB_OFF_LEVEL GPIO_PIN_SET
#define RGB_PWM_POLARITY TIM_OCPOLARITY_LOW
#define RGB_TIM                   htim2
#define RGB_R_PORT                GPIOA
#define RGB_R_PIN                 GPIO_PIN_1
#define RGB_G_PORT                GPIOA
#define RGB_G_PIN                 GPIO_PIN_2
#define RGB_B_PORT                GPIOA
#define RGB_B_PIN                 GPIO_PIN_3
#define RGB_R_CH                  TIM_CHANNEL_2
#define RGB_G_CH                  TIM_CHANNEL_3
#define RGB_B_CH                  TIM_CHANNEL_4
#define RGB_PSC                   71u
#define RGB_PERIOD                1000u
#define RGB_ARR                   (RGB_PERIOD - 1u)

/* Active-high keys; PC13 polarity must be checked on the board. */
#define KEY_WAKEUP_GPIO_PORT       GPIOA
#define KEY_WAKEUP_PIN             GPIO_PIN_0
#define KEY_PC13_GPIO_PORT         GPIOC
#define KEY_PC13_PIN               GPIO_PIN_13
#define KEY_USER_GPIO_PORT         GPIOB
#define KEY_USER_PIN               GPIO_PIN_15
#define KEY_ACTIVE_LEVEL           GPIO_PIN_SET
#define KEY_POLL_MS                10u
#define KEY_LONGPRESS_MS           1500u

/* PB5 buzzer is active high; expansion LED interface is active low. */
#define BUZZER_GPIO_PORT           GPIOB
#define BUZZER_PIN                 GPIO_PIN_5
#define BUZZER_ON_LEVEL            GPIO_PIN_SET
#define BUZZER_OFF_LEVEL           GPIO_PIN_RESET

/* CS100A direct socket: GND/PB3(ECHO)/PA15(TRIG)/3V3.
 * Corrected against vendor schematic/HAL example with user approval 2026-10-07.
 * PB3 is input-only on EXTI3; TIM2 stays dedicated to RGB.
 */
#define ULTRA_TRIG_PORT            GPIOA
#define ULTRA_TRIG_PIN             GPIO_PIN_15
#define ULTRA_ECHO_PORT            GPIOB
#define ULTRA_ECHO_PIN             GPIO_PIN_3
#define ULTRA_TRIG_PULSE_US        12u
#define ULTRA_ECHO_TIMEOUT_US      30000u
#define ULTRA_FILTER_N             5u

/* Ai-Thinker Rd-03, user-confirmed replacement for AT6010.
 * OT2 -> PB4: detection high, no detection low (datasheet V1.1.1 p9).
 * OT1/RX are for external USB-TTL setup only; USART3 stays with ESP8266.
 */
#define RADAR_MODEL                "Rd-03"
#define RADAR_CONFIG_UART_BAUD     115200u
#define RADAR_PORT                 GPIOB
#define RADAR_PIN                  GPIO_PIN_4
#define RADAR_ACTIVE_LEVEL         GPIO_PIN_SET
#define RADAR_DEBOUNCE_MS          1000u

/* Expansion IO1/IO2 DO pins; AO stays disconnected from shared SPI pins.
 * Use 5V supplies and translated DO, never connect the USB_DEVICE socket. */
#define MQ_SMOKE_PORT              GPIOA
#define MQ_SMOKE_PIN               GPIO_PIN_12
#define MQ_AIR_PORT                GPIOA
#define MQ_AIR_PIN                 GPIO_PIN_11
#define MQ_ALARM_LEVEL             GPIO_PIN_RESET
#define MQ_SCAN_PERIOD_MS          200u
/* Module manual recommends at least five minutes; not a substitute for aging. */
#define MQ_WARMUP_S                150u

/* DHT11: input/open-drain direction switching, no long busy wait. */
#define DHT11_PORT                 GPIOB
#define DHT11_PIN                  GPIO_PIN_12
/* Datasheet requests >2s;20ms start agrees with Adafruit DHT11 implementation. */
#define DHT11_PERIOD_MS            2100u
#define DHT11_START_LOW_MS         20u
#define DHT11_FAULT_LIMIT          10u
#define DHT11_POWERUP_MS           1000u
#define DHT11_START_MAX_MS         30u
#define DHT11_CAPTURE_TIMEOUT_US   7000u
#define DHT11_CAPTURE_WIDTHS       82u
#define DHT11_EDGE_MAX_US          200u
#define DHT11_RESPONSE_MIN_US      60u
#define DHT11_RESPONSE_MAX_US      110u
#define DHT11_BIT_LOW_MIN_US       35u
#define DHT11_BIT_LOW_MAX_US       75u
#define DHT11_ZERO_MIN_US          10u
#define DHT11_ZERO_MAX_US          45u
#define DHT11_ONE_MIN_US           55u
#define DHT11_ONE_MAX_US           95u
#define DHT11_TEMP_MIN_X10         (-200)
#define DHT11_TEMP_MAX_X10         600
#define DHT11_HUM_MAX_X10          1000
#define ULTRA_PERIOD_MS            100u
#define ULTRA_FAILURE_LIMIT        5u
#define ULTRA_US_PER_CM            58u
#define ULTRA_MIN_ECHO_US          116u

/* EC11 A/B; SW unconnected. Confirm=PB15; Back=PC13. */
#define EC11_A_PORT                GPIOA
#define EC11_A_PIN                 GPIO_PIN_12
#define EC11_B_PORT                GPIOA
#define EC11_B_PIN                 GPIO_PIN_11

/* TIM4 HAL timebase: 1 MHz CNT, 1 kHz update IRQ at priority 4. */
extern TIM_HandleTypeDef htim4;
#define TICK_TIM                  htim4
#define TICK_TIM_PSC              (72u - 1u)
#define TICK_TIM_ARR              (1000u - 1u)
#define US_CNT_REG()              (TICK_TIM.Instance->CNT)
#define BSP_US_TIM                TIM4
#define BSP_US_TIM_PSC            TICK_TIM_PSC
#define BSP_US_TIM_ARR            TICK_TIM_ARR
#define BSP_US_PER_MS             1000u
#define BSP_US_BUSYWAIT_LIMIT     BSP_US_PER_MS

/* SG90: TIM1 CH1, 50 Hz. Verify BDTR.MOE when PWM is started in M5. */
#define SERVO_PORT                 GPIOA
#define SERVO_PIN                  GPIO_PIN_8
#define SERVO_TIM                  htim1
#define SERVO_CH                   TIM_CHANNEL_1
#define SERVO_PSC                  71u
#define SERVO_ARR                  19999u
#define SERVO_CCR_MIN              500u
#define SERVO_CCR_MAX              2500u
#define SERVO_OPEN_ANGLE           90u
#define SERVO_SLEW_MS_PER_DEG      50u
#define SERVO_MOE_MASK             TIM_BDTR_MOE

/* TB6612: A never drives; DO NOT configure TIM3_CH1 or force PA6 low
 * during SPI reads. CS=0 also puts TB6612 in standby.
 * Give/safe state: PA4=0, PA7=0, SCK/PA5 idle=0, FAN CCR=0.
 * PA6 high impedance in deselected SPI relies on the device pulldown;
 * board/transient verification remains mandatory in M4/M5.
 */
#define TB_STBY_PORT               GPIOA
#define TB_STBY_PIN                GPIO_PIN_4
#define TB_AIN1_PORT               GPIOA
#define TB_AIN1_PIN                GPIO_PIN_5
#define TB_PWMA_PORT               GPIOA
#define TB_PWMA_PIN                GPIO_PIN_6
#define TB_AIN2_PORT               GPIOB
#define TB_AIN2_PIN                GPIO_PIN_0
#define TB_BIN1_PORT               GPIOA
#define TB_BIN1_PIN                GPIO_PIN_7
#define TB_BIN2_PORT               GPIOB
#define TB_BIN2_PIN                GPIO_PIN_13
#define TB_SAFE_LEVEL              GPIO_PIN_RESET
#define FAN_PORT                   GPIOB
#define FAN_PIN                    GPIO_PIN_1
#define FAN_TIM                    htim3
#define FAN_CH                     TIM_CHANNEL_4
#define FAN_PSC                    0u
#define FAN_ARR                    3599u
#define FAN_SAFE_CCR               0u
#define FAN_RAMP_MS                1000u
#define FAN_FIRE_SPEED             100u
#define FAN_AIR_SPEED              45u

/* SPI1 mode 0, APB2 / 8 = 9 MHz; /4 requires M4 comparison evidence. */
#define W25Q_SPI                   hspi1
#define W25Q_CS_PORT               TB_STBY_PORT
#define W25Q_CS_PIN                TB_STBY_PIN
#define W25Q_SCK_PORT              TB_AIN1_PORT
#define W25Q_SCK_PIN               TB_AIN1_PIN
#define W25Q_MISO_PORT             TB_PWMA_PORT
#define W25Q_MISO_PIN              TB_PWMA_PIN
#define W25Q_MOSI_PORT             TB_BIN1_PORT
#define W25Q_MOSI_PIN              TB_BIN1_PIN
#define W25Q_SPI_BAUD_INIT         SPI_BAUDRATEPRESCALER_8
#define W25Q_SPI_BAUD_FAST         SPI_BAUDRATEPRESCALER_4
#define W25Q_SPI_PRESCALER         W25Q_SPI_BAUD_INIT
#define W25Q_SPI_CLOCK_HZ          (BSP_APB2_HZ / 8u)
#define W25Q_JEDEC_ID              0xEF4017u
#define W25Q_PAGE_SIZE             256u
#define W25Q_SECTOR_SIZE           4096u
#define W25Q_CAPACITY_BYTES        0x800000u

/* Independent KV sectors, reserved sector, then 16 log sectors.
 * Confirmed: 64 KB from 0x003000 ends at 0x012FFF.
 */
#define FLASH_KV_ADDR_A            0x000000u
#define FLASH_KV_ADDR_B            0x001000u
#define FLASH_KV_ADDR              FLASH_KV_ADDR_A
#define FLASH_KV_BANK_A_ADDR       FLASH_KV_ADDR_A
#define FLASH_KV_BANK_B_ADDR       FLASH_KV_ADDR_B
#define FLASH_KV_BANK_SIZE         W25Q_SECTOR_SIZE
#define KV_RECORD_SIZE            512u
#define KV_SLOTS_PER_BANK          (FLASH_KV_BANK_SIZE / KV_RECORD_SIZE)
#define FLASH_KV_RECORD_SIZE       KV_RECORD_SIZE
#define FLASH_KV_SLOTS_PER_BANK    KV_SLOTS_PER_BANK
#define FLASH_RSVD_ADDR            0x002000u
#define FLASH_RESERVED_ADDR        FLASH_RSVD_ADDR
#define FLASH_RESERVED_SIZE        W25Q_SECTOR_SIZE
#define FLASH_LOG_ADDR             0x003000u
#define FLASH_LOG_SECTORS          16u
#define FLASH_LOG_SIZE             (FLASH_LOG_SECTORS * W25Q_SECTOR_SIZE)
#define FLASH_LOG_END_EXCLUSIVE    (FLASH_LOG_ADDR + FLASH_LOG_SIZE)
#define LOG_ITEM_SIZE              32u

/* OLED SSD1306: 7-bit address; HAL expects the left-shifted address. */
#define OLED_I2C                   hi2c1
#define OLED_SCL_PORT              GPIOB
#define OLED_SCL_PIN               GPIO_PIN_6
#define OLED_SDA_PORT              GPIOB
#define OLED_SDA_PIN               GPIO_PIN_7
#define OLED_ADDR_7BIT             0x3Cu
#define OLED_ADDR                  (OLED_ADDR_7BIT << 1)
#define OLED_I2C_CLOCK_HZ          400000u

/* ESP8266 USART3: RX DMA1 CH3 circular; TX DMA1 CH2 normal. */
#define ESP_UART                   huart3
#ifndef ESP_UART_BAUD
#define ESP_UART_BAUD              115200u
#endif
#define ESP_TX_PORT                GPIOB
#define ESP_TX_PIN                 GPIO_PIN_10
#define ESP_RX_PORT                GPIOB
#define ESP_RX_PIN                 GPIO_PIN_11
#define ESP_RST_PORT               GPIOB
#define ESP_RST_PIN                GPIO_PIN_9
#define ESP_IO0_PORT               GPIOB
#define ESP_IO0_PIN                GPIO_PIN_8
#define ESP_RX_BUF_SIZE            512u
#define ESP_AT_TIMEOUT_MS          3000u
#define ESP_JOIN_TIMEOUT_MS        20000u
#define ESP_RETRY_MAX              3u
#define ESP_BACKOFF_MS             60000u

/* ADC1 CH16/17, DMA1 CH1 circular half-word. */
#define ADC_TEMP_CHANNEL           ADC_CHANNEL_TEMPSENSOR
#define ADC_VREF_CHANNEL           ADC_CHANNEL_VREFINT
#define ADC_SAMPLE_TIME            ADC_SAMPLETIME_239CYCLES_5
#define ADC_CHANNEL_COUNT          2u
#define ADC_SAMPLES_PER_CH          16u
#define ADC_FULL_SCALE             4095u
#define VREFINT_TYP_MV             1200u
#define ADC_VREFINT_NOMINAL_MV      VREFINT_TYP_MV

/* Requested candidate addresses retained, NOT verified F103 calibration.
 * Do not dereference/use as calibration until provenance and safe access
 * are established. Nonzero/non-FFFF alone does not prove calibration.
 */
#define TS_CAL1_ADDR               ((const volatile uint16_t *)0x1FFFF7B8u)
#define VREFINT_CAL_ADDR           ((const volatile uint16_t *)0x1FFFF7BAu)
#define TS_CAL2_ADDR               ((const volatile uint16_t *)0x1FFFF7C2u)
#define VREFINT_CAL_VREF_MV         3300u
#define TS_CAL1_TEMP_C             30
#define TS_CAL2_TEMP_C             110
#define TS_TYP_SLOPE_UV_PER_C      4300
#define ADC_FACTORY_CAL_ENABLED    0u
#define ADC_CAL_EMPTY_ZERO         0x0000u
#define ADC_CAL_EMPTY_ERASED       0xFFFFu

/** @brief Check a previously acquired candidate word for an empty value.
 *  @return Nonzero for zero/erased words; zero does NOT establish provenance.
 *  @note No address access; caller must first establish safe readable mapping.
 */
static inline int bsp_cal_word_is_empty(uint16_t value)
{
    return (value == ADC_CAL_EMPTY_ZERO) || (value == ADC_CAL_EMPTY_ERASED);
}

/* IWDG: nominal LSI; actual timeout varies with LSI tolerance. */
#define BSP_LSI_NOMINAL_HZ          40000u
#define IWDG_PRESCALER              IWDG_PRESCALER_64
#define IWDG_RELOAD                 2500u

/* Defaults; integer temperature in tenths of a degree, no %f. */
#define DEF_SMOKE_CONFIRM_MS        3000u
#define DEF_AIR_CONFIRM_MS          10000u
#define DEF_TEMP_ALARM_C_X10        500
#define DEF_INTRUSION_DIST_CM       100u
#define DEF_INTRUSION_CONFIRM_MS    2000u
#define DEF_ALARM_RELEASE_MS        10000u
#define DEF_TELEMETRY_PERIOD_MS     5000u

/** @brief Enable board GPIO clocks and release JTAG while keeping SWD.
 *  @note M0 implementation must not enable motors or device drivers.
 */
void bsp_board_init(void);

#ifdef __cplusplus
}
#endif
#endif /* __BSP_PIN_H */
