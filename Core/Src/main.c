/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  RUN_STOPPED = 0,
  RUN_ACTIVE  = 1
} RunState;

typedef enum
{
  DIR_FORWARD = 0,
  DIR_REVERSE = 1
} Direction;
typedef struct
{
  uint8_t header;
  uint8_t run;
  uint8_t dir;
  uint8_t step_l;
  uint8_t step_h;
  uint8_t checksum;
} RxPacket;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RX_HEADER      0xAAU
#define STEP_MIN_MS    100U
#define STEP_MAX_MS    2000U
#define KEY_DIR_PIN        GPIO_PIN_12
#define KEY_PAUSE_PIN      GPIO_PIN_13
#define KEY_DEBOUNCE_MS    30U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static char feedback_buf[64];
static volatile uint8_t tx_busy = 0U;
static volatile uint8_t feedback_pending = 0U;
static RxPacket rx_packet;
static volatile uint8_t packet_ready = 0U;
static volatile uint8_t key_dir_event = 0U;
static volatile uint8_t key_pause_event = 0U;

static volatile uint32_t last_dir_edge_tick = 0U;
static volatile uint32_t last_pause_edge_tick = 0U;
static RunState run_state = RUN_ACTIVE;
static Direction direction = DIR_FORWARD;
static uint16_t step_ms = 300U;
static uint32_t last_step_tick = 0U;
static uint8_t led_index = 0U;

static const uint16_t led_pins[3] =
{
  GPIO_PIN_0,
  GPIO_PIN_1,
  GPIO_PIN_2
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void service_feedback(void);
static uint8_t process_packet(const RxPacket *packet);
static void all_leds_off(void);
static void show_only_led(void);
static void refresh_oled_all(void);
static void refresh_oled_led(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  led_index = 0U;

  OLED_Init();
  last_step_tick = HAL_GetTick();
  show_only_led();
  refresh_oled_all();
  if (HAL_UART_Receive_IT(
        &huart1,
        (uint8_t *)&rx_packet,
        sizeof(rx_packet)
    ) != HAL_OK)
  {
    Error_Handler();
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t dir_action;
    uint8_t pause_action;

    __disable_irq();
    dir_action = key_dir_event;
    pause_action = key_pause_event;
    key_dir_event = 0U;
    key_pause_event = 0U;
    __enable_irq();

    /* 方向按键 */
    if (dir_action)
    {
      direction = (direction == DIR_FORWARD)
                ? DIR_REVERSE
                : DIR_FORWARD;

      last_step_tick = HAL_GetTick();
      refresh_oled_all();

      feedback_pending = 1U;
    }

    /* 暂停/继续按键 */
    if (pause_action)
    {
      run_state = (run_state == RUN_ACTIVE)
                ? RUN_STOPPED
                : RUN_ACTIVE;

      if (run_state == RUN_ACTIVE)
      {
        last_step_tick = HAL_GetTick();
      }

      refresh_oled_all();

      feedback_pending = 1U;
    }
    {
      RxPacket local_packet;
      uint8_t has_packet = 0U;

      __disable_irq();
      if (packet_ready)
      {
        local_packet = rx_packet;
        packet_ready = 0U;
        has_packet = 1U;
      }
      __enable_irq();

      if (has_packet)
      {
        /* 先重新启动接收 */
        if (HAL_UART_Receive_IT(
                &huart1,
                (uint8_t *)&rx_packet,
                sizeof(rx_packet)
            ) != HAL_OK)
        {
          Error_Handler();
        }

        process_packet(&local_packet);
      }
    }
    if (run_state == RUN_ACTIVE)
    {
      uint32_t now = HAL_GetTick();

      if ((uint32_t)(now - last_step_tick) >= step_ms)
      {
        last_step_tick = now;

        if (direction == DIR_FORWARD)
        {
          led_index = (uint8_t)(
              (led_index + 1U) % 3U
          );
        }
        else
        {
          led_index = (uint8_t)(
              (led_index + 2U) % 3U
          );
        }

        show_only_led();
        refresh_oled_led();
      }
    }
    service_feedback();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    tx_busy = 0U;
  }
}
static void service_feedback(void)
{
  int length;

  if (tx_busy || !feedback_pending)
  {
    return;
  }

  feedback_pending = 0U;

  length = snprintf(
      feedback_buf,
      sizeof(feedback_buf),
      "STATE=%s,DIR=%s,STEP=%u,LED=%u\r\n",
      (run_state == RUN_ACTIVE) ? "RUN" : "PAUSE",
      (direction == DIR_FORWARD) ? "FWD" : "REV",
      (unsigned int)step_ms,
      (unsigned int)(led_index + 1U)
  );

  if (length <= 0 ||
      length >= (int)sizeof(feedback_buf))
  {
    return;
  }

  tx_busy = 1U;

  if (HAL_UART_Transmit_IT(
          &huart1,
          (uint8_t *)feedback_buf,
          (uint16_t)length
      ) != HAL_OK)
  {
    tx_busy = 0U;
    feedback_pending = 1U;
  }
}
static uint8_t process_packet(const RxPacket *packet)
{
  uint16_t received_step;
  uint8_t expected_check;

  received_step =
      (uint16_t)packet->step_l |
      ((uint16_t)packet->step_h << 8U);

  expected_check = (uint8_t)(
      packet->header +
      packet->run +
      packet->dir +
      packet->step_l +
      packet->step_h
  );

  if (packet->header != RX_HEADER)
  {
    return 0U;
  }

  if (packet->checksum != expected_check)
  {
    return 0U;
  }

  if (packet->run > 1U || packet->dir > 1U)
  {
    return 0U;
  }

  if (received_step < STEP_MIN_MS ||
      received_step > STEP_MAX_MS)
  {
    return 0U;
  }

  run_state = (RunState)packet->run;
  direction = (Direction)packet->dir;
  step_ms = received_step;
  last_step_tick = HAL_GetTick();

  refresh_oled_all();
  feedback_pending = 1U;

  return 1U;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    packet_ready = 1U;
  }
}
static void all_leds_off(void)
{
  uint8_t i;

  for (i = 0U; i < 3U; i++)
  {
    HAL_GPIO_WritePin(
        GPIOA,
        led_pins[i],
        GPIO_PIN_RESET
    );
  }
}

static void show_only_led(void)
{
  all_leds_off();

  HAL_GPIO_WritePin(
      GPIOA,
      led_pins[led_index],
      GPIO_PIN_SET
  );
}
static void refresh_oled_all(void)
{
  char line[17];

  snprintf(
      line,
      sizeof(line),
      "STATE:%-5s",
      (run_state == RUN_ACTIVE) ? "RUN" : "PAUSE"
  );
  OLED_ShowString(1, 1, line);

  snprintf(
      line,
      sizeof(line),
      "DIR:%-3s",
      (direction == DIR_FORWARD) ? "FWD" : "REV"
  );
  OLED_ShowString(2, 1, line);

  snprintf(
      line,
      sizeof(line),
      "STEP:%04u ms",
      (unsigned int)step_ms
  );
  OLED_ShowString(3, 1, line);

  refresh_oled_led();
}

static void refresh_oled_led(void)
{
  char line[17];

  snprintf(
      line,
      sizeof(line),
      "LED:%u   ",
      (unsigned int)(led_index + 1U)
  );
  OLED_ShowString(4, 1, line);
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  uint32_t now = HAL_GetTick();

  if (GPIO_Pin == KEY_DIR_PIN)
  {
    if ((uint32_t)(now - last_dir_edge_tick)
        >= KEY_DEBOUNCE_MS)
    {
      last_dir_edge_tick = now;
      key_dir_event = 1U;
    }
  }
  else if (GPIO_Pin == KEY_PAUSE_PIN)
  {
    if ((uint32_t)(now - last_pause_edge_tick)
        >= KEY_DEBOUNCE_MS)
    {
      last_pause_edge_tick = now;
      key_pause_event = 1U;
    }
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
