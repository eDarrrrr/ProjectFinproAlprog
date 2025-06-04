# Project Finpro
## Kelompok diluar logic
Anggota:
- Muhammad Haidar Al Ghifari - 2306266792
- Nofrizza Hilal Raffa Kharisma - 2306203381
- Raja Juan Ready - 2306220431
- Muhammad Hafizh Nurian - 2306211881

## Langkah-langkah Menjalankan

1. **Build Project Otomatis**
   - **Jalankan** `build.bat`  
     (klik dua kali atau via terminal di root folder project)

2. **Jalankan Server**
   - Jalankan:
     ```
     server.exe
     ```

3. **Jalankan Client**
   - Bisa dijalankan beberapa kali di komputer berbeda/terminal berbeda:
     ```
     client.exe
     ```

4. **(Opsional) Jalankan Client Barcode Kamera (Python)**
   - Pastikan sudah install dependensi:
     ```
     pip install opencv-python pyzbar
     ```
   - Jalankan:
     ```
     python ClientNodeCamera.py
     ```

---

## Daftar Command di Server

| Command                  | Fungsi                                        |
|--------------------------|-----------------------------------------------|
| `help`                   | Lihat daftar command dan penjelasannya        |
| `list`                   | Daftar client yang terhubung                  |
| `log`                    | Lihat log absensi (teks)                      |
| `find <npm>`             | Cari log berdasar NPM                         |
| `addnpm <npm> <nama>`    | Tambah mahasiswa ke database                  |
| `exportjson`             | Ekspor log biner ke JSON                      |
| `resetlog`               | Reset log harian biner                        |
| `exit` / `quit`          | Tutup server dan koneksi client               |

---

**Catatan:**  
- Semua perintah dijalankan dari folder utama (bukan dari folder src).
- Jalankan `build.bat` setiap kali selesai mengedit kode sumber di src supaya semua file hasil build & data terbaru ada di luar src.
- Client Python untuk barcode hanya opsional, bisa dipakai jika ingin absen via scan kamera.

---
