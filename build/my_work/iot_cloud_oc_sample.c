#include "hi_io.h"
#include "iot_watchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "lwip/sockets.h"
#include "wifi_connect.h"

#include "E53_IA1.h"
#include "hi_adc.h"
#include "oc_mqtt.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "hi_time.h"

#define MSGQUEUE_COUNT 16 // number of Message Queue Objects
#define MSGQUEUE_SIZE 10
#define CLOUD_TASK_STACK_SIZE (1024 * 10)
#define CLOUD_TASK_PRIO 24
#define LED_TASK_STACK_SIZE (1024 * 4)
#define LED_TASK_PRIO 25
#define SENSOR_TASK_STACK_SIZE (1024 * 2)
#define SENSOR_TASK_PRIO 25
#define TASK_DELAY_3S 3

#define  COUNT   10
#define  FREQ_TIME    20000

#define LED_GPIO 2

typedef struct { // object data type
    char *Buf;
    uint8_t Idx;
} MSGQUEUE_OBJ_t;

MSGQUEUE_OBJ_t msg;
osMessageQueueId_t mid_MsgQueue; // message queue id

#define CLIENT_ID "6671a4a97dbfd46fabc0e777_20041224_0_1_2024070910"
#define USERNAME "6671a4a97dbfd46fabc0e777_20041224"
#define PASSWORD "1b0c2bdfd9419840ecb725ae3803706bde30eac51dff01c641786419d0faaa8a"

typedef enum {
    en_msg_cmd = 0,
    en_msg_report,
} en_msg_type_t;

typedef struct {
    char *request_id;
    char *payload;
} cmd_t;

typedef struct {
    int lum;
    int temp;
    int hum;
} report_t;

typedef struct {
    en_msg_type_t msg_type;
    union {
        cmd_t cmd;
        report_t report;
    } msg;
} app_msg_t;

typedef struct {
    int connected;
    int led;
    int motor;
} app_cb_t;
static app_cb_t g_app_cb;

static void ReportMsg(report_t *report)
{
    oc_mqtt_profile_service_t service;
    oc_mqtt_profile_kv_t Temperature;
    oc_mqtt_profile_kv_t Humidity;
    oc_mqtt_profile_kv_t Luminance;
    oc_mqtt_profile_kv_t led;
    oc_mqtt_profile_kv_t motor;

    service.event_time = NULL;
    service.service_id = "Agriculture";
    service.service_property = &Temperature;
    service.nxt = NULL;

    Temperature.key = "temp";
    Temperature.value = &report->temp;
    Temperature.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    Temperature.nxt = &Humidity;

    Humidity.key = "humi";
    Humidity.value = &report->hum;
    Humidity.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    Humidity.nxt = &Luminance;

    Luminance.key = "Luminance";
    Luminance.value = &report->lum;
    Luminance.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    Luminance.nxt = &led;

    led.key = "LightStatus";
    led.value = g_app_cb.led ? "ON" : "OFF";
    led.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    led.nxt = &motor;

    motor.key = "MotorStatus";
    motor.value = g_app_cb.motor ? "ON" : "OFF";
    motor.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    motor.nxt = NULL;

    oc_mqtt_profile_propertyreport(USERNAME, &service);
    return;
}

void MsgRcvCallback(uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size)
{
    app_msg_t *app_msg;

    int ret = 0;
    app_msg = malloc(sizeof(app_msg_t));
    app_msg->msg_type = en_msg_cmd;
    app_msg->msg.cmd.payload = (char *)recv_data;

    printf("recv data is %s\n", recv_data);
    ret = osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U);
    if (ret != 0) {
        free(recv_data);
    }
    *resp_data = NULL;
    *resp_size = 0;
}

static void oc_cmdresp(cmd_t *cmd, int cmdret)
{
    oc_mqtt_profile_cmdresp_t cmdresp;
    ///< do the response
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = NULL;
    (void)oc_mqtt_profile_cmdresp(NULL, &cmdresp);
}
///< COMMAND DEAL
#include <cJSON.h>
static void DealCmdMsg(cmd_t *cmd)
{
    cJSON *obj_root, *obj_cmdname, *obj_paras, *obj_para;

    int cmdret = 1;

    obj_root = cJSON_Parse(cmd->payload);
    if (obj_root == NULL) {
        oc_cmdresp(cmd, cmdret);
    }

    obj_cmdname = cJSON_GetObjectItem(obj_root, "command_name");
    if (obj_cmdname == NULL) {
        cJSON_Delete(obj_root);
    }
    if (strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_light") == 0) {
        obj_paras = cJSON_GetObjectItem(obj_root, "paras");
        if (obj_paras == NULL) {
            cJSON_Delete(obj_root);
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Light");
        if (obj_para == NULL) {
            cJSON_Delete(obj_root);
        }
        ///< operate the LED here
        if (strcmp(cJSON_GetStringValue(obj_para), "ON") == 0) {
            g_app_cb.led = 1;
            LightStatusSet(ON);
            printf("Light On!");
        } else {
            g_app_cb.led = 0;
            LightStatusSet(OFF);
            printf("Light Off!");
        }
        cmdret = 0;
    } else if (strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Motor") == 0) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (obj_paras == NULL) {
            cJSON_Delete(obj_root);
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
        if (obj_para == NULL) {
            cJSON_Delete(obj_root);
        }
        if (strcmp(cJSON_GetStringValue(obj_para), "ON") == 0) {
            g_app_cb.motor = 1;
            MotorStatusSet(ON);
            printf("Motor On!");
        } else {
            g_app_cb.motor = 0;
            MotorStatusSet(OFF);
            printf("Motor Off!");
        }
        cmdret = 0;
    }

    cJSON_Delete(obj_root);
}

static int CloudMainTaskEntry(void)
{
    app_msg_t *app_msg;

    uint32_t ret = WifiConnect("P40 Pro", "12345678");

    device_info_init(CLIENT_ID, USERNAME, PASSWORD);
    oc_mqtt_init();
    oc_set_cmd_rsp_cb(MsgRcvCallback);

    while (1) {
        app_msg = NULL;
        (void)osMessageQueueGet(mid_MsgQueue, (void **)&app_msg, NULL, 0U);
        if (app_msg != NULL) {
            switch (app_msg->msg_type) {
                case en_msg_cmd:
                    DealCmdMsg(&app_msg->msg.cmd);
                    break;
                case en_msg_report:
                    ReportMsg(&app_msg->msg.report);
                    break;
                default:
                    break;
            }
            free(app_msg);
        }
    }
    return 0;
}



void SensorTaskEntry(void)
{

 unsigned short data1 = 0;
 int ret,ret0 = 0;

    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO,IOT_GPIO_DIR_OUT);

    app_msg_t *app_msg;
    E53IA1Data data;

 ret = E53IA1Init();
 if (ret != 0)
 {
 printf("E53_IA1 Init failed!\r\n");
 return;
 }
    while (1) {

 ret = E53IA1ReadData(&data);

 if (ret != 0) {
 printf("E53_IA1 Read Data failed!\r\n");
 return;
 }

 app_msg = malloc(sizeof(app_msg_t));
 printf("SENSOR:lumi:%.2f", data.Lux);
 if (app_msg != NULL) {
 app_msg->msg_type = en_msg_report;
    app_msg->msg.report.lum = (int)data.Lux;
    if (osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U) != 0) 
    {
    free(app_msg);
    }
 if ((int)data.Lux < 800) {
 IoTGpioSetOutputVal(LED_GPIO, 1); 
 sleep(1);
 } 
 else {
 IoTGpioSetOutputVal(LED_GPIO, 0);
 sleep(1); 
 }
 sleep(TASK_DELAY_3S);
 }
    }
}


static void IotMainTaskEntry(void)
{
 mid_MsgQueue = osMessageQueueNew(MSGQUEUE_COUNT, MSGQUEUE_SIZE, NULL);
 if (mid_MsgQueue == NULL) 
 {
 printf("Failed to create Message Queue!\n");
 }
 osThreadAttr_t attr;
 attr.name = "CloudMainTaskEntry";
 attr.attr_bits = 0U;
 attr.cb_mem = NULL;
 attr.cb_size = 0U;
 attr.stack_mem = NULL;
 attr.stack_size = CLOUD_TASK_STACK_SIZE;
 attr.priority = CLOUD_TASK_PRIO;
 if (osThreadNew((osThreadFunc_t)CloudMainTaskEntry, NULL, &attr) == NULL)
 {
 printf("Failed to create CloudMainTaskEntry!\n");
 }
 attr.stack_size = SENSOR_TASK_STACK_SIZE;
 attr.priority = SENSOR_TASK_PRIO;
 attr.name = "SensorTaskEntry";
 if (osThreadNew((osThreadFunc_t)SensorTaskEntry, NULL, &attr) == NULL)
 {
 printf("Failed to create SensorTaskEntry!\n");
 }
 
}


APP_FEATURE_INIT(IotMainTaskEntry);

