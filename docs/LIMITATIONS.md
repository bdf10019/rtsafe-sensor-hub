# 限制與待辦

## 已知限制

- `board_io_sim.c` 是無硬體依賴的展示 BSP，尚未連接真實 IMU、encoder、force、PWM 或 watchdog。
- Windows 非 ASCII 路徑需 ASCII staging；建置腳本會複製大量上游來源與 SDK，第一次執行需要額外空間。
- 本機 Zephyr SDK 的 QEMU 缺少執行時 DLL；韌體與 ztest 已交叉編譯，但目標端測試未執行。
- Yocto layer 只完成 metadata 靜態驗證，尚未在 Linux builder 建立 image。
- 52-byte v1 stream 沒有 resynchronization、認證、加密、重放保護或 schema negotiation。
- 安全門檻為啟動時設定；目前沒有簽章設定檔、持久化校正或遠端更新。
- Simulator 的 execution time 是模型輸入，不等於主機排程 latency benchmark。
- 專案未依任何功能安全標準認證，不可直接作為人身安全保護層。

## 後續優先順序

1. 導入一塊公開可取得的 Cortex-M 開發板，完成 sensor/PWM/watchdog BSP。
2. 在 Linux CI 或容器執行 QEMU ztest 與 Twister，保存 JUnit 結果。
3. 在 Scarthgap builder 完成 kas image、QEMU 開機與 systemd path integration test。
4. 加入 HIL fault injection、長時間 timing trace 與 deadline percentile 報告。
5. 設計帶 framing、認證與 key rotation 的 transport v2。
6. 製作 checksummed release archive，讓 Yocto recipe 移除 `externalsrc`。
