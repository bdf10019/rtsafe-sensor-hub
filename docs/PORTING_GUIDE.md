# 實機移植指南

## 1. 選擇 Zephyr board

以實際板名取代 `qemu_cortex_m3`，先確認 UART、clock、sensor bus 與 GPIO/PWM 的 devicetree 定義。若加入自訂 board，將 board metadata 與 overlay 放在獨立 BSP 目錄。

## 2. 實作 BSP

依 `firmware/zephyr/include/board_io.h` 實作三個函式：

- `rtsafe_board_read_sensors`：一次取得同步時間戳、IMU、encoder、force 與心跳狀態；無效資料應清楚設定 validity flag。
- `rtsafe_board_apply_output`：在 `motor_enabled=false` 時以硬體安全順序關閉 PWM/enable pin。
- `rtsafe_board_publish_frame`：將完整 52-byte frame 送入 UART、CAN-FD、USB 或共享記憶體，不可在控制執行緒內阻塞。

不要直接修改可移植核心來加入 register access；板級差異應留在 BSP。

## 3. 校正設定

依感測器 datasheet、機構極限與量測結果調整 `rtsafe_config_t`。至少驗證：單位轉換、meters per tick、力量方向、IMU 軸向、heartbeat timeout、deadline budget 與停止門檻。

## 4. 實機驗證

建議順序：

1. 僅記錄輸入，不驅動馬達，核對時間戳與物理單位。
2. 注入斷線、NaN、stale heartbeat、encoder jump 與力量超限。
3. 以邏輯分析儀量測 99.9/99.99 percentile control latency。
4. 驗證 queue overflow、看門狗 reset、brownout 與啟動/關機安全狀態。
5. 透過 HIL 重播長時間 trace，核對 MCU 與主機模型的狀態序列。

## 5. Linux 連線

讓 transport daemon 將 frame 原樣寫入 supervisor 可讀的檔案或 pipe。產品化時應加入 framing/resynchronization、存取權限、來源驗證、log rotation 與資源上限。
