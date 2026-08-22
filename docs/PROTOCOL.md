# 遙測協定 v1

每個 frame 固定 52 bytes、little-endian。CRC 使用 CRC16-CCITT，初始值 `0xFFFF`、多項式 `0x1021`，涵蓋 byte 0–49。

| Offset | 長度 | 欄位 | 說明 |
|---:|---:|---|---|
| 0 | 2 | magic | ASCII `RT` |
| 2 | 1 | version | `1` |
| 3 | 1 | type | `1` = telemetry |
| 4 | 2 | payload length | `32` |
| 6 | 4 | sequence | 每筆遞增的 uint32 |
| 10 | 8 | timestamp_us | 裝置單調時間 |
| 18 | 1 | state | 0–4 對應安全狀態 |
| 19 | 1 | motor_enabled | 僅接受 0 或 1 |
| 20 | 2 | reserved | v1 固定為 0 |
| 22 | 4 | fault_flags | 故障 bitmask |
| 26 | 4 | acceleration | float32, m/s² |
| 30 | 4 | yaw rate | float32, rad/s |
| 34 | 4 | velocity | float32, m/s |
| 38 | 4 | force | float32, N |
| 42 | 4 | pwm_limit | float32, 0–1 |
| 46 | 4 | deadline_misses | 累計 uint32 |
| 50 | 2 | CRC | CRC16-CCITT |

## Fault bitmask

| Bit | 名稱 | 意義 |
|---:|---|---|
| 0 | IMU_INVALID | IMU 無效或非有限值 |
| 1 | ENCODER_INVALID | 編碼器無效 |
| 2 | FORCE_INVALID | 力量資料無效或非有限值 |
| 3 | HEARTBEAT_LOST | 心跳逾時或來自未來 |
| 4 | FORCE_LIMIT | 原始力量絕對值達停止門檻 |
| 5 | ENCODER_JUMP | 單週期 tick 差超限 |
| 6 | DEADLINE_MISS | 執行時間超過預算 |
| 7 | TIME_REGRESSION | 時間戳未遞增 |
| 8 | IMU_RANGE | 加速度量值或 yaw rate 超限 |

## 相容性規則

- 接收端必須拒絕未知 magic、version、type、payload length、state、布林值或 CRC。
- v1 接收端以固定 52-byte 邊界讀取，不做 stream resynchronization。
- 序號跳號不會讓 frame 本身解碼失敗，但 supervisor 會將 stream 標為無效。
- 協定只偵測意外損毀，不提供來源驗證、機密性或重放保護。
