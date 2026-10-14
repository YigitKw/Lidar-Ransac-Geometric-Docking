# LIDAR Tabanlı Otonom Mobil Robot Şarj İstasyonu Tespiti

[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

Bu çalışma, 2B LIDAR sensöründen elde edilen TOML formatındaki tarama verilerini ayrıştıran, ortamdaki doğrusal sınırları ardışık RANSAC ve Temel Bileşen Analizi (PCA) yöntemleriyle modelleyen, $\ge 60^\circ$ geometrik köşe kesişimlerini belirleyerek mobil robot ile şarj istasyonu arasındaki bağıl konumu hesaplayan ve sonuçları Gnuplot üzerinden görselleştiren bir C uygulamasıdır.

---

## Sistem Mimarisi ve İşlem Akışı

Uygulama, veri akışını birbirine bağlı beş temel modül üzerinden yürütmektedir:

1. **Veri Ayrıştırma ve Dil Seçimi (`parseTomlScanFile`):** Çalışma başlangıcında etkileşimli dil seçimi (İngilizce varsayılan, '2' tuşu ile Türkçe) veya `--tr`/`--en` komut satırı parametreleri sunar. Yerel `.toml` dosyasını (tırnak temizleme ve UTF-8 geniş karakterli dosya yolu desteğiyle) veya belirtilen HTTP/HTTPS bağlantısını okur. Ağ adresleri `libcurl` kütüphanesi aracılığıyla yerel diske aktarılır; dosya içeriği harici bir ayrıştırıcı kütüphane kullanılmaksızın standart C kütüphanesiyle ayrıştırılır.
2. **Koordinat Dönüşümü ve Filtreleme (`convertPolarToCartesian`, `filterInvalidLaserRanges`):** Sensör sınırları dışındaki ölçümler (`NaN`, `Inf`, menzil altı ve menzil üstü değerler) elenir. Geçerli kutupsal koordinatlar ($r, \theta$), Kartezyen koordinat sistemine ($x, y$) dönüştürülür.
3. **Doğru Tespiti ve Optimizasyon (`extractWallsUsingRANSAC`, `fitLineUsingPCA`):** Nokta bulutundaki doğrusal segmentler ardışık RANSAC algoritmasıyla ayrıştırılır. Dikey doğrularda eğimin tanımsız kalmasını önlemek amacıyla doğru parametreleri kovaryans matrisinin özvektörleri (PCA) üzerinden optimize edilir.
4. **Köşe Kesişim Analizi (`computeDockingIntersections`):** Tespit edilen doğru çiftleri arasındaki açılar yön vektörlerinin iç çarpımı ile hesaplanır. Aralarındaki açı $60^\circ$ veya üzerinde olan ve kesişim noktası fiziksel duvar segment sınırlarına yakın bulunan geometriler şarj istasyonu hedefi olarak kaydedilir.
5. **Etkileşimli Görselleştirme (`renderVisualizationWithGnuplot`):** Robot konumu $(0, 0)$, ham tarama noktaları, tespit edilen doğrusal segmentler, hedef kesişim noktası ve robottan hedefe uzanan mesafe vektörü Gnuplot boru hattı üzerinden 1:1 ölçekli olarak çizdirilir. Görselleştirme penceresi tam ekran boyutunda başlatılır; gecikmesiz beyaz tuval çizimi uygulanır. Kullanıcı yön tuşları, fare tekerleği ve 'a' tuşu ile harita üzerinde gezinebilir.

---

## Kurulum ve Sistem Gereksinimleri

Program, ISO C99 standardına uygun saf C dili ile geliştirilmiştir.

### Gereksinimler:
- **C Derleyicisi:** MinGW GCC veya Code::Blocks (MinGW bileşeni içeren sürüm).
- **libcurl:** Ağ üzerinden TOML verilerini indirmek için dinamik bağlantı kütüphanesi ve başlık dosyaları.
- **Gnuplot:** Grafiksel arayüz çıktısının oluşturulması için sistem ortam değişkenlerine (`PATH`) eklenmiş Gnuplot çalıştırılabilir dosyası.

### Derleme:

**Yöntem 1: Code::Blocks Ortamı**
`Lidar/Proje.cbp` proje dosyasını Code::Blocks ile açarak **Build and Run (F9)** komutunu çalıştırınız.

**Yöntem 2: MinGW Komut Satırı**
`Lidar` dizininde terminal üzerinden şu komutları uygulayınız:

```powershell
gcc -Wall -O2 -c main.c -o obj/Debug/main.o -I"C:/Program Files/curl-8.16.0_12-win64-mingw/include"
gcc -o bin/Debug/Proje.exe obj/Debug/main.o -L"C:/Program Files/curl-8.16.0_12-win64-mingw/lib" -lcurl -lm
```

*(Not: `libcurl` dizin yollarını yerel kurulumunuza göre düzenleyiniz. Çalıştırılabilir dosya `bin/Debug/` dizininde üretilir; `libcurl-x64.dll` dosyası bu dizinde yer almalıdır).*

---

## Kullanım Senaryoları

Program doğrudan başlatıldığında kullanıcıyı iki dilli bir karşılama ekranı karşılar:

```text
================================================================================
        LIDAR RANSAC Line Detection and Docking / Şarj İstasyonu Tespiti        
================================================================================
Language / Dil:
  English  : [ENTER]
  Türkçe   : [2'ye bas + ENTER]
> 
```

Dil tercihi yapıldıktan sonra dosya girdi istemi ekrana gelir:

```text
TOML dosya adı, dosya yolu veya URL girin ya da sürükleyip bırakın: [ENTER = varsayılan]
> 
```

### Girdi Yöntemleri:

1. **Varsayılan Parametre (ENTER):** Doğrudan onaylandığında sistem, varsayılan test adresine (`http://abilgisayar.kocaeli.edu.tr/lidar1.toml`) bağlanarak dosyayı indirir ve işler.
2. **Dosya Adı:** Çalışma dizininde veya yaygın sistem klasörlerinde (`Downloads`, `Desktop`, `Documents`, `OneDrive`) yer alan `.toml` dosyasının adı girilebilir.
3. **Sürükle-Bırak:** Dosya konsol penceresine sürüklendiğinde oluşan tırnak işaretleri (`"..."`, `'...'`) otomatik olarak temizlenir.
4. **URL Girdisi:** Doğrudan `http://` veya `https://` formatında bir web bağlantısı tanımlanabilir.
5. **Komut Satırı Parametreleri:** Etkileşimli konsol adımları komut satırından doğrudan atlanabilir:
   ```powershell
   .\bin\Debug\Proje.exe --tr samples/lidar.toml
   .\bin\Debug\Proje.exe --en http://abilgisayar.kocaeli.edu.tr/lidar1.toml
   ```

### Örnek Konsol Çıktısı:

```text
=== TOML Başlığı ===
  Zaman Damgası   : 1970-01-01T02:54:13
  Referans Çerçeve: base_scan
  angle_min=0.0000  angle_max=6.2800  angle_increment=0.017493
  range_min=0.12  range_max=3.50  toplam ölçüm=360
====================
RANSAC 8 doğrusal segment buldu.
Açısı >= 60 derece olan 1 şarj kesişimi bulundu:
Kesişim 1 (d6 & d7): (-1.287, 1.369) mesafe=1.88m açı=89.8 derece
Grafiği kapatmak için Enter'a basın... (Gezinme: Yön Tuşları = Kaydır, Ctrl+Tekerlek veya +/- = Yakınlaştır, 'a' = Sıfırla/Ortala)
```

### Etkileşimli Grafik Gezinme Kontrolleri (Gnuplot):

Görselleştirme penceresi masaüstü odağını engellemeden tam ekran boyutunda açılır. Kullanıcı aşağıdaki kontrollerle harita üzerinde serbestçe gezinebilir:

| Kontrol / Tuş | İşlev | Tanım |
| :--- | :--- | :--- |
| **Yön Tuşları** ($\leftarrow, \rightarrow, \uparrow, \downarrow$) | Gezinme (Pan) | İzometrik düzlemde kadrajı istenen yöne doğru kaydırır. |
| **Ctrl + Fare Tekerleği** | İmleç Odaklı Yakınlaştırma (Zoom) | İmlecin bulunduğu noktayı merkeze alarak dinamik büyütme ve küçültme yapar. |
| **`+` / `-` Tuşları** *(veya `z` / `x`)* | Tek Tuşla Yakınlaştırma | Herhangi bir yardımcı tuşa basmadan doğrudan kademeli yakınlaşıp uzaklaşır. |
| **Sağ Tıkla Sürükleme** | Kutu Yakınlaştırma (Box Zoom) | Seçilen dikdörtgen alanı anında ekrana sığdıracak şekilde büyütür. |
| **`a` Tuşu** | Otomatik Ölçekleme (Autoscale) | Görünümü sıfırlayarak tüm noktaları, duvarları ve robotu kadraja sığdıracak şekilde yeniden ortalar. |

Bu yönlendirmeler grafik başlığının hemen altına alt başlık olarak yazdırılır ve konsol isteminde hatırlatılır. Konsolda `Enter` tuşuna basıldığında grafik kapatılır ve program sonlanır.

---

## Algoritmik Parametreler ve Kısıtlar

Algoritmanın hesaplama parametreleri `main.c` kaynak dosyasında tanımlıdır:

| Parametre / Sabit | Varsayılan Değer | Fonksiyon / Kapsam | Tanım ve Tasarım Gerekçesi |
| :--- | :--- | :--- | :--- |
| `minAngleThresholdDegrees` | `60.0^\circ` | `computeDockingIntersections` | Şarj istasyonu köşe aramasında kabul edilen minimum kesişim açısıdır. Düzensiz duvar birleşimlerini eler. |
| `DISTANCE_TOLERANCE` | `0.025\text{ m}` | `extractWallsUsingRANSAC` | Noktanın doğru modeline ait kabul edilmesi (inlier) için mesafe eşiğidir; sensör gürültü payını sınırlar. |
| `MIN_INLIER_THRESHOLD` | `8\text{ nokta}` | `extractWallsUsingRANSAC` | Geçerli bir doğru segmenti oluşturmak için gereken minimum nokta sayısıdır. |
| `MIN_WALL_LENGTH` | `0.22\text{ m}` | `extractWallsUsingRANSAC` | Kabul edilen en küçük duvar uzunluğudur; çevresel küçük engelleri eler. |
| `MAX_NEIGHBOR_GAP_SQ` | `0.35^2\text{ m}^2` | `extractWallsUsingRANSAC` | Doğrultu üzerindeki ardışık noktalar arası izin verilen maksimum boşluktur; ayrık duvarları böler. |
| `MAX_RANSAC_ITERATIONS` | `3000` | `extractWallsUsingRANSAC` | Her segment için gerçekleştirilen rastgele hipotez testi sayısıdır. |
| Köşe Toleransı | `0.25\text{ m}` | `isPointNearLineSegment` | Kesişim noktasının fiziksel segment sınırlarına olan maksimum izin verilen mesafesidir. |

---

## Algoritmik Yöntem ve Karmaşıklık Analizi

### 1. İşlem Akış Diyagramı

```
[ TOML / Web Girdisi ]
         │
         ▼
[ parseTomlScanFile ]          ───>  Dinamik bellek tahsisi (realloc) ile kütüphanesiz metin ayrıştırma
         │
         ▼
[ filterInvalidLaserRanges ]   ───>  Geçersiz (NaN / Inf) ve sınır dışı verilerin filtrelenmesi (-1)
         │
         ▼
[ convertPolarToCartesian ]    ───>  x = r * cos(θ), y = r * sin(θ) Kartezyen koordinat dönüşümü
         │
         ▼
[ extractWallsUsingRANSAC ]    ───>  Maksimum inlier kümesine sahip doğrusal hipotezlerin belirlenmesi
         │
         ▼
[ fitLineUsingPCA ]            ───>  Kovaryans özvektörleri üzerinden dik açılara dayanıklı doğru uydurma
         │
         ▼
[ Mesafe Tabanlı Kümeleme ]    ───>  35 cm eşiğini aşan süreksizliklerde segment ayrıştırması
         │
         ▼
[ computeDockingIntersections ]───>  Segment uç noktası doğrulaması ile fiziksel köşe kesişim hesabı
         │
         ▼
[ renderVisualizationWithGnuplot ] ─> Gnuplot boru hattı üzerinden 1:1 ölçekli grafiksel gösterim
```

### 2. Algoritmik Tasarım Tercihleri

- **Köşe Noktalarının Paylaşımı (Corner Sharing):**  
  Klasik RANSAC yaklaşımında bir doğruya dahil edilen noktalar kümeden çıkarılır. İki duvarın dik kesiştiği köşe noktalarında bu durum, ikinci duvarın inlier sayısının eşik değerin altına düşmesine yol açabilmektedir. Uygulamada, aralarındaki açı $45^\circ$'den büyük olan doğruların ortak köşe noktalarını paylaşmasına izin verilmiştir.
- **PCA Tabanlı Doğru Modelleme:**  
  Klasik en küçük kareler regresyonunda (OLS), dikey doğrultularda ($x = c$) eğim tanımsız hale gelmektedir. Bu kısıt, $2 \times 2$ kovaryans matrisinin özvektör açısının kapalı formda hesaplanması ($\theta = \frac{1}{2} \text{atan2}(2C_{xy}, C_{xx} - C_{yy})$) ile çözülmüş ve tüm yönelimlerde sayısal kararlılık sağlanmıştır.
- **Vektörel İç Çarpım ile Açı Hesabı:**  
  Eğim değerleri yerine doğru yön vektörlerinin skaler çarpımı ($\cos\theta = \frac{|\vec{v}_1 \cdot \vec{v}_2|}{\|\vec{v}_1\| \|\vec{v}_2\|}$) kullanılmıştır. Bu yaklaşım, dik kesişimlerde paydayı sıfır yapan tekillikleri ortadan kaldırmaktadır.

---

### 3. Karmaşıklık Analizi

- $N$: LIDAR taramasındaki toplam ölçüm sayısı ($N \approx 360$).
- $K$: Tespit edilen doğrusal segment sayısı ($4 \le K \le 8$).
- $I$: RANSAC iterasyon sayısı ($I = 3000$).

#### Zaman Karmaşıklığı:
1. **TOML Ayrıştırma:** $\mathcal{O}(N)$, tek geçişli akış okuma.
2. **Kartezyen Dönüşüm:** $\mathcal{O}(N)$, tek trigonometrik izdüşüm.
3. **RANSAC Doğru Tespiti:** $\mathcal{O}(K \cdot I \cdot N)$. Her segment için $I$ deneme ve $N$ noktanın mesafe testi gerçekleştirilir ($6 \times 10^6$ temel işlem adımı).
4. **Sıralama ve Kümeleme:** $\mathcal{O}(K \cdot M \log M)$, burada $M \le N$ segment içi nokta sayısıdır.
5. **Kesişim Analizi:** $\mathcal{O}(K^2)$, $\binom{K}{2}$ doğru çifti kombinasyonu.

$$\text{Toplam Zaman Karmaşıklığı} = \mathcal{O}(K \cdot I \cdot N + K^2)$$

Hesaplama süresi yerel veri setlerinde yaklaşık **5-15 milisaniye** aralığında tamamlanmaktadır.

#### Alan Karmaşıklığı:
Dinamik dizilerde saklanan Kartezyen noktalar, inlier indisleri ve doğru modelleri doğrusal bellek gerektirmektedir. Toplam bellek tüketimi 100 KB seviyesindedir:

$$\text{Toplam Alan Karmaşıklığı} = \mathcal{O}(N)$$

---

## Dizin Yapısı

```
Proje1/
├── .gitignore                # Git izleme dışı dosya yapılandırması
├── README.md                 # İngilizce teknik dokümantasyon
├── README.tr.md              # Türkçe teknik dokümantasyon
└── Lidar/
    ├── main.c                # Algoritmik boru hattı ve görselleştirme kodu
    ├── Proje.cbp             # Code::Blocks proje yönetim dosyası
    ├── samples/              # Örnek LIDAR TOML tarama verileri
    │   ├── lidar.toml
    │   ├── lidar2.toml
    │   └── scan_data_NaN.toml
    └── bin/
        └── Debug/
            ├── Proje.exe     # Derlenmiş ikili dosya
            └── libcurl-x64.dll # Ağ istekleri için çalışma zamanı dinamik kütüphanesi
```
