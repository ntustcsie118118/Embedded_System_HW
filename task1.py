from gpiozero import LED, Button
import time
import sys

# 初始化 GPIO 腳位
led = LED(17)
# 啟用內部上拉電阻，按鈕按下時會接通 GND (訊號變為 False)
button = Button(27, pull_up=True)

# 狀態變數，用於控制主迴圈
is_running = True

def interrupt_handler():
    """
    中斷處理常式 (ISR): 當按鈕被按下時觸發
    """
    global is_running
    print("\n[中斷觸發] 偵測到按鈕按下，正在停止 LED 閃爍系統...")
    is_running = False

def responsive_sleep(duration):
    """
    高響應的延遲函數，允許在中斷發生時立即跳出等待
    """
    start_time = time.time()
    while time.time() - start_time < duration:
        if not is_running:
            return False
        time.sleep(0.05) # 短暫休眠以減少 CPU 負載
    return True

# 綁定按鈕按下事件與中斷處理常式 (硬體中斷的軟體抽象層)
button.when_pressed = interrupt_handler

print("LED 系統已啟動。")
print("閃爍模式：亮 1 秒，暗 3 秒。")
print("請按下按鈕以觸發中斷並結束程式。")

try:
    while is_running:
        # Step 1: Turn on the LED for 1 second
        led.on()
        if not responsive_sleep(1):
            break
            
        # Step 1: Turn it off for 3 seconds
        led.off()
        if not responsive_sleep(3):
            break

except KeyboardInterrupt:
    print("\n接收到鍵盤中斷 (Ctrl+C)。")
finally:
    # 確保程式結束時關閉 LED 並釋放資源
    led.off()
    button.close()
    led.close()
    print("系統已安全關閉。")
    sys.exit(0)