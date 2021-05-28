# M251BSP_7_SEGMENT
 M251BSP_7_SEGMENT


update @ 2021/05/28

1. use GPIO and QSPI to drive 74HC595 , for 7 segment

	PA0 : QSPI0_MOSI0 , DS , SER
	
	PF2 : QSPI0_CLK , SHCP , SCK
	
	PA3 : QSPI0_SS , STCP , RCLK

check define as below 

	ENABLE_GPIO_DRIVE_7SEGMENT , 

	ENABLE_QSPI_DRIVE_7SEGMENT

2. below is 74HC595 with 7segment schematic

![image](https://github.com/released/M251BSP_7_SEGMENT/blob/main/74HC595_7segment_schematic.jpg)


power on log message  , 

![image](https://github.com/released/M251BSP_7_SEGMENT/blob/main/log_counter.jpg)


when digi is 0001 , 

![image](https://github.com/released/M251BSP_7_SEGMENT/blob/main/LA_counter_0001.jpg)

when digi is 0396 , 

![image](https://github.com/released/M251BSP_7_SEGMENT/blob/main/LA_counter_0396.jpg)

below is GPIO drive waveform , 

![image](https://github.com/released/M251BSP_7_SEGMENT/blob/main/LA_counter_GPIO.jpg)


