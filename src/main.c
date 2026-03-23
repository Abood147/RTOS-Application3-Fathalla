/* --------------------------------------------------------------
   Application: 03
   Release Type: Interrupt-Driven Task Synchronization
   Class: Real Time Systems - Sp 2026
   Author: Abdulrahman Fathalla
   AI Use: Used Claude to assist with code structure, debugging, and ISR/semaphore implementation
---------------------------------------------------------------*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "math.h"

#define LED_PIN         GPIO_NUM_2
#define LDR_PIN         GPIO_NUM_1
#define BUTTON_PIN      GPIO_NUM_4
#define LDR_ADC_CHANNEL ADC1_CHANNEL_0

#define AVG_WINDOW       10
#define SENSOR_THRESHOLD 20
#define BUFFER_SIZE      50

// Circular buffer shared between sensor and logger tasks
float sensorBuffer[BUFFER_SIZE];
int bufferIdx   = 0;  // next write position
int bufferCount = 0;  // number of valid readings stored

// Synchronization objects
SemaphoreHandle_t xButtonSem;
SemaphoreHandle_t xLogMutex;


// LED blink task — toggles every 700ms (1.4s period), priority 1
void led_task(void *pvParameters) {
    bool led_status = false;
    while (1) {
        gpio_set_level(LED_PIN, led_status);
        led_status = !led_status;
        vTaskDelay(pdMS_TO_TICKS(700));
    }
    vTaskDelete(NULL);
}

// Console print task — prints status every 7 seconds, priority 1
void print_status_task(void *pvParameters) {
    while (1) {
        printf("[STATUS] Security system operational — all clear.\n");
        vTaskDelay(pdMS_TO_TICKS(7000));
    }
    vTaskDelete(NULL);
}

// Sensor task — reads LDR every 500ms, stores in circular buffer, priority 2
void sensor_task(void *pvParameters) {

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Variables to compute LUX
    int Analog_Value_Read;
    float Vmeasure = 0.;
    float Rmeasure = 0.;
    float lux = 0.;
    // Variables for moving average
    float luxreadings[AVG_WINDOW] = {0};
    int idx = 0;
    float sum = 0;

    // Pre-fill the readings array with an initial sample to avoid startup anomaly
    for(int i = 0; i < AVG_WINDOW; ++i) {
        Analog_Value_Read =  adc1_get_raw(LDR_ADC_CHANNEL);
        Vmeasure = (Analog_Value_Read / 4095.0) * 3.3; 
        Rmeasure = (Vmeasure * 10000.0) / (3.3 - Vmeasure); 
        if (Rmeasure < 1.0f) Rmeasure = 1.0f; // prevent division by zero
        lux = pow((50.0 / Rmeasure), (1.0/0.7)); 
        luxreadings[i] = lux;
        sum += luxreadings[i];
    }

    const TickType_t periodTicks = pdMS_TO_TICKS(500); // 500 ms period
    TickType_t lastWakeTime = xTaskGetTickCount(); // initialize last wake time

    while (1) {

        // Read current sensor value
        Analog_Value_Read = adc1_get_raw(LDR_ADC_CHANNEL);
        //printf("**raw **: Sensor %d\n", Analog_Value_Read);

        // Compute LUX
        Vmeasure = (Analog_Value_Read / 4095.0) * 3.3; 
        Rmeasure = (Vmeasure * 10000.0) / (3.3 - Vmeasure); 
        if (Rmeasure < 1.0f) Rmeasure = 1.0f; // prevent division by zero
        lux = pow((50.0 / Rmeasure), (1.0/0.7)); 
       
        // Update moving average buffer 
        sum -= luxreadings[idx];       // remove oldest value from sum
        luxreadings[idx] = lux;        // place new reading
        sum += lux;                 // add new value to sum
        idx = (idx + 1) % AVG_WINDOW;
        float avg = sum / AVG_WINDOW; // compute average

        // Store in circular buffer (shared with logger)
        xSemaphoreTake(xLogMutex, portMAX_DELAY);
        sensorBuffer[bufferIdx] = lux;
        bufferIdx = (bufferIdx + 1) % BUFFER_SIZE;
        if (bufferCount < BUFFER_SIZE) bufferCount++;
        xSemaphoreGive(xLogMutex);

        if (avg > SENSOR_THRESHOLD) {
            printf("**SECURITY ALERT**: Possible Breach! AvgLux=%.2f (threshold=%.2d)\n", avg, SENSOR_THRESHOLD);
        } else {
          printf("[SENSOR] Lux avg=%.2f\n", avg);
        }

        vTaskDelayUntil(&lastWakeTime, periodTicks);
    }
}

// ISR — fires on button press (falling edge), gives semaphore to wake logger
void IRAM_ATTR button_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Logger task — blocks until button press, then computes and dumps buffer stats, priority 3
void logger_task(void *pvParameters) {
    while (1) {
        // Block here until ISR gives the semaphore
        xSemaphoreTake(xButtonSem, portMAX_DELAY);
        printf("[LOGGER] Admin log dump triggered.\n");

        // Lock mutex to safely copy buffer
        xSemaphoreTake(xLogMutex, portMAX_DELAY);
        int count = bufferCount;
        float localBuffer[BUFFER_SIZE];
        for (int i = 0; i < count; i++) localBuffer[i] = sensorBuffer[i];
        xSemaphoreGive(xLogMutex);

        // Compute stats
        float min = localBuffer[0], max = localBuffer[0], sum = 0;
        int exceeded = 0;
        for (int i = 0; i < count; i++) {
            if (localBuffer[i] < min) min = localBuffer[i];
            if (localBuffer[i] > max) max = localBuffer[i];
            sum += localBuffer[i];
            if (localBuffer[i] > SENSOR_THRESHOLD) exceeded++;
        }
        float avg = count > 0 ? sum / count : 0;

        printf("[LOGGER] Readings: %d | Min: %.2f | Max: %.2f | Avg: %.2f | Threshold exceedances: %d\n",
               count, min, max, avg, exceeded);
    }
    vTaskDelete(NULL);
}


void app_main() {
    // Initialize LED GPIO     
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    //LDR input
    gpio_reset_pin(LDR_PIN);
    gpio_set_direction(LDR_PIN, GPIO_MODE_INPUT);
 

    // ADC config
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Button input with internal pull-up, falling edge interrupt
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_PIN);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    // Create synchronization objects
    xButtonSem = xSemaphoreCreateBinary();
    xLogMutex  = xSemaphoreCreateMutex();

    // Create all tasks pinned to core 1

    xTaskCreatePinnedToCore(led_task, "BlinkTask", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(print_status_task, "PrintTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(sensor_task, "SensorTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(logger_task, "LoggerTask", 4096, NULL, 3, NULL, 1);

}
