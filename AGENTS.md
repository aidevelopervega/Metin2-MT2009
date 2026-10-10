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
4. **Dal (Branch) Yönetimi:**
   * `main` dalı doğrudan değiştirilmeyecektir. Yeni geliştirmeler, modernizasyonlar ve CMake geçişi ayrı özellik dallarında (`feature/...`) yürütülecektir.
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

