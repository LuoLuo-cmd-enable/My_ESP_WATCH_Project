/*
 * deepseek_ai.h
 * DeepSeek LLM API client (OpenAI-compatible chat completions).
 *
 * Design mirrors components/bsp/get_weather.c:
 *  - dedicated FreeRTOS task + semaphore trigger (non-blocking for UI)
 *  - HTTP(S) POST via esp_http_client
 *  - cJSON parse of the answer
 *  - result/status posted back to LVGL via lvgl_msg_send_nonblocking
 */

#ifndef _DEEPSEEK_AI_H_
#define _DEEPSEEK_AI_H_

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ★★★ 在这里填入你自己的 DeepSeek API Key ★★★
 * 获取方式：https://platform.deepseek.com/  -> API Keys -> 创建
 * 注意：请勿把真实 Key 提交到公开仓库！ */
#define DEEPSEEK_API_KEY        "xxxxxxxxxxxxxxxxxxxxxxxxxx"

/* API 端点（DeepSeek 兼容 OpenAI 格式） */
#define DEEPSEEK_BASE_URL       "https://api.deepseek.com"
#define DEEPSEEK_CHAT_PATH      "/chat/completions"

/* 使用的模型：deepseek-chat（V3）或 deepseek-reasoner（R1 推理） */
#define DEEPSEEK_MODEL          "deepseek-chat"

/* 生成的最大 token 数（1 token ≈ 0.75 个英文单词，中文约 1~2 字） */
#define DEEPSEEK_MAX_TOKENS     1024

/* 单次 HTTP 请求/响应的超时（AI 生成可能较慢，需给足时间） */
#define DEEPSEEK_TIMEOUT_MS     60000

/* 响应缓冲区大小（存放完整 JSON 响应）：
 * max_tokens=1024 时回答约 2~4KB，含 usage 字段 JSON 总 <8KB。
 * 需 ≤ mbedTLS IN_CONTENT_LEN(16384)，否则 TLS 记录层会终止连接。 */
#define DEEPSEEK_RESP_MAX       8192

/** 初始化 DeepSeek 客户端（创建任务/信号量/互斥锁）
 * @return 无
 */
void deepseek_ai_init(void);

/** 发起一次对话请求（非阻塞：仅入队，立即返回）
 * @param question 问题文本（会被内部复制）
 * @return 入队成功返回 ESP_OK
 */
esp_err_t deepseek_ai_ask(const char *question);

/** 查询是否正在等待/获取回答
 * @return true = 请求进行中
 */
bool deepseek_ai_busy(void);

/** 获取最新回答（静态缓冲，无需释放）
 * @return 回答字符串指针；无回答时返回 NULL
 */
const char *deepseek_ai_get_answer(void);

#ifdef __cplusplus
}
#endif

#endif
