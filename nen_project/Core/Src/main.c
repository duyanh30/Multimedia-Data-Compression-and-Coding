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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "usb_device.h"
#include "opus.h"
#include <stdint.h>
#include "opus_types.h"
#include "opus_defines.h"
#include "usbd_cdc_if.h"
#include "cmsis_compiler.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* main.c - USER CODE BEGIN PV */
#define STACK_HEAP_MAGIC 0xCDCDCDCD // Giá trị dùng để đánh dấu
#define PCM_FRAME_SIZE   960    // 1  frame 20ms - 2 bytes, 320 frames - 640 bytes
#define COMPRESSED_SIZE  128    // Khai báo cho dư dả dễ điều chỉnh bitrate

uint8_t pcm_buffer[PCM_FRAME_SIZE]; // Mảng lưu giá trị frame
uint32_t pcm_ptr = 0;	// Biến đếm
volatile uint8_t frame_ready_flag = 0; // Flag báo hiệu ready để gửi tiếp frame
uint8_t compressed_data[COMPRESSED_SIZE]; // Mảng chứa giá trị frame nén

// Khai báo Linker Symbols dưới dạng mảng để lấy địa chỉ chuẩn xác 100%
extern uint8_t _sdata[]; // Địa chỉ bắt đầu của RAM - bắt đầu của phần static
extern uint8_t _end[];    // Kết thúc Static, Bắt đầu Heap
extern uint8_t _estack[]; // Đỉnh cao nhất của RAM (Cuối Stack)

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void paint_memory(void) { // Hàm tô màu lấy mốc để xác định vùng Heap và Stack khi nén
    uint32_t *start = (uint32_t*)sbrk(0); // Bắt đầu từ đỉnh hiện tại của Heap
    uint32_t *end = (uint32_t*)__get_MSP(); // Đến vị trí hiện tại của Stack

    // Lùi lại một chút để an toàn, không đè vào Stack của chính hàm này
    end -= 32;

    while (start < end) {
        *start++ = STACK_HEAP_MAGIC;
    }
}


uint32_t get_stack_watermark(void) { // Dò mức Stack sâu nhất (High Watermark)
    // Quét từ đỉnh Heap (vùng thấp) đi lên đỉnh RAM (vùng cao)
    // Vị trí đầu tiên còn giá trị  MAGIC chính là giới hạn mà Stack chưa bao giờ chạm tới
    uint32_t *current = (uint32_t*)sbrk(0);
    uint32_t *limit = (uint32_t*)_estack;

    while (current < limit && *current == STACK_HEAP_MAGIC) {
        current++;
    }

    // Stack used = Đỉnh RAM - Địa chỉ thấp nhất mà Stack từng lấn tới
    return (uint32_t)_estack - (uint32_t)current;
}

// 3. Dò mức Heap cao nhất
uint32_t get_heap_watermark(void) {
    uint32_t *start = (uint32_t*)_end;
    uint32_t *current = (uint32_t*)sbrk(0);

    // Nếu sbrk lỗi hoặc chưa cấp phát
    if ((uint32_t)current <= (uint32_t)start) return 0;

    return (uint32_t)current - (uint32_t)start;
}



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
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  // Khởi tạo biến cycle count để đo load CPU


  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;


    // Khởi tạo Opus

  OpusEncoder *encoder = NULL; // Khởi tạo con trỏ


  int err; // Biến báo lỗi
  encoder = opus_encoder_create(24000, 1, OPUS_APPLICATION_VOIP, &err); // Cấp phát bộ nhớ heap cho con trỏ encoder

  if(err != OPUS_OK || encoder == NULL) {
       while(1) { // Lỗi khởi tạo: Nháy LED liên tục
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_Delay(10);
        }
    }

    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(16000)); // Set bitrate khi nén
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(2)); // Giảm load cho CPU (Complexity 1-10)

    paint_memory(); // Gọi hàm paint trước khi nén


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  if (frame_ready_flag == 1)
	        {

		  	  	uint32_t t_start = DWT->CYCCNT; // Lưu giá trị cycle ban đầu

	            // --- THỰC HIỆN NÉN ---
	            // Chuyển sang (int16_t*) vì PCM là 16-bit, pass con trỏ, mảng lưu frame, số frame, mảng lưu frame nén, size
	            int n_bytes = opus_encode(encoder, (int16_t*)pcm_buffer, 480, compressed_data, COMPRESSED_SIZE);

	            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

	            pcm_ptr = 0; // Reset lại biến count
	           	frame_ready_flag = 0; // Reset lại flag để USB-CDC có thể nhận tiếp frame mới

	            uint32_t t_end = DWT->CYCCNT; // Lưu giá trị cycle sau khi nén

	            // --- TÍNH TOÁN THÔNG SỐ ---
	            uint32_t cycles_spent = t_end - t_start; // Giá trị cycle để nén
	            // Load (%) = (Cycles / 1.92M) * 100
	            uint32_t cpu_load = (cycles_spent * 100) / 1920000; // CPU chạy 96MHz trong 20ms -> 1920000 chu kì -> tính load

	            uint32_t peak_stack  = get_stack_watermark(); // Lấy đỉnh của stack
	            uint32_t peak_heap   = get_heap_watermark();  // Lấy đỉnh của heap
	            uint32_t static_used = (uint32_t)_end - (uint32_t)_sdata; // Tính phần static RAM

	            uint32_t total_ram_used = static_used + peak_heap + peak_stack; // Tổng ram sử dụng
	            // Ép kiểu float lúc tính % để tránh tràn số khi nhân 100
	            uint32_t ram_percent = (uint32_t)(((float)total_ram_used / (128.0f * 1024.0f)) * 100.0f); // Ram load

	            // --- GỬI TELEMETRY ---
	            char msg[128]; // Khai báo mảng gửi về PC
	            // Format: CPU%, RAM%, Static(KB), Heap(KB), Stack(KB), n_bytes, R
	            int msg_len = snprintf(msg, sizeof(msg), "%lu,%lu,%lu,%lu,%lu,%d,R\n", // Gửi thêm tín hiệu ready để PC gửi tiếp
	                                   cpu_load, ram_percent,
	                                   static_used/1024, peak_heap/1024, peak_stack/1024, n_bytes);

	            uint32_t timeout = 10000;
	            extern USBD_HandleTypeDef hUsbDeviceFS;
	            USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;

	            // Gửi chuỗi Telemetry
	            CDC_Transmit_FS((uint8_t*)msg, msg_len);

	            // Đợi cho đến khi USB gửi xong (TxState == 0) hoặc hết timeout
	            while(hcdc->TxState != 0 && timeout--) {
	                // Đợi một chút
	            }

	            // Gửi dữ liệu nén Binary ngay sau đó
	            if (n_bytes > 0) {
	                CDC_Transmit_FS((uint8_t*)compressed_data, n_bytes);
	            }


	        }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */




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
