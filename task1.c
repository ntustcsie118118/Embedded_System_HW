#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <gpiod.h>

// Raspberry Pi 5 的 40-pin GPIO 通常掛載於 gpiochip4
#define CHIP_PATH "/dev/gpiochip4"
#define LED_PIN 17
#define BUTTON_PIN 27

volatile bool is_running = true;

// 在 v2 中，控制權由 gpiod_line_request 物件管理
struct gpiod_line_request *led_request = NULL;
struct gpiod_line_request *button_request = NULL;

// 中斷處理執行緒：負責監控按鈕狀態
void *button_interrupt_thread(void *arg) {
    while (is_running) {
        // 使用 v2 API 讀取按鈕狀態
        int val = gpiod_line_request_get_value(button_request, BUTTON_PIN);
        
        // 按下按鈕時接通 GND，電位會被拉低至 INACTIVE (0)
        if (val == GPIOD_LINE_VALUE_INACTIVE) {
            printf("\n[中斷觸發] 偵測到按鈕按下，正在停止 LED 閃爍系統...\n");
            is_running = false;
            break;
        }
        usleep(50000); // 50ms 檢查一次，避免 CPU 100% 佔用
    }
    return NULL;
}

// 高響應延遲函數，單位為秒
bool responsive_sleep(int seconds) {
    for (int i = 0; i < seconds * 10; i++) {
        if (!is_running) return false;
        usleep(100000); 
    }
    return true;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line_settings *led_settings, *btn_settings;
    struct gpiod_line_config *led_cfg, *btn_cfg;
    struct gpiod_request_config *req_cfg;
    pthread_t thread_id;
    
    unsigned int led_offset = LED_PIN;
    unsigned int btn_offset = BUTTON_PIN;

    // 1. 開啟 GPIO 晶片
    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        perror("無法開啟 GPIO 晶片 (請確認路徑是否為 /dev/gpiochip4)");
        return -1;
    }

    // 2. 配置 LED 腳位 (輸出)
    led_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(led_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    led_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(led_cfg, &led_offset, 1, led_settings);

    // 3. 配置 Button 腳位 (輸入，並啟用內部上拉電阻)
    btn_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(btn_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(btn_settings, GPIOD_LINE_BIAS_PULL_UP); // v2 輕鬆啟用 Pull-up
    btn_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(btn_cfg, &btn_offset, 1, btn_settings);

    // 4. 建立請求配置
    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "task1_app");

    // 5. 向作業系統核心請求腳位控制權
    led_request = gpiod_chip_request_lines(chip, req_cfg, led_cfg);
    button_request = gpiod_chip_request_lines(chip, req_cfg, btn_cfg);

    // 釋放不再需要的設定檔記憶體
    gpiod_line_settings_free(led_settings);
    gpiod_line_settings_free(btn_settings);
    gpiod_line_config_free(led_cfg);
    gpiod_line_config_free(btn_cfg);
    gpiod_request_config_free(req_cfg);

    if (!led_request || !button_request) {
        perror("無法請求 GPIO 線路控制權");
        gpiod_chip_close(chip);
        return -1;
    }

    printf("LED 系統已啟動 (libgpiod v2 版本)。\n");
    printf("閃爍模式：亮 1 秒，暗 3 秒。\n");
    printf("請按下按鈕以觸發中斷並結束程式。\n");

    // 6. 啟動按鈕監聽執行緒
    pthread_create(&thread_id, NULL, button_interrupt_thread, NULL);

    // 7. 主迴圈：控制 LED 閃爍
    while (is_running) {
        // 點亮 LED (ACTIVE)
        gpiod_line_request_set_value(led_request, led_offset, GPIOD_LINE_VALUE_ACTIVE);
        if (!responsive_sleep(1)) break;

        // 關閉 LED (INACTIVE)
        gpiod_line_request_set_value(led_request, led_offset, GPIOD_LINE_VALUE_INACTIVE);
        if (!responsive_sleep(3)) break;
    }

    // 8. 資源釋放與清理
    gpiod_line_request_set_value(led_request, led_offset, GPIOD_LINE_VALUE_INACTIVE); // 確保熄滅
    pthread_join(thread_id, NULL); // 等待中斷執行緒結束
    gpiod_line_request_release(led_request);
    gpiod_line_request_release(button_request);
    gpiod_chip_close(chip);

    printf("系統已安全關閉。\n");
    return 0;
}