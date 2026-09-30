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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "CLCD_I2C.h"
#include "RC522.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
    ST_LOGIN = 0,

    ST_DOOR_OPEN,

    ST_LOCKED,

    ST_ADMIN_OLD_PASSWORD,

    ST_ADMIN_NEW_PASSWORD

} AppState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PASSWORD_FLASH_ADDR    0x0800FC00UL
#define PASSWORD_MAGIC         0xA55AU
#define PASSWORD_MAX_LEN       16U
#define PASSWORD_MAX_LEN      8
#define UID_LEN               4

#define MAX_WRONG_ATTEMPTS    3

#define LOCK_TIME_MS          30000UL
#define DOOR_OPEN_TIME_MS     5000UL
#define BUZZER_TIME_MS        3000UL

#define RFID_SCAN_PERIOD_MS   100UL

#define SERVO_CCR_0_DEG       25U
#define SERVO_CCR_90_DEG      75U
#define SERVO_CCR_180_DEG     125U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
CLCD_I2C_Name LCD1;


/* =========================
   RFID
   ========================= */

#define UID_LEN 4

uint8_t status;
uint8_t str[16];

uint8_t admin_uid[UID_LEN] =
{
    0xFC, 0x31, 0x03, 0x07
};


/* =========================
   PASSWORD
   ========================= */

char system_password[PASSWORD_MAX_LEN + 1] = "1234";

char input_buffer[17] = {0};

uint8_t input_len = 0;

uint8_t wrong_count = 0;


/* =========================
   STATE
   ========================= */
AppState app_state = ST_LOGIN;



/* =========================
   TIMER
   ========================= */

uint32_t door_open_tick = 0;

uint32_t lock_start_tick = 0;

uint32_t buzzer_start_tick = 0;

uint32_t last_rfid_scan = 0;

bool buzzer_active = false;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void Password_SaveFlash(void);
void Password_LoadFlash(void);

char Keypad_Scan(void);

void HandleKey(char key);

void ClearInput(void);
void AppendInput(char key);
void BackspaceInput(void);

void LCD_ShowInput(const char *title);

void Door_Open(void);
void Door_Close(void);

void Buzzer_On(void);
void Buzzer_Off(void);

bool RFID_IsAdmin(uint8_t *uid);
void RFID_Process(void);

void CheckSystemTimers(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
char Keypad_Scan(void)
{
    const char keymap[4][4] =
    {
        {'*','0','#','A'},
        {'7','8','9','B'},
        {'4','5','6','C'},
        {'1','2','3','D'}
    };

    GPIO_TypeDef *colPort[4] =
    {
        GPIOA,
        GPIOB,
        GPIOB,
        GPIOB
    };
  uint16_t colPin[4] =
    {
        GPIO_PIN_8,
        GPIO_PIN_15,
        GPIO_PIN_14,
        GPIO_PIN_13
    };

    GPIO_TypeDef *rowPort[4] =
    {
        GPIOA,
        GPIOA,
        GPIOA,
        GPIOA
    };

    uint16_t rowPin[4] =
    {
        GPIO_PIN_12,
        GPIO_PIN_11,
        GPIO_PIN_10,
        GPIO_PIN_9
    };

    for (int c = 0; c < 4; c++)
    {
        HAL_GPIO_WritePin(
            GPIOA,
            GPIO_PIN_8,
            GPIO_PIN_SET
        );

        HAL_GPIO_WritePin(
            GPIOB,
            GPIO_PIN_13 |
            GPIO_PIN_14 |
            GPIO_PIN_15,
            GPIO_PIN_SET
        );

        HAL_GPIO_WritePin(
            colPort[c],
            colPin[c],
            GPIO_PIN_RESET
        );

        for (volatile int i = 0; i < 200; i++);

        for (int r = 0; r < 4; r++)
        {
            if (HAL_GPIO_ReadPin(
                    rowPort[r],
                    rowPin[r])
                == GPIO_PIN_RESET)
            {
                return keymap[r][c];
            }
        }
    }

    return 0;
}
bool RFID_IsAdmin(uint8_t *uid)
{
    for (uint8_t i = 0; i < UID_LEN; i++)
    {
        if (uid[i] != admin_uid[i])
        {
            return false;
        }
    }

    return true;
}


void RFID_Process(void)
{
    if (HAL_GetTick() - last_rfid_scan < RFID_SCAN_PERIOD_MS)
    {
        return;
    }

    last_rfid_scan = HAL_GetTick();

    status = MFRC522_Request(PICC_REQIDL, str);

    if (status != MI_OK)
        return;

    status = MFRC522_Anticoll(str);

    if (status != MI_OK)
        return;

    if (RFID_IsAdmin(str))
    {
        ClearInput();

        app_state = ST_ADMIN_OLD_PASSWORD;

        LCD_ShowInput("MK hien tai:");
    }
}
void Password_SaveFlash(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0;

    HAL_FLASH_Unlock();

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = PASSWORD_FLASH_ADDR;
    erase.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return;
    }

    /* Luu MAGIC */
    HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_HALFWORD,
        PASSWORD_FLASH_ADDR,
        PASSWORD_MAGIC
    );

    /* Luu password */
    for (uint32_t i = 0; i <= PASSWORD_MAX_LEN; i += 2)
    {
        uint16_t data;

        data = (uint8_t)system_password[i];

        if (i + 1 <= PASSWORD_MAX_LEN)
        {
            data |= ((uint16_t)(uint8_t)system_password[i + 1] << 8);
        }
        else
        {
            data |= 0xFF00;
        }

        HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_HALFWORD,
            PASSWORD_FLASH_ADDR + 2 + i,
            data
        );
    }

    HAL_FLASH_Lock();
}
void ClearInput(void)
{
    memset(input_buffer, 0, sizeof(input_buffer));
    input_len = 0;
}

void AppendInput(char key)
{
    if (input_len < PASSWORD_MAX_LEN)
    {
        input_buffer[input_len++] = key;
        input_buffer[input_len] = '\0';
    }
}
void Password_LoadFlash(void)
{
    uint16_t magic;

    magic = *(volatile uint16_t *)PASSWORD_FLASH_ADDR;

    /* Flash chua tung luu password */
    if (magic != PASSWORD_MAGIC)
    {
        strcpy(system_password, "1234");
        Password_SaveFlash();
        return;
    }

    const char *flash_password =
        (const char *)(PASSWORD_FLASH_ADDR + 2);

    uint8_t i;

    for (i = 0; i < PASSWORD_MAX_LEN; i++)
    {
        system_password[i] = flash_password[i];

        if (flash_password[i] == '\0')
        {
            break;
        }
    }

    system_password[PASSWORD_MAX_LEN] = '\0';

    /* Bao ve neu du lieu Flash loi */
    if (i == PASSWORD_MAX_LEN)
    {
        strcpy(system_password, "1234");
    }
}
void BackspaceInput(void)
{
    if (input_len > 0)
    {
        input_len--;
        input_buffer[input_len] = '\0';
    }
}
void LCD_ShowInput(const char *title)
{
    char hidden[17] = {0};

    /* Tao chuoi **** */
    for (uint8_t i = 0; i < input_len && i < 16; i++)
    {
        hidden[i] = '*';
    }

    hidden[input_len] = '\0';

    /* Xoa LCD */
    CLCD_I2C_Clear(&LCD1);

    /* Dong 1 */
    CLCD_I2C_SetCursor(&LCD1, 0, 0);
    CLCD_I2C_WriteString(&LCD1, title);

    /* Dong 2 */
    CLCD_I2C_SetCursor(&LCD1, 0, 1);

    if (input_len == 0)
    {
        CLCD_I2C_WriteString(&LCD1, "_");
    }
    else
    {
        CLCD_I2C_WriteString(&LCD1, hidden);
    }
}
void Servo_SetAngle(uint8_t angle)
{
    if (angle > 180)
        angle = 180;

    uint32_t ccr =
        SERVO_CCR_0_DEG +
        ((uint32_t)angle *
        (SERVO_CCR_180_DEG - SERVO_CCR_0_DEG))
        / 180;

    __HAL_TIM_SET_COMPARE(
        &htim3,
        TIM_CHANNEL_4,
        ccr
    );
}

void Door_Open(void)
{
    Servo_SetAngle(90);
}

void Door_Close(void)
{
    Servo_SetAngle(0);
}
void Buzzer_On(void)
{
    HAL_GPIO_WritePin(
        Buzzer_GPIO_Port,
        Buzzer_Pin,
        GPIO_PIN_SET
    );

    buzzer_active = true;
    buzzer_start_tick = HAL_GetTick();
}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(
        Buzzer_GPIO_Port,
        Buzzer_Pin,
        GPIO_PIN_RESET
    );

    buzzer_active = false;
}
void CheckSystemTimers(void)
{
    uint32_t now = HAL_GetTick();

    static uint32_t last_display_second = 0;


    /* =====================================
       CUA TU DONG DONG
       ===================================== */

    if (app_state == ST_DOOR_OPEN)
    {
        if (now - door_open_tick >= DOOR_OPEN_TIME_MS)
        {
            Door_Close();

            ClearInput();

            app_state = ST_LOGIN;

            LCD_ShowInput("Nhap mat khau");
        }
    }

    /* =====================================
       KHOA HE THONG 30 GIAY
       ===================================== */

    if (app_state == ST_LOCKED)
    {
        uint32_t elapsed =
            now - lock_start_tick;


        /* Hien thi dem nguoc */
        if (now - last_display_second >= 1000)
        {
            last_display_second = now;

            uint32_t remaining;

            if (elapsed < LOCK_TIME_MS)
            {
                remaining =
                    (LOCK_TIME_MS - elapsed + 999) / 1000;
            }
            else
            {
                remaining = 0;
            }

            char buf[17];

            snprintf(buf,
                     sizeof(buf),
                     "Con %lu giay",
                     remaining);

            CLCD_I2C_Clear(&LCD1);

            CLCD_I2C_SetCursor(&LCD1, 0, 0);
            CLCD_I2C_WriteString(&LCD1,
                                 "HE THONG KHOA");

            CLCD_I2C_SetCursor(&LCD1, 0, 1);
            CLCD_I2C_WriteString(&LCD1,
                                 buf);
        }


        /* Het 30 giay */
        if (elapsed >= LOCK_TIME_MS)
        {
            Buzzer_Off();

            ClearInput();

            app_state = ST_LOGIN;

            LCD_ShowInput("Nhap mat khau");
        }
    }


    /* =====================================
       BUZZER
       ===================================== */

    if (buzzer_active)
    {
        if (now - buzzer_start_tick >= BUZZER_TIME_MS)
        {
            Buzzer_Off();
        }
    }
}
void HandleKey(char key)
  {
      switch (app_state)
      {
          case ST_LOGIN:

              if (key >= '0' && key <= '9')
              {
                  AppendInput(key);
                  LCD_ShowInput("Nhap mat khau");
              }

              else if (key == '*')
              {
                  BackspaceInput();
                  LCD_ShowInput("Nhap mat khau");
              }

              else if (key == '#')
              {
                  /* ===== MỤC 25: MẬT KHẨU ĐÚNG ===== */
                  if (strcmp(input_buffer, system_password) == 0)
                  {
                      wrong_count = 0;
                      ClearInput();

                      CLCD_I2C_Clear(&LCD1);

                      CLCD_I2C_SetCursor(&LCD1, 0, 0);
                      CLCD_I2C_WriteString(&LCD1, "MAT KHAU DUNG");

                      CLCD_I2C_SetCursor(&LCD1, 0, 1);
                      CLCD_I2C_WriteString(&LCD1, "CUA DA MO");

                      Door_Open();

                      door_open_tick = HAL_GetTick();

                      app_state = ST_DOOR_OPEN;
                  }

                  /* ===== MỤC 26: MẬT KHẨU SAI ===== */
                  else
                  {
                      wrong_count++;

                      ClearInput();

                      if (wrong_count >= 3)
                      {
                          wrong_count = 0;

                          Door_Close();

                          Buzzer_On();

                          buzzer_start_tick = HAL_GetTick();
                          lock_start_tick = HAL_GetTick();

                          app_state = ST_LOCKED;

                          CLCD_I2C_Clear(&LCD1);

                          CLCD_I2C_SetCursor(&LCD1, 0, 0);
                          CLCD_I2C_WriteString(&LCD1, "HE THONG KHOA");

                          CLCD_I2C_SetCursor(&LCD1, 0, 1);
                          CLCD_I2C_WriteString(&LCD1, "Con 30 giay");
                      }
                      else
                      {
                          char msg[17];

                          snprintf(msg,
                                   sizeof(msg),
                                   "Con %d lan",
                                   3 - wrong_count);

                          CLCD_I2C_Clear(&LCD1);

                          CLCD_I2C_SetCursor(&LCD1, 0, 0);
                          CLCD_I2C_WriteString(&LCD1, "SAI MAT KHAU");

                          CLCD_I2C_SetCursor(&LCD1, 0, 1);
                          CLCD_I2C_WriteString(&LCD1, msg);

                          HAL_Delay(1000);

                          LCD_ShowInput("Nhap mat khau");
                      }
                  }
              }

              break;
              /* ==================================
                         ADMIN - NHAP MAT KHAU HIEN TAI
               ================================== */
          case ST_ADMIN_OLD_PASSWORD:

                      if (key >= '0' && key <= '9')
                      {
                          AppendInput(key);
                          LCD_ShowInput("MK hien tai:");
                      }

                      else if (key == '*')
                      {
                          BackspaceInput();
                          LCD_ShowInput("MK hien tai:");
                      }

                      else if (key == '#')
                      {
                          if (strcmp(input_buffer, system_password) == 0)
                          {
                              ClearInput();

                              app_state = ST_ADMIN_NEW_PASSWORD;

                              LCD_ShowInput("Nhap MK moi:");
                          }
                          else
                          {
                              ClearInput();

                              app_state = ST_LOGIN;

                              CLCD_I2C_Clear(&LCD1);
                              CLCD_I2C_WriteString(&LCD1, "SAI MAT KHAU");

                              HAL_Delay(1000);

                              LCD_ShowInput("Nhap mat khau");
                          }
                      }

                      break;


                  /* ==================================
                     ADMIN - NHAP MAT KHAU MOI
                     ================================== */
                  case ST_ADMIN_NEW_PASSWORD:

                      if (key >= '0' && key <= '9')
                      {
                          AppendInput(key);
                          LCD_ShowInput("Nhap MK moi:");
                      }

                      else if (key == '*')
                      {
                          BackspaceInput();
                          LCD_ShowInput("Nhap MK moi:");
                      }

                      else if (key == '#')
                      {
                          if (input_len >= 4)
                          {
                              strcpy(system_password,
                                     input_buffer);

                              Password_SaveFlash();

                              ClearInput();

                              app_state = ST_LOGIN;

                              CLCD_I2C_Clear(&LCD1);

                              CLCD_I2C_SetCursor(&LCD1, 0, 0);
                              CLCD_I2C_WriteString(&LCD1, "DOI MAT KHAU");

                              CLCD_I2C_SetCursor(&LCD1, 0, 1);
                              CLCD_I2C_WriteString(&LCD1, "THANH CONG");

                              HAL_Delay(1500);

                              LCD_ShowInput("Nhap mat khau");
                          }
                      }

                      break;


                  case ST_DOOR_OPEN:
                      /* Không xử lý mật khẩu khi cửa đang mở */
                      break;


                  case ST_LOCKED:
                      /* Không cho nhập khi đang bị khóa 30 giây */
                      break;


                  default:
                      break;
              }
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
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  /* LCD */
  CLCD_I2C_Init(
      &LCD1,
      &hi2c1,
      0x4E,
      16,
      2
  );


  /* RFID */
  MFRC522_Init();


  /* Servo */
  HAL_TIM_PWM_Start(
      &htim3,
      TIM_CHANNEL_4
  );

  Door_Close();


  /* Buzzer */
  Buzzer_Off();

  Password_LoadFlash();

  /* LCD */
  CLCD_I2C_Clear(&LCD1);


  LCD_ShowInput("Nhap mat khau");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  char key = Keypad_Scan();

	     if (key != 0)
	     {
	         HandleKey(key);

	         /* Cho nha phim */
	         while (Keypad_Scan() != 0)
	         {
	             HAL_Delay(10);
	         }

	         HAL_Delay(20);
	     }


	     /* RFID chi hoat dong o man hinh Login */
	     if (app_state == ST_LOGIN)
	     {
	         RFID_Process();
	     }


	     /* Kiem tra servo, khoa 30s, buzzer */
	     CheckSystemTimers();

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
	 hi2c1.Instance = I2C1;
	    hi2c1.Init.ClockSpeed = 100000;
	    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	    hi2c1.Init.OwnAddress1 = 0;
	    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	    hi2c1.Init.OwnAddress2 = 0;
	    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

	    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
	    {
	        Error_Handler();
	    }


  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 1439;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 25;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RFID_CS_Pin|KP_C1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, RFID_RST_Pin|KP_C2_Pin|KP_C3_Pin|KP_C4_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : Buzzer_Pin */
  GPIO_InitStruct.Pin = Buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Buzzer_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RFID_CS_Pin */
  GPIO_InitStruct.Pin = RFID_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(RFID_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RFID_RST_Pin */
  GPIO_InitStruct.Pin = RFID_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(RFID_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KP_C2_Pin KP_C3_Pin KP_C4_Pin */
  GPIO_InitStruct.Pin = KP_C2_Pin|KP_C3_Pin|KP_C4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : KP_C1_Pin */
  GPIO_InitStruct.Pin = KP_C1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(KP_C1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KP_R4_Pin KP_R3_Pin KP_R2_Pin KP_R1_Pin */
  GPIO_InitStruct.Pin = KP_R4_Pin|KP_R3_Pin|KP_R2_Pin|KP_R1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
