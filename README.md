
# 🐾 NekoBoot

NekoBoot is a custom **UEFI application** that displays a branded boot screen
(with logo + fake boot log) before safely chainloading **Windows Boot Manager**.

This project is **not a simulator** — it builds a real `.efi` binary using
EDK II and runs directly in UEFI firmware.

---

## ✨ Features

- Custom BMP logo rendered via GOP
- Fake OS-style boot log for aesthetics
- "Booting Neko OS..." animation
- Automatically chainloads Windows (`bootmgfw.efi`)
- Built entirely using **EDK II**
- Fully automated **GitHub Actions build**
- No firmware modification required

---

## 📂 Project Structure
```
NekoBoot/
├── NekoBoot.c          # Main UEFI application
├── NekoBoot.inf        # EDK II module definition
├── nekologo.bmp        # Boot logo (24-bit BMP)
├── README.md
└── .github/
└── workflows/
└── build.yml   # GitHub Actions EFI build
```
---

## 🛠 Building

This project is built automatically using **GitHub Actions**.

To build:
1. Push to the repository
2. Go to **Actions**
3. Download the `NekoBoot.efi` artifact

No local build environment is required.

---

## 🚀 Usage

1. Copy `NekoBoot.efi` to a USB drive or EFI System Partition and rename to `BOOTX64.efi`:

EFI/BOOT/BOOTX64.efi

2. Boot from it using your system’s UEFI boot menu

3. You will see:
- Logo
- Fake boot log
- Boot animation
- Windows starts normally

---

## ⚠️ Safety Notes

- Does **not** overwrite Windows bootloader
- Does **not** modify firmware
- Safe unless manually installed as default boot entry
- BMP must be **24-bit, uncompressed**

---

## 📜 License

MIT License — use, modify, learn, experiment.

---

## 🐱 Why?

Because boot screens should have personality.
