/*
 * deepseek_ai.c
 * DeepSeek LLM API client (OpenAI-compatible chat completions).
 */

#include "deepseek_ai.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "lvgl_display.h"
#include "wifi_manager.h"

#define TAG "deepseek_ai"

/* 问题最大长度 */
#define AI_QUESTION_MAX 512

/* 回答提取结果最大长度 */
#define AI_ANSWER_MAX   (DEEPSEEK_RESP_MAX / 2)

/* 任务栈：8KB 栈已被两次 CORRUPT HEAP 证明不够（栈溢出）。
 * 16KB 内部栈是唯一被证明不溢出的组合（第6轮）。 */
#define AI_TASK_STACK   16384
#define AI_TASK_PRIO    5

static TaskHandle_t s_ai_task = NULL;
static SemaphoreHandle_t s_ai_trigger_sem = NULL;   /* 触发一次请求 */
static SemaphoreHandle_t s_ai_lock = NULL;          /* 保护问题缓冲 */
static bool s_ai_busy = false;

/* 待发送的问题（跨线程缓存） */
static char s_ai_question[AI_QUESTION_MAX];

/* 最新回答（静态缓冲，LVGL 任务直接读取，不跨任务传堆指针） */
static char s_ai_answer[AI_ANSWER_MAX];

/* 任务栈缓冲：放 PSRAM，避免挤占内部 RAM（TLS 握手主要吃堆不吃栈） */
static StackType_t *s_ai_task_stack = NULL;
static StaticTask_t s_ai_task_tcb;

/* ------------------------------------------------------------------ */

static void ai_post_status(const char *text, uint32_t color_hex)
{
    (void)lvgl_msg_send_nonblocking(LVGL_MSG_AI_STATUS, (int32_t)color_hex, text);
}

static void ai_post_answer(const char *answer)
{
    /* 回答存入静态缓冲，然后只发一个“就绪”信号（无堆指针传递） */
    if (answer != NULL) {
        strncpy(s_ai_answer, answer, sizeof(s_ai_answer) - 1);
        s_ai_answer[sizeof(s_ai_answer) - 1] = '\0';
    }
    (void)lvgl_msg_send_nonblocking(LVGL_MSG_AI_ANSWER, 0, NULL);
}

const char *deepseek_ai_get_answer(void)
{
    return s_ai_answer;
}

/** 构造 POST body（用 cJSON 自动处理字符串转义） */
static char *build_chat_body(const char *question)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddStringToObject(root, "model", DEEPSEEK_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", DEEPSEEK_MAX_TOKENS);
    cJSON_AddBoolToObject(root, "stream", 0);

    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    if (messages == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", question);
    cJSON_AddItemToArray(messages, msg);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}

/** 从 DeepSeek 响应 JSON 中提取 choices[0].message.content */
static bool parse_answer(const char *json, char *out, size_t out_cap)
{
    if (json == NULL || out == NULL || out_cap == 0) return false;
    out[0] = '\0';

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) return false;

    bool ok = false;
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) < 1) goto exit;

    cJSON *choice0 = cJSON_GetArrayItem(choices, 0);
    if (!cJSON_IsObject(choice0)) goto exit;

    cJSON *message = cJSON_GetObjectItem(choice0, "message");
    if (!cJSON_IsObject(message)) goto exit;

    cJSON *content = cJSON_GetObjectItem(message, "content");
    if (!cJSON_IsString(content) || content->valuestring == NULL) goto exit;

    strncpy(out, content->valuestring, out_cap - 1);
    out[out_cap - 1] = '\0';
    ok = (out[0] != '\0');

exit:
    cJSON_Delete(root);
    return ok;
}

/* ------------------------------------------------------------------ */

/* 采用与 get_weather.c 相同的同步读取模式（工程内已验证稳定）：
 *   esp_http_client_open → fetch_headers → read_response
 * 不再使用 perform + event_handler（该路径内部缓冲管理疑似导致 CORRUPT HEAP） */
static esp_err_t ai_http_post(const char *body, char *out_buf, size_t out_cap)
{
    if (body == NULL || out_buf == NULL || out_cap == 0) return ESP_ERR_INVALID_ARG;
    out_buf[0] = '\0';

    char url[160];
    snprintf(url, sizeof(url), "%s%s", DEEPSEEK_BASE_URL, DEEPSEEK_CHAT_PATH);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = DEEPSEEK_TIMEOUT_MS,
        .buffer_size = 1024,       /* 与 get_weather 一致，仅头部解析用 */
        .buffer_size_tx = 1024,
        /*
         * HTTPS 证书验证策略（重要）：
         * 不校验服务器证书（skip_cert_common_name_check + 不挂证书捆绑包）。
         * 原因：ESP32-S3 内部 RAM 紧张，证书捆绑包 RSA 验签在 INTERNAL_MEM_ALLOC
         *       下会因内存不足失败（PK verify failed 0x4290），在 DEFAULT/EXTERNAL
         *       下会触发 esp_http_client 的 PSRAM 缓冲释放 bug（CORRUPT HEAP）。
         * TLS 仍全程加密（防窃听），API Key 作为鉴权；这是嵌入式设备常见妥协，
         * 与 OneNET MQTT 直接明文同属权衡。
         */
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) return ESP_FAIL;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Authorization", "Bearer " DEEPSEEK_API_KEY);

    size_t body_len = strlen(body);

    /* 建立连接（write_len 必须传 body 长度，否则服务器收不到请求体） */
    esp_err_t err = esp_http_client_open(client, (int)body_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    /* 发送 POST body */
    int written = esp_http_client_write(client, body, (int)body_len);
    if (written != (int)body_len) {
        ESP_LOGE(TAG, "write body failed: %d/%d", written, (int)body_len);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    /* 读取响应头，得到 content-length */
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "fetch_headers failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    if ((size_t)content_length >= out_cap) {
        ESP_LOGW(TAG, "response too large: %d (cap %d)", content_length, (int)out_cap);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    /* 阻塞读取完整响应体（read_response 内部处理 content-length 循环；
     * 关闭动态缓冲后 16KB 固定 TLS 缓冲可容纳完整响应，不再 -0x7100） */
    int read_len = esp_http_client_read_response(client, out_buf, (int)out_cap - 1);
    if (read_len < 0) {
        ESP_LOGE(TAG, "read_response failed: %d", read_len);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    out_buf[read_len] = '\0';

    int status = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGW(TAG, "HTTP status=%d, body=%.200s", status, out_buf);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[DBG] response len=%d status=%d", read_len, status);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

static void ai_task(void *param)
{
    (void)param;

    /* 大缓冲：标准 malloc（ALWAYSINTERNAL=0 时大块自动进 PSRAM），
     * 与 cJSON/http_client 分配器同源，避免 heap_caps 与 free 混用边界问题 */
    char *response = malloc(DEEPSEEK_RESP_MAX);
    char *answer = malloc(AI_ANSWER_MAX);
    if (response == NULL || answer == NULL) {
        ESP_LOGE(TAG, "alloc buffers failed");
        free(response);
        free(answer);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "[DBG] buffers: response=%p answer=%p", response, answer);

    while (1) {
        if (s_ai_trigger_sem == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xSemaphoreTake(s_ai_trigger_sem, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 取走问题缓存（static：不占任务栈，512B 在 BSS） */
        static char question[AI_QUESTION_MAX];
        memset(question, 0, sizeof(question));
        if (s_ai_lock != NULL) {
            xSemaphoreTake(s_ai_lock, portMAX_DELAY);
            strncpy(question, s_ai_question, sizeof(question) - 1);
            xSemaphoreGive(s_ai_lock);
        }

        if (question[0] == '\0') {
            s_ai_busy = false;   /* 兜底：避免 busy 状态卡死 */
            continue;
        }

        ai_post_status("Thinking...", 0xF0B429);

        /* 检查 WiFi 是否可用 */
        if (!wifi_manager_is_enabled()) {
            s_ai_busy = false;
            ai_post_status("No WiFi", 0x9A9A9A);
            continue;
        }
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            s_ai_busy = false;
            ai_post_status("No WiFi", 0x9A9A9A);
            continue;
        }

        /* 构造请求体 */
        char *body = build_chat_body(question);
        if (body == NULL) {
            s_ai_busy = false;
            ai_post_status("Build body fail", 0xFF3333);
            continue;
        }
        ESP_LOGI(TAG, "ask: %.100s", question);
        ESP_LOGI(TAG, "[DBG] before perform: free_int=%d", (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        /* 自动重试：网络抖动/连接中断时最多重试 3 次，间隔 2s */
        esp_err_t err = ESP_FAIL;
        int retry = 0;
        for (; retry < 3; retry++) {
            memset(response, 0, DEEPSEEK_RESP_MAX);
            err = ai_http_post(body, response, DEEPSEEK_RESP_MAX);
            if (err == ESP_OK) break;
            ESP_LOGW(TAG, "request failed (%s), retry %d/3...", esp_err_to_name(err), retry + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        cJSON_free(body);
        ESP_LOGI(TAG, "[DBG] after cJSON_free(body)");
        ESP_LOGI(TAG, "[DBG] after perform: err=%s (retry=%d)", esp_err_to_name(err), retry);

        if (err != ESP_OK) {
            s_ai_busy = false;
            ai_post_status("HTTP fail", 0xFF3333);
            continue;
        }

        /* 解析回答 */
        memset(answer, 0, AI_ANSWER_MAX);
        ESP_LOGI(TAG, "[DBG] parsing response...");
        if (!parse_answer(response, answer, AI_ANSWER_MAX)) {
            ESP_LOGI(TAG, "[DBG] parse failed");
            s_ai_busy = false;
            ai_post_status("Parse fail", 0xFF3333);
            continue;
        }
        ESP_LOGI(TAG, "[DBG] parse ok, answer len=%d", (int)strlen(answer));

        s_ai_busy = false;
        ai_post_status("Done", 0x33CC66);
        ESP_LOGI(TAG, "[DBG] posting answer to LVGL...");
        ai_post_answer(answer);
        ESP_LOGI(TAG, "[DBG] answer posted");
    }
}


void deepseek_ai_init(void)
{
    if (s_ai_task != NULL) return;

    s_ai_lock = xSemaphoreCreateMutex();
    s_ai_trigger_sem = xSemaphoreCreateBinary();
    if (s_ai_lock == NULL || s_ai_trigger_sem == NULL) {
        ESP_LOGE(TAG, "semaphore create failed");
        return;
    }

    if (s_ai_task_stack == NULL) {
        s_ai_task_stack = heap_caps_malloc(AI_TASK_STACK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s_ai_task_stack == NULL) {
        ESP_LOGE(TAG, "alloc internal stack failed");
        return;
    }

    s_ai_task = xTaskCreateStaticPinnedToCore(ai_task, "deepseek_ai",
                                              AI_TASK_STACK / sizeof(StackType_t),
                                              NULL, AI_TASK_PRIO, s_ai_task_stack,
                                              &s_ai_task_tcb, 1);
    if (s_ai_task == NULL) {
        ESP_LOGE(TAG, "task create failed");
        return;
    }
    ESP_LOGI(TAG, "DeepSeek AI client initialized (16KB internal stack)");
}

esp_err_t deepseek_ai_ask(const char *question)
{
    if (question == NULL || question[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (s_ai_task == NULL || s_ai_lock == NULL || s_ai_trigger_sem == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (deepseek_ai_busy()) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_ai_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    strncpy(s_ai_question, question, sizeof(s_ai_question) - 1);
    s_ai_question[sizeof(s_ai_question) - 1] = '\0';
    s_ai_busy = true;
    xSemaphoreGive(s_ai_lock);

    if (xSemaphoreGive(s_ai_trigger_sem) != pdTRUE) {
        s_ai_busy = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool deepseek_ai_busy(void)
{
    bool busy = false;
    if (s_ai_lock != NULL) {
        if (xSemaphoreTake(s_ai_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            busy = s_ai_busy;
            xSemaphoreGive(s_ai_lock);
        }
    }
    return busy;
}
