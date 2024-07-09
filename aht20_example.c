#include "aht20.h"

#include <stdio.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_gpio_ex.h"
#include "iot_watchdog.h"
#include "hi_time.h"

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_gpio.h"
#include "hi_io.h"
#include "hi_i2c.h"
#include "iot_gpio.h"
#define COUNT 10

#define STACK_SIZE     (4096)
#define I2C_DATA_RATE  (400 * 1000)  // 400K
#define DELAY_S        (3)
#define duoji_GPIO 11
#define mada_GPIO 9
#define shuibeng_GPIO 6


typedef enum {
    OFF = 0,
    ON
} E53IA1Status;

void Aht20TestTask(void)
{
    static int count = 1000;
    uint32_t retval = 0;
    int duoji=0;

    IoTGpioInit(duoji_GPIO);
    IoTGpioSetFunc(duoji_GPIO,0);
    IoTGpioSetDir(duoji_GPIO,IOT_GPIO_DIR_OUT);

        IoTGpioInit(mada_GPIO);
    IoTGpioSetFunc(mada_GPIO, 0);
    IoTGpioSetDir(mada_GPIO, IOT_GPIO_DIR_OUT);

    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);

    hi_i2c_init(HI_I2C_IDX_0, I2C_DATA_RATE);

    retval = AHT20_Calibrate();
    printf("AHT20_Calibrate: %u\r\n", retval);

    while (count--) {
        unsigned int time = 00;
        float temp = 0.0, humi = 0.0;

        retval = AHT20_StartMeasure();

        retval = AHT20_GetMeasureResult(&temp, &humi);
        printf("  temp = %.2f, humi = %.2f\r\n",
                retval,temp, humi);

        sleep(DELAY_S);
    
    if (humi > 80) {
 IoTGpioSetOutputVal(mada_GPIO, 1);  
 } else {
IoTGpioSetOutputVal(mada_GPIO, 0); 
 }
 if(humi>80  && duoji==0)
    {       
         EngineTurnRight();
        TaskMsleep(time);

 duoji=1;
 }
 if(humi<40&&duoji==1)
 {
         RegressMiddle();
        TaskMsleep(time);
 duoji=0;
 }

 if ( humi < 40)
  {
    IoTGpioSetOutputVal(shuibeng_GPIO, 1); 
 }
 else
 {
    IoTGpioSetOutputVal(shuibeng_GPIO, 0); 
 }

}
}

void RegressMiddle(void)
{
    unsigned int duoji = 15000;
    for (int i = 0; i < COUNT; i++) {
      
    }
}


void EngineTurnRight(void)
{
    unsigned int duoji = 5000;
    for (int i = 0; i < COUNT; i++) {
      
    }
}


void Aht20Test(void)
{
    osThreadAttr_t attr;

    attr.name = "Aht20Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(Aht20TestTask, NULL, &attr) == NULL) {
        printf("[Aht20Test] Failed to create Aht20TestTask!\n");
    }
}
APP_FEATURE_INIT(Aht20Test);
