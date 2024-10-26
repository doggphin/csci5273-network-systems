#include "http_server.h"

// Writes 
#define RESPONSE_STATUS_BUFSIZE 32
void get_response_status(HttpStatusCode status_code, char* out_buf) {
    char* get_response_status_from_status_code(HttpStatusCode status_code) {
        switch(status_code) {
            case STATUSCODE_OK:
                return "200 OK\0";
            case STATUSCODE_BAD_REQUEST:
                return "400 Bad Request\0";
            case STATUSCODE_FORBIDDEN:
                return "403 Forbidden\0";
            case STATUSCODE_NOT_FOUND:
                return "404 Not Found\0";
            case STATUSCODE_METHOD_NOT_ALLOWED:
                return "405 Method Not Allowed\0";
            case STATUSCODE_HTTP_VERSION_UNSUPPORTED:
                return "505 HTTP Version Not Supported\0";
        }
    }
    
    out_buf = strcpy(out_buf, get_response_status_from_status_code(status_code));
}


int read_request(char *msg, int message_len, HttpRequest *out_request) {
    ConnectionType connection_type;
    RequestMethod request_method;
    char file_path[MAXSIZE];

    char *token;
    char *rest = msg;
    // read_X means read this current iteration's string as X
    // First delimited string should be the method
    for(int read_method = 1, read_file = 0, read_connection = 0; (token = strtok_r(rest, " \r\n", &rest));) {
        if(read_method) {
            if(strcmp(token, "GET") == 0) {
                request_method = REQUESTMETHOD_GET;
            } else if(strcmp(token, "POST") == 0) {
                request_method = REQUESTMETHOD_POST;
            } else if(strcmp(token, "HEAD") == 0) {
                request_method = REQUESTMETHOD_HEAD;
            } else {
                return 1;
            }
            
            printf("SUCCESS: Got the method (%d)!\n", (int)request_method);

            read_file = 1;  // After method, next is always the file name
            read_method = 0;
            continue;
        }
        if(read_file) {
            strcpy(file_path, token);
            read_file = 0;

            printf("SUCCESS: Got the file (%s)!\n", file_path);

            continue;
        }
        if(read_connection) {
            connection_type = strcmp(token, "keep-alive") ? CONNECTIONTYPE_NONE : CONNECTIONTYPE_KEEP_ALIVE;

            printf("SUCCESS: Got the connection type (%d)!\n", (int)connection_type);

            read_connection = 0;
            break;  // No more fields are read after connection
        }

        // Check if next bit marks "keep-alive"
        if(strcmp(token, "Connection:") == 0) {
            read_connection = 1;
        }
    }

    out_request->request_method = request_method;
    out_request->connection_type = connection_type;
    out_request->file_path = file_path;

    return 0;
}


/*
    Expects char buffers to be of size MAXSIZE
*/
int get_file_content(char *file_request,
                    FileType *out_file_type,
                    char **out_file_data,
                    size_t *out_file_data_len,
                    HttpStatusCode *out_status_code)
{
    printf("Getting file extension!\n");
    char* file_extension = strrchr(file_request, '.');
    if(file_extension == NULL && strlen(file_request) == 1 && file_request[0] == '/') {
        file_request = "/index.html\0";
        *out_file_type = FILETYPE_HTML;
    } else {
        printf("Getting something that isn't the index, %s!\n", file_extension);
        if(strcmp(file_extension, "html") == 0) {
            *out_file_type = FILETYPE_HTML;
        } else if(strcmp(file_extension, ".txt") == 0) {
            *out_file_type = FILETYPE_TXT;
        } else if(strcmp(file_extension, ".png") == 0) {
            *out_file_type = FILETYPE_PNG;
        } else if(strcmp(file_extension, ".gif") == 0) {
            *out_file_type = FILETYPE_GIF;
        } else if(strcmp(file_extension, ".jpg") == 0) {
            *out_file_type = FILETYPE_JPG;
        } else if(strcmp(file_extension, ".ico") == 0) {
            *out_file_type = FILETYPE_ICO;
        } else if(strcmp(file_extension, ".css") == 0) {
            *out_file_type = FILETYPE_CSS;
        } else if(strcmp(file_extension, ".js") == 0) {
            *out_file_type = FILETYPE_JS;
        } else {
            *out_file_type = FILETYPE_ERROR;
            *out_status_code = STATUSCODE_BAD_REQUEST;
            *out_file_data = (char*)malloc(256 * sizeof(char));
            sprintf(*out_file_data, "Extension \"%s\" of requested file \"%s\" not recognized!", file_extension, file_request);
            printf("Ran into error with extension: \"%s\"\n", *out_file_data);
            *out_file_data_len = strlen(*out_file_data);
            return 0;
        }
    }
    
    printf("SUCCESS: Got the file extension (%d)!\n", *out_file_type);
    printf("SUCCESS: File to read is \"%s\"!\n", file_request);

    char formatted_file_request[256];
    sprintf(formatted_file_request, ".%s", file_request);

    FILE *file_ptr = fopen(formatted_file_request, "rb");
    if(file_ptr == NULL) {
        *out_file_type = FILETYPE_ERROR;
        *out_status_code = STATUSCODE_NOT_FOUND;
        *out_file_data = (char*)malloc(256 * sizeof(char));
        sprintf(*out_file_data, "Requested file does not exist!");
        *out_file_data_len = strlen(*out_file_data);
        return 0;
    }
    fseek(file_ptr, 0, SEEK_END);
    *out_file_data_len = ftell(file_ptr);
    rewind(file_ptr);

    *out_file_data = (char*)malloc(*out_file_data_len * sizeof(char));
    fread(*out_file_data, *out_file_data_len, 1, file_ptr);
    fclose(file_ptr);
    
    return 0;
}


int write_response(HttpRequest *request, char* out_response) {
    int err;
    char *file_data_buf;
    size_t file_data_len;
    HttpStatusCode status_code = STATUSCODE_OK;
    FileType file_type;

    err = get_file_content(request->file_path, &file_type, &file_data_buf, &file_data_len, &status_code);
    if(err != 0) {
        printf("Returning early!\n");
        return err;
    }

    char response_status[RESPONSE_STATUS_BUFSIZE];
    get_response_status(status_code, response_status);

    char *keep_alive_buf;
    switch(request->connection_type) {
        case CONNECTIONTYPE_KEEP_ALIVE:
            keep_alive_buf = "Connection: Keep-alive\r\n\0";
            break;
        default:
            keep_alive_buf = "";
            break;
    }
    
    char *content_type_buf;
    switch(file_type) {
        case FILETYPE_ERROR:
            content_type_buf = "text/plain\0";
            break;
        case FILETYPE_HTML:
            content_type_buf = "text/html\0";
            break;
        case FILETYPE_TXT:
            content_type_buf = "text/plain\0";
            break;
        case FILETYPE_PNG:
            content_type_buf = "image/png\0";
            break;
        case FILETYPE_GIF:
            content_type_buf = "image/gif\0";
            break;
        case FILETYPE_JPG:
            content_type_buf = "image/jpg\0";
            break;
        case FILETYPE_ICO:
            content_type_buf = "image/x-icon\0";
            break;
        case FILETYPE_CSS:
            content_type_buf = "text/css\0";
            break;
        case FILETYPE_JS:
            content_type_buf = "application/javascript\0";
            break;
        default:
            content_type_buf = "\0";
            break;
    }


    bzero(out_response, MAXSIZE);
    sprintf(out_response,
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "%s"
        "\r\n",

        response_status, 
        content_type_buf, 
        file_data_len,
        request->connection_type == CONNECTIONTYPE_KEEP_ALIVE ? keep_alive_buf : ""
    );


    for(int i=0; i<MAXSIZE; i++) {
        if(out_response[i] == '\0') {
            memcpy(&out_response[i], file_data_buf, file_data_len);
            break;
        }
    }

    printf(
        "SUCCESS: Response is:\n"
        "=-=-=-=-=\n"
        "%s\n"
        "=-=-=-=-=\n"
        , out_response
    );
    
    free(file_data_buf);

    return 0;
}


int handle_request(char* msg, int req_len, char* out_response) {
    printf("START: Handling request!\n");

    HttpRequest req;
    if(read_request(msg, req_len, &req) != 0) {
        perror("Received an invalid request!");
        return 1;
    }

    printf("SUCCESS: Read request!\n");
    printf("START: Writing response!\n");

    if(write_response(&req, out_response) != 0) {
        perror("Ran into an error making a response!");
        return 1;
    }


    printf("SUCCESS: Wrote response!\n");
    return 0;
}


int main(int argc, char** argv) {
    int port = 7520;
    int listen_fd, conn_fd, message_len;
    pid_t child_pid;
    socklen_t client_len;
    char buf[MAXSIZE];
    struct sockaddr_in client_addr, server_addr;

    // Create a socket for the socket
    // If sockfd < 0, there was an error in the creation of the socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Problem in creating the socket!");
        exit(2);
    }

    // Prepare socket address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    //listen to the socket by creating a connection queue, then wait for clientents
    listen (listen_fd, LISTENQ);

    for(;;) {
        client_len = sizeof(client_addr);
        
        //Accept a connection
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    
        printf("Received request...\n");

        // If it's 0, this is a child process
        if((child_pid = fork()) == 0) {
            printf("Child created for dealing with client requests.\n");
            close(listen_fd);

            while((message_len = recv(conn_fd, buf, MAXSIZE, 0)) > 0) {
                char response[MAXSIZE];
                handle_request(buf, message_len, response);
                send(conn_fd, response, strlen(response), 0);
            }   

            if(message_len < 0) {
                perror("Read error!");
                exit(1);
            }

            close(conn_fd);
            exit(0);
        }
    }

    close(listen_fd);
}