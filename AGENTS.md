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

1. **Çift Motor Zorunluluğu (MySQL & MariaDB):**
   * Proje, mevcut en güncel kararlı **MySQL** ve **MariaDB** sürümlerinin **HER İKİSİNİ DE** gerçek anlamda ve eksiksiz destekleyecektir. Yalnızca birini desteklemek kesinlikle yeterli kabul edilmez.
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
    * Her iki veritabanı motorunda da (en güncel kararlı MySQL ve en güncel kararlı MariaDB) gerçek fonksiyonel testler (%100 başarı) geçmeden ilgili aşama asla tamamlanmış sayılamaz.
    * Testlerde veri bütünlüğü, karakter/eşya kayıtları, normal ve kasıtlı hatalı sorgular, bağlantı kopması/yeniden bağlanma (reconnect) ve transaction izolasyon sorunları somut olarak doğrulanacaktır.
11. **Resmî Hedef Sürümler:**
    * Güncel MySQL ve MariaDB hedef sürümleri, ilgili geliştirme aşamasına gelindiğinde resmî kaynaklardan (Oracle MySQL ve MariaDB Foundation) tespit edilerek `PROJECT_STATE.md` içine işlenecektir.

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
   * Her doğrulamada Run ID, commit SHA, üç matris işinin gerçek sonucu ve gerektiğinde test loglarındaki assertion sayıları ayrı ayrı kontrol edilecektir. Bir işin genel sonucunun SUCCESS olması, tek başına belirli bir assertion sayısının kanıtı olarak sunulamaz.
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
