#ifndef HTTP_CONSTANTS_HPP
#define HTTP_CONSTANTS_HPP

// --- HTTP 4xx CLIENT ERROR ---

#define HTTP_BAD_REQUEST					400 // İstek hatalı veya bozuk (Sözdizimi hatası)
#define HTTP_UNAUTHORIZED					401 // Kimlik doğrulaması gerekiyor
#define HTTP_PAYMENT_REQUIRED				402 // Ödeme gerekli
#define HTTP_FORBIDDEN						403 // Kimlik doğrulandı ama yetki yok (Erişim reddedildi)
#define HTTP_NOT_FOUND						404 // Kaynak bulunamadı
#define HTTP_METHOD_NOT_ALLOWED				405 // HTTP metodu (GET, POST vb.) bu kaynak için izin verilmiyor
#define HTTP_NOT_ACCEPTABLE					406 // İstemcinin Accept başlıklarına uygun içerik yok
#define HTTP_PROXY_AUTHENTICATION_REQUIRED	407 // Proxy üzerinden kimlik doğrulaması gerekiyor
#define HTTP_REQUEST_TIMEOUT				408 // Sunucu isteği beklerken zaman aşımına uğradı
#define HTTP_CONFLICT						409 // İstek, sunucunun mevcut durumu ile çelişiyor
#define HTTP_GONE							410 // Kaynak kalıcı olarak silinmiş
#define HTTP_LENGTH_REQUIRED				411 // Content-Length başlığı eksik
#define HTTP_PRECONDITION_FAILED			412 // İstemcinin belirttiği ön koşullar sunucu tarafından karşılanmıyor
#define HTTP_PAYLOAD_TOO_LARGE				413 // İstek gövdesi (Payload) sunucunun işleyebileceğinden çok daha büyük
#define HTTP_URI_TOO_LONG					414 // İstenen URI çok uzun
#define HTTP_UNSUPPORTED_MEDIA_TYPE			415 // Medya formatı (Content-Type) sunucu tarafından desteklenmiyor
#define HTTP_RANGE_NOT_SATISFIABLE			416 // İstenen veri aralığı (Range) karşılanamıyor
#define HTTP_EXPECTATION_FAILED				417 // Expect başlığındaki gereksinimler karşılanamıyor
#define HTTP_IM_A_TEAPOT					418 // Nisan şakası (RFC 2324 - Kahve demlenemez, çünkü ben bir çaydanlığım)
#define HTTP_MISDIRECTED_REQUEST			421 // İstek, yanıt üretemeyecek bir sunucuya yönlendirildi
#define HTTP_UNPROCESSABLE_ENTITY			422 // İstek doğru formatta ama semantik hatalar içeriyor (WebDAV)
#define HTTP_LOCKED							423 // Kaynak kilitli (WebDAV)
#define HTTP_FAILED_DEPENDENCY				424 // Önceki bir isteğin başarısız olması nedeniyle başarısız (WebDAV)
#define HTTP_TOO_EARLY						425 // Sunucu, tekrarlanma riski olan bir isteği işlemek istemiyor
#define HTTP_UPGRADE_REQUIRED				426 // İstemcinin farklı bir protokole (örn. TLS/1.0'dan 1.2'ye) geçmesi gerekiyor
#define HTTP_PRECONDITION_REQUIRED			428 // Sunucu, isteğin koşullu olmasını gerektiriyor
#define HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE	431 // İstek başlıkları (Header alanları) çok büyük
#define HTTP_UNAVAILABLE_FOR_LEGAL_REASONS	451 // Yasal nedenlerden dolayı içerik engellendi

#endif // HTTP_CONSTANTS_HPP