# 嵌入式系統實驗作業 1 (Embedded System Lab 1)

本專案包含在 Raspberry Pi 5 上開發的 GPIO 控制實驗，主要使用 `libgpiod` (v2 API) 進行 C 語言開發，並提供 Python 版本作為對照。

## 專案內容

### Task 1: 中斷觸發 LED 閃爍系統
- **功能**：LED 以「亮 1 秒、暗 3 秒」的循環閃爍。當按下按鈕（Button）時，觸發中斷訊號立即停止程式並安全關閉 LED。
- **實作檔案**：
  - `task1.c`: 使用 C 語言與多執行緒 (pthread) 監聽按鈕狀態。
  - `task1.py`: 使用 Python `gpiozero` 程式庫實作。
  - `task1`: 已編譯的執行檔。

### Task 2: PIR 動態偵測警報系統
- **功能**：使用 PIR 感測器偵測物體移動。若偵測到移動，兩顆 LED 會交替閃爍作為警報，並在偵測停止後維持 5 秒的警報狀態。支援 `Ctrl+C` 安全退出系統。
- **實作檔案**：
  - `task2.c`: 使用 C 語言實作動態偵測與計時邏輯。
  - `task2`: 已編譯的執行檔。


## 如何執行

### 1. 安裝依賴項目
確保系統已安裝 `libgpiod` 開發庫：
```bash
sudo apt update
sudo apt install libgpiod-dev

# 編譯 Task 1
gcc task1.c -o task1 -lgpiod -lpthread

# 編譯 Task 2
gcc task2.c -o task2 -lgpiod

# 執行 Task 1
./task1

# 執行 Task 2
./task2
