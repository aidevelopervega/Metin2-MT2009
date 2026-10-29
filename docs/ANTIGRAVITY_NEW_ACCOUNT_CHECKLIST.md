# MT2009 — Yeni Antigravity Hesabı İlk Açılış Denetim Listesi

> Bu liste yeni Google Antigravity hesabının **ilk oturumda, hiçbir dosyayı değiştirmeden** yapacağı denetimleri ve her birinin başarı şartını tanımlar.
> Kuralların tek kaynağı `AGENTS.md`'dir. Arka plan bilgisi: [`ANTIGRAVITY_ACCOUNT_HANDOFF.md`](ANTIGRAVITY_ACCOUNT_HANDOFF.md).
> Komutları Antigravity ajanı kendisi çalıştırır; kullanıcıya terminal görevi verilmez.

## Genel Kurallar

- Bölüm 1–5 **salt-okunurdur**: commit, push, reset, clean, checkout, dosya silme, veritabanı silme, CI tetikleme **yapılmaz**.
- Hiçbir parola, token, cookie veya anahtar ekrana basılmaz, belgeye yazılmaz. `gh auth token`, `git credential fill` yasaktır.
- Bir madde başarısızsa sonraki geliştirme işine geçilmez; sonuç kullanıcıya sade Türkçe ile `PASS` / `FAIL` / `BLOCKED` / `UNKNOWN` olarak bildirilir.
- Ağ gerektiren komutlar (`git ls-remote`, `git fetch`, `gh …`) Antigravity sandbox'ında `github.com` çözümlenemediği için kullanıcı onaylı sandbox dışı çalıştırma isteyebilir. Erişim yoksa sonuç **BLOCKED** yazılır, tahmin edilmez.

---

## 1. Hesap ve Workspace

| # | Denetim | Başarı şartı |
| :--- | :--- | :--- |
| 1.1 | Açık workspace | `D:\mt22009` |
| 1.2 | Etkin kurallar | `D:\mt22009\AGENTS.md` workspace kuralı olarak yüklü; ajan §1–§12 başlıklarını özetleyebiliyor |
| 1.3 | Global kural çakışması | `~/.gemini/AGENTS.md`, `~/.gemini/GEMINI.md`, `~/.gemini/config/rules/` yok **veya** MT2009 kurallarıyla çelişmiyor |
| 1.4 | MCP / hooks / plugins | Yeni hesapta eklenen MCP sunucusu, hook veya plugin varsa listelenmiş ve projeye yan etkisi değerlendirilmiş |
| 1.5 | Araç izinleri | Terminal otomatik çalıştırma ve workspace dışı dosya erişimi kullanıcı tarafından bilinçli ayarlanmış (ajan yalnızca durumu raporlar) |
| 1.6 | Eski hesap verisi | Eski hesabın sohbet/oturum dosyalarına dayanılmıyor; gereken bilgi yalnızca depodaki belgelerden okunuyor |

## 2. Belgeler

| # | Denetim | Başarı şartı |
| :--- | :--- | :--- |
| 2.1 | Okuma | `AGENTS.md`, `PROJECT_STATE.md` (§1, §7, §9), `docs/ANTIGRAVITY_ACCOUNT_HANDOFF.md`, `docs/SERVER_X64_GOALS.md`, `docs/CLIENT_X64_PROTOCOL_MIGRATION.md` okundu |
| 2.2 | Açık hedefler | Ajan `H-1` … `H-9` açık hedeflerini (handoff §9) eksiksiz sayabiliyor |
| 2.3 | Sabitler | `MT2_PROTOCOL_VERSION_X64 = 0x20090064` (`server/src/common/tables.h`), `GOLD_MAX = 1000000000000LL` (`server/src/common/length.h`) kaynakta mevcut |

## 3. Git ve GitHub

| # | Komut | Başarı şartı |
| :--- | :--- | :--- |
| 3.1 | `git branch --show-current` | `dev/windows-gcc-baseline` |
| 3.2 | `git status --porcelain=v1 --untracked-files=all` | Boş çıktı (ya da önceden var olan farklar açıkça listelenip **dokunulmadan** raporlandı) |
| 3.3 | `git rev-parse HEAD` | Kaydedildi |
| 3.4 | `git ls-remote --exit-code origin refs/heads/dev/windows-gcc-baseline` | Çıkış kodu `0` ve SHA = 3.3 |
| 3.5 | `git fetch --no-tags origin dev/windows-gcc-baseline` + `git rev-list --left-right --count HEAD...origin/dev/windows-gcc-baseline` | `0 0` |
| 3.6 | `git remote -v` (kimlik bilgisi maskelenerek) | `https://github.com/aidevelopervega/Metin2-MT2009.git` |
| 3.7 | `git config --get core.hooksPath` | `D:/mt22009/.git/hooks` ve içinde `pre-commit`, `pre-push` mevcut |
| 3.8 | `gh --version`, `gh auth status --hostname github.com` | Oturum açık, `repo` kapsamı var; token **gösterilmedi** |
| 3.9 | `gh run list --repo aidevelopervega/Metin2-MT2009 --branch dev/windows-gcc-baseline --limit 3` (bir kez) | Sonuç olduğu gibi raporlandı; tam `headSha` karşılaştırıldı; polling yok |
| 3.10 | `git diff origin/dev/windows-gcc-baseline --stat -- client extern/client ci/run-full-freebsd.txt` | Boş |

## 4. Yerel Araç Zinciri

| # | Komut | Başarı şartı |
| :--- | :--- | :--- |
| 4.1 | `gcc --version` | `16.2.0` (MinGW-W64 x86_64-ucrt-posix-seh) |
| 4.2 | `cmake --version`, `ninja --version` | `4.4.2`, `1.13.2` (daha yeni sürüm farkı raporlanır, kurulum değiştirilmez) |
| 4.3 | `python --version` | `3.11.x` |
| 4.4 | `git --version`, `git lfs version` | Git `2.55.x`, LFS kurulu |
| 4.5 | `D:\mt22009\scratch\handoff\isolated_db\mariadb-12.3.3-winx64\bin\mariadbd.exe --version` | `Ver 12.3.3-MariaDB for Win64` |
| 4.6 | `Test-Path D:\mt22009\server\files\share\bin\game_r41023.exe` ve `db_r41023.exe` | `True` |
| 4.7 | `Test-Path D:\mt22009\build\windows-gcc-ucrt\tests\test_live_db.exe` | `True` (yoksa Bölüm 6'da derleme gerekir) |
| 4.8 | `CMakePresets.json` | `windows-gcc-ucrt` ve `freebsd-gcc14` presetleri mevcut |

## 5. Güvenlik ve Gizlilik

| # | Denetim | Başarı şartı |
| :--- | :--- | :--- |
| 5.1 | `git ls-files` içinde `serverfiles.env`, `.env`, `*.cnf` (example hariç), `kurulum.py`, `push_to_github.bat`, `scratch/` | Hiçbiri takipte değil |
| 5.2 | `git check-ignore -v scratch/handoff/session_scripts_original/run_g2_g5_kademe_c.ps1` | `.gitignore` `scratch/` kuralı eşleşiyor |
| 5.3 | Depo görünürlüğü | `gh repo view aidevelopervega/Metin2-MT2009 --json visibility` → `PUBLIC` olduğu bilinerek sır denetimi yapılıyor |
| 5.4 | `.ssh` | Okunmadı; Git HTTPS ile çalıştığı doğrulandı |

---

## 6. İlk Yazma İzni Gerektiren Doğrulamalar (Bölüm 1–5 PASS olduktan sonra)

Bu bölüm yeni hesabın referans test değerlerini **kendi** kanıtıyla yeniden üretmesi içindir. Kaynak kod değiştirmez; yalnızca derleme çıktısı ve test veritabanı üretir.

| # | İş | Başarı şartı |
| :--- | :--- | :--- |
| 6.1 | `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_limited.ps1 -Build` | Çıkış kodu `0`, `0` hata, `0` uyarı; rapor `CPU Hard-Cap ACTIVE %50`, affinity `0xFC0`, öncelik `0x4000` gösteriyor |
| 6.2 | `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_limited.ps1 -CTest` | `10/10` (`test_packet_wire 123/123`, `test_db_cache 129/129`, `test_db_cache_integration 162/162` çevrimdışı) |
| 6.3 | `server\src\game\game_r41023.exe --check-lua` | `11/11` |
| 6.4 | `python tests/run_quest_matrix.py` | `202/202` |
| 6.5 | Kademe C (izole MariaDB 12.3.3, port `3307`, yalnızca test veri dizini) | `test_live_db 354/354`, `--test-itemshop 15/15`, `test_db_cache_integration 187/187`, idempotent migration `PASS`, `--protocol-framing-audit 10/10`, serverfiles `0 syserr` |

> [!IMPORTANT]
> 6.5 şu an yalnızca `scratch\handoff\session_scripts_original\` içindeki eski oturum betikleriyle çalışabilir ve bu betikler eski oturum yoluna bağlıdır. Doğru yol, açık hedef **H-1** kapsamında sırsız ve parametreli bir `tests/` betiği hazırlamak, onu Windows'ta çalıştırıp 6.5 değerlerini üretmek ve ardından commit etmektir. Bu yapılmadan 6.5 değerleri yeni hesapta "doğrulandı" sayılamaz.

## 7. Yeni Hesap Hazır Sayılma Şartı

Yeni hesap ancak şu durumda "devralındı" kabul edilir:

1. Bölüm 1–5'in tamamı `PASS` (ağ erişimi yoksa ilgili maddeler `BLOCKED` olarak açıkça yazılmış).
2. Bölüm 6.1–6.4 yeni hesapta yeniden çalıştırılıp aynı sonuçlar alınmış.
3. 6.5 için H-1 tamamlanmış veya `BLOCKED` gerekçesi kullanıcıya bildirilmiş.
4. Sonuçlar `PROJECT_STATE.md` §9 içine yeni hesabın kanıtı olarak işlenmiş.
5. FreeBSD durumu dürüstçe `UNVERIFIED` (ve son CI `40182578376 FAILURE`) olarak korunmuş.
