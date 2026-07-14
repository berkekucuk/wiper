## Proje Nedir?

`wiper`, Windows çalışırken çift tıklanabilen ve sistem yeniden başladığında
hedef diski **otomatik olarak** silen bir araçtır. Kullanıcıdan herhangi bir
ek giriş beklenmez — her şey programın kendisi tarafından yönetilir.

İki modu vardır:

- **PartitionWipe** — belirli bir partition'ı siler (varsayılan)
- **FullDiskWipe** — tüm fiziksel diski sıfırlar (işletim sistemi dahil)

---

## Neden Böyle Bir Şeye İhtiyaç Var?

Bir bilgisayarın diskini tamamen silmek istediğini düşün.
Bunu **Windows açıkken yapamazsın:**

- Windows kendi dosyalarını RAM'e yüklemiş halde çalışıyor
- C: sürücüsündeki dosyalar işletim sistemi tarafından kilitli
- `format C:` komutunu denersen Windows reddeder

Çözüm: **sistemi başka bir ortamdan başlatmak** ve ana diski o ortamdan silmek.

---

## Çözüm Alternatifleri

### Linux Live USB

USB'den bir Linux başlatıp `shred` veya `dd` ile disk silinebilir.

**Sorunlar:** Kullanıcı USB hazırlamak zorunda, boot sırasında manuel seçim
yapması gerekir, yazılım tarafından otomatikleştirilemez.

### WinPE (Windows Preinstallation Environment)

Microsoft'un geliştirici araçlarıyla oluşturulan minimal bir Windows ortamıdır.
Windows kurulumu ve disk hazırlama için kullanılır.

**Sorunlar:** Windows ADK (~2 GB) indirilip ISO oluşturulması gerekir.
Kullanıcı tarafından kendiniz oluşturulduğu için Secure Boot imzası yoktur.
Yine manuel müdahale gerektirir.

### WinRE (Windows Recovery Environment) — Seçilen Yöntem

WinRE, her Windows kurulumunun içine **gömülü** olarak gelen mini bir kurtarma
işletim sistemidir. Disk üzerinde gizli bir partition'da `winre.wim` adıyla
saklanır. Sistem açılırken sorun çıkarsa veya "Gelişmiş Başlangıç Seçenekleri"
menüsünden girilirse, Windows bu dosyayı RAM'e yükler ve oradan çalıştırır.

**WinRE neden seçildi:**

| | Linux USB | WinPE | WinRE |
|---|---|---|---|
| Kullanıcı hazırlığı | ✅ USB gerekir | ✅ ADK gerekir | ❌ Zaten kurulu |
| Secure Boot uyumlu | ❌ | ❌ | ✅ MS imzalı |
| Yazılımdan tetiklenebilir | ❌ | ❌ | ✅ `reagentc /boottore` |
| Kullanıcı müdahalesi | ✅ Gerekir | ✅ Gerekir | ❌ Gerekmez |

WinRE her makinede hazır, Microsoft tarafından imzalı ve yazılım üzerinden
tetiklenebilir. Bu nedenle tek tıkla çalışan bir araç için en uygun seçenek.

---

## Nasıl Çalışır?

`wiper.exe` Windows'ta yönetici (Administrator) olarak çalışır ve
şu adımları izler:

### Windows'ta (EXE çalışırken)

**[1/5] WinRE'yi bul**

`reagentc /info` komutuyla `winre.wim` dosyasının diskteki tam yolu bulunur.

**[2/5] Disk tipini tespit et** 

Windows IOCTL API'si üzerinden hedef diskin HDD mi, SATA SSD mi,
yoksa NVMe SSD mi olduğu anlaşılır. Bu bilgi, uygun silme komutunu
seçmek için kullanılır.

**[3/5] WinRE imajını yamala**

`winre.wim` DISM ile geçici bir klasöre açılır (mount edilir). İçine
silme scriptleri gömülür ve WinRE'nin başlangıç ayar dosyası
(`winpeshl.ini`) değiştirilerek açılışta doğrudan bu scriptlerin
çalışması sağlanır. Ardından değişiklikler `winre.wim`'e geri yazılır.

**[4/5] Geri sayım**

10 saniye geri sayım başlar. Bu sürede herhangi bir tuşa basılırsa
işlem iptal edilir.

**[5/5] Yeniden başlat**

`reagentc /boottore` ile BCD'ye "bir sonraki açılışta WinRE'ye gir"
bayrağı eklenir. Ardından sistem yeniden başlatılır.

---

### WinRE'de (sistem yeniden başlayınca)

WinRE `winre.wim`'den RAM'e yüklenir. `winpeshl.ini` okunur ve
bizim script dosyamız çalıştırılır:

---

## Silme Modları

`main.cpp` içindeki tek satırla mod değiştirilir:

```cpp
constexpr WinREPatcher::WipeMode kWipeMode = WinREPatcher::WipeMode::PartitionWipe;  // varsayılan
// veya:
constexpr WinREPatcher::WipeMode kWipeMode = WinREPatcher::WipeMode::FullDiskWipe;
```

| | PartitionWipe | FullDiskWipe |
|---|---|---|
| Hedef | Tek partition | Tüm fiziksel disk |
| Komut | `format /P:1 (HDD)` / `format /Q (SDD)` | `clean all` (HDD) / `clean`+TRIM (SSD) |
| Disk tipi tespiti | ✅ | ✅  |
| MBR/GPT silinir mi | ❌ | ✅ |
| Sistem bitince | Reboots → Windows açılır | Shutdown → sistem kapanır |

**PartitionWipe:**

**HDD İçin (Rastgele Veri ile Üzerine Yazma):**
Sadece hedeflenen bölüme rastgele veri yazarak güvenli silme sağlar.
1. `diskpart` → Diskpart aracı açılır, `select disk 0`, `select partition 5`, `assign letter=Z` komutları ile hedef partition seçilip geçici bir sürücü harfi (Z:) atanır, ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /P:1 /Y` → Partition'daki tüm alanın üzerine 1 tur sıfır, ardından 1 tur rastgele veri yazılarak eski veriler yok edilir.
3. `diskpart` → Diskpart tekrar açılır, `select disk 0`, `select partition 5`, `remove letter=Z` komutları ile geçici Z: harfi kaldırılır, ardından `exit` ile çıkılır.
4. `wpeutil reboot` → İşletim sistemi zarar görmediği için sistem yeniden başlatılarak normal Windows'a dönülür.

**SSD İçin (Hızlı Format ve TRIM):**
SSD ömrünü (TBW) tüketmemek ve wear-leveling sorunlarına takılmamak için rastgele veri yerine donanımsal temizlik kullanılır.
1. `diskpart` → Diskpart aracı açılır, `select disk 0`, `select partition 5`, `assign letter=Z` komutları ile hedef partition seçilip geçici bir sürücü harfi (Z:) atanır, ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /Q /Y` → Hızlı format (Quick Format) uygulanarak dosya tablosu saniyeler içinde sıfırlanır. (Not: Modern Windows/WinRE ortamlarında NTFS hızlı formatı, arka planda otomatik olarak TRIM/UNMAP sinyali de gönderir.)
3. `defrag Z: /L` → (Güvenlik Ağı) WinRE'nin minimal yapısında otomatik TRIM'in kaçırılma ihtimaline karşı, Windows'un yerleşik aracıyla TRIM/UNMAP komutu açıkça ve zorunlu olarak tekrar tetiklenir. Bu, SSD kontrolcüsüne blokların boş olduğunu kesin olarak bildirir.
4. `diskpart` → Diskpart tekrar açılır, `select disk 0`, `select partition 5`, `remove letter=Z` komutları ile geçici Z: harfi kaldırılır, ardından `exit` ile çıkılır.
5. `wpeutil reboot` → Sistem yeniden başlatılarak normal Windows'a dönülür.

---

**FullDiskWipe:**

**HDD İçin Seçilen Yöntem: `clean all` (NIST 800-88 Standardı)**
1. `diskpart` → Diskpart aracı açılır, `select disk 0`, `clean all` komutları ile disk tamamen sıfırlanır; ardından `exit` ile CMD ortamına dönülür.
2. `wpeutil shutdown` → İşletim sistemi kalmadığı için sistem tamamen kapatılır (reboot anlamsızdır).
(Neden eski DoD standartları gibi rastgele veri yazılmadı? Günümüz yüksek yoğunluklu HDD'lerinde tek geçiş sıfır yazmak veriyi donanımsal olarak (mikroskopla bile) kurtarılamaz hale getirdiği için güncel NIST standartlarında yeterli kabul edilir.)

**HDD İçin Alternatif Teori: Dinamik Bölümleme ve Toplu Format**
(Karşılaştırma amacıyla belgelenmiştir, projede kullanılmaz)
Eğer sıfır yerine mutlaka rastgele veri yazılmak istenseydi mimari şu olurdu:
1. `diskpart` → Diskpart aracı açılır, `select disk 0`, `clean`, `create partition primary`, `assign letter=Z` komutları ile disk RAW hale getirilip geçici bir sürücü harfi (Z:) atanır; ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /P:1 /Y` → Tüm diske 1 tur sıfır + 1 tur rastgele veri yazılır.
3. `diskpart` → Diskpart tekrar açılır, `select disk 0`, `clean` komutuyla geçici partition tablosu silinir ve disk tekrar ham (RAW) bırakılır; ardından `exit` ile çıkılır.
4. `wpeutil shutdown` → Sistem tamamen kapatılır.
Ancak bu mimari (limitler ve partition hataları nedeniyle) çok kırılgandır ve modern HDD'lerde gereksizdir. Bu yüzden stabil olan clean all seçilmiştir.

**SSD İçin Seçilen Yöntem: Partition Oluştur, Hızlı Formatla (TRIM Tetikle) ve Temizle.**
SSD'lerde `clean all` (sıfır yazmak) veya `format /P:1` (rastgele veri yazmak) iki büyük soruna yol açar:
- **Ömür Tüketimi (TBW):** Terabaytlarca veriyi boş yere yazmak SSD bellek hücrelerinin ömrünü ciddi şekilde tüketir.
- **Güvenlik İllüzyonu (Wear-Leveling):** Siz tüm sektörlere sıfır yazdığınızı sansanız da SSD kontrolcüsü bunu diske dağıtır ve eski verinin bir kısmı gizli yedek alanlarda fiziksel olarak kalabilir.

Bu yüzden SSD'ler için en doğru yazılımsal yaklaşım donanımın kendi mekanizmasını kullanmaktır:
1. `diskpart` → Diskpart aracı açılır, `select disk 0`, `clean`, `create partition primary`, `assign letter=Z` komutları ile disk RAW hale getirilip geçici bir sürücü harfi (Z:) atanır; ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /Q /Y` → Hızlı format uygulanır. (Not: Modern Windows/WinRE ortamlarında NTFS hızlı formatı, arka planda otomatik olarak TRIM/UNMAP sinyali de gönderir.)
3. `defrag Z: /L` → (Güvenlik Ağı) PartitionWipe moduyla aynı felsefeyle, WinRE'nin minimal yapısında otomatik TRIM'in kaçırılma ihtimaline karşı TRIM/UNMAP komutu açıkça tekrar tetiklenir. SSD kontrolcüsü bu sinyali alınca tüm NAND hücrelerini donanımsal olarak sıfırlar. Bu işlem saniyeler sürer ve SSD ömrünü (TBW) tüketmez.
4. `diskpart` → Diskpart tekrar açılır, `select disk 0`, `clean` komutuyla geçici partition tablosu silinir ve disk tekrar ham (RAW) bırakılır; ardından `exit` ile çıkılır.
5. `wpeutil shutdown` → Sistem tamamen kapatılır.

---

### `clean all` Askeri Düzeyde Güvenlik Sağlar mı?

`diskpart clean all` komutu diskin başından sonuna kadar her sektöre sıfır (0x00) yazar. Bunun güvenliği disk tipine göre değişir:

**HDD (Manyetik Disk) İçin: ÇOK GÜVENLİ**
Manyetik plakalardaki her sektör fiziksel olarak sıfırlanır. Modern yüksek yoğunluklu disklerde tek bir geçiş (single pass) sıfır yazmak, veriyi kurtarılamaz hale getirmek için yeterlidir. Askeri (DoD) standartları bile modern HDD'ler için bunu yeterli bulur.

**SSD (NVMe / SATA) İçin: GÜNLÜK KULLANIM İÇİN YETERLİ, ASKERİ DÜZEY DEĞİL**
SSD kontrolcüleri, NAND bellek ömrünü uzatmak için yazma işlemlerini sürekli farklı fiziksel hücrelere dağıtır (Wear Leveling) ve görünmeyen gizli yedek alanlar barındırır (Over-provisioning).
Siz `clean all` ile sıfır yazsanız bile, SSD bunu farklı hücrelere yazabilir ve eski verilerinizin bir kısmı o gizli yedek alanlarda veya henüz "çöp toplaması" (garbage collection) yapılmamış bloklarda fiziksel olarak kalabilir.
Sıradan veri kurtarma yazılımları bu veriyi göremez, günlük kullanım için güvenlidir. Ancak diskin donanım seviyesinde incelenebileceği askeri standartlar (NIST 800-88) için yeterli değildir (Bunun için ATA Secure Erase veya NVMe Format komutları şarttır).

### Alternatif Silme Araçları (format vs cipher vs sdelete vs clean all)

Windows ekosisteminde bilinen farklı güvenli silme araçları vardır. Bunların kullanım amaçları ve çalıştıkları seviyeler birbirinden farklıdır:

**Partition (Sürücü Harfi) Seviyesinde Çalışanlar:**
1. **`format /P:N` (Partition'ı Yok Etmek İçin)**: Tüm partition'ı içindeki her şeyle birlikte yok eder ve üzerine rastgele veri yazarak yeni boş bir dosya sistemi kurar. Projemizdeki `PartitionWipe` modunda tam olarak bu yerleşik araç kullanılmıştır.
2. **`cipher /w` (Sadece Boş Alanı Temizlemek İçin)**: Partition'ı yok etmez, mevcut dosyalara dokunmaz. Yalnızca daha önceden silinmiş dosyaların izini kaybettirmek için "boş alanı" güvenli şekilde siler. Amacımız her şeyi (işletim sistemi dahil) yok etmek olduğu için projemize uygun değildir.
3. **`sdelete` (İsviçre Çakısı)**: Sysinternals aracıdır. Hem `cipher`'ın boş alan temizleme işini hem de tek tek dosya/klasör silme işini yapar. Ancak Windows'ta yerleşik (built-in) değildir. WinRE'ye `.exe` olarak dışarıdan gömülmesi gerekir (ekstra bağımlılık). Üstelik o da yazılım tabanlı olduğu için SSD'lerdeki wear-leveling (aşınma dengeleme) kısıtlamalarına takılır.

**Fiziksel Disk Seviyesinde Çalışanlar:**
4. **`diskpart clean all` (Her Şeyi Sıfırlarla Doldurmak İçin)**: Diğer üçünün aksine sürücü harfleriyle veya dosya sistemleriyle ilgilenmez. Doğrudan donanımsal fiziksel diske (Disk 0) odaklanır. İşletim sistemi, MBR, GPT, EFI ve Kurtarma bölümleri dahil ne var ne yoksa tek hamlede sıfırlar. Sadece "0" yazar, rastgele veri yazmaz.

Sonuç olarak projemizde sıfır bağımlılık ilkesiyle; sadece belirli bir bölümü silmek için `format` (PartitionWipe), işletim sistemi dahil diskin tamamını silmek için ise yerleşik ve en stabil araç olan `clean all` (HDD) ve `clean`+TRIM (SSD) kullanılmıştır.
---

## Teknik Kararlar ve Trade-off'lar

### Neden startnet.cmd değil winpeshl.ini?

`winpeshl.ini` WinRE'nin gerçek başlangıç noktasıdır. `startnet.cmd` ise
sadece WinPE'de çalışır — WinRE'de `winpeshl.ini` onu bypass eder.
Bu fark deneme yanılmayla keşfedildi.

### Neden ATA Secure Erase kullanılmadı?

SATA SSD'ler için en güvenilir yöntemdir — firmware tüm hücreleri sıfırlar.
Fakat çoğu BIOS önyükleme öncesi güvenlik komutu olarak diski "dondurur"
(frozen state). Donmuş diske Secure Erase gönderilemez; çözümek için
üretici aracı (Samsung Magician vb.) veya uyku/devam döngüsü gerekir.
Her ortamda güvenilir çalışmadığı için tercih edilmedi.

### Neden NVMe Format komutu kullanılmadı?

NVMe Format (Admin opcode 0x80) kriptografik sıfırlama yapar ve çok hızlıdır.
Fakat WinRE içinden göndermek `IOCTL_STORAGE_PROTOCOL_COMMAND` kullanan
ayrı bir C++ binary gerektirir. Bu, projeye ikinci bir alt proje eklemek
demektir. Karmaşıklığa değmedi.

### Neden format tüm disk için kullanılmadı?

`format` komutu yalnızca partition seviyesinde çalışır — tüm disk için her
partition'ı tek tek formatlamak gerekir ve MBR/GPT sektörlerine dokunamaz.
Dinamik bölümleme ile etrafından dolaşmak (tek devasa partition açmak) mümkün olsa da hata payı yüksektir.
Bu yüzden tüm disk için `diskpart clean all` çok daha stabil, güncel standartlara (NIST) uygun ve temiz bir çözümdür.

### Neden diskpart format yerine CMD format kullanıldı?

`diskpart` içindeki `format` komutu `/P:N` (rastgele veri) parametresini
desteklemez. Windows'un `format.exe` aracı destekler. Bu yüzden
partition seçimi diskpart ile, silme ise format.exe ile yapıldı.

---

## Kalıcı ve Geçici Değişiklikler

| Değişiklik | Kalıcı mı? |
|------------|-----------|
| `winre.wim` içindeki scriptler | ✅ Kalıcı |
| Silinen veri | ✅ Kalıcı — geri alınamaz |
| BCD bootsequence | ❌ 1 boot sonra otomatik silinir |
| DISM mount klasörü | ❌ Commit sonrası silinir |

---

## Konfigürasyon

`src/main.cpp` içindeki üç sabiti düzenle:

```cpp
// Silme modu
constexpr WinREPatcher::WipeMode kWipeMode = WinREPatcher::WipeMode::PartitionWipe;
// Alternatif: WipeMode::FullDiskWipe (tüm diski sıfırlar)

// Hedef disk numarası (diskpart: 'list disk')
constexpr int kWipeDiskNumber = 0;

// Hedef partition numarası — sadece PartitionWipe modunda kullanılır
// (diskpart: 'select disk N' → 'list partition')
constexpr int kWipePartitionNumber = 5;
```

### Disk ve Partition Numarasını Bulmak

```
diskpart
list disk          → disk numarasını öğren
select disk 0
list partition     → partition numarasını öğren
exit
```

---

## DISM Mount Temizliği

DISM işlemi yarıda kesilirse mount klasörü geride kalabilir.
Bunu temizlemek için:

```sh
dism /Cleanup-Mountpoints
```
## Gereksinimler

### Çalışma Ortamı

| Gereksinim | Detay |
|------------|-------|
| **İşletim Sistemi** | Windows 10 / 11 |
| **Yetki** | Administrator (Yönetici) |
| **WinRE** | Etkin olmalı (`reagentc /info` ile kontrol edilebilir) |
| **Secure Boot** | Sorun yok — WinRE Microsoft tarafından imzalı |
| **BitLocker** | Hedef disk BitLocker ile şifreli olsa bile araç sorunsuz çalışır. format veya clean komutları şifreli blokların üzerine fiziksel olarak yazar. WinRE'nin kendisi zaten şifreli ortamla uyumlu çalışacak şekilde tasarlanmıştır.

### Derleme Aracı

[w64devkit](https://github.com/skeeto/w64devkit)

---

## Derleme (Build)

proje klasörüne git:

```sh
# Yapılandır (Ninja, GCC, Release modu)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Derle
cmake --build build
```

Çıktı: `build\wiper.exe`

Değişiklik yapıp tekrar derlemek için:

```sh
cmake --build build --clean-first
```

---

## Kullanım

```sh
# Yönetici terminalinde:
build\wiper.exe
```

Program şu adımları izler:

1. WinRE konumunu tespit eder ve ekrana basar
2. Disk tipini tespit eder (HDD / SSD)
3. `winre.wim` içine silme scriptlerini gömer
4. 10 saniyelik geri sayım başlar — herhangi bir tuşa basarak iptal edilebilir
5. Sistem WinRE üzerinden yeniden başlatılır
