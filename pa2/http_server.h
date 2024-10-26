#ifndef HTTP_SERVER
#define HTTP_SERVER

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define MAXSIZE 4096
#define LISTENQ 8

typedef enum {
    FILETYPE_ERROR = 0,
    FILETYPE_HTML = 1,
    FILETYPE_TXT = 2,
    FILETYPE_PNG = 3,
    FILETYPE_GIF = 4,
    FILETYPE_JPG = 5,
    FILETYPE_ICO = 6,
    FILETYPE_CSS = 7,
    FILETYPE_JS = 8,
} FileType;

typedef enum {
    STATUSCODE_OK                          = 200,
    STATUSCODE_BAD_REQUEST                 = 400,
    STATUSCODE_FORBIDDEN                   = 403,
    STATUSCODE_NOT_FOUND                   = 404,
    STATUSCODE_METHOD_NOT_ALLOWED          = 405,
    STATUSCODE_HTTP_VERSION_UNSUPPORTED    = 505,
} HttpStatusCode;

typedef enum {
    HTTP_1_0,
    HTTP_1_1
} HttpVersion;

typedef enum {
    REQUESTMETHOD_GET = 0,
    REQUESTMETHOD_POST = 1,
    REQUESTMETHOD_HEAD = 2,
} RequestMethod;

typedef enum {
    CONNECTIONTYPE_KEEP_ALIVE = 0,
    CONNECTIONTYPE_NONE = 1
} ConnectionType;

typedef struct HttpRequest {
    RequestMethod request_method;
    ConnectionType connection_type;
    char* file_path;
} HttpRequest;

#endif