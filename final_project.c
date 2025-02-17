#include "main.h"
#include "lcd.h" // Include your LCD library
#include "usart.h" // Include your UART library
#include "gpio.h" // Include your GPIO library
#include "string.h" // For string manipulation
#include "stdio.h" // For sprintf

#define TRIG_PIN GPIO_PIN_0 // Define your TRIG pin
#define ECHO_PIN GPIO_PIN_1 // Define your ECHO pin
#define PIR_PIN GPIO_PIN_2 // Define your PIR sensor pin
#define LED_PIN GPIO_PIN_3 // Define your LED pin
#define BUZZER_PIN GPIO_PIN_4 // Define your Buzzer pin

#define WIFI_SSID "your_wifi_ssid" // Replace with your Wi-Fi SSID
#define WIFI_PASSWORD "your_wifi_password" // Replace with your Wi-Fi password
#define THINGSPEAK_API_KEY "your_thingspeak_api_key" // Replace with your ThingSpeak API key

void SystemClock_Config(void);
void Ultrasonic_Init(void);
float Get_Distance(void);
void Send_Data_To_Thingspeak(float distance);
void Alert_Motion_Detected(void);
void ESP8266_Init(void);
void ESP8266_Send(float distance);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    
    // Initialize peripherals
    MX_GPIO_Init();
    MX_USART2_UART_Init(); // Initialize UART for USB to TTL
    LCD_Init(); // Initialize LCD
    ESP8266_Init(); // Initialize ESP8266

    Ultrasonic_Init(); // Initialize Ultrasonic sensor

    while (1) {
        // Measure distance
        float distance = Get_Distance();
        
        // Display distance on LCD
        LCD_Clear();
        LCD_Print("Distance: %.2f cm", distance);
        
        // Send data to Thingspeak
        Send_Data_To_Thingspeak(distance);
        
        // Check for motion
        if (HAL_GPIO_ReadPin(GPIOB, PIR_PIN) == GPIO_PIN_SET) {
            Alert_Motion_Detected();
        }
        
        // Send distance data to Serial Monitor
        char buffer[50];
        sprintf(buffer, "Distance: %.2f cm\n", distance);
        HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        
        HAL_Delay(1000); // Delay for 1 second
    }
}

void Ultrasonic_Init(void) {
    // Initialize the ultrasonic sensor pins
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = TRIG_PIN | ECHO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

float Get_Distance(void) {
    // Trigger the ultrasonic sensor
    HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOA, TRIG_PIN, GPIO_PIN_RESET);
    
    // Measure the duration of the echo
    uint32_t start_time = HAL_GetTick();
    while (HAL_GPIO_ReadPin(GPIOA, ECHO_PIN) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - start_time > 100) return -1; // Timeout
    }
    
    start_time = HAL_GetTick();
    while (HAL_GPIO_ReadPin(GPIOA, ECHO_PIN) == GPIO_PIN_SET) {
        if (HAL_GetTick() - start_time > 100) return -1; // Timeout
    }
    
    uint32_t duration = HAL_GetTick() - start_time;
    float distance = (duration * 0.034) / 2; // Calculate distance in cm
    return distance;
}

void Send_Data_To_Thingspeak(float distance) {
    ESP8266_Send(distance); // Send data to Thingspeak via ESP8266
}

void Alert_Motion_Detected(void) {
    HAL_GPIO_WritePin(GPIOB, LED_PIN, GPIO_PIN_SET); // Turn on LED
    HAL_GPIO_WritePin(GPIOB, BUZZER_PIN, GPIO_PIN_SET); // Turn on Buzzer
    HAL_Delay(1000); // Alert duration
    HAL_GPIO_WritePin(GPIOB, LED_PIN, GPIO_PIN_RESET); // Turn off LED
    HAL_GPIO_WritePin(GPIOB, BUZZER_PIN, GPIO_PIN_RESET); // Turn off Buzzer
}

void ESP8266_Init(void) {
    // Reset the ESP8266
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT+RST\r\n", 9, HAL_MAX_DELAY);
    HAL_Delay(2000); // Wait for the module to reset

    // Set the ESP8266 to station mode
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT+CWMODE=1\r\n", 14, HAL_MAX_DELAY);
    HAL_Delay(1000);

    // Connect to Wi-Fi
    char wifi_cmd[100];
    sprintf(wifi_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    HAL_UART_Transmit(&huart2, (uint8_t *)wifi_cmd, strlen(wifi_cmd), HAL_MAX_DELAY);
    HAL_Delay(5000); // Wait for connection
}

void ESP8266_Send(float distance) {
    // Ensure the ESP8266 is in the correct mode
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 46, HAL_MAX_DELAY);
    HAL_Delay(2000); // Wait for connection

    // Prepare the HTTP GET request
    char http_request[200];
    sprintf(http_request, "GET /update?key=%s&field1=%.2f\r\n", THINGSPEAK_API_KEY, distance);

    // Send the length of the HTTP request
    char length_cmd[50];
    sprintf(length_cmd, "AT+CIPSEND=%d\r\n", strlen(http_request));
    HAL_UART_Transmit(&huart2, (uint8_t *)length_cmd, strlen(length_cmd), HAL_MAX_DELAY);
    HAL_Delay(2000); // Wait for the ESP8266 to respond

    // Send the actual HTTP GET request
    HAL_UART_Transmit(&huart2, (uint8_t *)http_request, strlen(http_request), HAL_MAX_DELAY);
    HAL_Delay(2000); // Wait for the response

    // Close the connection
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT+CIPCLOSE\r\n", 14, HAL_MAX_DELAY);
    HAL_Delay(1000); // Wait for the command to execute
}
