/*
    Как это работает:
1. Инициализация NVS:  
   Выполняется стандартная инициализация NVS-хранилища. При обнаружении проблем с разделами 
   NVS выполняется их очистка.

2. Проверка версии прошивки:  
   При каждом запуске проверяется сохраненная в NVS версия прошивки. Если версия отсутствует 
   или не совпадает с текущей (указанной в `FIRMWARE_VERSION`), активируется флаг сброса настроек.

3. Сброс настроек:  
   При необходимости сброса в NVS записываются заводские значения адреса и скорости Modbus, 
   а также обновляется версия прошивки.

4. Чтение настроек:  
   Настройки всегда читаются из NVS (либо восстановленные заводские, либо ранее сохраненные).

   Особенности:
 - При изменении `CONFIG_FIRMWARE_VERSION` в новом релизе прошивки произойдет автоматический сброс настроек.
 - Заводские настройки хранятся в коде и могут быть легко изменены.
 - Все операции с NVS защищены проверками ошибок.
*/

#include "nvs_settings.h"
#include "project_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "NVS";

                                    // Считанные с NVS параметры
                                    uint8_t nvs_mb_addr;
                                    uint32_t nvs_mb_speed;
                                    uint8_t nvs_sp_addr;
                                    uint32_t nvs_sp_speed;

// Конфигурация параметров
typedef struct {
    uint16_t reg_addr_msw;  // Адрес старших бит
    uint16_t reg_addr_lsw;  // Адрес младших бит
    const char* nvs_key;    // Ключ в NVS
    uint32_t value;         // Текущее значение
} mb_sp_param_t;


// Параметры устройства
enum {
    PARAM_MB_ADDR,
    PARAM_MB_SPEED,
    PARAM_SP_ADDR,
    PARAM_SP_SPEED,
    PARAM_COUNT  // Всегда последний элемент
};

// Инициализация параметров с регистрами и ключами
static mb_sp_param_t device_params[] = 
{
    [PARAM_MB_ADDR]         = {1000, 1001, "mb_addr",  1},
    [PARAM_MB_SPEED]        = {1002, 1003, "mb_speed",  9600},

    [PARAM_SP_ADDR]         = {1004, 1005, "sp_addr",  0},
    [PARAM_SP_SPEED]        = {1006, 1007, "sp_speed",  115200},
    
};

static nvs_handle_t dev_handle;


esp_err_t nvs_storage_init()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_LOGW(TAG, "Очистка NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}


esp_err_t load_all_parameters(void) 
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("device_cfg", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    // Проверка версии прошивки
    char stored_version[16] = {0};
    size_t required_size;
    // bool need_reset = false;

    err = nvs_get_str(nvs_handle, "device_cfg", stored_version, &required_size);
    if (err != ESP_OK) return err;

    // Загрузка всех параметров из NVS
    for (int i = 0; i < PARAM_COUNT; i++) 
    {
        err = nvs_get_u32(dev_handle, device_params[i].nvs_key, &device_params[i].value);
        if (err == ESP_ERR_NVS_NOT_FOUND) 
        {
            ESP_LOGW(TAG, "Параметр %s не найден, инициализация 0", device_params[i].nvs_key);
            device_params[i].value = 0;
            nvs_set_u32(dev_handle, device_params[i].nvs_key, 0);
        }
        ESP_LOGI(TAG, "Загружен %s = %" PRIu32, device_params[i].nvs_key, device_params[i].value);
    }
    return nvs_commit(dev_handle);
}


/**
 * @brief Поиск параметра по адресу регистра
 * @param address Адрес регистра Modbus
 * @param part Указатель для возврата части параметра (0-MSW, 1-LSW)
 * @return Указатель на параметр или NULL
 */
static mb_sp_param_t* find_param_by_address(uint16_t address, uint8_t* part) 
{
    for (int i = 0; i < PARAM_COUNT; i++) 
    {
        if (address == device_params[i].reg_addr_msw) 
        {
            *part = 0;
            return &device_params[i];
        }
        if (address == device_params[i].reg_addr_lsw) 
        {
            *part = 1;
            return &device_params[i];
        }
    }
    return NULL;
}



/**
 * @brief Callback на запись регистра
 * @param address Адрес регистра
 * @param value Значение для записи
 * @return ESP_OK при успехе или код ошибки
 */
esp_err_t write_holding_register(uint16_t address, uint16_t value) 
{
    uint8_t part;
    mb_sp_param_t *param = find_param_by_address(address, &part);
    if (!param) 
    {
        ESP_LOGE(TAG, "Неверный адрес регистра: %d", address);
        return ESP_ERR_INVALID_ARG;
    }

    // Обновление значения
    uint32_t new_value = part ? 
        (param->value & 0xFFFF0000) | value : 
        (value << 16) | (param->value & 0xFFFF);

    // Проверка на изменение значения
    if (new_value != param->value) 
    {
        param->value = new_value;
        
        // Сохранение в NVS
        esp_err_t err = nvs_set_u32(dev_handle, param->nvs_key, param->value);
        if (err != ESP_OK) 
        {
            ESP_LOGE(TAG, "Ошибка записи %s: 0x%x", param->nvs_key, err);
            return err;
        }
        
        err = nvs_commit(dev_handle);
        if (err != ESP_OK) 
        {
            ESP_LOGE(TAG, "Ошибка коммита %s: 0x%x", param->nvs_key, err);
            return err;
        }
        
        ESP_LOGI(TAG, "Обновлен %s = %" PRIu32, param->nvs_key, param->value);
    }
    return ESP_OK;
}


/**
 * @brief Callback на чтение регистра
 * @param address Адрес регистра
 * @param value Указатель для возврата значения
 * @return ESP_OK при успехе или код ошибки
 */
esp_err_t read_holding_register(uint16_t address, uint16_t *value) 
{
    uint8_t part;
    mb_sp_param_t *param = find_param_by_address(address, &part);
    if (!param) 
    {
        ESP_LOGE(TAG, "Чтение несуществующего регистра: %d", address);
        return ESP_ERR_INVALID_ARG;
    }

    *value = part ? 
        (param->value & 0xFFFF) : 
        (param->value >> 16) & 0xFFFF;
    
    return ESP_OK;
}


// /**
//  * @brief Инициализация Modbus-Sp обработчиков (заглушка)
//  */
// void modbus_interface_init() 
// {
//     // Регистрация обработчиков чтения/записи
//     //set_holding_handler(write_holding_register, read_holding_register);
//     ESP_LOGI(TAG, "Modbus обработчики инициализированы");
// }







// }


    // // Инициализация NVS
    // ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    // {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ESP_ERROR_CHECK(nvs_flash_init());
    // }
    // else
    // {
    //     ESP_ERROR_CHECK(ret);
    // }

    // nvs_handle_t nvs_handle;
    // ESP_ERROR_CHECK(nvs_open("device_cfg", NVS_READWRITE, &nvs_handle));

    // // Проверка версии прошивки
    // char stored_version[16] = {0};
    // size_t required_size;
    // bool need_reset = false;

    // ret = nvs_get_str(nvs_handle, "fw_ver", stored_version, &required_size);

    // // Сброс настроек если:
    // // 1. Версия не найдена (первый запуск)
    // // 2. Версия не совпадает с текущей
    // if (ret != ESP_OK || strcmp(stored_version, FIRMWARE_VERSION) != 0)
    // {
    //     need_reset = true;
    // }






    // // Восстановление заводских настроек
    // if (need_reset)
    // {
    //     ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "mb_addr", MODBUS_FACTORY_ADDR));
    //     ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "mb_speed", MODBUS_FACTORY_SPEED));
    //     ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "sp_addr", SP_FACTORY_ADDR));
    //     ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "sp_speed", SP_FACTORY_SPEED));
    //     ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "fw_ver", FIRMWARE_VERSION));
    //     ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    //     ESP_LOGI(TAG, "Devics settings reset to factory");
    // }

    // // Чтение настроек из NVS
    // ESP_ERROR_CHECK(nvs_get_u8(nvs_handle, "mb_addr", &nvs_mb_addr));
    // ESP_ERROR_CHECK(nvs_get_u32(nvs_handle, "mb_speed", &nvs_mb_speed));
    // ESP_ERROR_CHECK(nvs_get_u8(nvs_handle, "sp_addr", &nvs_sp_addr));
    // ESP_ERROR_CHECK(nvs_get_u32(nvs_handle, "sp_speed", &nvs_sp_speed));
    // nvs_close(nvs_handle);

    // ESP_LOGI(TAG, "Mb address: %d, speed: %lu", nvs_mb_addr, nvs_mb_speed);
    // ESP_LOGI(TAG, "Sp address: %d, speed: %lu", nvs_sp_addr, nvs_sp_speed);
// }
