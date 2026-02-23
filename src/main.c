/* --------------------------------------------------------------
   Application: 02 - Rev1
   Release Type: Baseline Preemption
   Class: Real Time Systems - Sp 2026
   Author: [M Borowczak] 
   Email: [mike.borowczak@ucf.edu]
   Company: [University of Central Florida]
   Website: theDRACOlab.com
   AI Use: Commented inline -- None
---------------------------------------------------------------*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
// TODO1: ADD IN additional INCLUDES BELOW
#include "driver/adc.h"
#include "math.h"
// TODO1: ADD IN additional INCLUDES ABOVE

#define LED_PIN GPIO_NUM_2  // Using GPIO2 for the LED

// TODO2: ADD IN LDR_PIN to gpio pin 32
#define LDR_PIN GPIO_NUM_1

// TODO3: ADD IN LDR_ADC_CHANNEL -- if you used gpio pin 32 it should map to ADC1_CHANNEL4
#define LDR_ADC_CHANNEL ADC1_CHANNEL_0

// TODO99: Consider Adding AVG_WINDOW and SENSOR_THRESHOLD as global defines
#define AVG_WINDOW 10
#define SENSOR_THRESHOLD 0.5

//TODO9: Adjust Task to blink an LED at 1 Hz (1000 ms period: 500 ms ON, 500 ms OFF);
//Consider supressing the output
void led_task(void *pvParameters) {
    bool led_status = false;
    TickType_t currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );

    while (1) {
        currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );
        gpio_set_level(LED_PIN, led_status);  //TODO: Set LED pin high or low based on led_status flag;
        led_status = !led_status;  //TODO: toggle state for next loop 
        
        printf("[POWER] System power is %s @ %lu\n", led_status ? "ON" : "OFF", currentTime);
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay for 500 ms using MS to Ticks Function vs alternative which is MS / ticks per ms
       
    
    }
    vTaskDelete(NULL); // We'll never get here; tasks run forever
}

//TODO10: Task to print a message every 1000 ms (1 seconds)
void print_status_task(void *pvParameters) {
    TickType_t currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );
    TickType_t previousTime = 0;
    while (1) {
        previousTime = currentTime;
        currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );
        
        // Prints a periodic message based on a thematic area. Output a timestamp (ms) and period (ms)
        printf("[STATUS] Security system operational @ time %lu [period = %lu]!\n",currentTime, currentTime-previousTime);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1000 ms
    }
    vTaskDelete(NULL); // We'll never get here; tasks run forever
}

//TODO11: Create new task for sensor reading every 500ms
void sensor_task(void *pvParameters) {
    //TODO110 Configure ADC (12-bit width, 0-3.3V range with 11dB attenuation)
    //adc1_config_width(ADC_WIDTH_BIT_12);
    //adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_12); //could be ADC_ATTEN_DB_11
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

    //TODO11a consider where AVG_WINDOW is defined, it could be here, or global value 
    // int AVG_WINDOW = 10;
    // int SENSOR_THRESHOLD = 500;

    //See TODO 99
    // Pre-fill the readings array with an initial sample to avoid startup anomaly
    for(int i = 0; i < AVG_WINDOW; ++i) {
        Analog_Value_Read =  adc1_get_raw(LDR_ADC_CHANNEL);
        Vmeasure = (Analog_Value_Read / 4095.0) * 3.3; //TODO11b correct this with the equation seen earlier
        Rmeasure = (Vmeasure * 10000.0) / (3.3 - Vmeasure); //TODO11c correct this with the equation seen earlier
        lux = pow((50.0 / Rmeasure), (1.0/0.7)); //TODO11d correct this with the equation seen earlier
        luxreadings[i] = lux;
        sum += luxreadings[i];
    }

    const TickType_t periodTicks = pdMS_TO_TICKS(500); // 500 ms period
    TickType_t lastWakeTime = xTaskGetTickCount(); // initialize last wake time

    while (1) {
    TickType_t now = xTaskGetTickCount();
    printf("[SENSOR] Time=%lu ms\n", pdTICKS_TO_MS(now));

        // Read current sensor value
        Analog_Value_Read = adc1_get_raw(LDR_ADC_CHANNEL);
        //printf("**raw **: Sensor %d\n", Analog_Value_Read);

        // Compute LUX
        Vmeasure = (Analog_Value_Read / 4095.0) * 3.3; //TODO11e correct this with the equation seen earlier
        Rmeasure = (Vmeasure * 10000.0) / (3.3 - Vmeasure); //TODO11f correct this with the equation seen earlier
        lux = pow((50.0 / Rmeasure), (1.0/0.7)); //TODO11g correct this with the equation seen earlier
       
        // Update moving average buffer 
        sum -= luxreadings[idx];       // remove oldest value from sum
        
        luxreadings[idx] = lux;        // place new reading
        sum += lux;                 // add new value to sum
        idx = (idx + 1) % AVG_WINDOW;
        float avg = sum / AVG_WINDOW; // compute average

        //TODO11h Check threshold and print alert if exceeded or below based on context
        if (avg > SENSOR_THRESHOLD) {
            printf("**SECURITY ALERT**: Possible Breach! AvgLux=%.2f (threshold=%.2f)\n", avg, SENSOR_THRESHOLD);
        } else {
          //TODO11i
          printf("Lux avg=%.2f\n", avg);
        }
        //TODO11j: Print out time period [to help with answering Eng/Analysis quetionst (hint check Application Solution #1 )
        //https://wokwi.com/projects/430683087703949313
        //TODO11k Replace vTaskDelay with vTaskDelayUntil with parameters &lastWakeTime and periodTicks
        vTaskDelayUntil(&lastWakeTime, periodTicks);

    }
}


void app_main() {
    // Initialize LED GPIO     
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    // TODO4 : Initialize LDR PIN as INPUT [2 lines mirroring those above]
    gpio_reset_pin(LDR_PIN);
    gpio_set_direction(LDR_PIN, GPIO_MODE_INPUT);
 

    // TODO5 : Set ADC1's resolution by calling:
    // function adc1_config_width(...) 
    // with parameter ADC_WIDTH_BIT_12
    adc1_config_width(ADC_WIDTH_BIT_12);


    // TODO6: Set the the input channel to 11 DB Attenuation using
    // function adc1_config_channel_atten(...,...) 
    // with parameters LDR_ADC_CHANNEL and ADC_ATTEN_DB_11
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);


    // Instantiate/ Create tasks: 
    // . pointer to task function, 
    // . descriptive name, [has a max length; located in the FREERTOS_CONFIG.H]
    // . stack depth, 
    // . parameters [optional] = NULL 
    // . priority [0 = low], 
    // . pointer referencing this created task [optional] = NULL
    // Learn more here https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/01-xTaskCreate
    
    // TODO7: Pin tasks to core 1    
    // Convert these xTaskCreate function calls to  xTaskCreatePinnedToCore() function calls
    // The new function takes one more parameter at the end (0 or 1);
    // pin all your tasks to core 1 

    // This is a special (custom) espressif FreeRTOS function
    // . pointer to task function, 
    // . descriptive name, [has a max length; located in the FREERTOS_CONFIG.H]
    // . stack depth, 
    // . parameters [optional] = NULL 
    // . priority [0 = low], 
    // . pointer referencing this created task [optional] = NULL
    // . core [0,1] to pin task too 
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_additions.html#_CPPv423xTaskCreatePinnedToCore14TaskFunction_tPCKcK8uint32_tPCv11UBaseType_tPC12TaskHandle_tK10BaseType_t
    
    //xTaskCreate(led_task, "LED", 2048, NULL, 1, NULL);
    //xTaskCreate(print_status_task, "STATUS", 2048, NULL, 1, NULL);
    xTaskCreatePinnedToCore(led_task, "BlinkTask", 2048, NULL, 0, NULL, 1);
    xTaskCreatePinnedToCore(print_status_task, "PrintTask", 2048, NULL, 1, NULL, 1);

    // TODO8: Make sure everything still works as expected before moving on to TODO9 (above).

    //TODO12 Add in new Sensor task; make sure it has the correct priority to preempt 
    //the other two tasks.
    xTaskCreatePinnedToCore(sensor_task, "SensorTask", 4096, NULL, 2, NULL, 1);

    //TODO13: Make sure the output is working as expected and move on to the engineering
    //and analysis part of the application. You may need to make modifications for experiments. 
    //Make sure you can return back to the working version!
}
