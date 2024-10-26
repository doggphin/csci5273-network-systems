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
        printf("Current token is: \"%s\"\n", token);

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
                    char *out_file_data,
                    size_t *out_file_data_len,
                    HttpStatusCode *out_status_code)
{

    char* file_extension = strrchr(file_request, '.');
    if(file_extension == NULL) {
        printf("\"%s\" is asking for the index!\n", file_request);
    }
    printf("File extension is %s\n", file_extension);
    return 0;
}


int write_response(HttpRequest *request, char* out_response) {
    int err;
    char content_type_buf[MAXSIZE];
    char file_data_buf[MAXSIZE];
    size_t file_data_len;
    HttpStatusCode status_code;
    FileType file_type;

    printf("Reading file contents!\n");
    err = get_file_content(request->file_path, &file_type, file_data_buf, &file_data_len, &status_code);
    if(err != 0) {
        return err;
    }
    printf("Read file contents!\n");

    char response_status[RESPONSE_STATUS_BUFSIZE];
    get_response_status(status_code, response_status);

    char *keep_alive_buf = "Connection: Keep-alive\r\n\0";

    bzero(out_response, MAXSIZE);
    sprintf(out_response,
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "%s"
        "\r\n",
        response_status, content_type_buf, file_data_len, request->connection_type == CONNECTIONTYPE_KEEP_ALIVE ? keep_alive_buf : ""
    );
    
    return 1;
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

    char response[MAXSIZE];
    if(write_response(&req, response) != 0) {
        perror("Ran into an error making a response!");
        return 1;
    }

    printf("SUCCESS: Wrote response!\n");

    return 0;
}


int main(int argc, char** argv) {
    int port = 7504;
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
                printf("String received from client!\n");

                char response[MAXSIZE];
                handle_request(buf, message_len, response);

                send(conn_fd, buf, message_len, 0);
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