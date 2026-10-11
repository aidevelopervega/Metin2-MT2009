# MT2009 — CLIENT SOURCE, CLIENT FILES VE SERVER FILES ANALİZİ

**Tarih:** 11 Ekim 2026  

---

## 1. CLIENT SOURCE MODÜLLERİ (16 MODÜL + 1 SLN)

Client kaynak kodları Visual Studio 2022 (v143 araç seti) ve C++20 (`stdcpp20`) standardını hedeflemektedir. Tüm projeler **Win32 (x86 32-bit)** olarak yapılandırılmıştır; projede **x64 konfigürasyonu bulunmamaktadır**.

| Modül Adı | Dosya Sayısı | İşlevi ve Görevi |
| :--- | :--- | :--- |
| **UserInterface** | 199 | Ana istemci giriş noktası (`UserInterface.cpp`), ağ akışı (`PythonNetworkStream.cpp`), uygulama döngüsü |
| **EterLib** | 152 | Grafik motoru temel kütüphanesi, DirectX 8/9 sarmalayıcıları, kamera, yazı tipi, gökyüzü |
| **GameLib** | 100 | Oyun dünyası, aktörler, uçan nesneler, eşya verileri, harita yükleyici |
| **EterBase** | 48 | Dosya I/O, bellek havuzları, şifreleme yardımcıları, zamanlayıcı |
| **EffectLib** | 37 | Parçacık (particle) efektleri, görsel efekt yöneticisi |
| **EterGrnLib** | 26 | Granny 3D modelleme, iskelet ve animasyon yöneticisi |
| **MilesLib** | 21 | Miles Sound System ses motoru yöneticisi |
| **SpeedTreeLib** | 19 | SpeedTree RT orman, ağaç ve bitki örtüsü yöneticisi |
| **EterPythonLib** | 18 | Python UI bileşenleri, ızgara pencereleri, butonlar |
| **EterPack** | 14 | `.data` / `.index` ve `.epk` / `.eix` arşiv paket çözücü |
| **ScriptLib** | 13 | Python script çalışma zamanı ve bağlayıcıları |
| **EterLocale** | 13 | String dönüşümleri, dil tabloları, ANSI codepage eşleşmeleri |
| **SphereLib** | 11 | Küresel çarpışma (collision) tespit kütüphanesi |
| **EterImageLib** | 9 | TGA, BMP, DXTC resim format işleyicisi |
| **PRTerrainLib** | 8 | Yükseklik haritası ve arazi kaplama yöneticisi |
| **CWebBrowser** | 4 | Dahili nesne market için ActiveX / Internet Explorer web kontrolü |
| **Metin2Client.sln** | 1 | Ana Visual Studio 2022 çözüm dosyası |

---

## 2. CLIENT FILES (KLİENT DİZİNİ) ENVANTERİ

`mt2009 - FULL SOURCE/Klient` dizini, oyunu başlatmak için gereken tüm 32-bit çalışma zamanı varlıklarını eksiksiz barındırmaktadır:

1. **Çalıştırılabilir Dosyalar (Executables):**
   - `metin2client.exe`: 13.585.920 bayt — **PE32 (x86 32-bit)**.
   - `config.exe`: 3.817.784 bayt — **PE32 (x86 32-bit)**.
2. **Dinamik Kütüphaneler (DLLs - Tümü PE32 x86):**
   - `MSS32.dll` (Miles Sound System)
   - `SpeedTreeRT.dll` (SpeedTree RT)
   - `granny2.dll` (Granny 3D Runtime)
   - `devil.dll` (DevIL Image Library)
   - `d3dx9_43.dll` (DirectX 9 D3DX)
   - `msvcr100.dll` (VC++ 2010 CRT)
   - `WebView2Loader.dll` (Edge WebView2)
3. **Alt Klasörler:**
   - `pack/`: 85 adet PackMakerLite arşiv dosyası (`.data`, `.index`, `Index`, açma/paketleme betikleri).
   - `miles/`: 10 adet Miles ses sürücüsü (`mssa3d.m3d`, `mssds3d.m3d`, `mssmp3.asi` vb.).
   - `lib/`: 50 adet Python 2.7 standart kütüphane modülü.
   - `BGM/`: 20 adet oyun içi müzik parçası (`enter_the_east.mp3` vb.).
   - `cache/`, `mark/`, `screenshot/`, `upload/`: Oyun içi boş dizinler.

---

## 3. SERVER FILES (NEW_FTP.TAR.GZ) ENVANTERİ

Kök dizinde veya kaynak klasöründe açık bir `Server Files` klasörü bulunmamaktadır. Ancak `mt2009 - FULL SOURCE/new_ftp.tar.gz` arşivi (50.566.464 bayt) incelendiğinde, **16.127 dosyadan oluşan tam bir FreeBSD sunucu dosyası paketi** tespit edilmiştir:

* **Ana Konum:** `usr/home/mt2/`
* **Alt Klasörler:**
  * `auth/`: Kimlik doğrulama sunucu dizini (`CONFIG`, `game` symlink).
  * `chs/`: Kanal sunucu dizinleri (`ch1/`, `ch2/`, `ch99/` vb.).
  * `db/`: Veritabanı sunucu dizini (`conf.txt`, `item_proto.txt`, `item_names.txt`, `mob_proto.txt`, `mob_names.txt`).
  * `share/`: Ortak veri klasörü:
    * `share/data/`: Canavar, harita, zindan modelleri ve verileri.
    * `share/locale/poland/`: Polonya MT2009 görevleri (`quest/`), haritaları (`map/`), yerelleştirme metinleri.
  * `start.sh`, `stop.sh`, `clear.sh`: Sunucu başlatma, durdurma ve log temizleme kabuk betikleri.

---

## 4. DİL VE YERELLEŞTİRME KAYNAKLARI (LOCALES-MT2009.RAR)

Kök dizindeki `locales-mt2009.rar` (12.584.352 bayt) salt okunur olarak incelendiğinde:
* 1.942 dosya içermektedir.
* İstemci tarafı yerelleştirme dosyalarını (`pl/itemdesc.txt`, `pl/item_names.txt`, `pl/locale_game.txt`, `pl/locale_interface.txt`, `pl/quest/`, `pl/map/` vb.) barındırmaktadır.

---

## 5. İSTEMCİ X64 DUVARI (CLIENT 64-BIT REALITY CHECK)

Sunucu (Game & DB) tamamen 64-bit native hale getirilmiştir. Ancak Metin2 2009 İstemcisinin x64'e taşınması önünde **bağımsız SDK lisans ve ikili kütüphane engelleri** bulunmaktadır:
1. **Granny 3D:** Rad Game Tools Granny 2.9/2.11 kapalı kaynaklı ticari bir kütüphanedir; depoda yalnızca 32-bit `granny2.dll` ve `granny2.*.lib` mevcuttur.
2. **SpeedTree RT:** IDV SpeedTreeRT 32-bit kapalı kaynak SDK'sıdır.
3. **Miles Sound System:** RAD Miles 32-bit kapalı kaynak SDK'sıdır (`mss32.dll`).
4. **Python 2.7:** `python27-static.lib` ve `Source/Extern/lib` içerisindeki 121 adet `.obj` dosyası 32-bit x86 COFF nesneleridir.

**Stratejik Karar:** Sunucu x64 modernizasyonu ve klasör taşıması, İstemci 32-bit mimarisinden izole yürütülmeli; istemci-sunucu arasındaki ağ protokolü (`#pragma pack(1)`) ve paket boyutları 32-bit istemciyi destekleyecek şekilde korunmalıdır.
