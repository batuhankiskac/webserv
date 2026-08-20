#ifndef HTTP_CONSTANTS_HPP
#define HTTP_CONSTANTS_HPP

// --- HTTP 2xx SUCCESS ---

#define HTTP_OK							200 // Request successful
#define HTTP_CREATED					201 // Request completed and a new resource was created
#define HTTP_NO_CONTENT					204 // Request successful, but the response has no body

// --- HTTP 3xx REDIRECTION ---

#define HTTP_MOVED_PERMANENTLY			301 // Resource permanently moved
#define HTTP_FOUND						302 // Resource temporarily available at another URI
#define HTTP_SEE_OTHER					303 // Response should be retrieved from another URI
#define HTTP_TEMPORARY_REDIRECT			307 // Resource temporarily moved (preserves the method)
#define HTTP_PERMANENT_REDIRECT			308 // Resource permanently moved (preserves the method)

// --- HTTP 4xx CLIENT ERROR ---

#define HTTP_BAD_REQUEST					400 // Request is invalid or malformed (syntax error)
#define HTTP_UNAUTHORIZED					401 // Authentication is required
#define HTTP_PAYMENT_REQUIRED				402 // Payment is required
#define HTTP_FORBIDDEN						403 // Authenticated but not authorized (access denied)
#define HTTP_NOT_FOUND						404 // Resource not found
#define HTTP_METHOD_NOT_ALLOWED				405 // HTTP method (GET, POST, etc.) is not allowed for this resource
#define HTTP_NOT_ACCEPTABLE					406 // No content matches the client's Accept headers
#define HTTP_PROXY_AUTHENTICATION_REQUIRED	407 // Proxy authentication is required
#define HTTP_REQUEST_TIMEOUT				408 // Server timed out while waiting for the request
#define HTTP_CONFLICT						409 // Request conflicts with the server's current state
#define HTTP_GONE							410 // Resource has been permanently deleted
#define HTTP_LENGTH_REQUIRED				411 // Content-Length header is missing
#define HTTP_PRECONDITION_FAILED			412 // Preconditions specified by the client are not met by the server
#define HTTP_PAYLOAD_TOO_LARGE				413 // Request body (payload) is too large for the server to handle
#define HTTP_URI_TOO_LONG					414 // Requested URI is too long
#define HTTP_UNSUPPORTED_MEDIA_TYPE			415 // Media format (Content-Type) is not supported by the server
#define HTTP_RANGE_NOT_SATISFIABLE			416 // Requested range cannot be satisfied
#define HTTP_EXPECTATION_FAILED				417 // Requirements in the Expect header cannot be met
#define HTTP_IM_A_TEAPOT					418 // April Fools' joke (RFC 2324 - I cannot brew coffee because I am a teapot)
#define HTTP_MISDIRECTED_REQUEST			421 // Request was directed to a server unable to produce a response
#define HTTP_UNPROCESSABLE_ENTITY			422 // Request is well-formed but contains semantic errors (WebDAV)
#define HTTP_LOCKED							423 // Resource is locked (WebDAV)
#define HTTP_FAILED_DEPENDENCY				424 // Request failed because a previous request failed (WebDAV)
#define HTTP_TOO_EARLY						425 // Server is unwilling to process a request that may be replayed
#define HTTP_UPGRADE_REQUIRED				426 // Client must switch to a different protocol (e.g. TLS/1.0 to 1.2)
#define HTTP_PRECONDITION_REQUIRED			428 // Server requires the request to be conditional
#define HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE	431 // Request header fields are too large
#define HTTP_UNAVAILABLE_FOR_LEGAL_REASONS	451 // Content is blocked for legal reasons

// --- HTTP 5xx SERVER ERROR ---

#define HTTP_INTERNAL_SERVER_ERROR			500 // Server encountered an unexpected condition
#define HTTP_NOT_IMPLEMENTED				501 // Server does not support the functionality required to fulfill the request
#define HTTP_BAD_GATEWAY					502 // Gateway server received an invalid response from the upstream server
#define HTTP_SERVICE_UNAVAILABLE			503 // Server is temporarily unable to serve requests (maintenance or overload)
#define HTTP_GATEWAY_TIMEOUT				504 // Gateway did not receive a timely response from the upstream server
#define HTTP_VERSION_NOT_SUPPORTED			505 // Server does not support the HTTP version used in the request

#endif // HTTP_CONSTANTS_HPP
