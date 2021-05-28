/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include	"project_config.h"


/*_____ D E C L A R A T I O N S ____________________________________________*/
//#define ENABLE_GPIO_DRIVE_7SEGMENT
#define ENABLE_QSPI_DRIVE_7SEGMENT
#define QSPI_CLK_USE_PA2
//#define QSPI_CLK_USE_PF2

#define QSPI_TARGET_FREQ						(100000ul)

#define CONTROL_DIGIT1							(PA1)
#define CONTROL_DIGIT2							(PA2)
#define CONTROL_DIGIT3							(PF3)
#define CONTROL_DIGIT4							(PB3)
#define DELAY_FACTOR  							(100)
#define NUM_OF_DIGITS 							(4)


#define DIO										(PA0)
#define RCLK										(PA3)

#if defined (QSPI_CLK_USE_PA2)
#define SCLK										(PA2)	//(PF2)
#elif defined (QSPI_CLK_USE_PF2)
#define SCLK										(PF2)
#endif

/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint32_t counter_tick = 0;

int digit_data[NUM_OF_DIGITS] = {0};
int scan_position = 0;

// digit pattern for a 7-segment display
const uint8_t digit_pattern[16] =
{
	0x3F , //B00111111,  // 0
	0x06 , //B00000110,  // 1
	0x5B , //B01011011,  // 2
	0x4F , //B01001111,  // 3
	0x66 , //B01100110,  // 4
	0x6D , //B01101101,  // 5
	0x7D , //B01111101,  // 6
	0x07 , //B00000111,  // 7
	0x7F , //B01111111,  // 8
	0x6F , //B01101111,  // 9
	0x77 , //B01110111,  // A
	0x7C , //B01111100,  // b
	0x39 , //B00111001,  // C
	0x5E , //B01011110,  // d
	0x79 , //B01111001,  // E
	0x71 , //B01110001   // F
};

const uint8_t LED_0F[] = 
{// 0	 1	  2	   3	4	 5	  6	   7	8	 9	  A	   b	C    d	  E    F    -
	0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90,0x8C,0xBF,0xC6,0xA1,0x86,0xFF,0xbf
};


/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void tick_counter(void)
{
	counter_tick++;
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void compare_buffer(uint8_t *src, uint8_t *des, int nBytes)
{
    uint16_t i = 0;	
	
    for (i = 0; i < nBytes; i++)
    {
        if (src[i] != des[i])
        {
            printf("error idx : %4d : 0x%2X , 0x%2X\r\n", i , src[i],des[i]);
			set_flag(flag_error , ENABLE);
        }
    }

	if (!is_flag_set(flag_error))
	{
    	printf("%s finish \r\n" , __FUNCTION__);	
		set_flag(flag_error , DISABLE);
	}

}

void reset_buffer(void *dest, unsigned int val, unsigned int size)
{
    uint8_t *pu8Dest;
//    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;

	#if 1
	while (size-- > 0)
		*pu8Dest++ = val;
	#else
	memset(pu8Dest, val, size * (sizeof(pu8Dest[0]) ));
	#endif
	
}

void copy_buffer(void *dest, void *src, unsigned int size)
{
    uint8_t *pu8Src, *pu8Dest;
    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;
    pu8Src  = (uint8_t *)src;


	#if 0
	  while (size--)
	    *pu8Dest++ = *pu8Src++;
	#else
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
	#endif
}

void dump_buffer(uint8_t *pucBuff, int nBytes)
{
    uint16_t i = 0;
    
    printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        printf("0x%2X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            printf("\r\n");
        }            
    }
    printf("\r\n\r\n");
}

void  dump_buffer_hex(uint8_t *pucBuff, int nBytes)
{
    int     nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            printf("%02X ", pucBuff[nIdx + i]);
        printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                printf("%c", pucBuff[nIdx + i]);
            else
                printf(".");
            nBytes--;
        }
        nIdx += 16;
        printf("\n");
    }
    printf("\n");
}

/*
	74HC59
	PIN11 : SCLK
	PIN12 : RCLK
	PIN14 : SER
	
	PIN15 : QA 	// A
	PIN1  : QB 	// B
	PIN2  : QC 	// C
	PIN3  : QD 	// D
	PIN4  : QE 	// E
	PIN5  : QF 	// F
	PIN6  : QG 	// G
	PIN7  : QH	// DP 	

*/
void GPIO_LED_OUT(uint8_t X)
{
	uint8_t i;
	for(i = 8 ; i >= 1; i--)
	{
		if (X & 0x80) 
			DIO = 1; 
		else 
			DIO = 0;
		
		X <<= 1;
		SCLK = 0;
		SCLK = 1;
	}
}

void GPIO_Seven_Segment_setValue(uint16_t value)
{
	uint8_t  *led_table;
	uint8_t i;
	uint8_t LED[4] = {0};

	uint16_t number;
	uint16_t digit_base;
  
	// get the value to be displayed
	number = value ;
	
	digit_base = 10;

	// get every values in each position of those 4 digits based on "digit_base"
	//
	// digit_base should be <= 16
	//
	// for example, if digit_base := 2, binary values will be shown. If digit_base := 16, hexidecimal values will be shown
	//
	for (i = 0; i < NUM_OF_DIGITS; i++)
	{
		digit_data[i] = number % digit_base;
		number /= digit_base;
	}

	LED[0] = digit_data[0];
	LED[1] = digit_data[1];
	LED[2] = digit_data[2];
	LED[3] = digit_data[3];

	/*************/
	led_table = (uint8_t*) LED_0F + LED[0];
	i = *led_table;

	GPIO_LED_OUT(i);
//	printf("digit : 0x%2X , " , i);
	GPIO_LED_OUT(0x01);		

	RCLK = 0;
	RCLK = 1;

	/*************/
	led_table = (uint8_t*) LED_0F + LED[1];
	i = *led_table;

	GPIO_LED_OUT(i);	
//	printf(" 0x%2X , " , i);	
	GPIO_LED_OUT(0x02);		

	RCLK = 0;
	RCLK = 1;

	/*************/
	led_table = (uint8_t*) LED_0F + LED[2];
	i = *led_table;

	GPIO_LED_OUT(i);
//	printf(" 0x%2X , " , i);		
	GPIO_LED_OUT(0x04);	

	RCLK = 0;
	RCLK = 1;

	/*************/
	led_table = (uint8_t*) LED_0F + LED[3];
	i = *led_table;

	GPIO_LED_OUT(i);
//	printf(" 0x%2X , \r\n" , i);		
	GPIO_LED_OUT(0x08);		

	RCLK = 0;
	RCLK = 1;

}

void QSPI_WriteData(uint8_t u8Value)
{
    // /CS: active
    QSPI_SET_SS_LOW(QSPI0);

    QSPI_WRITE_TX(QSPI0, u8Value);

    // wait tx finish
    while (QSPI_IS_BUSY(QSPI0));

    // /CS: de-active
    QSPI_SET_SS_HIGH(QSPI0);
}

void QSPI_Master_Init(void)
{
    /* Setup QSPI0 multi-function pins */
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA0MFP_Msk | SYS_GPA_MFPL_PA3MFP_Msk);	
    SYS->GPA_MFPL |= SYS_GPA_MFPL_PA0MFP_QSPI0_MOSI0 |  SYS_GPA_MFPL_PA3MFP_QSPI0_SS ;

	#if defined (QSPI_CLK_USE_PA2)
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA2MFP_Msk );	
    SYS->GPA_MFPL |= SYS_GPA_MFPL_PA2MFP_QSPI0_CLK ;

    /* Enable QSPI0 clock pin (PA2) schmitt trigger */
    PA->SMTEN |= GPIO_SMTEN_SMTEN2_Msk;
	
	#elif defined (QSPI_CLK_USE_PF2)
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF2MFP_Msk );	
    SYS->GPF_MFPL |= SYS_GPF_MFPL_PF2MFP_QSPI0_CLK ;

    /* Enable QSPI0 clock pin (PA2) schmitt trigger */
    PF->SMTEN |= GPIO_SMTEN_SMTEN2_Msk;
	#endif
	
    /* Enable QSPI0 I/O high slew rate */
    GPIO_SetSlewCtl(PA, 0x3F, GPIO_SLEWCTL_HIGH);	

    QSPI_Open(QSPI0, QSPI_MASTER, QSPI_MODE_0, 8, QSPI_TARGET_FREQ);

    /* Enable the automatic hardware slave select function. Select the SS pin and configure as low-active. */
    QSPI_EnableAutoSS(QSPI0, QSPI_SS, QSPI_SS_ACTIVE_LOW);
}


void Seven_Segment_updatedigit(void)
{
	uint16_t i;
	uint8_t pattern;

	#if 1
	uint8_t  *led_table;
	uint8_t  digit_change;
	
	switch(scan_position)
	{
		case 0:
			digit_change = 0x01;
			break;
		case 1:
			digit_change = 0x02;
			break;
		case 2:
			digit_change = 0x04;
			break;
		case 3:
			digit_change = 0x08;
			break;			
	}
	
    // /CS: active
    QSPI_SET_SS_LOW(QSPI0);

	led_table = (uint8_t*) LED_0F + digit_data[scan_position];
	i = *led_table;
    QSPI_WRITE_TX(QSPI0, i);

//    // wait tx finish
//    while (QSPI_IS_BUSY(QSPI0));

    QSPI_WRITE_TX(QSPI0, digit_change);

    // wait tx finish
    while (QSPI_IS_BUSY(QSPI0));

    // /CS: de-active
    QSPI_SET_SS_HIGH(QSPI0);

	#else
	// turn off all digit
	CONTROL_DIGIT1 = FALSE;
	CONTROL_DIGIT2 = FALSE;
	CONTROL_DIGIT3 = FALSE;
	CONTROL_DIGIT4 = FALSE;

	// get the digit pattern of the position to be updated
	pattern = digit_pattern[digit_data[scan_position]];

	QSPI_WriteData(~pattern);	//QSPI_WriteData(pattern);

	switch(scan_position)
	{
		case 0:
			CONTROL_DIGIT1 = TRUE;
			break;
		case 1:
			CONTROL_DIGIT2 = TRUE;
			break;
		case 2:
			CONTROL_DIGIT3 = TRUE;
			break;
		case 3:
			CONTROL_DIGIT4 = TRUE;
			break;			
	}
	#endif

	// go to next update position
	scan_position++;

	if (scan_position >= NUM_OF_DIGITS)
	{
		scan_position = 0; 
	}
}

void Seven_Segment_setValue(uint16_t value)
{
	uint16_t i;
	uint16_t number;
	uint16_t digit_base;
  
	// get the value to be displayed
//	number = value / DELAY_FACTOR;
	number = value ;
	
	digit_base = 10;

	// get every values in each position of those 4 digits based on "digit_base"
	//
	// digit_base should be <= 16
	//
	// for example, if digit_base := 2, binary values will be shown. If digit_base := 16, hexidecimal values will be shown
	//
	for (i = 0; i < NUM_OF_DIGITS; i++)
	{
		digit_data[i] = number % digit_base;
		number /= digit_base;
	}

	// update one digit
	Seven_Segment_updatedigit();

	CLK_SysTickDelay(4);

}

void Seven_Segment_loop()
{
	static uint16_t number = 0;

	if ((get_tick() % 1000) == 0)
	{
		if (is_flag_set(flag_revert))
		{
			number++;
			if (number == 9999)
			{
				set_flag(flag_revert , DISABLE);
			}			
		}
		else
		{
			number--;
			if (number == 0)
			{
				set_flag(flag_revert , ENABLE);
			}
		}

		printf("%s : %4d\r\n" , __FUNCTION__, number);
		
	}

	#if defined (ENABLE_GPIO_DRIVE_7SEGMENT)
	GPIO_Seven_Segment_setValue(number);
	#elif defined (ENABLE_QSPI_DRIVE_7SEGMENT)
	Seven_Segment_setValue(number);	
	#endif
}

/*
	PA0 : QSPI0_MOSI0 , DS , SER
	PF2 : QSPI0_CLK , SHCP , SCK
	PA3 : QSPI0_SS , STCP , RCLK
*/

void Seven_Segment_Setup(void)
{
	#if defined (ENABLE_GPIO_DRIVE_7SEGMENT)
    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA0MFP_Msk)) | (SYS_GPA_MFPL_PA0MFP_GPIO);

	#if defined (QSPI_CLK_USE_PA2)	
    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA2MFP_Msk)) | (SYS_GPA_MFPL_PA2MFP_GPIO);
	#elif defined (QSPI_CLK_USE_PF2)
    SYS->GPF_MFPL = (SYS->GPF_MFPL & ~(SYS_GPF_MFPL_PF2MFP_Msk)) | (SYS_GPF_MFPL_PF2MFP_GPIO);
	#endif
	
    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA3MFP_Msk)) | (SYS_GPA_MFPL_PA3MFP_GPIO);

    GPIO_SetMode(PA, BIT0, GPIO_MODE_OUTPUT);

	#if defined (QSPI_CLK_USE_PA2)
    GPIO_SetMode(PA, BIT2, GPIO_MODE_OUTPUT);
	
	#elif defined (QSPI_CLK_USE_PF2)
    GPIO_SetMode(PF, BIT2, GPIO_MODE_OUTPUT);
	#endif
    GPIO_SetMode(PA, BIT3, GPIO_MODE_OUTPUT);
	
	DIO = FALSE;
	RCLK = FALSE;
	SCLK = FALSE;
	
	#elif defined (ENABLE_QSPI_DRIVE_7SEGMENT)

    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA1MFP_Msk)) | (SYS_GPA_MFPL_PA1MFP_GPIO);
    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA2MFP_Msk)) | (SYS_GPA_MFPL_PA2MFP_GPIO);
    SYS->GPF_MFPL = (SYS->GPF_MFPL & ~(SYS_GPF_MFPL_PF3MFP_Msk)) | (SYS_GPF_MFPL_PF3MFP_GPIO);
    SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB3MFP_Msk)) | (SYS_GPB_MFPL_PB3MFP_GPIO);

    GPIO_SetMode(PA, BIT1, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PA, BIT2, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PF, BIT3, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PB, BIT3, GPIO_MODE_OUTPUT);

	CONTROL_DIGIT1 = FALSE;
	CONTROL_DIGIT2 = FALSE;
	CONTROL_DIGIT3 = FALSE;
	CONTROL_DIGIT4 = FALSE;
	
	QSPI_Master_Init();	
	#endif

	set_flag(flag_revert , ENABLE);

}


void GPIO_Init (void)
{
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB14MFP_Msk)) | (SYS_GPB_MFPH_PB14MFP_GPIO);

    GPIO_SetMode(PB, BIT14, GPIO_MODE_OUTPUT);

//    GPIO_SetMode(PB, BIT15, GPIO_MODE_OUTPUT);	
}

void TMR1_IRQHandler(void)
{
//	static uint32_t LOG = 0;

	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		if ((get_tick() % 1000) == 0)
		{
//        	printf("%s : %4d\r\n",__FUNCTION__,LOG++);
			PB14 ^= 1;
		}

		if ((get_tick() % 50) == 0)
		{

		}	
    }
}


void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(UART0);

	if (res == 'x' || res == 'X')
	{
		NVIC_SystemReset();
	}

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		switch(res)
		{
			case '1':
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
				NVIC_SystemReset();		
				break;
		}
	}
}

void UART0_IRQHandler(void)
{

    if(UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(UART0) == 0)
        {
            UARTx_Process();
        }
    }

    if(UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk| UART_INTSTS_BUFERRINT_Msk));
    }	
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART0_IRQn);
	
	#if (_debug_log_UART_ == 1)	//debug
	printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetCPUFreq());
	printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHXTFreq());
	printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetLXTFreq());	
	printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetPCLK0Freq());
	printf("CLK_GetPCLK1Freq : %8d\r\n",CLK_GetPCLK1Freq());	
	#endif	

}



void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable HIRC clock (Internal RC 48MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);

    /* Wait for HIRC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);

//    /* Wait for HIRC clock ready */
//    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    /* Enable HIRC clock (Internal RC 48MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);

    /* Wait for HIRC clock ready */
    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);	

//    CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);

//    /* Wait for HIRC clock ready */
//    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);	

    /* Switch HCLK clock source to Internal RC and HCLK source divide 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    /* Enable UART clock */
    CLK_EnableModuleClock(UART0_MODULE);

    /* Select UART clock source from HIRC */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC, 0);

    /* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk)) |
                    (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

    CLK_SetModuleClock(QSPI0_MODULE, CLK_CLKSEL2_QSPI0SEL_PCLK0, MODULE_NoMsk);
    CLK_EnableModuleClock(QSPI0_MODULE);

//    CLK_EnableModuleClock(PDMA_MODULE);

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}


/*
 * This is a template project for M251 series MCU. Users could based on this project to create their
 * own application without worry about the IAR/Keil project settings.
 *
 * This template application uses external crystal as HCLK source and configures UART0 to print out
 * "Hello World", users may need to do extra system configuration based on their system design.
 */
int main()
{
    SYS_Init();

	UART0_Init();
	GPIO_Init();
	TIMER1_Init();

	Seven_Segment_Setup();

    /* Got no where to go, just loop forever */
    while(1)
    {

		Seven_Segment_loop();
    }
}


/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
