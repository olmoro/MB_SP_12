/**
 * Проект mb_sp_12 as an inter-network gateway (межсетевой шлюз)
 * 2025 май
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "project_config.h"
#include "board.h"
#include "uart1_task.h"
#include "uart2_task.h"
#include "nvs_settings.h"

static const char *TAG = "UART Gateway";
// static QueueHandle_t uart1_queue, uart2_queue;
// static SemaphoreHandle_t uart1_mutex, uart2_mutex;

void app_main(void)
{
    /** Инициализация NVS хранилища:
     * - очистка, если нет места или прошивается новая версия;
     * 
    */
    ESP_ERROR_CHECK(nvs_storage_init());

    /** Загрузка всех параметров из NVS
     * - проверка версии прошивки;
     * - сброс настроек если версия не найдена (первый запуск) 
     *   или версия не совпадает с текущей;
     */
    ESP_ERROR_CHECK(load_all_parameters());

    // Инициализация периферии
    boardInit();
    uart_mb_init();
    uart_sp_init();

    // Создание задач
    xTaskCreate(uart1_task, "UART1 Task", 4096, NULL, 5, NULL);
    xTaskCreate(uart2_task, "UART2 Task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System initialized");

    /* Проверка RGB светодиода */
    ledsBlue();
}
