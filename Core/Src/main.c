/* USER CODE BEGIN Header */

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "sdmmc.h"
#include "spi.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "Task.h"
#include "AHRS.h"
#include "Flash.h"
#include <stdio.h>
#include "Health.h"
#include "SysTime.h"
#include "Hardware.h"
#include "Scheduler.h"
#include "usbd_cdc_if.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// #define USER_DEBUG_AHRS
// #define USER_DEBUG_DEVICE
#define USER_DEBUG_HOST_TOOL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int main(void)
{
/******系统初始化*******/
  MPU_Config();
  HAL_Init();
  SystemClock_Config();
  SysTime_Init();

/*******USB初始化*******/
  MX_USB_DEVICE_Init();
  HAL_Delay(3000);

  MX_GPIO_Init();
  
  MX_I2C1_Init();
  MX_I2C2_Init();

  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI4_Init();

  MX_UART4_Init();
  MX_UART7_Init();
  MX_UART8_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();

  // MX_SDMMC1_SD_Init();

  Scheduler_Init();
  Hardware_Init();
  Flash_Load();
  Task_Init();

  while(1)
  {
    Scheduler_Run();

#ifdef USER_DEBUG_AHRS
    static uint32_t tick = 0;
    if(millis() - tick >= 100)
    {
      tick = millis();
      printf("Yaw: %6.1f°| Roll: %6.1f°| Pitch: %6.1f° \r\n",
      AHRS.Yaw  *57.2958f,
      AHRS.Roll *57.2958f,
      AHRS.Pitch*57.2958f);
    }
#endif

#ifdef USER_DEBUG_DEVICE
    static uint32_t tick = 0;
    if(millis() - tick >= 100)
    {
      tick = millis();
      printf("GYR: %6.3f %6.3f %6.3f | ACC: %6.2f %6.2f %6.2f | MAG: %5.0f %5.0f %5.0f | BAR: %.1f Pa %.1f C | HEALTH: %d %d %d %d\r\n",
      HW_Sample.Gyr[0],
      HW_Sample.Gyr[1],
      HW_Sample.Gyr[2],
      HW_Sample.Acc[0],
      HW_Sample.Acc[1],
      HW_Sample.Acc[2],
      HW_Sample.Mag[0],
      HW_Sample.Mag[1],
      HW_Sample.Mag[2],
      HW_Sample.Pressure,
      HW_Sample.Temperature,
      Health_Status(&GYR_Health),
      Health_Status(&ACC_Health),
      Health_Status(&MAG_Health),
      Health_Status(&BAR_Health));
    }
#endif

#ifdef USER_DEBUG_HOST_TOOL
    char buffer[128] = {0};
    static uint32_t tick = 0;
    if(millis() - tick >= 100)
    {
      tick = millis();
      int length = snprintf(buffer,sizeof(buffer),"%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%.1f,%.1f,%d,%d,%d,%d\n",
      AHRS.Yaw  *57.2958f,
      AHRS.Roll *57.2958f,
      AHRS.Pitch*57.2958f,
      HW_Sample.Mag[0],
      HW_Sample.Mag[1],
      HW_Sample.Mag[2],
      HW_Sample.Pressure,
      HW_Sample.Temperature,
      Health_Status(&GYR_Health),
      Health_Status(&ACC_Health),
      Health_Status(&MAG_Health),
      Health_Status(&BAR_Health));

      CDC_Transmit_FS((uint8_t *)buffer,length);
    }
#endif
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 120;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 20;
  RCC_OscInitStruct.PLL.PLLR = 20;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
