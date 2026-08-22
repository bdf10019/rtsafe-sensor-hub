# 驗證紀錄

驗證日期：2026-08-23。所有測試都在專案本機執行，未上傳產物或呼叫外部服務。

## 主機端

執行：

```powershell
.\scripts\Build-Test.ps1 -Configuration Release
```

結果：Release 建置成功，CTest 4/4 通過。

| 測試 | 覆蓋內容 | 結果 |
|---|---|---|
| unit | CRC known vector、編解碼、濾波、初始化、故障與狀態轉移 | 通過 |
| integration | 6 類端對端情境、safe-stop 反應、deadline 累計、CRC 破壞 | 通過 |
| public_safety | 識別字、憑證、私人路徑、binary 與大型檔案 | 通過 |
| metadata | Zephyr pin、ztest、Yocto layer/recipe/systemd 一致性 | 通過 |

端對端測試使用 2,000 cycles、seed `424242`。`force-spike` 在首次 fault 的同一週期進入 `SAFE_STOP`；`crc-corruption` 注入一筆破壞並由 supervisor 正確拒絕。

## Cortex-M3 交叉編譯

目標：Zephyr 4.4.0、Zephyr SDK 1.0.1、`qemu_cortex_m3/ti_lm3s6965`。

| 映像 | FLASH | RAM | 結果 |
|---|---:|---:|---|
| 多執行緒韌體 | 23,856 B / 256 KiB | 12,712 B / 64 KiB | 編譯與連結通過 |
| ztest 核心測試 | 26,488 B / 256 KiB | 6,504 B / 64 KiB | 編譯與連結通過 |

目前 Windows SDK 的 QEMU 執行檔啟動時缺少 DLL，回傳 `0xc0000135`，所以本次未執行 Cortex-M ztest。主機端相同核心測試已實際執行；目標端 runtime 測試仍列為缺口。

## Yocto

kas、layer、recipe 與 systemd units 已通過專案靜態檢查。此 Windows 主機沒有支援的 Linux/WSL2 建置環境，因此未下載 Poky、未建置完整 image，也未啟動 qemux86-64。此項不標示為已完成。
