# METİN2 MT2009 — GELİŞTİRİCİ VE AJAN ANAYASASI (AGENTS.md)

Bu belge, bu projede çalışacak tüm yapay zeka ajanları ve geliştiriciler için **değişmez kuralları ve çalışma protokolünü** belirler.

---

## 1. TEMEL MİMARİ VE PLATFORM PRENSİPLERİ

1. **Tek Kaynak Kod (Single-Source):**
   * Metin2 sunucusu (Game, DB ve çekirdek kütüphaneler), hem **Windows amd64 (x86_64)** hem de **FreeBSD amd64 (x86_64)** üzerinde çalışabilen tek bir kod tabanından geliştirilecektir.
2. **Derleyici Standardı:**
   * Her iki platformda da yalnızca **GCC** (veya platform uyumlu Clang) kullanılacaktır.
   * **Visual Studio ve MSVC KESİNLİKLE KULLANILMAYACAKTIR.** (Windows tarafında MinGW-w64 / UCRT64 GCC kullanılacaktır).
3. **Öncelik: Güvenli Native 64-Bit Uyumluluğu:**
   * Sunucu doğrudan 64-bit (amd64) mimarisini hedefleyecektir.
   * Windows (LLP64) ile FreeBSD (LP64) arasındaki `long` türü farklarına azami dikkat gösterilecektir.
4. **C++ Standardı:**
   * Başlangıç standardı **C++20**'dir (`-std=c++20`).
   * Standart yükseltmesi (C++23 vb.) ancak tüm platformlarda ve bağımlılıklarda uyumluluk kesin olarak doğrulandıktan sonra gündeme gelebilir.
5. **Oyun Mantığı ve Ağ Protokolü Dokunulmazlığı:**
   * Mevcut MT2009 oyun mekanikleri, formülleri ve istemci-sunucu ağ paket protokolü (`packet.h`, `Packet.h`) korunacaktır.
   * İstemci 32-bit Windows binary'si olduğu için sunucu tarafındaki paket boyutları ve struct hizalamaları (`#pragma pack(1)`) bozulamaz.

---

## 2. GELİŞTİRME VE İŞ BİRLİĞİ KURALLARI

1. **Eşzamanlı Değişiklik Yasağı:**
   * Aynı anda birden fazla ajan aynı kaynak dosyalarını veya modülleri değiştiremez.
2. **Ölçülebilir Başarı ve Test Zorunluluğu:**
   * Her görev için baştan belirlenmiş somut başarı kriterleri bulunacaktır.
   * Derlenip yerel olarak test edilmemiş hiçbir işlem "tamamlandı" kabul edilemez.
3. **Görev Öncesi Doğrulama:**
   * Yeni bir göreve başlamadan önce `git status` ve `PROJECT_STATE.md` dosyasının içeriği mutlaka kontrol edilmelidir.
4. **Dal (Branch) Yönetimi ve Yerel Güvenlik Koruması:**
   * `main` dalı doğrudan değiştirilmeyecektir. Yeni geliştirmeler, modernizasyonlar ve CMake geçişi ayrı özellik dallarında (`feature/...` veya `dev/...`) yürütülecektir.
   * Depo genelinde `.githooks/` (`pre-commit`, `pre-push`) yapılandırması (`core.hooksPath = .githooks`) aktiftir. Yerel ortamda `main` dalına doğrudan commit atılması veya `main` dalına doğrudan/force push yapılması engellenmiştir.
5. **Belge Kirliliğini Önleme:**
   * Projede yönetim ve hafıza amacıyla yalnızca `AGENTS.md` ve `PROJECT_STATE.md` kullanılacaktır.
   * Yeni MD, TXT, günlük (log), rapor veya tarihçe dosyaları oluşturulması yasaktır.
6. **Güvenlik ve Gizlilik:**
   * Şifreler, veritabanı bağlantı parolaları, tokenlar ve özel anahtarlar kesinlikle Git takibine alınamaz ve GitHub'a yüklenemez.
7. **Kullanıcı Odaklı İletişim:**
   * Proje sahibi kodlama bilmemektedir. Tüm açıklamalar, raporlar ve bilgilendirmeler açık, sade ve Türkçe olacaktır.

---

## 3. KESİN MODÜL TAMAMLAMA VE KALICI DÜZELTME POLİTİKASI

1. **Aşama ve Modül Geçiş Yasağı:**
   * Mevcut modüldeki/aşamada tespit edilmiş tüm derleme hataları, derleyici uyarıları, platform uyumsuzlukları ve test eksikleri tamamen çözülmeden kesinlikle bir sonraki modüle geçilemez.
2. **Kök Neden Çözümü (Geçici Bayrak ve Bastırma Yasağı):**
   * Hatalar ve uyarılar geçici derleme bayraklarıyla (`-D...` geçici yamalarıyla) veya uyarıları görmezden gelerek/bastırarak çözülemez.
   * Her sorunun kök nedeni tespit edilecek; kod seviyesinde doğru, kalıcı ve standartlara uygun çözümler uygulanacaktır.
3. **Çift Platform Bütünlüğü (Windows & FreeBSD):**
   * Windows amd64 için yapılan düzeltmeler FreeBSD amd64 kodunu asla bozmayacaktır (`#ifdef __WIN32__` blokları izole tutulacaktır).
   * Test ortamı bulunmayan platformlar (ör. şu an aktif olmayan FreeBSD ortamı) raporda ve `PROJECT_STATE.md` içinde açıkça "test edilmedi/doğrulanmadı" olarak belirtilecek; denenmemiş platformlar asla başarılı ilan edilmeyecektir.
4. **Tekrar Çalıştırılabilir ve Kalıcı Testler:**
   * Testler tek kullanımlık (geçici) olmayacaktır. Modüllere ait küçük, hızlı ve otomatik çalıştırılabilir test kaynakları ve betikleri Git deposunda saklanacaktır.
   * Her düzeltmeden sonra modül temiz ortamda yeniden derlenecek; hem yeni işlevler hem de önceki sistemler regresyon testleriyle doğrulanacaktır.
5. **Dürüst Doğrulama ve Belge Disiplini:**
   * Hiçbir sorun gerçekte test edilmeden "çözüldü" olarak işaretlenemez.
   * Bir aşamada çözülemeyen bir engel varsa sonraki modüle geçilmeyecek, engel ve kök neden şeffaf şekilde kullanıcıya raporlanacaktır.
   * `AGENTS.md` ve `PROJECT_STATE.md` dışında hiçbir yeni Markdown belgesi oluşturulmayacaktır.

---

## 4. KALICI VERİTABANI VE MYSQL / MARIADB UYUMLULUK ANAYASASI

Bu kural seti, projenin veritabanı altyapısı, `libsql` kütüphanesi, `db` ve `game` sunucuları ile SQL şemaları için **kesin ve değişmez standartları** belirler.

1. **Çift Motor Zorunluluğu (MariaDB 12.3 LTS & MySQL 8.4 LTS):**
   * Proje yalnızca iki resmi hedef veritabanı motorunu zorunlu destek ve test matrisinde bulundurur:
     1. **MariaDB 12.3 LTS** — Ana veritabanı.
     2. **MySQL 8.4 LTS** — Desteklenen alternatif.
   * **MariaDB 11.4 LTS** artık zorunlu destek ve CI test matrisinde yer almaz. Yalnızca tek motoru desteklemek kesinlikle yeterli kabul edilmez.
2. **Kapsam:**
   * `libsql` bağlantı/soyutlama katmanı ve tüm ilgili istemci kütüphaneleri.
   * `db` sunucusundaki veritabanı işlemleri, sorgular ve önbellek mekanizmaları.
   * `game` sunucusundaki veritabanı kullanan bütün sistemler (karakter, envanter, lonca, pazar, log vb.).
   * Mevcut tüm SQL dosyaları, tablo şemaları (`sql/*.sql`), indeksler ve başlangıç verileri.
   * Gerekli tüm sunucu kaynak kodları, yapılandırma dosyaları ve veri dönüşüm (migration) mekanizmaları.
3. **SQL Mode Dokunulmazlığı:**
   * SQL mode (`STRICT_TRANS_TABLES`, `NO_ZERO_DATE`, `ONLY_FULL_GROUP_BY` vb.) devre dışı bırakılamaz veya uyumluluk sağlamak adına gevşetilemez/kapatılamaz.
4. **Hata Gizleme ve Kolaycı Yaklaşım Yasağı:**
   * `INSERT IGNORE`, `UPDATE IGNORE` ve benzeri hataları/uyarıları maskeleyen yaklaşımlar çözüm kabul edilemez.
   * SQL ve istemci kütüphanesi hataları sessizce yok sayılamaz (`catch-all` ile yutulamaz).
   * Uyumsuzlukların kök nedenleri kod ve şema seviyesinde kalıcı olarak çözülecektir; geçici hileler ve yamalar yasaktır.
5. **Şeffaf Uyumluluk Katmanı:**
   * MySQL ve MariaDB arasındaki gerçek sözdizimi, protokol veya davranış farkları gerektiğinde açık, temiz, belgelenmiş ve test edilebilir uyumluluk katmanlarıyla çözülecektir.
6. **Veri Tipleri ve Bütünlük Standartları:**
   * Veri tipleri, `PRIMARY KEY`, indeksler, `AUTO_INCREMENT`, `NULL`/`NOT NULL` kuralları, `DEFAULT` değerleri, tarih/saat alanları (`DATETIME`, `TIMESTAMP`), karakter setleri (`utf8mb4`), collation ve transaction (ACID) davranışları titizlikle incelenecek ve standartlaştırılacaktır.
7. **Veri Güvenliği ve Geri Dönüş Güvencesi:**
   * Mevcut tablolar ve kullanıcı verileri korunacaktır.
   * Veri kaybı doğurabilecek yapısal dönüşümler öncesinde doğrulanmış yedek ve geri dönüş (rollback) planı bulunmak zorundadır.
8. **Çift Senaryo Doğrulaması:**
   * Hem sıfırdan temiz kurulum (yeni veritabanı oluşturma) hem de mevcut MT2009 veritabanını yükseltme (migration) senaryoları test edilecektir.
9. **Bütünsel Mimari:**
   * SQL şema dosyaları ile `libsql`, `db` ve `game` C++ kaynak kodları birbirinden kopuk değil, birlikte ve tutarlı olarak geliştirilip değerlendirilecektir.
10. **Çift Platform ve Çift Motor Test Zorunluluğu:**
    * Windows amd64 ve FreeBSD amd64 GCC derleme ve çalışma uyumluluğu korunacaktır.
    * Her iki hedef veritabanı motorunda da (**MariaDB 12.3 LTS** ve **MySQL 8.4 LTS**) gerçek fonksiyonel testler (%100 başarı) geçmeden ilgili aşama asla tamamlanmış sayılamaz.
    * Testlerde veri bütünlüğü, karakter/eşya kayıtları, normal ve kasıtlı hatalı sorgular, bağlantı kopması/yeniden bağlanma (reconnect) ve transaction izolasyon sorunları somut olarak doğrulanacaktır.
11. **Resmî Hedef Sürümler:**
    * Kalıcı hedef veritabanı sürümleri **MariaDB 12.3 LTS** (ana veritabanı) ve **MySQL 8.4 LTS** (desteklenen alternatif) olarak sabitlenmiştir; **MariaDB 11.4 LTS** zorunlu destek ve test matrisinden çıkarılmıştır.

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
   * Her doğrulamada Run ID, commit SHA, iki matris işinin (`MariaDB 12.3 LTS`, `MySQL 8.4 LTS`) gerçek sonucu ve gerektiğinde test loglarındaki assertion sayıları ayrı ayrı kontrol edilecektir. Bir işin genel sonucunun SUCCESS olması, tek başına belirli bir assertion sayısının kanıtı olarak sunulamaz.
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

**Proje:** `D:\mt22009` | **Geliştirme Dalı:** `dev/windows-gcc-baseline` | **Uzak Depo:** `x64bit-source/Metin2-MT2009`

Bu standart, tüm MT2009 geliştirmelerinde Git, GitHub ve CI operasyonlarının tek merkezden, güvenilir ve kanıta dayalı olarak yürütülmesini belirler. Antigravity, tüm işlemleri kendi terminalinden yürütecek; kullanıcıdan normal geliştirme sürecinde manuel PowerShell veya Git yönlendirmesi talep edilmeyecektir.

1. **Görev Başlangıcı:**
   * Mevcut çalışma klasörünün `D:\mt22009` olduğu doğrulanacaktır.
   * Geçerli Git dalı (`git branch --show-current`) ve çalışma ağacı durumu (`git status`) kontrol edilecektir.
   * `AGENTS.md` ve `PROJECT_STATE.md` dosyaları okunarak önceki teknik durum ve açık hedefler doğrulanacaktır.
   * Gerekirse GitHub'daki güncel geliştirme dalıyla senkronizasyon kontrol edilecektir.
   * `main` dalına kesinlikle dokunulmayacak, doğrudan commit veya push yapılmayacaktır.
   * GitHub Desktop için ikinci bir klon veya ayrı çalışma deposu oluşturulmayacaktır.

2. **Geliştirme ve Test Disiplini:**
   * Yalnızca görevin gerektirdiği kaynak dosyaları değiştirilecektir; alakasız dosyalara dokunulmayacaktır.
   * Önceden doğrulanmış test sonuçları gereksiz yere baştan çalıştırılmayacak veya tekrar üretilmeyecektir.
   * Kod değişikliğine uygun hedefli regresyon ve derleme kontrolleri gerçekleştirilecektir.
   * Gerçek binary derlemesi yalnızca nesne dosyalarının (`.o`) oluşmasıyla değil, tam bağlama (link) ile çalıştırılabilir dosyanın üretilmesi ve mimarisinin (PE/ELF64) doğrulanmasıyla geçerli sayılacaktır.
   * Test edilmemiş aşamalar asla başarılı veya tamamlanmış gösterilmeyecektir.
   * Gereksiz tam derleme, tekrarlanan denemeler ve ajan kredisi tüketiminden kaçınılacaktır.

3. **Git ve GitHub Senkronizasyonu:**
   * Anlamlı ve doğrulanmış geliştirme aşaması tamamlandığında:
     1. Gerekli proje belgeleri (`PROJECT_STATE.md`, gerekiyorsa `AGENTS.md`) güncellenecektir.
     2. Değişiklikler `dev/windows-gcc-baseline` dalında commit edilecektir.
     3. Doğrudan `origin dev/windows-gcc-baseline` dalına push yapılacaktır.
     4. Yerel HEAD ile GitHub'daki uzak dalın commit SHA değerlerinin eşleştiği (`git rev-parse HEAD` vs uzak) doğrulanacaktır.
     5. Commit SHA bilgisi sonuç raporunda açıkça bildirilecektir.
   * Tamamlanan işler yalnızca yerel bilgisayarda bırakılmayacaktır.
   * Normal koşullarda kullanıcıdan manuel Git veya PowerShell işlemleri talep edilmeyecektir.

4. **GitHub Actions ve FreeBSD CI Yönetimi:**
   * GitHub CLI (`gh`) işlemleri Antigravity tarafından kendi terminali üzerinden yürütülecektir.
   * Mevcut yetkilendirilmiş `gh` oturumu kullanılacaktır.
   * Yeni push ile ilişkili CI Run ID değeri tam SHA üzerinden tespit edilecektir.
   * CI tamamlanmışsa iki FreeBSD işinin (`MariaDB 12.3 LTS`, `MySQL 8.4 LTS`) gerçek durumu ayrı ayrı kontrol edilecektir.
   * Başarısız işlerde gerçek derleme, link veya test hataları iş günlüklerinden detaylı incelenecek; genel `ssh exited with code 2` mesajı tek başına kök neden olarak sunulmayacaktır.
   * CI henüz çalışıyorsa durum `RUNNING` olarak bildirilecek ve durulacaktır.
   * Sürekli polling, sonsuz bekleme döngüsü veya otomatik tekrar çalıştırma kesinlikle yapılmayacaktır.
   * Sonraki görevde ilgili Run ID üzerinden kontrole devam edilebilecek bilgiler `PROJECT_STATE.md` içinde ve raporda korunacaktır.
   * Antigravity görevi sona erdikten sonra CI'yi kendiliğinden takip edebildiği varsayılmayacaktır.
   * GitHub erişimi engellenirse token çıkarma, kimlik doğrulama güvenliğini aşma veya sonsuz deneme yapılmayacak; gerçek engel kullanıcıya tek seferde raporlanacaktır.

5. **Proje Hafızası ve Belge Disiplini:**
   * `AGENTS.md` kalıcı çalışma kurallarını, `PROJECT_STATE.md` ise doğrulanmış teknik ilerlemeyi içerecektir.
   * `PROJECT_STATE.md` içinde özellikle şunlar eksiksiz korunacaktır:
     * Son doğrulanmış commit SHA
     * Son CI Run ID ve doğrulanmış durumu
     * Başarılı ve başarısız test aşamaları
     * Açık teknik sorunlar
     * Sonraki geliştirme adımı
   * Belge güncellemeleri mümkün olduğunca ilgili geliştirme commit'ine dahil edilecektir. Sırf belge güncellemek için gereksiz CI döngüleri oluşturulmayacaktır.

6. **Son Rapor Standardı:**
   * Her tamamlanan geliştirme görevinde kısa ve kanıta dayalı rapor sunulacaktır:
     * Yapılan değişiklik
     * Çalıştırılan testler ve gerçek sonuçları
     * Commit ve push durumu
     * Yerel ve uzak SHA eşleşmesi
     * CI Run ID ve iki FreeBSD işinin (`MariaDB 12.3 LTS`, `MySQL 8.4 LTS`) gerçek durumu
     * Devam eden veya çözülemeyen sorunlar
   * Başarı kanıtı bulunmayan hiçbir aşama tamamlandı ilan edilmeyecektir.

---

## 7. KALICI SERVER EXTERN SÜRÜM YÜKSELTME ANAYASASI

Server ve Common Extern bağımlılıklarının en güncel kararlı resmî sürümlerine yükseltilmesi projenin kalıcı ve zorunlu hedefidir.

1. **Öncelikli Bileşenler:**
   * Lua (5.0'dan güvenli yükseltme), Crypto++, Boost, DevIL (IL/ILU/ILUT), RapidJSON, fmt, zlib, OpenSSL, MySQL/MariaDB istemci kütüphaneleri ve Server'ın kullandığı diğer harici bileşenler.
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

## 8. İSTEMCİ KAYNAK VE SDK DOKUNULMAZLIĞI (CLIENT STRICT EMBARGO)

Kullanıcı açıkça yeni bir İstemci Modernizasyonu aşaması başlatana kadar:
1. `client/src/` altındaki hiçbir dosya değiştirilemez.
2. `client/files/` altındaki hiçbir dosya değiştirilemez.
3. `extern/client/` altındaki hiçbir başlık veya kütüphane değiştirilemez.
4. Client x64 derlemesi veya SDK (Granny, Miles Sound, SpeedTree, DirectX, Python) yükseltmesi başlatılamaz.
5. İstemci kaynakları yalnızca 32-bit ağ protokolü ve veri yapıları için **salt-okunur (read-only) referans** olarak kullanılır.

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

## 10. SÜREKLİ VE OTONOM GELİŞTİRME PROTOKOLÜ (NO REDUNDANT PROMPTS)

1. **Gereksiz Prompt Bekleme Yasağı:**
   * Her küçük görev sonunda kullanıcıdan yeni ve uzun prompt istenmeyecektir.
   * Tamamlanan iş kapatılacak; `PROJECT_STATE.md` güncellenecek; sıradaki somut ve güvenli adıma doğrudan kurallara dayanarak devam edilecektir.
2. **Tek Doğruluk Kaynağı:**
   * `PROJECT_STATE.md` gerçekleşen işler, açık engeller, aktif aşama, ortam commit'leri ve test sonuçları için tek güncel durum belgesidir.
3. **Kalıcı Kural Dokunulmazlığı:**
   * Kurallar her oturumda yeniden keşfedilmeyecek; `AGENTS.md` değişmez anayasa olarak uygulanacaktır.

---

## 11. KALICI GELİŞTİRME YOL HARİTASI VE ÇALIŞMA SIRASI

Bu sıra, projenin modernizasyonunda takip edilecek değişmez operasyonel iş akışını belirler:

1. **Mevcut FreeBSD x64 game/db Başarısını Koru:**
   * Doğrulanmış ELF64 `game` ve `db` derleme, bağlama ve wire protokolü başarısı her aşamada korunacak; tamamlanmış adımlar tekrarlanmayacaktır.
2. **CMake + Ninja Derleme Altyapısı:**
   * Windows MinGW-w64 GCC 16.2 UCRT ve FreeBSD 15.1 amd64 GCC 14 için birleşik, tek merkezî `CMakeLists.txt` ve Ninja derleme mimarisi kurulacaktır.
3. **CMake ile Gerçek İkili ve Test Doğrulaması:**
   * CMake üzerinden FreeBSD ortamında gerçek `game` ve `db` ELF64 binary'leri, Windows tarafında ise sunucu kütüphaneleri ve regresyon testleri doğrulanacaktır.
4. **Eski Makefile Eşdeğerlik Güvencesi:**
   * Mevcut Makefile sistemleri, CMake altyapısının %100 eşdeğerliği ve başarısı kanıtlanıncaya kadar depoda korunacaktır; aceleyle silinmeyecektir.
5. **Aşamalı Server/Common Extern Sürüm Yükseltmesi:**
   * Server ve Common Extern bağımlılıkları (Crypto++, Boost, DevIL, Lua, RapidJSON, fmt vb.) CMake altyapısı üzerinden en son doğrulanmış resmi kararlı sürümlere adım adım yükseltilecektir.
6. **Bütünsel Test ve Çoklu Ortam Doğrulaması:**
   * Her aşamada ilgili birim, entegrasyon, canlı SQL ve wire protokolü testleri çalıştırılacak; platform bağımlılıkları ve Git/CI senkronizasyonu kanıtlanacaktır.

