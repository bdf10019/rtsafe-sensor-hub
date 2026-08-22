# Yocto 整合

`meta-rtsafe` 將 C++ telemetry supervisor 與 systemd path unit 裝入映像。當
`/var/lib/rtsafe/telemetry.bin` 變更時，服務驗證 capture 並寫出
`/var/lib/rtsafe/report.json`。

layer 以 Yocto Project 5.0 Scarthgap LTS 為目標；`kas.yml` 建置
`qemux86-64` 的 `core-image-minimal` 並加入 supervisor package。

```bash
cd yocto
kas build kas.yml
```

目前 recipe 使用 `externalsrc`，讓 monorepo 在尚無 release archive 時仍可建置。
正式發行應改用固定版本且含 checksum 的 source archive。

Yocto 完整建置需要支援的 Linux host、container 或 WSL2，並需預留大量磁碟空間。
downloads、shared state 與 image 均已排除 Git。本版僅在 Windows 完成 metadata
靜態驗證，尚未宣稱 image 建置或 QEMU 開機通過。
