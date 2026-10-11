# MT2009 — GERÇEK DOSYA AĞACI VE KAYNAK KOD ENVANTERİ

**Tarih:** 11 Ekim 2026  
**Depo Kökü:** `D:\mt22009`  
**Git Takip Durumu:** 34.986 dosya Git tarafından izlenmektedir (Tracked).  
**Fiziksel Disk Durumu:** Disk üzerinde `.git` hariç toplam 38.336 dosya mevcuttur.  

---

## 1. DİSK ÜZERİNDEKİ TÜM DOSYALARIN KATEGORİK DAĞILIMI

| Kategori | Dosya Sayısı | Açıklama | Git Durumu |
| :--- | :--- | :--- | :--- |
| **Server Source** | 659 | Sunucu C++ kaynakları (`db`, `game`, `libthecore`, `libsql`, `libgame`, `libpoly`, `common`) | **Tracked** (34 dosya .obj/s geçici) |
| **Server Extern** | 16.732 | Sunucu üçüncü parti başlıkları ve kütüphaneleri (`boost`, `cryptopp`, `mysql`, `fmt`, `msl` vb.) | **Tracked** (5 adet .a dahil) |
| **Client Source** | 693 | İstemci Visual Studio / C++20 kaynakları (16 kütüphane ve `UserInterface`) | **Tracked** |
| **Client Extern** | 17.579 | İstemci üçüncü parti başlıkları, 32-bit kütüphaneleri ve derlenmiş python .obj dosyaları | **Tracked** (cryptlib-Debug.lib hariç) |
| **Client Runtime (Klient)** | 315 | 32-bit `metin2client.exe`, `config.exe`, 7 DLL, 10 Miles sürücüsü, 85 pack arşivi, 50 python lib | **Ignored** (.gitignore kuralı) |
| **SQL Şemaları** | 6 | `account.sql`, `common.sql`, `log.sql`, `player.sql`, `world.sql` ve şema dosyaları | **Tracked** |
| **Testler** | 89 | Otomatik test kaynakları (`test_*.cpp`, betikler, build çıktıları) | **Tracked** (testler) / **Ignored** (build) |
| **Büyük Arşivler** | 3 | `new_ftp.tar.gz` (50.5 MB), `locales-mt2009.rar` (12.5 MB), `PackMakerLite.zip` (1.3 MB) | **Ignored** (.gitignore kuralı) |
| **Yönetim ve Araçlar** | 2.260 | `AGENTS.md`, `PROJECT_STATE.md`, `.githooks/`, `scratch/` geçici dosyaları | **Tracked** (docs/hooks) / **Ignored** (scratch) |
| **TOPLAM (Disk)** | **38.336** | **Tüm ağaç eksiksiz taranmıştır.** | |

---

## 2. MEVCUT KLASÖR YAPISI VE KULLANIMLARI

1. `mt2009 - FULL SOURCE/Server Source/Server/`
   - `common/`: Ortak ağ protokolü paketleri (`tables.h`, `service.h`, `length.h`, `item_length.h`).
   - `libthecore/`: Çekirdek C kütüphanesi (soket, tampon, DES/TEA şifreleme, sinyal, zamanlayıcı).
   - `libsql/`: Asenkron MySQL/MariaDB bağlantı ve kuyruk yöneticisi (`CAsyncSQL`, `SQLResult`).
   - `libgame/`: Oyun içi ızgara (`CGrid`), zemin niteliği (`CAttribute`), Targa görüntü işleme.
   - `libpoly/`: Polinom ve hasar/statü formül hesaplayıcısı.
   - `db/`: Veritabanı önbellek ve eşitleme sunucusu (`db.exe` / `db_r1`).
   - `game/`: Oyun mantık sunucusu (`game.exe` / `game_r1`).
2. `mt2009 - FULL SOURCE/Source/Source Client/`
   - 16 adet statik C++ kütüphanesi + 1 adet ana çalıştırılabilir proje (`UserInterface`).
   - `Metin2Client.sln` çözüm dosyası.
3. `mt2009 - FULL SOURCE/Klient/`
   - Çalışmaya hazır tam MT2009 32-bit Windows istemci paketi.
4. `mt2009 - FULL SOURCE/new_ftp.tar.gz`
   - Gerçek FreeBSD sunucu kurulum paketinin tamamı (16.127 dosya, `usr/home/mt2/` altında).
