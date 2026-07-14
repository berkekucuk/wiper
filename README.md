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

### WinRE (Windows Recovery Environment) — Seçilen Yöntem

WinRE, Windows'un zaten içinde gelen ve sistem bozulduğunda kullanılan küçük bir
kurtarma ortamıdır. Her Windows kurulumunda hazır bulunur; kurulum dosyalarının
üzerine eklenmiş gibi çalışan bir "acil durum" işletim sistemidir.

Teknik olarak WinRE, WinPE tabanlı bir kurtarma ortamıdır. WinPE ise Windows'u
yüklemek, disk hazırlamak veya sistem kurtarmak için kullanılan daha genel bir
temel ortamdır. WinRE ise bunun daha dar ve daha özel bir sürümüdür: yalnızca
kurtarma ve bakım işleri için tasarlanmıştır.

WinRE'nin disk üzerinde gizli bir partition'da `winre.wim` adıyla saklanır.
Sistem açılırken sorun çıkarsa veya "Gelişmiş Başlangıç Seçenekleri" menüsünden
seçilirse, Windows bu dosyayı RAM'e yükler ve oradan çalıştırır.

**WinRE neden seçildi:**

| | Linux USB | WinRE |
|---|---|---|
| Kullanıcı hazırlığı | ❌ USB gerekir |✅ Zaten kurulu |
| Secure Boot uyumlu | ❌ | ✅ MS imzalı |
| Yazılımdan tetiklenebilir | ❌ | ✅ `reagentc /boottore` |
| Kullanıcı müdahalesi | ❌ Gerekir | ✅ Gerekmez |

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

---

### PartitionWipe:

**HDD İçin (Rastgele Veri ile Üzerine Yazma):**
Sadece hedeflenen bölüme rastgele veri yazarak güvenli silme sağlar.
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `select partition N`, `assign letter=Z` komutları ile hedef partition seçilip geçici bir sürücü harfi (Z:) atanır, ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /P:1 /Y` → Partition'daki tüm alanın üzerine 1 tur sıfır, ardından 1 tur rastgele veri yazılarak eski veriler yok edilir.
3. `diskpart` → Diskpart tekrar açılır, `select disk N`, `select partition N`, `remove letter=Z` komutları ile geçici Z: harfi kaldırılır, ardından `exit` ile çıkılır.
4. `wpeutil reboot` → İşletim sistemi zarar görmediği için sistem yeniden başlatılarak normal Windows'a dönülür.

**SSD İçin (Hızlı Format ve TRIM):**
SSD ömrünü (TBW) tüketmemek ve wear-leveling sorunlarına takılmamak için rastgele veri yerine donanımsal temizlik kullanılır.
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `select partition N`, `assign letter=Z` komutları ile hedef partition seçilip geçici bir sürücü harfi (Z:) atanır, ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /Q /Y` → Hızlı format (Quick Format) uygulanarak dosya tablosu saniyeler içinde sıfırlanır. (Not: Modern Windows/WinRE ortamlarında NTFS hızlı formatı, arka planda otomatik olarak TRIM/UNMAP sinyali de gönderir.)
3. `defrag Z: /L` → (Güvenlik Ağı) WinRE'nin minimal yapısında otomatik TRIM'in kaçırılma ihtimaline karşı, Windows'un yerleşik aracıyla TRIM/UNMAP komutu açıkça ve zorunlu olarak tekrar tetiklenir. Bu, SSD kontrolcüsüne blokların boş olduğunu kesin olarak bildirir.
4. `diskpart` → Diskpart tekrar açılır, `select disk N`, `select partition N`, `remove letter=Z` komutları ile geçici Z: harfi kaldırılır, ardından `exit` ile çıkılır.
5. `wpeutil reboot` → Sistem yeniden başlatılarak normal Windows'a dönülür.

---

### FullDiskWipe:

**HDD İçin Seçilen Yöntem: `clean all` (NIST 800-88 Standardı)**
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `clean all` komutları ile disk tamamen sıfırlanır; ardından `exit` ile CMD ortamına dönülür.
2. `wpeutil shutdown` → İşletim sistemi kalmadığı için sistem tamamen kapatılır (reboot anlamsızdır).
(Neden eski DoD standartları gibi rastgele veri yazılmadı? Günümüz yüksek yoğunluklu HDD'lerinde tek geçiş sıfır yazmak veriyi donanımsal olarak (mikroskopla bile) kurtarılamaz hale getirdiği için güncel NIST standartlarında yeterli kabul edilir.)

**HDD İçin Alternatif Teori: Dinamik Bölümleme ve Toplu Format**
(Karşılaştırma amacıyla belgelenmiştir, projede kullanılmaz)
Eğer sıfır yerine mutlaka rastgele veri yazılmak istenseydi mimari şu olurdu:
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `clean`, `create partition primary`, `assign letter=Z` komutları ile disk RAW hale getirilip geçici bir sürücü harfi (Z:) atanır; ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /P:1 /Y` → Tüm diske 1 tur sıfır + 1 tur rastgele veri yazılır.
3. `diskpart` → Diskpart tekrar açılır, `select disk N`, `clean` komutuyla geçici partition tablosu silinir ve disk tekrar ham (RAW) bırakılır; ardından `exit` ile çıkılır.
4. `wpeutil shutdown` → Sistem tamamen kapatılır.
Ancak bu mimari (limitler ve partition hataları nedeniyle) çok kırılgandır ve modern HDD'lerde gereksizdir. Bu yüzden stabil olan clean all seçilmiştir.

**SSD İçin Seçilen Yöntem: Partition Oluştur, Hızlı Formatla (TRIM Tetikle) ve Temizle.**
SSD'lerde `clean all` (sıfır yazmak) veya `format /P:1` (rastgele veri yazmak) iki büyük soruna yol açar:
- **Ömür Tüketimi (TBW):** Terabaytlarca veriyi boş yere yazmak SSD bellek hücrelerinin ömrünü ciddi şekilde tüketir.
- **Güvenlik İllüzyonu (Wear-Leveling):** Siz tüm sektörlere sıfır yazdığınızı sansanız da SSD kontrolcüsü bunu diske dağıtır ve eski verinin bir kısmı gizli yedek alanlarda fiziksel olarak kalabilir.

Bu yüzden SSD'ler için en doğru yazılımsal yaklaşım donanımın kendi mekanizmasını kullanmaktır:
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `clean`, `create partition primary`, `assign letter=Z` komutları ile disk RAW hale getirilip geçici bir sürücü harfi (Z:) atanır; ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /Q /Y` → Hızlı format uygulanır. (Not: Modern Windows/WinRE ortamlarında NTFS hızlı formatı, arka planda otomatik olarak TRIM/UNMAP sinyali de gönderir.)
- **`format Z:`**: `Z:` sürücü harfine bağlı partition'ı biçimlendirir.
- **`/FS:NTFS`**: Hedef partition'a NTFS dosya sistemini kurar. Windows ortamında standart ve güvenli bir seçimdir.
- **`/Q`**: Hızlı format anlamına gelir. Dosya tablosunu yeniden kurar, fakat eski verileri fiziksel olarak doğrudan silmez.
- **`/Y`**: Format sırasında gelen "devam etmek istiyor musunuz?" onay sorusunu kaldırır. Scriptlerde otomatik çalışması için gereklidir.
3. `defrag Z: /L` → (Güvenlik Ağı) PartitionWipe moduyla aynı felsefeyle, WinRE'nin minimal yapısında otomatik TRIM'in kaçırılma ihtimaline karşı TRIM/UNMAP komutu açıkça tekrar tetiklenir. SSD kontrolcüsü bu sinyali alınca tüm NAND hücrelerini donanımsal olarak sıfırlar. Bu işlem saniyeler sürer ve SSD ömrünü (TBW) tüketmez.
4. `diskpart` → Diskpart tekrar açılır, `select disk N`, `clean` komutuyla geçici partition tablosu silinir ve disk tekrar ham (RAW) bırakılır; ardından `exit` ile çıkılır.
5. `wpeutil shutdown` → Sistem tamamen kapatılır.


**Arka Planda Tam Olarak Ne Olur? (Hızlı Formatın Sırrı)**

Normal (tam) bir format işlemi diskin her sektörünü tek tek okuyup bozuk sektör (bad sector) taraması yapar ve (eski Windows sürümlerinde) üzerine sıfır yazardı. Ancak **`/Q` (Hızlı Format)** bambaşka bir şey yapar:

1. **Ana Dosya Tablosu (MFT - Master File Table) Silinir/Yeniden Yazılır:** NTFS, diskindeki tüm dosyaların nerede olduğunu bu tabloda tutar. Hızlı format, bu tabloyu anında siler veya boş bir tablo ile değiştirir.
2. **Kök Dizin (Root Directory) Temizlenir:** Diske "burada artık dosya yok" bilgisi yazılır.
3. **Verilerin Üzerine Yazılmaz:** Eski dosyalarınızın 1 ve 0'ları (manyetik plakada veya NAND hücrelerinde) fiziksel olarak olduğu gibi kalır. Sadece işletim sistemi o alanları "boş ve üzerine yazılabilir" olarak işaretler.
4. **KRİTİK ADIM: TRIM / UNMAP Sinyali (Sadece SSD'ler için):** Modern Windows (Vista ve sonrası, dolayısıyla WinRE de dahil), bir SSD üzerinde `/Q` ile hızlı format yaptığında, arka planda SSD kontrolcüsüne bir **TRIM (SATA) veya Deallocate/UNMAP (NVMe)** komutu gönderir. Bu komut şunu der: *"Bu bloklardaki veriler artık geçersiz, onları içsel olarak sil ve hücreleri sıfırla."*

---

### `clean` ve `format` Komutlarının Karşılaştırması

- **`clean`**: `diskpart` içinde kullanılan bir komuttur. Diskin genel yapısını temizler; yani partition tablosunu, partition kayıtlarını ve bölümleri "ham" duruma getirir. Bu nedenle **disk düzeyinde** çalışır. Yani önce hangi diskin seçileceği belirtilir (`select disk N`), sonra bu disk temizlenir.
- **`format`**: Windows'un bir partition veya sürücü harfi üzerinde dosya sistemi oluşturan komutudur. Bu nedenle **partition/volume düzeyinde** çalışır. Yani önce hedef partition seçilir, sonra ona bir dosya sistemi yazılır.

Özetle:
- `clean` → disk seviyesinde, fiziksel diski temizler.
- `format` → partition seviyesinde, belirli bir bölüme dosya sistemi kurar.

Bu yüzden:
- Tüm diski sıfırlamak için `clean` kullanılır.
- Sadece belirli bir partition'ı temizlemek/yeniden hazırlamak için `format` kullanılır.

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
