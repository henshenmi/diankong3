#电控组第三次培训作业

- **马晨曦**
- ***20263001274***
## 功能说明

实现三灯流水灯、按键控制方向与暂停、USART1 数据包控制、
软件 I²C OLED 状态显示及串口中断反馈。

## 引脚分配

| 功能 | 引脚 |
|---|---|
| LED1 | PA0 |
| LED2 | PA1 |
| LED3 | PA2 |
| 方向按键 | PB12 |
| 暂停按键 | PB13 |
| USART1 TX | PA9 |
| USART1 RX | PA10 |
| OLED SDA | PB0 |
| OLED SCL | PB1 |

## 串口数据包

数据包格式：

AA RUN DIR STEP_L STEP_H CHECK

- AA：固定帧头
- RUN：00 暂停，01 运行
- DIR：00 正向，01 反向
- STEP_L、STEP_H：间隔低字节、高字节
- CHECK：前五个字节相加后保留低 8 位

## 串口反馈格式

STATE=RUN,DIR=FWD,STEP=300,LED=1

串口反馈使用 HAL_UART_Transmit_IT() 发送，未使用 DMA。

## 程序说明

- 外部中断只记录按键事件。
- 串口接收完成回调只设置数据包就绪标志。
- 数据包校验、状态修改、OLED 刷新和流水灯由主循环处理。
- 流水灯使用 HAL_GetTick() 非阻塞计时。
## 参考资料与驱动来源
知涛研习
Keysking
