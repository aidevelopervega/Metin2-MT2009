# METİN2 MT2009 — GELİŞTİRİCİ VE AJAN ANAYASASI (AGENTS.md)

Bu belge, bu projede çalışacak tüm yapay zeka ajanları ve geliştiriciler için **değişmez kuralları ve çalışma protokolünü** belirler.

---

## 1. TEMEL MİMARİ VE PLATFORM PRENSİPLERİ

1. **Temel Amaç ve Tek Kaynak Kod (Single-Source):**
   * Temel amaç; güvenli, mantıksal olarak doğru, performanslı, düşük gecikmeli, kararlı ve eksiksiz **native x64 sunucu ve istemci** geliştirmesidir.
   * Metin2 sunucusu (Game, DB ve çekirdek kütüphaneler), hem **Windows amd64 (x86_64)** hem de **FreeBSD amd64 (x86_64)** üzerinde çalışabilen tek bir kod tabanından geliştirilecektir.
2. **Derleyici ve Araç Zinciri Standardı:**
   * **Sunucu (`server/src/**`, `tests/**`):** Her iki platformda da yalnızca **GCC** (Windows tarafında MinGW-w64 / UCRT64 GCC, FreeBSD tarafında amd64 GCC) kullanılacaktır; sunucu derlemesinde Visual Studio ve MSVC kullanılmaz.
   * **İstemci (`client/src/**`, `Metin2Client.sln`):** Mevcut MSVC / Visual Studio çözümünün ve uygun gerçek **native x64 (`PE32+`)** araç zincirinin kullanılmasına izin verilir; istemci gereksiz biçimde GCC'ye zorlanmaz.
3. **Öncelik: Gerçek Native 64-Bit Uyumluluğu:**
   * Sunucu (`ELF64` / `PE32+`) ve istemci (`PE32+`) çalıştırılabilir dosyaları ile yüklenen tüm yerel DLL, paylaşımlı kütüphane (`.so`) ve ikili bağımlılıklar (`.lib`, `.a`, `.obj`, `.pyd`) gerçek 64-bit (`amd64`) olmak zorundadır; zorunlu 32-bit (`x86`) kütüphane veya 32-bit süreç bağımlılığı bulunamaz.
   * 32-bit istemci 64-bit Windows üzerinde (WoW64) çalışabilir; gerçek 64-bit istemci ise 32-bit Windows üzerinde çalışamaz. Hedef 64-bit Windows'ta 32-bit uygulama çalıştırmak değil, **gerçek native x64 (`PE32+`) istemcidir**.
   * İşaretçi (`pointer`), tanıtıcı (`handle`), boyut (`size`), hizalama (`alignment`) ve ABI dönüşümlerinde 32-bit kırpılması (`truncation`) olamaz; `uint64_t`, `int64_t`, `uintptr_t`, `intptr_t` ve `size_t` yalnızca gerçekten gereken yerlerde doğru kullanılır. Windows (`LLP64`) ile FreeBSD (`LP64`) arasındaki `long` türü farklarına, istemcide ise `sizeof`, `offsetof`, `alignof`, 8 baytlık işaretçi genişliği ve doğru serileştirme kurallarına azami dikkat gösterilecektir.
4. **C++ Standardı:**
   * Başlangıç standardı **C++20**'dir (`-std=c++20` / `/std:c++20`).
   * Standart yükseltmesi (C++23 vb.) ancak tüm platformlarda ve bağımlılıklarda uyumluluk kesin olarak doğrulandıktan sonra gündeme gelebilir.
5. **Oyun Mantığı, Veri Bütünlüğü ve Ağ Protokolü Dokunulmazlığı:**
   * Mevcut MT2009 oyun mekanikleri, formülleri, item/Yang dupe korumaları, ACID garantileri, SQL güvenliği, tek kullanımlık `LoginByKey` (`anti-replay`), taşma (`overflow`), yarış koşulları (`race condition`) ve istemci-sunucu x64 ağ paket protokolü (`MT2_PROTOCOL_VERSION_X64 = 0x20090064U`, `packet.h`, `Packet.h`, `#pragma pack(1)`) korunacaktır.
   * **32-bit veri alanı $\neq$ x86 bağımlılığı:** Protokol gereği `uint32_t` / `int32_t` kalan `VID`, `flag`, `opcode`, `vnum`, soket veya koordinat gibi sabit genişlikli alanlar sırf x64 hedefi nedeniyle değiştirilmez.

---

## 2. GELİŞTİRME VE İŞ BİRLİĞİ KURALLARI

1. **Eşzamanlı Değişiklik Yasağı:**
   * Aynı anda birden fazla ajan aynı kaynak dosyalarını veya modülleri değiştiremez.
2. **Ölçülebilir Başarı ve Test Zorunluluğu:**
   * Her görev için baştan belirlenmiş somut başarı kriterleri bulunacaktır.
   * Somut bug veya güvenlik açığı varsa ilk gerçek kök neden bulunur, minimum kalıcı düzeltme uygulanır; etkilenen doğrulama `PASS` olmadan o iş tamamlanmış kabul edilmez veya sorun görmezden gelinerek ilerlenmez.
3. **Görev Öncesi Doğrulama:**
   * Yeni bir göreve başlamadan önce `git status` ve `PROJECT_STATE.md` dosyasının içeriği mutlaka kontrol edilmelidir.
4. **Dal (Branch) Yönetimi ve Yerel Güvenlik Koruması:**
   * `main` dalı doğrudan değiştirilmeyecektir. Tüm geliştirmeler `dev/windows-gcc-baseline` dalında yürütülecektir.
   * Depo genelinde `.githooks/` (`pre-commit`, `pre-push`) kancaları aktiftir; `.githooks/install.ps1` / `.githooks/install.sh` ile kalıcı `.git/hooks` dizinine kurulur ve `core.hooksPath = D:/mt22009/.git/hooks` olarak yapılandırılmıştır. Yerel ortamda `main` dalına doğrudan commit atılması veya `main` dalına doğrudan/force push yapılması engellenmiştir.
5. **Belge Kirliliğini Önleme (Kesin 3 Belge Sınırı):**
   * Depoda Git tarafından izlenen yalnızca **3 Markdown belgesi** bulunur:
     1. `AGENTS.md` — Kalıcı geliştirme, güvenlik ve çalışma kuralları.
     2. `PROJECT_STATE.md` — Güncel, doğrulanmış proje durumu ve önemli teknik kararlar.
     3. `docs/SERVER_X64_GOALS.md` — Sunucu + istemci native x64 teknik sözleşmesi, gerçek engeller ve kabul kriterleri.
   * Bu belgelerin hiçbiri kendiliğinden yeni iş başlatma yetkisi vermez; `docs/SERVER_X64_GOALS.md` kural kaynağı değildir, `AGENTS.md` ile çelişirse `AGENTS.md` geçerlidir.
   * Yeni `.md`, `.txt`, görev numarası (`M/G/INT/H` kodları), görev listesi, takip belgesi, otomatik roadmap, checklist, handoff, revizyon günlüğü veya rapor dosyası oluşturulması kesinlikle yasaktır.
6. **Güvenlik, Gizlilik ve Arşiv Yönetimi:**
   * Şifreler, veritabanı bağlantı parolaları, tokenlar ve özel anahtarlar kesinlikle Git takibine alınamaz ve GitHub'a yüklenemez.
   * Taşınabilir sunucu kurulumları için oyun verilerinin (`share/data`, `share/locale/poland`, `share/package` — toplam `14,341` dosya) tek resmî ve parolasız kaynak arşivi `server/serverfiles_share.tar.xz` dosyasıdır.
7. **Kullanıcı Odaklı İletişim:**
   * Proje sahibi kodlama bilmemektedir. Tüm açıklamalar, raporlar ve bilgilendirmeler açık, sade ve Türkçe olacaktır.

---

## 3. KESİN MODÜL TAMAMLAMA, KÖK NEDEN VE SADELİK POLİTİKASI

1. **Stop-the-Line — Gerçek Hatayı Çözmeden Geçme:**
   * Geliştirme, derleme, SQL, paket protokolü, çalışma zamanı, istemci veya CI sırasında gerçek bir hata çıktığında:
     1. İlk gerçek kök neden tespit edilir.
     2. Yalnızca o kök nedeni çözen en küçük kalıcı düzeltme uygulanır.
     3. Etkilenen test veya derleme çalıştırılır.
     4. Gerçek `PASS` alınmadan o iş kapatılmaz ve etkilenen alanda bir sonraki adıma geçilmez.
     5. Eksik harici SDK/ikili bağımlılık gibi kodla çözülemeyen gerçek bir engel varsa sahte çözüm veya sahte stub uydurulmaz; kanıtıyla açıkça `BLOCKED` olarak raporlanır.
   * Başarısız testi atlamak, hatayı veya uyarıyı geçici derleme bayraklarıyla (`-D...`, `-Wno-*`) bastırmak ya da sahte `PASS` üretmek kesinlikle yasaktır.
2. **Minimum Doğru Kod ve Performans Disiplini (Bloat Yasağı):**
   * Gereksiz sınıf, wrapper, soyutlama (`abstraction`), fallback, uyumluluk katmanı (`compatibility layer`), framework, bağımlılık veya doküman eklenmez.
   * İstenmeyen refactor, toplu kod değişimi veya tekrar test yapılmaz; çalışan kod yalnızca bir görev listesinde yazıyor diye değiştirilmez.
   * Daha kısa kod uğruna doğruluk, güvenlik ve bakım yapılabilirlik feda edilmez; öncelik kod miktarında değil, doğruluk, sadelik ve gerçek çalışma sonucundadır.
   * Gereksiz bellek ayırma (`allocation`), kopyalama, kilit çekişmesi (`lock contention`), SQL sorgusu, ağ paketi ve gecikme (`latency`) oluşturulmaz; ölçülmemiş mikro-optimizasyonlarla kod karmaşıklaştırılmaz.
   * Yeni VM, yeni CI altyapısı veya gereksiz test sistemi önerilmez.
3. **Çift Platform Bütünlüğü (Windows & FreeBSD):**
   * Windows amd64 için yapılan düzeltmeler FreeBSD amd64 kodunu asla bozmayacaktır (`#ifdef __WIN32__` blokları izole tutulacaktır).
   * Doğrulanmamış platform veya bileşenler raporda ve `PROJECT_STATE.md` içinde açıkça `UNVERIFIED` olarak belirtilecek; denenmemiş hiçbir adım `VERIFIED` ilan edilmeyecektir.
4. **Tekrar Çalıştırılabilir Testler ve Gereksiz Tekrar Test Yasağı:**
   * Testler tek kullanımlık (geçici) olmayacaktır. Modüllere ait küçük, hızlı ve otomatik çalıştırılabilir test kaynakları ve betikleri Git deposunda (`tests/`) saklanacaktır.
   * Önceden `VERIFIED` olarak kanıtlanmış işler, yeni oturum veya hesap açıldığında otomatik olarak tekrar test edilmez; yalnızca yapılan değişiklik ilgili alanı gerçekten etkiliyorsa ilgili regresyon testi çalıştırılır.
5. **Dürüst Doğrulama ve Belge Disiplini:**
   * Hiçbir sorun gerçekte test edilmeden "çözüldü" olarak işaretlenemez.
   * Bir aşamada çözülemeyen bir engel varsa sonraki modüle geçilmeyecek, engel ve kök neden şeffaf şekilde kullanıcıya raporlanacaktır.
   * `AGENTS.md`, `PROJECT_STATE.md` ve `docs/SERVER_X64_GOALS.md` dışında hiçbir yeni Markdown belgesi oluşturulmayacaktır.

---

## 4. KALICI VERİTABANI VE MARIADB 12.3 LTS STANDART ANAYASASI

Bu kural seti, projenin veritabanı altyapısı, `libsql` kütüphanesi, `db` ve `game` sunucuları ile SQL şemaları için **kesin ve değişmez standartları** belirler.

1. **Tek Veritabanı Standardı (MariaDB 12.3 LTS):**
   * Proje yalnızca **MariaDB 12.3 LTS** veritabanı motorunu zorunlu destek ve test standardı olarak kullanır.
   * **MySQL 8.4 LTS** (ve **MariaDB 11.4 LTS**) desteği kalıcı olarak sona ermiştir; gelecekteki Windows ve FreeBSD CI testleri, paketleme, kurulum, veri migrasyonu ve regresyon testleri yalnızca **MariaDB 12.3 LTS** ile çalışır.
   * MariaDB Connector/C (`libmariadb`) tarafından kullanılan MySQL uyumlu C API (`mysql.h`, `mysql_*` fonksiyonları, `include/mysql` başlık yolu veya C++ kodundaki legacy `libmysql` adlandırmaları) eski MySQL sunucusu bağımlılığı sayılmaz; MariaDB'nin çalışması için gerekli bu C API uyumluluk katmanları korunur.
2. **Kapsam (SQL, Migration ve WS Bileşenleri):**
   * `libsql` bağlantı/soyutlama katmanı ve MariaDB Connector/C (`libmariadb`) istemci kütüphanesi.
   * `db` sunucusundaki veritabanı işlemleri, sorgular ve önbellek mekanizmaları.
   * `game` sunucusundaki veritabanı kullanan bütün sistemler (karakter, envanter, lonca, pazar, log vb.).
   * Mevcut tüm SQL dosyaları, tablo şemaları (`sql/*.sql`), indeksler, başlangıç verileri, migration betikleri ve projede bulunan WS bileşenleri x64 uyumluluk kapsamındadır.
   * Gerekli tüm sunucu kaynak kodları, yapılandırma dosyaları ve veri dönüşüm (migration) mekanizmaları.
3. **SQL Mode Dokunulmazlığı:**
   * SQL mode (`STRICT_TRANS_TABLES`, `NO_ZERO_DATE`, `ONLY_FULL_GROUP_BY` vb.) devre dışı bırakılamaz veya uyumluluk sağlamak adına gevşetilemez/kapatılamaz.
4. **Hata Gizleme ve Kolaycı Yaklaşım Yasağı:**
   * `INSERT IGNORE`, `UPDATE IGNORE` ve benzeri hataları/uyarıları maskeleyen yaklaşımlar çözüm kabul edilemez.
   * SQL ve istemci kütüphanesi hataları sessizce yok sayılamaz (`catch-all` ile yutulamaz).
   * Uyumsuzlukların kök nedenleri kod ve şema seviyesinde kalıcı olarak çözülecektir; geçici hileler ve yamalar yasaktır.
5. **Şeffaf Uyumluluk Katmanı:**
   * MariaDB Connector/C ve MariaDB 12.3 LTS gereksinimleri açık, temiz, belgelenmiş ve test edilebilir katmanlarla yönetilecektir.
6. **Veri Tipleri, 64-Bit Kimlik/Ekonomi Tutarlılığı ve Bütünlük Standartları:**
   * Veri tipleri, `PRIMARY KEY`, indeksler, `AUTO_INCREMENT`, `NULL`/`NOT NULL` kuralları, `DEFAULT` değerleri, tarih/saat alanları (`DATETIME`, `TIMESTAMP`), karakter setleri (`utf8mb4`), collation ve transaction (ACID) davranışları titizlikle incelenecek ve standartlaştırılacaktır.
   * `AID`, `PID`, `ITEM_ID` ve ilgili kalıcı kimlikler, Yang/ekonomi değerleri ile `PRIMARY KEY`, `FOREIGN KEY` ve `JOIN` sütunlarında tip ve işaretlilik (`signed` / `unsigned`) uyumu; SQL şeması, C++ tipleri, serileştirme, önbellek (`cache`) ve ağ protokolü arasında tam tutarlılık zorunludur.
   * **SQL tabloları x86/x64 ikilisi değildir:** Sırf 64-bit hedefi nedeniyle tüm `INT` sütunları körü körüne `BIGINT` yapılmaz; yalnızca kanıtlanmış gerçek bir şema hatası varsa yeni migration oluşturulur.
7. **Veri Güvenliği ve Geri Dönüş Güvencesi:**
   * Mevcut tablolar ve kullanıcı verileri korunacaktır; kimlik daralması (`ID narrowing`), taşma (`overflow`), kırpılma (`truncation`), hatalı `JOIN` ve item/Yang dupe riski oluşturulamaz.
   * Üretim veritabanlarına asla dokunulmaz; veri kaybı doğurabilecek yapısal dönüşümler öncesinde doğrulanmış yedek ve geri dönüş (rollback) planı bulunmak zorundadır.
8. **Çift Senaryo Doğrulaması:**
   * Hem sıfırdan temiz kurulum (yeni veritabanı oluşturma) hem de mevcut MT2009 veritabanını yükseltme (migration) senaryoları test edilecektir.
9. **Bütünsel Mimari:**
   * SQL şema dosyaları ile `libsql`, `db` ve `game` C++ kaynak kodları birbirinden kopuk değil, birlikte ve tutarlı olarak geliştirilip değerlendirilecektir.
10. **Çift Platform ve MariaDB 12.3 LTS Test Zorunluluğu:**
    * Windows amd64 ve FreeBSD amd64 GCC derleme ve çalışma uyumluluğu korunacaktır.
    * Hedef veritabanı motorunda (**MariaDB 12.3 LTS**) gerçek fonksiyonel testler (%100 başarı) geçmeden ilgili aşama asla tamamlanmış sayılamaz.
    * Testlerde veri bütünlüğü, karakter/eşya kayıtları, normal ve kasıtlı hatalı sorgular, bağlantı kopması/yeniden bağlanma (reconnect) ve transaction izolasyon sorunları somut olarak doğrulanacaktır.
11. **Resmî Hedef Sürüm:**
    * Kalıcı ve tek hedef veritabanı sürümü **MariaDB 12.3 LTS** olarak sabitlenmiştir; **MySQL 8.4 LTS** ve **MariaDB 11.4 LTS** destek ve test matrisinden kalıcı olarak çıkarılmıştır.

---

## 5. KALICI GITHUB ACTIONS VE CI ERİŞİM STANDARDI

GitHub CLI Windows ortamında kuruludur ve x64bit-source yetkili hesabıyla güvenli oturum açılmıştır. Önceki CI erişim sorunlarının tekrarlanmaması ve sonuçların güvenilir doğrulanması için aşağıdaki kurallar değişmez standart olarak uygulanacaktır:

1. **Varsayılan ve Tek Otomatik Yöntem:**
   * GitHub Actions sonuçlarını okumak için varsayılan ve tek otomatik yöntem, mevcut oturumla `gh run list` ve `gh run view` komutlarıdır.
2. **Kimlik Bilgisi ve Token Toplama Yasağı:**
   * `git credential fill`, `gh auth token`, PowerShell veya Python üzerinden token çıkarma ve token kullanarak manuel Authorization başlığı oluşturma kesinlikle yasaktır. Alternatif kimlik bilgisi toplama yöntemleri denenemez.
3. **CLI Erişim Denetimi ve Hata Bildirimi:**
   * GitHub CLI erişilemezse önce `gh --version` ve `gh auth status --hostname github.com` kontrol edilecektir. Başarısızsa tek seferde sorun kullanıcıya bildirilecek ve durulacaktır.
4. **Tam SHA Karşılaştırması:**
   * Özel depoda CI sorgularken `--commit` ile kısaltılmış SHA sorgusuna güvenilmeyecektir. Geliştirme dalındaki koşular listelenecek ve dönen tam `headSha` ile ilgili kaynak commit'i karşılaştırılacaktır.
5. **Somut Doğrulama ve Detaylı Denetim:**
   * Her doğrulamada Run ID, commit SHA, FreeBSD `MariaDB 12.3 LTS` işinin gerçek sonucu ve gerektiğinde test loglarındaki assertion sayıları ayrı ayrı kontrol edilecektir. Bir işin genel sonucunun SUCCESS olması, tek başına belirli bir assertion sayısının kanıtı olarak sunulamaz.
6. **Tahmin Yasağı ve UNKNOWN Durumu:**
   * GitHub API/CLI sonucu alınamadığında CI durumu asla tahmin edilemez; açıkça UNKNOWN olarak bildirilecektir.
7. **Polling ve Döngü Yasağı:**
   * Sürekli polling, bekleme döngüleri ve tekrarlanan background task oluşturulması yasaktır. Bir kez sorgulanacak, mevcut sonuç bildirilecek ve durulacaktır.
8. **Erişim Hatası ile CI Başarısızlığı Ayrımı:**
   * Git push ile GitHub Actions CI'nin çalışması ve yerel CLI ile sonuçların okunması farklı işlemlerdir. Yerel `gh` erişim hatası, FreeBSD CI'nin başarısız olduğu anlamına gelmez.
9. **Yeniden Çalıştırma Yasağı:**
   * Önceden başarıyla doğrulanan CI testleri, yalnızca sonuç sorgulama problemi nedeniyle asla yeniden çalıştırılamaz.
10. **Kod ve Dal Dokunulmazlığı:**
    * Mevcut başarılı MT2009 kodları, `AGENTS.md` dışındaki dosyalar ve `main` dalı bu süreçte kesinlikle değiştirilemez.

---

## 6. KALICI GİT, GİTHUB VE CI ÇALIŞMA STANDARDI

**Proje:** `D:\mt22009` | **Geliştirme Dalı:** `dev/windows-gcc-baseline` | **Uzak Depo:** `aidevelopervega/Metin2-MT2009` (PUBLIC; `gh` oturum hesabı: `x64bit-source`, depoda `ADMIN` yetkili)

Bu standart, tüm MT2009 geliştirmelerinde Git, GitHub ve CI operasyonlarının tek merkezden, güvenilir ve kanıta dayalı olarak yürütülmesini belirler. Antigravity, tüm işlemleri kendi terminalinden yürütecek; kullanıcıdan normal geliştirme sürecinde manuel PowerShell veya Git yönlendirmesi talep edilmeyecektir.

1. **Görev Başlangıcı:**
   * Mevcut çalışma klasörünün `D:\mt22009` olduğu doğrulanacaktır.
   * Geçerli Git dalı (`git branch --show-current`) ve çalışma ağacı durumu (`git status`) kontrol edilecektir.
   * `AGENTS.md` ve `PROJECT_STATE.md` dosyaları okunarak önceki teknik durum ve açık hedefler doğrulanacaktır.
   * Gerekirse GitHub'daki güncel geliştirme dalıyla senkronizasyon kontrol edilecektir.
   * `main` dalına kesinlikle dokunulmayacak, doğrudan commit veya push yapılmayacaktır.
   * GitHub Desktop için ikinci bir klon veya ayrı çalışma deposu oluşturulmayacaktır.

2. **Geliştirme Öncesi Hata Önleme ve 4 Kademeli Test Standardı (`A -> B -> C -> D`):**
   * Her geliştirme öncesinde çalışan işlevlere olası etkiler, veri kaybı ve güvenlik riskleri (özellikle veritabanı yetkileri, ağ protokolü, tek kullanımlık `LoginByKey` oturum güvenliği ve dosya silme işlemleri) değerlendirilecek; gereksiz dosya değişiklikleri engellenecek; yapılan her işlemin izole ve geri alınabilir olması sağlanacaktır.
   * Önceden doğrulanmış `PASS` kanıtları korunacak; yeni kanıt olmadan hiçbir senaryoya `PASS` yazılmayacak; FreeBSD'ye özgü davranışlar yalnızca Windows `PASS` sonucuna dayanarak `PASS` ilan edilmeyecektir.
   * Tüm doğrulamalar aşağıdaki **4 kademeli sırayla** yürütülecek; `A`, `B` veya `C` kademesinde yakalanabilen hatalar için pahalı `D` kademesi asla gereksiz yere çalıştırılmayacaktır:
     - **Kademe A (Statik ve Sözdizimi Kontrolü — Tüm Değişikliklerde Zorunlu):** Dosya, `git diff`, YAML (`actionlint`), PowerShell AST ve POSIX shell (`sh -n`) sözdizimi doğrulaması.
     - **Kademe B (Windows x64 Derleme ve Birim Testleri — C++/CMake/Test Değişikliklerinde Zorunlu):** Yerel `cmake --build --preset windows-gcc-ucrt` (tam PE64 bağlama doğrulaması), `ctest --preset windows-gcc-ucrt` (`10/10 PASS`), `game_r41023.exe --check-lua` (`11/11 PASS`) ve `python tests/run_quest_matrix.py` (`202/202 PASS`).
     - **Kademe C (Yerel Canlı SQL ve Protokol Duman Testleri — DB/Auth/Game/SQL/Betik Değişikliklerinde Zorunlu):** Değişiklikle ilgili yerel MariaDB 12.3 LTS şema/sorgu testleri ve/veya Windows x64 `db_r41023.exe` + `game_r41023.exe` + `test_server_smoke_client.exe` protokol/oturum testleri.
     - **Kademe D (Tam FreeBSD 15.1 amd64 GCC 14 CI — Yalnızca Bilinçli Tetiklemeyle):** `Kademe A + B + C` yerel olarak %100 geçtikten ve aylık bütçe kontrol edildikten sonra, yalnızca revizyon kapanışı veya FreeBSD'ye özgü doğrulama gerektiğinde `ci/run-full-freebsd.txt` tetikleme dosyası güncellenerek `MariaDB 12.3 LTS` üzerinde tam FreeBSD CI çalıştırılır.

3. **Git ve GitHub Senkronizasyonu:**
   * Anlamlı ve doğrulanmış geliştirme aşaması tamamlandığında:
     1. Gerekli proje belgeleri (`PROJECT_STATE.md`, gerekiyorsa `AGENTS.md`) güncellenecektir.
     2. Değişiklikler `dev/windows-gcc-baseline` dalında commit edilecektir.
     3. Doğrudan `origin dev/windows-gcc-baseline` dalına push yapılacaktır.
     4. Yerel HEAD ile GitHub'daki uzak dalın commit SHA değerlerinin eşleştiği (`git rev-parse HEAD` vs uzak) doğrulanacaktır.
     5. Commit SHA bilgisi sonuç raporunda açıkça bildirilecektir.
   * Sıradan commit ve push işlemleri `ci/run-full-freebsd.txt` dosyasını değiştirmeyecek ve pahalı FreeBSD CI'ı otomatik başlatmayacaktır.

4. **GitHub Actions Maliyet Kontrolü ve FreeBSD CI Yönetimi:**
   * **Bilinçli Tetikleme (`ci/run-full-freebsd.txt`):** `.github/workflows/freebsd-gcc-test.yml` yalnızca `dev/windows-gcc-baseline` dalında `ci/run-full-freebsd.txt` dosyası değiştirildiğinde çalışır (`.github/workflows/` varsayılan `main` dalında bulunmadığından ve `main` dalına dokunulmadığından `workflow_dispatch` kullanılmaz). Tam CI başlatmak sıradan push'ların yan etkisi değil, ayrı ve bilinçli bir karardır.
   * **Aylık Kota ve Bütçe Kuralı:**
     - Aylık GitHub Actions ücretsiz kotası (GitHub Pro): **3.000 dakika** (hesap genelinde paylaşılır).
     - Proje içi normal kullanım üst sınırı: **2.400 dakika**.
     - Son doğrulamalar ve beklenmedik kritik hatalar için zorunlu rezerv: **600 dakika**.
     - Tek işli (`MariaDB 12.3 LTS`) tam FreeBSD koşusunun ölçülen maliyeti önbelleksiz **~22–25 Actions dakikasıdır** (`ccache` + `libcryptopp.a` önbelleklemesi aktiftir).
     - Her tam koşu öncesinde mevcut ayın depo bazlı koşu süreleri (`gh api "repos/.../actions/runs?created=>=YYYY-MM-01"`) ve tahmini maliyet kontrol edilir; hesap genelindeki harici kullanım verisine (`user` scope gerektiren billing API) erişilemiyorsa rakam uydurulmaz, hesap geneli kullanımın `UNKNOWN` olduğu açıkça belirtilir. Bütçe yetersizse GitHub-hosted CI başlatılmaz.
   * **Hata Sonrası Tekrar Çalıştırma Yasağı:** Tam CI başarısız olduğunda kök neden yerel olarak belirlenip düzeltilmeden aynı test asla tekrar çalıştırılmaz.
   * **Sonuç Sorgulama:** CI sonuçları `gh run list` ve `gh run view` ile tek seferde kontrol edilir; `gh run watch`, sürekli polling veya sonsuz bekleme döngüsü kesinlikle yapılmaz; çalışmakta olan işler `RUNNING` olarak bildirilir ve asla `SUCCESS` varsayılmaz.

5. **Proje Hafızası ve Belge Disiplini:**
   * `AGENTS.md` kalıcı çalışma kurallarını, `PROJECT_STATE.md` ise doğrulanmış teknik ilerlemeyi içerecektir.
   * `PROJECT_STATE.md` içinde özellikle şunlar eksiksiz korunacaktır:
     * Son doğrulanmış commit SHA
     * Son CI Run ID ve doğrulanmış durumu
     * Başarılı ve başarısız test aşamaları
     * Açık teknik sorunlar ve CI bütçe/tetikleme durumu
     * Sonraki geliştirme adımı
   * Belge güncellemeleri mümkün olduğunca ilgili geliştirme commit'ine dahil edilecektir.

6. **Son Rapor Standardı:**
   * Her tamamlanan geliştirme görevinde kısa ve kanıta dayalı rapor sunulacaktır:
     * Yapılan değişiklik
     * Çalıştırılan yerel test kademeleri (`A`, `B`, `C`) ve gerçek sonuçları
     * Commit ve push durumu, yerel ve uzak SHA eşleşmesi
     * Gereksiz CI başlatılmadığının doğrulaması (veya bilinçli `D` kademesi tetiklenmişse CI Run ID ve FreeBSD `MariaDB 12.3 LTS` işinin gerçek durumu)
     * Devam eden veya çözülemeyen sorunlar
   * Başarı kanıtı bulunmayan hiçbir aşama tamamlandı ilan edilmeyecektir.

---

## 7. KALICI SERVER EXTERN SÜRÜM YÜKSELTME ANAYASASI

Server ve Common Extern bağımlılıklarının en güncel kararlı resmî sürümlerine yükseltilmesi projenin kalıcı ve zorunlu hedefidir.

1. **Öncelikli Bileşenler:**
   * Lua (**yalnızca Lua 5.5.1 tek üretim ve derleme motorudur**; eski Lua 5.0.3 ve Lua 5.2.3 motorları ile `game_r41023_lua50` geri dönüş ikilisi Rev 51 ile kalıcı olarak kaldırılmıştır), Crypto++, Boost, DevIL (IL/ILU/ILUT), RapidJSON, fmt, zlib, OpenSSL, MariaDB Connector/C (`libmariadb`) istemci kütüphanesi ve Server'ın kullandığı diğer harici bileşenler.
2. **Adım Adım Bağımlılık Yükseltme Protokolü:**
   * Tüm Extern aynı anda değiştirilemez; her bağımlılık tek tek ele alınır:
     1. Mevcut gerçek sürüm belirlenir.
     2. En son resmî kararlı sürüm doğrulanır.
     3. Lisans, güvenlik, API/ABI farkları incelenir.
     4. Doğrulanmış resmî kaynak ve SHA256 ile sürüm sabitlenir.
     5. Kaynak kodu veya uygun sistem paketi güncellenir.
     6. Gerekli Server Source API uyarlamaları yapılır.
     7. FreeBSD GCC 14 amd64 ve Windows GCC 16 UCRT üzerinde derlenip bağlanır.
     8. İlgili protokol, SQL, quest ve regresyon testleri çalıştırılır.
     9. Başarılı güncelleme küçük, izole ve geri alınabilir commit ile kaydedilir.
3. **Gerçek Derleme Şartı:**
   * Yalnızca yeni sürüm dosyalarını indirmek veya manifestte belirtmek UPGRADED sayılamaz; gerçek binary derlemesinde yeni sürüm fiziksel olarak bağlanmalıdır.
4. **Hata ve Engel Yönetimi:**
   * Güncelleme testleri bozarsa kök neden çözülür; uyumsuzluk maskelenemez. Gerçek ve aşılamayan engeller şeffaf biçimde `BLOCKED` olarak raporlanır.

---

## 8. NATIVE X64 İSTEMCİ GELİŞTİRME İZNİ VE KAPSAM SINIRI

Kullanıcı gerçek **native x64 istemci (`PE32+`)** geliştirmesine açık izin vermiştir:
1. **Kapsam ve Hedef:** Eski `client/**`, `client/src/**`, `extern/client/**` ve gerekli istemci derleme/yapılandırma alanlarının x64 dönüşümüne ilişkin mutlak ambargo kaldırılmıştır. Hedef WoW64'e dayanan 32-bit ikili değil, gerçek **Windows x64 çalıştırılabilir dosyasıdır (`PE32+`)**.
2. **İkili Bağımlılık, SDK ve Sahte Stub Yasağı:** İstemcinin yüklediği tüm yerel DLL, `.pyd`, `.lib`, `.obj` ve SDK bağımlılıklarının (Granny, SpeedTree, Miles, DirectX, Python vb.) mimari ve ABI uyumu gerçek dosyalar üzerinden doğrulanır; yalnız x86 olan ikililer x64 varsayılmaz. Gereken veya lisanslı bir x64 SDK/ikili eksikse sahte stub (`no-op`), çalışmayan kütüphane veya WoW64 çözümüyle başarı ilan edilemez; gerçek engel kanıtıyla raporlanır.
3. **Araç Zinciri ve 64-Bit ABI/Bellek Güvenliği:** İstemci için mevcut MSVC / Visual Studio x64 araç zincirinin (`Metin2Client.sln`) kullanılmasına izin verilir; istemci gereksiz biçimde GCC'ye zorlanmaz. İşaretçi (`pointer`), tanıtıcı (`handle`), paketleme (`#pragma pack(1)`), hizalama (`alignment`), çağrı kuralı (`calling convention`) ve bellek tipi hataları kök nedeninden düzeltilir.
4. **Protokol Sözleşmesi ve Oyun Mantığı Korunumu:** Sunucu ve istemci `MT2_PROTOCOL_VERSION_X64 = 0x20090064U` paket sözleşmesini (`sizeof`, `offsetof`, serileştirme ve alan genişlikleri, bkz. `docs/SERVER_X64_GOALS.md`) iki tarafta birebir tutturmak zorundadır. Kasıtlı olarak `uint32_t` / `int32_t` kalan `VID`, `opcode`, `flag` gibi alanlar x86 bağımlılığı değildir ve keyfi olarak genişletilmez. Mevcut oyun mantığı, formüller ve çalışan sistemler x64 dönüşümü gerektirmediği sürece değiştirilmez.

---

## 9. ÇOKLU ORTAM SENKRONİZASYON VE DOĞRULAMA STANDARDI

1. **Ortamlar:**
   * Windows Yerel Geliştirme Ortamı (`D:\mt22009`).
   * FreeBSD 15.1 amd64 Sanal Sunucusu gerçek çalışma ortamı.
   * FreeBSD sanal sunucusundaki kalıcı Git deposu (mevcutsa; CI'ın geçici checkout'u kalıcı depo olarak raporlanamaz).
   * GitHub Uzak Dalı (`origin/dev/windows-gcc-baseline`).
   * GitHub Actions FreeBSD CI temiz checkout ortamı.
2. **Kayıpsız Uzlaşma:**
   * Farklı commit veya yerel değişiklik varsa kayıpsız uzlaştırılır; asla force reset, zorla overwrite veya veri silme yapılmaz.
   * Erişilemeyen ortamlar asla `SYNCHRONIZED` olarak ilan edilemez; durum dürüstçe `UNKNOWN` veya `EPHEMERAL` olarak bildirilir.
3. **Tek Kaynak Kod Bütünlüğü:**
   * Windows ve FreeBSD aynı Git commit SHA'sındaki kaynak kodları kullanır; platform ikilileri ve nesne dosyaları kendi mimarilerine göre ayrı derlenir.

---

## 10. GÖREV DİSİPLİNİ, BELGE YETKİ SINIRI VE GEREKSİZ YÖNLENDİRME YASAĞI

1. **Belge Yetki Sınırı:**
   * `AGENTS.md` kalıcı kuralları, `PROJECT_STATE.md` güncel durumu, `docs/SERVER_X64_GOALS.md` ise teknik kabul kriterlerini belirler. Bu belgelerin hiçbiri kendiliğinden yeni iş başlatma yetkisi vermez.
2. **Gereksiz Yönlendirme, Görev ve Kod Şişirme Yasağı:**
   * Yeni `M/G/INT/H` hedef kodları, yapay görevler, görev listeleri, takip belgeleri veya otomatik roadmap oluşturulmaz; geçmiş görevler yeni iş gibi ele alınmaz.
   * Tamamlanmış ve `VERIFIED` kanıtlı işler sebepsiz yeniden doğrulanmaz; yalnızca yeni oturum açıldı diye derleme veya test döngüsü başlatılmaz.
   * Yeni `.md` dosyası, yeni VM veya yeni CI altyapısı oluşturulmaz.
   * Doğrudan çözülebilecek gerçek bir bug gereksiz analiz ve rapor sürecine dönüştürülmez; tamamlanan iş kapatılır ve `PROJECT_STATE.md` güncellenir.
3. **Kalıcı Kural Dokunulmazlığı:**
   * Kurallar her oturumda yeniden keşfedilmeyecek; `AGENTS.md` değişmez anayasa olarak uygulanacaktır.

---

## 11. WINDOWS YEREL GELİŞTİRME DERLEME CPU VE ÇEKİRDEK SINIRLANDIRMASI (YALNIZCA DERLEME)

Windows 11 Pro x64 (`Intel Core i5-12450H`, 8 fiziksel çekirdek [4 P-core + 4 E-core], 12 mantıksal işlemci) geliştirme bilgisayarında kaynak sınırlandırması **yalnızca derleme süreçlerine** uygulanır; derleme dışındaki test, analiz ve geliştirme işlemlerine hiçbir yapay kısıtlama uygulanmaz:

1. **Yalnızca Derleme Süreçleri İçin Sınırlar (`CMake`, `Ninja`, `Make`, `GCC`, `G++`, `MSVC/cl`, `linker`):**
   * **CPU Hard-Cap (`JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP`):** Derleme işlem ağacının toplam sistem CPU kullanımı en fazla **%50 (`CpuRate = 5000/10000`)** ile sınırlandırılır.
   * **Hibrit CPU Affinity (`JOB_OBJECT_LIMIT_AFFINITY` — 6 Mantıksal İşlemci):** Intel Core i5-12450H hibrit topolojisinde (`P-core LP 0..7 [0x0FF]`, `E-core LP 8..11 [0xF00]`) ilk 3 fiziksel P-core'u (`LP 0..5`, `0x03F`) kullanıcıya tamamen serbest bırakmak için derleme iş grubu **6 mantıksal işlemciye (`0xFC0` -> `LP 6..11`: 1 P-core [2 LP, `0x0C0`] + 4 E-core [4 LP, `0xF00`])** sabitlenir (eski `0xF00` 4-LP maskesi kaldırılmıştır).
   * **Derleme Paralelliği:** Maksimum **6 paralel derleme işi** (`CMAKE_BUILD_PARALLEL_LEVEL=6`, `-j6`) çalıştırılır.
   * **İşlem Önceliği (`JOB_OBJECT_LIMIT_PRIORITY_CLASS`):** Yalnızca derleme işlem ağacı **`BelowNormal` (`0x4000`)** önceliğinde çalışır; alt derleme süreçleri aynı Job Object sınırlarına tabidir ve derleme bittiği anda sınırlandırma sona erer.
2. **Derleme Dışı İşlemlerde Sınırlama Yasağı (`DEFAULT` Zamanlayıcı):**
   * `CTest`, MariaDB entegrasyon testleri, Lua ve Quest testleri, Packet Wire testleri, Python analiz/kaynak taramaları, Git işlemleri ve diğer tüm geliştirme/sistem işlemlerine **CPU hard-cap, affinity daraltması, `BelowNormal` öncelik veya yapay paralellik sınırı uygulanmaz**.
   * Bu işlemler Windows'un standart CPU zamanlayıcısı (`CPU_RATE_CONTROL_HARD_CAP = INACTIVE`), varsayılan işlem önceliği (`Normal`, `0x20`) ve tüm kullanılabilir mantıksal işlemcilerle (`0xFFF`, 12/12 LP) çalışır.
   * Kullanıcının diğer uygulamalarına veya sistem geneli kalıcı CPU/affinity/priority ayarlarına kesinlikle dokunulmaz.

---

## 12. ANTIGRAVITY HESAP VE OTURUM BAĞIMSIZLIĞI

1. **Hesaptan Bağımsız Hafıza:**
   * Proje bilgisi yalnızca depodaki 3 ana belgede (`AGENTS.md`, `PROJECT_STATE.md`, `docs/SERVER_X64_GOALS.md`) tutulur. Antigravity sohbet geçmişi, `brain/`, `conversations/` veya oturum `scratch` klasörleri hesaba bağlıdır ve proje hafızası sayılmaz.
2. **Oturum Yolu Yasağı:**
   * `C:\Users\...\.gemini\antigravity\brain\<conversation-id>\scratch` gibi oturuma özel yollar kalıcı test prosedürü olarak kullanılamaz. Tekrar çalıştırılması gereken test betikleri sırsız ve parametreli olarak `tests/` altında Git'e alınır.
   * Git'e konamayan yerel ikili test bağımlılıkları (ör. taşınabilir MariaDB 12.3.3) Git dışında `D:\mt22009\scratch\handoff\` altında tutulur; bu klasör silinmez ve içeriği commit edilmez.
3. **Yeni Oturum ve Hesap Geçiş Kuralı:**
   * Yeni bir Antigravity hesabı veya oturumu açıldığında `git status`, `AGENTS.md`, `PROJECT_STATE.md` ve `docs/SERVER_X64_GOALS.md` kontrol edilir. Önceden `VERIFIED` olarak kanıtlanmış işler yeni oturum veya hesap açıldığında otomatik olarak tekrar test edilmez; yalnızca ilgili kodu değiştiren bir geliştirme yapıldığında ilgili test çalıştırılır.
4. **Sır ve Oturum Verisi:**
   * Google oturumu, cookie, OAuth, API anahtarı, GitHub tokenı, SSH özel anahtarı ve veritabanı parolaları okunmaz, belgeye yazılmaz, Git'e eklenmez ve hesaplar arasında kopyalanmaz. Depo PUBLIC olduğundan her commit öncesinde sır denetimi zorunludur.
