# webserv

C++98 ile yazılmış, `epoll` tabanlı HTTP/1.0 ve HTTP/1.1 sunucusu.

## Derleme

```sh
make
```

Proje Linux ağ API’lerini (`epoll`) kullandığı için Linux ortamında derlenmelidir.

## Çalıştırma

```sh
./webserv config/example.conf
```

Sunucu tek bir yapılandırma dosyası alır. Desteklenen yönergeler:

- `server`: `listen`, `server_name`, `client_max_body_size`, `error_page`
- `location`: `root`, `index`, `allow_methods`, `autoindex`
- yükleme: `upload_enable`, `upload_store`
- CGI: `cgi_ext`, `cgi_path`
- yönlendirme: `return`

## Kapsamlı entegrasyon testi

Gerekli araçlar: `bash`, `make`, `curl`, `python3`, `find`, `awk`, `sed` ve CGI
testleri için `/usr/bin/php-cgi`.

```sh
./test/test.sh
```

Betik varsayılan olarak temiz derleme yapar. Seçenekler:

```sh
./test/test.sh --no-build
./test/test.sh --leak
./test/test.sh --leak --no-build
./test/test.sh --keep-temp
```

`--leak`, Valgrind mevcutsa bellek hatası, bellek sızıntısı ve açık dosya
tanıtıcısı kontrollerini ekler. `--keep-temp`, başarısız bir testi incelemek için
üretilen geçici siteyi, çalışma configini ve sunucu logunu korur.

Test paketi şu alanları doğrular:

- CLI kullanımı ve hatalı configlerin reddedilmesi
- temiz C++98 derlemesi
- HTTP/1.0 ve HTTP/1.1 yanıtları
- istek satırı, Host, header ve gövde doğrulaması
- 400, 403, 404, 405, 413, 414, 431, 500, 501 ve 505 yanıtları
- statik dosyalar, MIME tipleri, index fallback ve autoindex
- en uzun `location` eşleşmesi ve location sınırları
- upload → GET → DELETE yaşam döngüsü
- bellek içi ve geçici dosya üzerinden büyük gövde işleme
- `client_max_body_size` sınır değerleri
- chunked aktarım, chunk extension ve trailer doğrulaması
- CGI GET/POST, ortam değişkenleri, CGI status/header aktarımı
- 301, 302, 303, 307 ve 308 yönlendirmeleri
- özel hata sayfaları ve `..` traversal engeli
- aynı portta isim tabanlı virtual host ve farklı portta ikinci sunucu
- eşzamanlı istekler ve hatalı isteklerden sonra sunucu sağlığı
- SIGTERM ile temiz kapanış
- Valgrind bellek, hata ve FD raporu

## Test fixture yapısı

`config/test_all.conf` bir şablondur. İçindeki `__TEST_SITE__` alanları
`test/test.sh` tarafından çalışma anında oluşturulan mutlak geçici site yoluyla
değiştirilir. `website/` bu geçici site için kaynak fixture’dır.

Upload ve DELETE testleri doğrudan proje içindeki `website/` klasöründe
çalışmaz. Betik önce izole bir kopya oluşturur; böylece test tekrarları kaynak
ağacında dosya bırakmaz veya mevcut dosyaları silmez.
