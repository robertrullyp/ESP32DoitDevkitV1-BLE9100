# ESP32DoitDevkitV1-BLE9100

> Eksperimen **ESP32 DOIT DevKit V1** sebagai jembatan (**bridge**) **Bluetooth Low Energy (BLE)** ⇄ **UART** ke modul **Sensor DO BLE9100**. Fokusnya: membaca data sensor dari perangkat BLE Sensor DO via UART dan meneruskan ke Serial Monitor.

---

## Ringkas
- **Tujuan**: Proof-of-concept / eksperimen.
- **Board**: ESP32 DOIT DevKit V1 (Arduino framework).
- **Peran**: BLE peripheral (server) di ESP32 + bridge ke UART (Serial2) ke Sensor DO BLE9100.
- **Kegunaan**: Membaca data dari Sensor DO lewat BLE (tanpa kabel serial ke PC).

---

## Fitur
- **BLE Peripheral (Server)** pada ESP32: menyiarkan (advertise) nama perangkat & karakteristik (characteristic) untuk **Write**/**Notify**.
- **Bridge UART⇄BLE**: data dari BLE → diteruskan ke **UART** (Sensor DO). Respons dari UART → dikirim kembali ke Serial Monitor.
- **Serial Debug** (USB) @ `115200` untuk log & troubleshooting.

> Catatan: Proyek ini eksperimen sederhana. Untuk produksi, tambahkan framing, flow control, retry, buffering, dan proteksi overrun.

---

## Hardware

### Komponen
- 1× **ESP32 DOIT DevKit V1**
- 1× **DO SENSOR BLE9100** (atau Sensor seri BLE yang setara)
- Kabel USB (ESP32 ↔ PC) dan beberapa jumper
- (Opsional) Catu daya terpisah untuk Sensor DO jika butuh arus lebih besar

### Koneksi Dasar (disarankan)
| ESP32 (DOIT) | SIMCom BLE9100 | Keterangan                         |
|--------------|-----------------|------------------------------------|
| **GPIO17**   | **RX**          | TX ESP32 → RX modul (UART2 TX)     |
| **GPIO16**   | **TX**          | RX ESP32 ← TX modul (UART2 RX)     |
| **GND**      | **GND**         | Ground bersama                     |
| **5V/3V3**   | **VCC**         | **Sesuai** kebutuhan modul         |

> Default praktik umum UART2 ESP32: **TX2=GPIO17, RX2=GPIO16**. Silakan sesuaikan dengan kode bila berbeda.

---

## Struktur Proyek
Struktur mengikuti **PlatformIO** standar:

.
├─ src/ # kode utama (main.cpp)
├─ include/ # header (opsional)
├─ lib/ # library privat (opsional)
├─ test/ # unit/integration test (opsional)
└─ platformio.ini # konfigurasi PlatformIO (board, lib, monitor baud, dll)

---

## Dependensi & Tooling

- **Platform**: `ESP32 (espressif32)` dengan **Arduino framework**.
- **BLE**: Library **ESP32 BLE Arduino** (umum dipakai untuk BLE server/characteristic).
- **Serial Monitor**: `115200` baud.

> Di **PlatformIO**, dependensi dapat didefinisikan pada `platformio.ini` (bagian `lib_deps`).  
> Di **Arduino IDE**, pastikan sudah memasang **ESP32 Board Package** (Boards Manager) dan (bila perlu) library **ESP32 BLE Arduino** lewat Library Manager.

---

## Konfigurasi (di kode)

Cari/atur konstanta berikut (nama bisa berbeda tergantung implementasi):
- `UART_BAUD` (default lazim: `115200`)
- `UART_RX_PIN`, `UART_TX_PIN` (contoh lazim: `16`, `17`)
- `DEVICE_NAME` (nama BLE)
- `SERVICE_UUID`, `CHAR_NOTIFY_UUID`, `CHAR_WRITE_UUID`

Contoh pola inisialisasi UART & BLE (cuplikan umum):
```cpp
// UART ke modul Sensor DO
HardwareSerial SerialAT(2);
SerialAT.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

// BLE peripheral + 2 characteristic (Write untuk kirim ke UART, Notify untuk balasan)
Sesuaikan UUID dan nama perangkat agar mudah dikenali di aplikasi BLE.
```

## Cara Bangun & Flash
Opsi A — PlatformIO (VS Code)
1. Buka folder proyek di VS Code.
2. Pastikan ekstensi PlatformIO terpasang.
3. Sambungkan ESP32 via USB (cek port).
4. Upload (ikon panah kanan) → tunggu selesai flash.
5. Buka PlatformIO Monitor (baud 115200) untuk melihat log.

Opsi B — Arduino IDE
1. Pasang ESP32 Board lewat Boards Manager.
2. Instal ESP32 BLE Arduino jika belum ada.
3. Buat sketch dan salin isi src/main.cpp ke .ino (atau buka via Arduino-CLI/Arduino IDE dengan struktur yang sesuai).
4. Pilih board DOIT ESP32 DevKit V1 dan port yang benar.
5. Upload; bila perlu tekan tombol BOOT saat “Connecting…”.
6. Buka Serial Monitor @ 115200.

## Uji Coba BLE

1. Jalankan Serial Monitor untuk melihat log inisialisasi BLE (advertising).
2. Buka aplikasi nRF Connect / LightBlue di ponsel.
3. Scan & connect ke perangkat dengan DEVICE_NAME yang diset.
4. Temukan service/characteristic:
  Write: kirim teks → diteruskan ke UART Sensor DO.
  Notify: aktifkan notifikasi → lihat balasan (OK) dari Sensor DO. (tidak digunakan di sensor ble9100)

5. Bila tidak respon:
  Cek wiring TX/RX (harus silang).
  Pastikan Sensor DO menyala dan baud rate match.
  Pantau log di Serial untuk error.


## Kontribusi

Ini proyek eksperimen pribadi. PR/Issue dipersilakan untuk diskusi perbaikan kecil, namun harap maklum belum ada panduan kontribusi formal.
