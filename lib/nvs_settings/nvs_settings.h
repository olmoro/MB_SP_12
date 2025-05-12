#ifndef _NVS_SETTINGS_H_
#define _NVS_SETTINGS_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Инициализация NVS хранилища:
 *  - очистка, если нет места или прошивается новая версия;
 */
esp_err_t nvs_storage_init(void);

/**   Загрузка всех параметров из NVS
 * - проверка версии прошивки;
 * - сброс настроек если версия не найдена (первый запуск) или версия не совпадает с текущей;
 * - 
 * - 
 * - 
 */
esp_err_t load_all_parameters();


#ifdef __cplusplus
}
#endif

#endif // !_NVS_SETTINGS_H_
