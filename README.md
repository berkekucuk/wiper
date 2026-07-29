## Proje Nedir?

`wiper`, Windows çalışırken çift tıklanabilen ve sistem yeniden başladığında
hedef diski **otomatik olarak** geri döndürülemeyecek şekilde silen bir araçtır.
Kullanıcıdan herhangi bir ek giriş beklenmez — her şey programın kendisi tarafından yönetilir.

Bu araç, **işletim sisteminin kurulu olduğu diski dahi** canlı olarak silmek üzere tasarlanmıştır.

İki modda çalışır:
- **Disk Wipe** — Hedef diski WinRE üzerinden tamamen siler
- **File Wipe** — Belirtilen dosya/dizin listesini güvenli şekilde siler

---

## Neden Böyle Bir Şeye İhtiyaç Var?

Bir bilgisayarın diskini tamamen silmek gerektiğinde,
bu işlem **Windows açıkken yapılamaz:**

- Windows kendi dosyalarını RAM'e yüklemiş halde çalışıyor
- C: sürücüsündeki dosyalar işletim sistemi tarafından kilitli

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

Teknik olarak WinRE, WinPE tabanlı bir kurtarma ortamıdır. WinRE, gizli bir partition'da `winre.wim` adıyla saklanır. Sistem açılırken "Gelişmiş Başlangıç Seçenekleri" menüsünden seçilirse, Windows bu dosyayı RAM'e yükler ve oradan çalıştırır.

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

`wiper.exe` çalışma modunu `wiper_config.txt`'den okur ve ona göre
Disk Wipe veya File Wipe modunda çalışır.

---

### Disk Wipe Modu

Windows'ta yönetici (Administrator) olarak çalışır ve şu adımları izler:

**[1/5] WinRE'yi bul**
`reagentc /info` komutuyla `winre.wim` dosyasının diskteki tam yolu bulunur.

**[2/5] Disk tipini tespit et**
Windows IOCTL API'si üzerinden hedef diskin HDD mi, SATA SSD mi,
yoksa NVMe SSD mi olduğu anlaşılır. Bu bilgi, uygun silme komutunu seçmek için kullanılır.

**[3/5] WinRE imajını yamala**
`winre.wim` DISM ile geçici bir klasöre açılır (mount edilir). İçine
silme scriptleri gömülür ve WinRE'nin başlangıç ayar dosyası
(`winpeshl.ini`) değiştirilerek açılışta doğrudan bu scriptlerin
çalışması sağlanır. Ardından değişiklikler `winre.wim`'e geri yazılır.

> **Güvenlik notu:** Patch başlamadan önce `winre.wim`'in bir yedeği
> `%TEMP%\winre_wiper_backup_<PID>.wim` konumuna alınır. Geri sayım
> sırasında herhangi bir tuşa basılırsa bu yedek otomatik olarak geri
> yüklenir ve `winre.wim` orijinal haline döner.

**[4/5] Geri sayım**
10 saniye geri sayım başlar. Bu sürede herhangi bir tuşa basılırsa
işlem iptal edilir.

**[5/5] Yeniden başlat**
`reagentc /boottore` ile BCD'ye "bir sonraki açılışta WinRE'ye gir"
bayrağı eklenir. Ardından sistem yeniden başlatılır.

WinRE `winre.wim`'den RAM'e yüklenir. `winpeshl.ini` okunur ve
Wiper tarafından gömülen script dosyası çalıştırılır.

---

### File Wipe Modu

Hiçbir sistem değişikliği yapmaz, doğrudan Windows içinde çalışır:

**[1/3] Dosya listesini oku**
`wipe_list.txt` dosyasındaki her satır bir dosya veya dizin yoludur. Boş satırlar ve `#` ile başlayan yorum satırları atlanır.

**[2/3] Dosyaları sil**
Her dosya/dizin için:
- **SSD'deyse** → overwrite atlanır, sadece TRIM ile silinir
- **HDD'deyse** → seçilen stratejiye göre (ZeroFill / RandomFill / DOD3) üzerine yazılır, ardından dosya rasgele isimle taşınarak silinir

**[3/3] Özet**
Başarıyla silinen dosya sayısı gösterilir. Sistem yeniden başlatılmaz.

---

## Disk Mimarisi

Wiper'ın arka planda nasıl çalıştığını ve kullanılan silme komutlarının diskin tam olarak neresini hedef aldığını daha iyi kavrayabilmek için, standart bir diskin mimarisini anlamak gerekir:

![Disk Mimarisi](assets/diagram.webp)

### 1. Disk Seviyesi (En Üst Kısım)
**MBR / Protective MBR + GPT Header:** Diskin en başında (Sektör 0 ve takip eden ilk sektörler) bölümleme tablosu bilgileri yer alır. Tüm diskin "haritası" buradadır. Bilgisayar diske baktığında önce bu haritayı okur ve içinde kaç tane bölüm (partition) olduğunu anlar.

### 2. Partition 1 (Örnek C: Sürücüsü) Yapısı
NTFS dosya sistemine sahip bir bölümün iç yapısı (örneğin işletim sisteminin kurulu olduğu C: diski) şöyledir:

- **Boot Sector (VBR - Volume Boot Record):** Bölümün en başında yer alır. Bilgisayar açılırken, ana bootloader buraya gelerek o bölüme özel işletim sistemi yükleyicisini çalıştırır.
- **$MFT (Master File Table) & $MFTMirr:** NTFS dosya sisteminin beynidir. Diskteki her dosyanın adı, boyutu, oluşturulma tarihi ve hangi sektörlerde olduğu bilgisi bu tabloda tutulur. `$MFTMirr` ise bunun güvenliğini sağlamak için tutulan yedeğidir (ayna kopyası).
- **Other $ metadata files:** NTFS'e özel olan `$LogFile` (işlem günlüğü), `$Bitmap` (boş/dolu alan haritası), `$BadClust` (bozuk sektör listesi) gibi arka planda çalışan sistem dosyalarını temsil eder.
- **User files & directories:** Kaydedilen fotoğraflar, oyunlar ve belgeler gibi kullanıcı verileridir. Bu veriler diskteki *cluster* (küme) adı verilen alanlara dağınık bir şekilde yazılır. Gerçek veriler bu bölümde yer alır.
- **Unallocated / Slack space:** Bölüm içindeki henüz kullanılmamış boş alanları ve dosya boyutlarının cluster boyutuna tam oturmamasından kaynaklanan küçük boşlukları (slack space) ifade eder.

### 3. Partition 2 ... Partition N
Diskin geri kalanında, tıpkı birinci bölüm gibi kendi içinde bağımsız dosya sistemlerine (VBR, meta veri dosyaları vb.) sahip diğer bölümlerin (örneğin yedek disk olan D: sürücüsünün veya gizli kurtarma bölümlerinin) gelebileceği alanlardır. Her partition kendi içinde yukarıdaki "Partition 1" yapısını tekrar eder.

---

### HDD ve SSD'de Silme Sürecinin Farkı

#### HDD'de Silme (Dosya Sistemi Düzeyinde)

HDD'de bir dosyayı sildiğinizde, işletim sistemi yalnızca **dosya sistemindeki kaydını** (MFT girişi, bitmap'teki cluster haritası) siler ve o alanları "boş" olarak işaretler. Verinin fiziksel olarak manyetik plakada durduğu sektörlere **dokunulmaz**. Ta ki üzerine yeni bir veri yazılana kadar (overwrite) eski veri orada kalmaya devam eder. Bu nedenle HDD'de silinen veriler, üzerine yazılmadığı sürece kurtarılabilir.

#### SSD'de Silme (TRIM ve Garbage Collection)

SSD'de silme işlemi, HDD'den tamamen farklı bir mekanizmayla çalışır:

1. **TRIM Komutu:** Dosya silindiğinde işletim sistemi, SSD kontrolcüsüne bir TRIM komutu gönderir. Bu komut, "artık şu mantıksal adreslerdeki (LBA) veri geçerli değil, o fiziksel sayfaları boşaltabilirsin" anlamına gelir.

2. **Hızlı ve Güvenilir:** TRIM sayesinde işletim sistemi, SSD'nin hangi blokların gerçekten boş olduğunu bilmesini sağlar. Bu sayede yeni veri yazılacağında SSD, önce oku-sil-yaz döngüsüne girmeden doğrudan boş sayfalara yazabilir.

3. **Garbage Collection (GC):** SSD kontrolcüsü, TRIM ile boşaltılan alanları ve artık geçerliliği olmayan fiziksel sayfaları GC sürecinde toplar: içinde geçerli veri olan sayfaları başka bir bloğa taşır, ardından tüm bloğu siler (erase). Böylece yeni yazılara hazır hale getirir.

4. **TRIM'siz SSD:** Eğer TRIM desteklenmez veya çalışmazsa (örneğin USB üzerinden bağlı SSD, RAID, exFAT), silinen verinin fiziksel sayfaları hâlâ dolu görünür. SSD, üzerine yazma gelene kadar o sayfaları boşaltmaz ve yeni veriyi başka bir boş sayfaya yazar (overwrite'in işe yaramadığı durum budur).

> **Overwrite notu:** Overwrite (üzerine yazma) HDD'de güvenilirdir çünkü eski veri fiziksel olarak aynı sektöre yazılır. SSD'de ise wear-leveling ve FTL nedeniyle aynı mantıksal adrese yazsanız bile fiziksel hücre değişir — eski veri hâlâ bir NAND sayfasında durur. Bu yüzden SSD'de güvenli silme için overwrite değil, TRIM + NVMe Format gibi yöntemler kullanılır.

---

## Projede Kullanılan Silme Komutları

Disk mimarisi incelendikten sonra artık Windows'un varsayılan disk komutlarının nerede çalıştığına ve nerede yetersiz kaldığına bakılabilir:

- **`clean` Komutu (Disk Düzeyinde Çalışır):** Bu komut tamamen "1. Disk Seviyesi"ni hedef alır. Diskin en başındaki MBR/GPT haritasını (partition tablosunu) siler. Diskin haritası yok olduğu için bilgisayar diski "RAW" (ham ve boş) olarak görür. **Ancak:** `clean` komutu alt taraftaki partitionların içine girip oradaki "User files & directories" (gerçek veriler) kısmına ASLA dokunmaz. Yani işletim sistemi diski boş sansa da, basit bir veri kurtarma programı (harita olmadan da) verileri diskte bulabilir. Bu yüzden güvenli silme için tek başına yetersizdir.
- **`clean all` Komutu (Tüm Sektörleri Ezer):** Haritayı silmekle kalmaz, "1. Disk Seviyesi"nden başlar ve son partition'ın en son sektörüne kadar ayrım gözetmeksizin her şeyin (MFT, kullanıcı verileri, işletim sistemi) üzerine sıfır (0) yazar. Veriler fiziksel olarak yok olur.
- **`format` Komutu (Sürücü Düzeyinde Çalışır):** Bu komut diskin geneline değil, sadece belirli bir bölümün (örneğin sadece Partition 1 / C: sürücüsü) içine girer.
  - **`/Q` (Hızlı Format):** Yalnızca o bölümün yönetim dosyalarını (VBR, $MFT) silip baştan oluşturur. Verilerin üzerine sıfır yazmaz, o alanları sadece "boş ve üzerine yazılabilir alan" olarak işaretler.
  - **`/P:1` (Rastgele Format):** Önce MFT'yi yeniler, ardından o partition içindeki tüm cluster'ların üzerine 1 tur sıfır ve 1 tur rastgele veri (random bits) yazar. Fiziksel manyetik izleri bile yok ettiği için saatlerce sürer.
  - *Neden tüm disk formatlanmaz?* Çünkü format komutu partition içindedir; diskin en başındaki MBR/GPT haritasına dokunamaz. Tüm diski formatlamak diye bir şey yoktur, her partition ayrı formatlanmak zorundadır.

---

## Silme Stratejileri

HDDs (Mekanik Diskler) ve SSDs (Katı Hal Sürücüleri) veriyi tamamen farklı depoladığı için, Wiper alt taraftaki donanımı tespit edip ona göre tamamen farklı algoritmalar çalıştırır.

---

**HDD İçin Seçilen Yöntem: `clean all` (NIST 800-88 Standardı)**
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `clean all` komutları ile disk tamamen sıfırlanır; ardından `exit` ile CMD ortamına dönülür.
2. `wpeutil shutdown` → İşletim sistemi kalmadığı için sistem tamamen kapatılır (reboot anlamsızdır).
(Neden eski DoD standartları gibi rastgele veri yazılmadı? Günümüz yüksek yoğunluklu HDD'lerinde tek geçiş sıfır yazmak veriyi donanımsal olarak (mikroskopla bile) kurtarılamaz hale getirdiği için güncel NIST standartlarında yeterli kabul edilir.)

**HDD İçin Alternatif Yöntem: Dinamik Bölümleme ve Rastgele Veri Formatı (`RandomFill`)**
Eğer sıfır yerine mutlaka rastgele veri yazılmak istenirse bu mod kullanılabilir. Ancak bu mimari `clean all` işlemine göre çok daha fazla zaman alır. Yüksek bir güvenlik standardı hedeflenmediği sürece stabil ve hızlı olan `clean all` (ZeroFill) seçeneği varsayılan olarak önerilir. İstenirse `wiper_config.txt` içindeki `hdd_strategy=randomfill` ile bu mod aktif edilebilir. Mimari şu şekildedir:
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `clean`, `create partition primary`, `assign letter=Z` komutları ile disk RAW hale getirilip geçici bir sürücü harfi (Z:) atanır; ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /P:1 /Y`
- Disk üzerindeki tüm sektörleri tek tek tarar. (Bad sector var mı diye kontrol eder ve /P:1 parametresi nedeniyle her hücrenin üzerine sırayla önce sıfır (0), ardından rastgele bitler yazarak veriyi manyetik olarak yok eder.) Bu işlem disk boyutuna göre saatler sürebilir.
- İşlem bitince en son Partition'a NTFS (New Technology File System) yapısını kurar. Dosyaların nerede tutulacağını, izinleri ve dizin ağacını yönetecek olan ana tabloyu (MFT - Master File Table) bu alana yazar.
3. `diskpart` → Diskpart tekrar açılır, `select disk N`, `clean` komutuyla geçici partition tablosu silinir ve disk tekrar ham (RAW) bırakılır; ardından `exit` ile çıkılır.
4. `wpeutil shutdown` → Sistem tamamen kapatılır.

**SSD İçin Seçilen Yöntem: Partition Oluştur, Hızlı Formatla ve Temizle.**
SSD'lerde `clean all` (sıfır yazmak) veya `format /P:1` (rastgele veri yazmak) iki büyük soruna yol açar:
- **Ömür Tüketimi (TBW):** Terabaytlarca veriyi boş yere yazmak SSD bellek hücrelerinin ömrünü ciddi şekilde tüketir.
- **Güvenlik İllüzyonu (Wear-Leveling):** Tüm sektörlere sıfır yazıldığı düşünülse bile SSD kontrolcüsü bunu diske dağıtır ve eski verinin bir kısmı gizli yedek alanlarda fiziksel olarak kalabilir.

Bu mod, diski yeniden bölümlendirip hızlı format uygular:
1. `diskpart` → Diskpart aracı açılır, `select disk N`, `clean`, `create partition primary`, `assign letter=Z` komutları ile disk RAW hale getirilip geçici bir sürücü harfi (Z:) atanır; ardından `exit` ile CMD ortamına dönülür.
2. `format Z: /FS:NTFS /Q /Y`
- /Q parametresi nedeniyle sektör taramasını ve veri sıfırlamayı tamamen atlar. (Quick format)
- Doğrudan diskin en başına gidip sadece NTFS Dosya Tablosunu (MFT) yazar ve işlemi bitirir.
3. `diskpart` → Diskpart tekrar açılır, `select disk N`, `clean` komutuyla geçici partition tablosu silinir ve disk tekrar ham (RAW) bırakılır; ardından `exit` ile çıkılır.
4. `wpeutil shutdown` → Sistem tamamen kapatılır.

---

### File Wipe Stratejileri

File wipe modu, Windows içinde çalıştığı için disk seviyesinde komutlara erişemez.
Bunun yerine doğrudan dosya sistemi API'leri kullanılır.

**ZeroFill (`zerofill`)**
Dosyanın tüm sektörlerine sıfır (0x00) yazılır, ardından dosya silinir.
Tek geçiş (1 pass).

**RandomFill (`randomfill`)**
Dosyanın tüm sektörlerine rastgele veri yazılır, ardından dosya silinir.
Tek geçiş (1 pass).

**DOD3 (`dod3`)**
DOD 5220.22-M standardı: önce rastgele veri, sonra ters bitler (~random), sonra tekrar rastgele veri yazılır. Ardından dosya silinir. Üç geçiş (3 pass).

**Trim (`trim`)**
Overwrite tamamen atlanır, dosya doğrudan silinir. SSD'deyse TRIM komutu gönderilir.
HDD'deyse sadece dosya sistemi kaydı silinir (veri plakada kalır).

---

## Teknik Kararlar ve Trade-off'lar

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
| `winre.wim` içindeki scriptler | ❌ Geçici — iptal edilirse `%TEMP%` yedeğinden otomatik geri yüklenir |
| Silinen veri | ✅ Kalıcı — geri alınamaz |
| BCD bootsequence | ❌ 1 boot sonra otomatik silinir |
| DISM mount klasörü | ❌ Commit sonrası silinir |
| `%TEMP%` yedek dosyası | ❌ Rollback veya reboot sonrası anlamsız kalır |

---

## Konfigürasyon

Tüm ayarlar `wiper_config.txt` dosyasından okunur (derleme gerektirmez).

Dosya şu sırayla aranır:
1. **EXE ile aynı dizin** (örn. `build\wiper_config.txt`)
2. **EXE'nin bir üst dizini** (proje kökü, geliştirme için)

### Yapılandırma Dosyası (wiper_config.txt)

```ini
# Çalışma modu: diskwipe | filewipe
mode=diskwipe

# Disk silme ayarları (mode=diskwipe)
disk_number=0
hdd_diskwipe_strategy=zerofill       # zerofill | randomfill | quickformat
ssd_diskwipe_strategy=quickformat    # zerofill | randomfill | quickformat (recommended)

# Dosya silme ayarları (mode=filewipe)
wipe_list=wipe_list.txt
hdd_filewipe_strategy=zerofill  # zerofill | randomfill | dod3 | trim
ssd_filewipe_strategy=trim      # zerofill | randomfill | dod3 | trim (recommended) 

### Stratejiler

| Strateji | Kullanım Yeri | Açıklama |
|----------|---------------|----------|
| `zerofill` | Disk & File | Tüm sektörlere sıfır yazar (HDD `clean all`, file 1 pass) |
| `randomfill` | Disk & File | Sıfır + rastgele veri yazar (HDD `format /P:1`) |
| `quickformat` | Disk (SSD) | Sadece partition tablosunu temizler, sektör yazmaz (SSD için **default & recommended**) |
| `dod3` | File | DOD 5220.22-M: rastgele + ters + rastgele (3 pass) |
| `trim` | File | Overwrite atlanır, sadece dosya sistemi kaydı silinir + TRIM (SSD için **default & recommended**) |

### Disk Numarasını Bulmak

```
diskpart
list disk          → hedef disk numarasını öğren (örn. Disk 0)
exit
```

---

## DISM Mount Temizliği

DISM işlemi (geliştirme aşamasında vs.) yarıda kesilirse mount klasörü geride kalabilir. Bunu temizlemek için:
```sh
dism /Cleanup-Mountpoints
```

## Gereksinimler

| Gereksinim | Detay |
|------------|-------|
| **İşletim Sistemi** | Windows 10 / 11 |
| **Yetki** | Administrator (Yönetici) |
| **WinRE** | Etkin olmalı (`reagentc /info` ile kontrol edilebilir) |
| **Secure Boot** | Sorun yok — WinRE Microsoft tarafından imzalı |
| **BitLocker** | Hedef disk BitLocker ile şifreli olsa bile araç sorunsuz çalışır. `clean` komutu şifreli blokların üzerine fiziksel olarak yazar. WinRE'nin kendisi zaten şifreli ortamla uyumlu çalışacak şekilde tasarlanmıştır.

### Derleme Aracı
[w64devkit](https://github.com/skeeto/w64devkit)

---

## Derleme (Build)

Proje klasörüne git:

```sh
# Yapılandır (Ninja, GCC, Release modu)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Derle
cmake --build build
```

Çıktı: `build\wiper.exe`

---

## Kullanım

```sh
# Yönetici terminalinde (çalışma modu wiper_config.txt'den okunur):
build\wiper.exe
```

### Disk Wipe Modu

Program `wiper_config.txt` içinde `mode=diskwipe` varsa şu adımları izler:
1. WinRE konumunu tespit eder ve ekrana basar
2. Disk tipini tespit eder (HDD / SSD)
3. `winre.wim` içine diski güvenle silme scriptlerini gömer
4. 10 saniyelik geri sayım başlar — herhangi bir tuşa basarak iptal edilebilir
5. Sistem WinRE üzerinden yeniden başlatılır ve diski silip otomatik kapanır.

### File Wipe Modu

`wiper_config.txt` içinde `mode=filewipe` olarak değiştirip çalıştırın:

```ini
mode=filewipe
wipe_list=wipe_list.txt
file_strategy=zerofill
```

`wipe_list.txt` örneği:

```txt
C:\Klasor\SilinecekDosya.txt
C:\Gizli\*
D:\EskiProjeler
```
