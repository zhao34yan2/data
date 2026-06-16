#include "weather_manager.h"
#include "../secret_config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <algorithm>
#include <cctype>

static const char *TAG = "WeatherManager";

// HTTP 响应缓冲区（分配在 SPIRAM 上，避免占用宝贵的内部 RAM）
static char* response_buffer = NULL;
static int response_len = 0;
static const int RESPONSE_BUFFER_SIZE = 8192;

// GZIP 解压缓冲区（和风天气 API 默认返回 gzip 压缩数据）
static char* decompressed_buffer = NULL;
static const int DECOMPRESSED_BUFFER_SIZE = 8192;

esp_err_t WeatherManager::http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (response_buffer && response_len + evt->data_len < RESPONSE_BUFFER_SIZE - 1) {
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

WeatherManager::WeatherManager() {
    response_buffer = (char*)heap_caps_malloc(RESPONSE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    decompressed_buffer = (char*)heap_caps_malloc(DECOMPRESSED_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
}

WeatherManager& WeatherManager::getInstance() {
    static WeatherManager instance;
    return instance;
}

void WeatherManager::setApiConfig(const char* key, const char* host) {
    api_key_ = key;
    api_host_ = host;
}

bool WeatherManager::updateFromExternal(const std::string& city,
                                        const std::string& weather_text,
                                        const std::string& temperature,
                                        const std::string& update_time) {
    if (city.empty() || weather_text.empty() || temperature.empty()) {
        ESP_LOGW(TAG, "外部天气数据无效：city/text/temp 不能为空");
        return false;
    }

    latest_data_.city = city;
    latest_data_.text = weather_text;
    latest_data_.temp = temperature;
    latest_data_.update_time = update_time.empty() ? "mcp" : update_time;
    latest_data_.valid = true;

    ESP_LOGI(TAG, "天气已由外部写入: %s %s %s°C",
             latest_data_.city.c_str(),
             latest_data_.text.c_str(),
             latest_data_.temp.c_str());
    return true;
}

// 判断城市名是否可用于 UI 展示（排除 "Ip" 这类占位值）
static bool is_valid_display_city(const char* city) {
    if (city == nullptr || city[0] == '\0') {
        return false;
    }
    std::string normalized(city);
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), ::isspace), normalized.end());
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return normalized != "ip" && normalized != "auto_ip" && normalized != "unknown";
}

// 优先使用更像“真实地名”的字段，避免把 "Ip" 显示到页面上
static std::string pick_city_name_for_display(cJSON* first_city, const std::string& fallback_city) {
    const char* keys[] = {"adm2", "adm1", "name"};
    for (const char* key : keys) {
        cJSON* item = cJSON_GetObjectItem(first_city, key);
        if (item && cJSON_IsString(item) && is_valid_display_city(item->valuestring)) {
            return item->valuestring;
        }
    }
    return fallback_city;
}

// Open-Meteo 天气代码 → 中文描述
static const char* weather_code_to_text(int code) {
    if (code <= 0) return "晴";
    if (code <= 3) return "多云";
    if (code <= 48) return "雾";
    if (code <= 55) return "小雨";
    if (code <= 65) return "雨";
    if (code <= 75) return "雪";
    if (code <= 82) return "阵雨";
    if (code <= 86) return "阵雪";
    return "雷暴";
}

bool WeatherManager::update() {
    if (!response_buffer) {
        ESP_LOGW(TAG, "缓冲区未分配");
        return false;
    }

    // 第一步：获取城市和坐标
    double lat = FIXED_LAT, lon = FIXED_LON;
    std::string city_name = FIXED_CITY;

#ifdef FIXED_CITY
    ESP_LOGI(TAG, "使用固定城市: %s (%.2f, %.2f)", city_name.c_str(), lat, lon);
#else
    // 通过 ip-api.com 进行 IP 定位（免费，无需 API Key）
    response_len = 0;
    memset(response_buffer, 0, RESPONSE_BUFFER_SIZE);

    ESP_LOGI(TAG, "IP 定位中...");
    esp_http_client_config_t geo_config = {};
    geo_config.url = "http://ip-api.com/json/?lang=zh-CN";
    geo_config.event_handler = http_event_handler;
    geo_config.timeout_ms = 5000;

    esp_http_client_handle_t geo_client = esp_http_client_init(&geo_config);
    esp_err_t geo_err = esp_http_client_perform(geo_client);
    int geo_status = esp_http_client_get_status_code(geo_client);

    if (geo_err == ESP_OK && geo_status == 200 && response_len > 0) {
        response_buffer[response_len] = '\0';
        cJSON *root = cJSON_Parse(response_buffer);
        if (root) {
            cJSON *lat_o = cJSON_GetObjectItem(root, "lat");
            cJSON *lon_o = cJSON_GetObjectItem(root, "lon");
            cJSON *city_o = cJSON_GetObjectItem(root, "city");
            if (lat_o && lon_o) {
                lat = lat_o->valuedouble;
                lon = lon_o->valuedouble;
            }
            if (city_o && cJSON_IsString(city_o)) {
                city_name = city_o->valuestring;
            }
            cJSON_Delete(root);
            ESP_LOGI(TAG, "定位成功: %s (%.2f, %.2f)", city_name.c_str(), lat, lon);
        }
    } else {
        ESP_LOGW(TAG, "IP 定位失败，使用默认坐标");
    }
    esp_http_client_cleanup(geo_client);
#endif

    // 第二步：Open-Meteo 获取天气（免费，无需 API Key）
    response_len = 0;
    memset(response_buffer, 0, RESPONSE_BUFFER_SIZE);
    char weather_url[256];
    snprintf(weather_url, sizeof(weather_url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f&current=temperature_2m,weather_code&timezone=auto",
             lat, lon);

    ESP_LOGI(TAG, "获取天气数据...");
    esp_http_client_config_t weather_config = {};
    weather_config.url = weather_url;
    weather_config.event_handler = http_event_handler;
    weather_config.timeout_ms = 10000;
    weather_config.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t client = esp_http_client_init(&weather_config);

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    bool success = false;

    if (err == ESP_OK && status_code == 200 && response_len > 0) {
        response_buffer[response_len] = '\0';
        cJSON *root = cJSON_Parse(response_buffer);
        if (root) {
            cJSON *current = cJSON_GetObjectItem(root, "current");
            if (current) {
                cJSON *temp_o = cJSON_GetObjectItem(current, "temperature_2m");
                cJSON *code_o = cJSON_GetObjectItem(current, "weather_code");
                if (temp_o && code_o) {
                    char temp_buf[16];
                    snprintf(temp_buf, sizeof(temp_buf), "%.0f", temp_o->valuedouble);
                    latest_data_.temp = temp_buf;
                    latest_data_.text = weather_code_to_text(code_o->valueint);
                    latest_data_.city = city_name;
                    latest_data_.valid = true;
                    success = true;
                    ESP_LOGI(TAG, "天气更新成功: %s %s %s°C",
                             city_name.c_str(), latest_data_.text.c_str(), latest_data_.temp.c_str());
                }
            }
            cJSON_Delete(root);
        }
    } else {
        ESP_LOGE(TAG, "天气请求失败 (err=%d, status=%d)", err, status_code);
    }
    esp_http_client_cleanup(client);
    return success;
}

void WeatherManager::parseWeatherJson(const char* json_data) {
    // Open-Meteo 不使用此方法，直接内联解析
}
