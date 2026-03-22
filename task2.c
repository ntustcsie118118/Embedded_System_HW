#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <gpiod.h>
#include <time.h>

#define CHIP_PATH "/dev/gpiochip4"
#define LED1_PIN 17
#define LED2_PIN 27
#define PIR_PIN 22
#define ALARM_HOLD_SEC 5

volatile bool is_running = true;

// 系統訊號處理常式：攔截 Ctrl+C (SIGINT) 以便安全退出
void sigint_handler(int sig) {
    printf("\n[系統提示] 接收到中斷訊號 (Ctrl+C)，準備關閉系統...\n");
    is_running = false;
}

// 高響應延遲函數：拆分等待時間，確保在閃爍途中若 PIR 訊號消失或系統中斷，能立刻跳出
bool responsive_sleep(int seconds, struct gpiod_line_request *pir_req, unsigned int pir_offset) {
    for (int i = 0; i < seconds * 10; i++) {
        if (!is_running) return false;
        
        // 可選：若希望物件離開時立刻停止閃爍，可在此檢查 PIR 狀態
        // int pir_val = gpiod_line_request_get_value(pir_req, pir_offset);
        // if (pir_val == GPIOD_LINE_VALUE_INACTIVE) return false;
        
        usleep(100000); // 100ms
    }
    return true;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line_settings *out_settings, *in_settings;
    struct gpiod_line_config *leds_cfg, *pir_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *leds_request, *pir_request;
    
    unsigned int led_offsets[2] = {LED1_PIN, LED2_PIN};
    unsigned int pir_offset = PIR_PIN;

    // 註冊 SIGINT 處理器
    signal(SIGINT, sigint_handler);

    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        perror("無法開啟 GPIO 晶片");
        return -1;
    }

    // --- 配置兩顆 LED 為輸出 ---
    out_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(out_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    leds_cfg = gpiod_line_config_new();
    // 一次配置多個相同設定的腳位
    gpiod_line_config_add_line_settings(leds_cfg, led_offsets, 2, out_settings);

    // --- 配置 PIR 感測器為輸入 ---
    in_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(in_settings, GPIOD_LINE_DIRECTION_INPUT);
    pir_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(pir_cfg, &pir_offset, 1, in_settings);

    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "task2_pir_alarm");

    // 向核心請求腳位
    leds_request = gpiod_chip_request_lines(chip, req_cfg, leds_cfg);
    pir_request = gpiod_chip_request_lines(chip, req_cfg, pir_cfg);

    gpiod_line_settings_free(out_settings);
    gpiod_line_settings_free(in_settings);
    gpiod_line_config_free(leds_cfg);
    gpiod_line_config_free(pir_cfg);
    gpiod_request_config_free(req_cfg);

    if (!leds_request || !pir_request) {
        perror("無法請求 GPIO 線路");
        gpiod_chip_close(chip);
        return -1;
    }

    printf("系統已啟動：PIR 動態偵測中...\n");
    printf("按 Ctrl+C 可隨時結束程式。\n");

    time_t last_detection_time = 0; // 記錄最後一次偵測到移動的時間
    // 主迴圈
    while (is_running) {
        // int motion_detected = gpiod_line_request_get_value(pir_request, pir_offset);

        // if (motion_detected == GPIOD_LINE_VALUE_ACTIVE) {
        //     printf("[警報] 偵測到物體移動！\n");
            
        //     // LED 1 亮，LED 2 暗
        //     gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_ACTIVE);
        //     gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_INACTIVE);
        //     if (!responsive_sleep(1, pir_request, pir_offset)) continue;

        //     // 確保系統仍在運行才執行下半段
        //     if (!is_running) break;

        //     // LED 1 暗，LED 2 亮
        //     gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_INACTIVE);
        //     gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_ACTIVE);
        //     if (!responsive_sleep(1, pir_request, pir_offset)) continue;
        // } else {
        //     // 無物體移動時，確保兩顆 LED 皆關閉
        //     gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_INACTIVE);
        //     gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_INACTIVE);
        //     usleep(100000); // 休眠 100ms 再偵測下一次
        // }

        // int val = gpiod_line_request_get_value(pir_request, pir_offset);
        // printf("PIR Status: %d\n", val);
        // usleep(500000); // 每 0.5 秒印一次

        int motion = gpiod_line_request_get_value(pir_request, pir_offset);
        time_t current_time = time(NULL);

        if (motion == GPIOD_LINE_VALUE_ACTIVE) {
            last_detection_time = current_time; // 更新最後偵測時間
        }

        // 判斷是否在「警報維持時間」內
        if (difftime(current_time, last_detection_time) < ALARM_HOLD_SEC) {
            printf("[警報中] 正在閃爍 LED... (距上次偵測 %.0f 秒)\n", 
                    difftime(current_time, last_detection_time));
            
            // LED 1 亮 1 秒
            gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_ACTIVE);
            gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_INACTIVE);
            if (!responsive_sleep(1, pir_request, pir_offset)) break;

            // LED 2 亮 1 秒
            gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_INACTIVE);
            gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_ACTIVE);
            if (!responsive_sleep(1, pir_request, pir_offset)) break;
        } else {
            // 超過 5 秒沒偵測到移動，關閉 LED
            gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_INACTIVE);
            gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_INACTIVE);
            usleep(200000); // 輕微休眠減少 CPU 負載
        }
    }

    // 資源清理
    gpiod_line_request_set_value(leds_request, LED1_PIN, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(leds_request, LED2_PIN, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_release(leds_request);
    gpiod_line_request_release(pir_request);
    gpiod_chip_close(chip);

    printf("系統已安全關閉，GPIO 資源已釋放。\n");
    return 0;
}