# MT2009 — GIT VE FREEBSD CHECKOUT DENETİMİ

**Tarih:** 11 Ekim 2026  

---

## 1. GİTHUB UZAK DAL VE SENKRONİZASYON DOĞRULAMASI

* **Uzak Depo:** `https://github.com/x64bit-source/Metin2-MT2009.git`
* **Aktif Geliştirme Dalı:** `dev/windows-gcc-baseline`
* **Yerel HEAD Commit:** `d96b34371c1577e147fc57565707e028e6bd691c`
* **Uzak HEAD Commit:** `d96b34371c1577e147fc57565707e028e6bd691c`
* **Senkronizasyon Durumu:** Yerel ve uzak commit SHA değerleri tam eşleşmektedir (100% in sync). `main` dalı dokunulmamıştır.

---

## 2. GİTİGNORE VE İZLENMEYEN (IGNORED) DOSYALAR DENETİMİ

1. **GitHub 100 MB Sınırı Koruması:**
   - `mt2009 - FULL SOURCE/Source/Extern/lib/cryptlib-Debug.lib` (110.819.592 bayt > 100 MB) `.gitignore` içinde kesin olarak hariç tutulmuştur (satır 81). Bu sayede push blokajı engellenmiştir.
2. **Çalışma Zamanı ve Büyük Arşivler:**
   - `mt2009 - FULL SOURCE/Klient/` (Hazır 32-bit istemci paketi) `.gitignore` kuralıyla depoya yüklenmemektedir.
   - `mt2009 - FULL SOURCE/new_ftp.tar.gz` (50.5 MB sunucu paketi) `.gitignore` kuralıyla depoya yüklenmemektedir.
   - `locales-mt2009.rar` (12.5 MB) `*.rar` kuralıyla depoya yüklenmemektedir.
3. **Gizlilik ve Güvenlik:**
   - `kurulum.py`, `push_to_github.bat`, `Packleri*` dosyaları `.gitignore` altında filtrelenmiş; parola veya token sızıntısı önlenmiştir.

---

## 3. GİTHUB ACTIONS CHECKOUT VE SPARSE-CHECKOUT DENETİMİ

`.github/workflows/freebsd-gcc-test.yml` içerisindeki checkout yapılandırması:
```yaml
sparse-checkout: |
  tests
  mt2009 - FULL SOURCE/Server Source/Server/libpoly
  mt2009 - FULL SOURCE/Server Source/Server/libgame
  mt2009 - FULL SOURCE/Server Source/Server/libthecore
  mt2009 - FULL SOURCE/Server Source/Server/libsql
  mt2009 - FULL SOURCE/Server Source/Server/common
  mt2009 - FULL SOURCE/Server Source/Server/db
  mt2009 - FULL SOURCE/Server Source/Server/game/src/locale.hpp
  mt2009 - FULL SOURCE/Server Source/Extern/include
```

* **Olumlu Tespitler:**
  - Yalnızca FreeBSD sunucusu ve testlerinin ihtiyaç duyduğu dosyalar çekilmektedir; 17.000 dosyadan oluşan istemci kaynakları gereksiz yere checkout edilmemektedir.
  - Rev 18'de eklenen `locale.hpp` başlığı eksiksiz çekilmektedir.
* **Tetikleme (Paths Filter) İncelemesi:**
  - Push tetikleyicisi `docs/**` dizinini KAPSAMAMAKTADIR.
  - Bu sayede `docs/migration/` altındaki rapor belgelerinin veya `PROJECT_STATE.md` dosyasının commit edilmesi gereksiz CI koşusu TETİKLEMEZ.
