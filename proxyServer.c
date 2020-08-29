#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

//---------------------------Forward Declarations-------------------------//

//---------------------------Structures-----------------------------------//
//the next structure will hold data of the proxyserver
typedef struct proxy_data{
    unsigned int port;  //port of the proxy server
    unsigned int pool_size; 
    unsigned int num_request;
    char** filter;  //array of hosts to filter
    int num_lines;  //num  of hosts in filter
} proxy_data_t;

//the next structure will hold data of a request by the client
typedef struct request_data{
    unsigned int port;  //port of the server
    char* request;  //the parsed request
    char* host; //the parsed host
    char* protocol_type; // HTTP/1.0 or HTTP/.1.1
} request_data_t;

//--------------------------======----------------------------------//

//---------------------------Functions-----------------------------------//

//the next function gets argc and argv[], check for invalid input , and returns initialized proxy_data structure.
//if there is a isage error it returns NULL;
proxy_data_t* parse_cmd(int argc , char* argv[]);

//the next function gets a file and returns array of strings included in file seperated by lines, (used in parse_cmd function).
char** get_filter(FILE* file , int* counter);

//the next function is the (dispatch_fn) function which will be sent to the threads working,
//the function gets a client fd socket and "handles" it. it parses the request, 
//if the request is valid the function attempts to connect the server, else it returns an error message to the client.
int client_handler(void* arg);

//the next function gets the request from the client (string) checks its validation and returns 
//request_structure , if the host field is NULL then it holds an error in the request field
//to send the client. (used in client handler)
request_data_t* parse_request (char* str);

//the next function gets a number of error and the protocol type of the request , and concatenates an coordinate
//error message to a string to send later to the client. (used in client handler)
char* error_handler (int flag , char* protocol_type);

//the next function gets the data of a request and the client socket fd, and attempts to connect the server,
//it returns the answer to the client immediately. (used in client handler)
void connect_server(request_data_t* request_data , int client_sd);

//the next function gets a structure of request data and deallocates all it's fields. (used in client handler)
void destroy_data(request_data_t* request_data);

//---------------------------------------==============----------------------------------//

////-----------------------GLOBAL VARIABLE-----------------//
proxy_data_t* data; //(filter use)  
//--------------------======-------------------------//

//------------------------------------End Of Declarations--------------------------------//


//---------------------------The Program-----------------------------------//
int main (int argc , char* argv[])
{
    data = parse_cmd(argc,argv); //check args.
    if(data == NULL)
        return 0;
    int welcome_sd;		/* socket descriptor */
    if((welcome_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
	    perror("socket");
	    exit(1);
    }
    struct sockaddr_in srv;	/* used by bind() */
     /* create the socket */
    srv.sin_family = AF_INET; /* use the Internet addr family */
    srv.sin_port = htons(data->port); /* bind socket ‘fd’ to the port */
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(welcome_sd, (struct sockaddr*) &srv, sizeof(srv)) < 0) 
    {
        perror("bind"); 
        exit(1);
    }
    if(listen(welcome_sd, 5) < 0) 
    {
	    perror("listen");
	    exit(1);
    }
    threadpool* tp = create_threadpool(data->pool_size); //create threadpool.
    if(tp == NULL)
        return 0;
    for(int i = 0 ; i<data->num_request; i++)   //accept clients, for each client dispatch handler to threadpool.
    {
        int newfd = accept(welcome_sd,(struct sockaddr*) NULL,NULL);
        if(newfd < 0)
            continue;
        dispatch(tp,(dispatch_fn)client_handler,&newfd);
    }
    destroy_threadpool(tp);

    /*DESTROY PROXY DATA*/
    if(data != NULL)
    {
        if(data->filter != NULL)
        {
            for(int i = 0 ; i < data->num_lines ; i++)
                if(data->filter[i] != NULL)
                    free(data->filter[i]);
            free(data->filter);
        }
        free(data);
    }
    close(welcome_sd);
    return 0;
}

proxy_data_t* parse_cmd(int argc , char* argv[])
{
    //check for 4 arguments and atoi() of the last three argumenst.
    if(argc != 5)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        return NULL;
    }
    for(int i = 1; i < 4 ; i++)
        if(atoi(argv[i]) <= 0)
        {
            printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
            return NULL;
        }
    //init data fields.
    data = (proxy_data_t*)malloc(sizeof(proxy_data_t));
    if(data == NULL)
    {
        perror("MALLOC FAILED");
        exit(1);
    }
    data->port = atoi(argv[1]);
    data->pool_size = atoi(argv[2]);
    data->num_request = atoi(argv[3]);
    data->num_lines = 0;
    FILE* filterfile = fopen(argv[4], "r"); //open filter file.
    if(filterfile == NULL)
    {
        free(data);
        printf("FAILED READ FROM FILTER FILE.");
        return NULL;
    }
    else
        data->filter = get_filter(filterfile,&(data->num_lines)); //convert the file to array of strings.
    fclose(filterfile);
    return data;   
}

char** get_filter(FILE* file , int* counter)
{
    char** filter = (char**)malloc(sizeof(char*));
    if(filter == NULL)
    {
        perror("MALLOC FAILED");
        exit(1);
    }
    
    size_t len = 0;
    char *line = NULL;
    while(getline(&line, &len, file) != -1) //break the file to array of strings , each string holds a line.
    {
        filter = (char**)realloc(filter,(*counter+1)*(sizeof(char*)));
        if(filter == NULL)
        {
            perror("MALLOC FAILED");
            exit(1);
        }
        filter[*counter] = line;
        (*counter)++;
        line = NULL;
    }
    free(line);
    for(int i = 0; i<(*counter);i++) //remove the \r\n\ from the strings.
        strtok(filter[i],"\r\n");
    return filter; 
}

request_data_t* parse_request (char* str)
{
    //init request structure
    request_data_t* request_data = (request_data_t*)malloc(sizeof(request_data_t)); 
    if(request_data == NULL) //malloc failed 
    {   
        perror("MALLOC FAILED");
        return NULL;
    }
    request_data->port = 80;
    request_data->host = NULL;
    request_data->request = NULL;
    request_data->protocol_type = NULL;
    //get the first line of request.
    char* first_line = strtok(str , "\r\n");
    if(first_line == NULL) //case no \r\n is in request
    {
        request_data->request = error_handler(400,"HTTP/1.0");
        return request_data;
    }
    //get rest of the lines 
    char* rest_lines = strtok(NULL ,"\0");
    char* method = strtok(first_line , " "); //get the first word
    if(method == NULL)  //case of invalid first line.
    {
        request_data->request = error_handler(400,"HTTP/1.0");
        return request_data;
    }
    char* path = strtok(NULL , " "); //get the second word.
    if(path == NULL)    //case of invalid first line.
    {
        request_data->request = error_handler(400,"HTTP/1.0");
        return request_data;
    }
    char* protocol_type = strtok(NULL , " "); //get the third word
    if(protocol_type == NULL) //case of invalid first line.
    {
        request_data->request = error_handler(400,"HTTP/1.0");
        return request_data;
    }
    request_data->protocol_type = (char*)calloc((int)strlen(protocol_type)+1,sizeof(char));
    if(request_data->protocol_type == NULL)
    {   
        perror("MALLOC FAILED");
        return NULL;
    }
    strcpy(request_data->protocol_type,protocol_type);
    if(strcmp(protocol_type,"HTTP/1.1") != 0 && strcmp(protocol_type,"HTTP/1.0") != 0)  //case the protocol type is invalid
    {
        request_data->request = error_handler(400,request_data->protocol_type);
        return request_data;
    }
    if(rest_lines ==NULL)   //case there is only one line.
    {
        request_data->request = error_handler(400,request_data->protocol_type);
        return request_data;
    }
    char* line = strtok(rest_lines , "\r\n"); //get the next line.
    char* host = NULL;
    while(line != NULL) //for each line in the request search for "Host:" , and get the host token.  
    {
        if(strncmp(line,"Host:",5) == 0)
            {
                host = line+5;
                while((*host) == ' ')
                    host++;
            }
        line = strtok(NULL , "\r\n");
    }
    if(host == NULL) //case there is no host in the request (invalid).
    {
        request_data->request = error_handler(404,request_data->protocol_type);
        return request_data;
    }
    char* port_ptr = strchr(host,':');  //check if there is a port next to the host.
    if(port_ptr != NULL) //atoi the port, if exist.
    {
        request_data->port = atoi(port_ptr+1);
        if(request_data->port == 0)
        {
            request_data->request = error_handler(404,request_data->protocol_type);
            return request_data;
        }
    }
    if(strcmp(method , "GET") != 0) //check if the method is GET.
    {  
        request_data->request = error_handler(501,request_data->protocol_type);
        return request_data;
    }
    for(int i = 0 ; i < data->num_lines ; i++)  //check if the host is forbidden in filter file.
    {
        if(strcmp(data->filter[i],host) == 0)
            {
                request_data->request = error_handler(403,request_data->protocol_type);
                return request_data;
            }
    }
    
    int size = 37 + (int)strlen(path) + (int)strlen(protocol_type) + (int)strlen(host); //measuring the size of the string to be concatenate.
    char* request = (char*)calloc(size,sizeof(char)); 
    if(request == NULL)
    {   
        perror("MALLOC FAILED");
        return NULL;
    }
    //concatenate the request to send the server.
    strcat(request , "GET ");
    strcat(request , path);
    strcat(request , " ");
    strcat(request , protocol_type);
    strcat(request , "\r\nHost: ");
    strcat(request , host);
    strcat(request , "\r\nConnection: close\r\n\r\n");
    request_data->request = request;
    if(port_ptr !=NULL)
        *port_ptr = '\0';
    request_data->host = (char*)calloc((int)strlen(host)+1,sizeof(char));  
    if(request_data->host == NULL)
    {   
        perror("MALLOC FAILED");
        return NULL;
    }
    strcpy(request_data->host,host);
    return request_data;
}

char* error_handler (int flag , char* protocol_type)
{
    char* error_type = NULL;
    char* error_description = NULL;
    char* content_length = NULL;
    switch(flag) //init the error in coordinate to the flag.
    {
        case 400:
        error_type = "400 Bad Request";
        error_description = "Bad Request.";
        content_length = "113";
        break;
        case 403:
        error_type = "403 Forbidden";
        error_description = "Access denied.";
        content_length = "111";
        break;
        case 404:
        error_type = "404 Not Found";
        error_description = "File not found.";
        content_length = "112";
        break;
        case 500:
        error_type = "500 Internal Server Error";
        error_description = "Some server side error.";
        content_length = "144";
        break;
        case 501:
        error_type = "501 Not supported";
        error_description = "Method is not supported.";  
        content_length = "129";    
    }
    char* error = (char*)calloc(300,sizeof(char));
    if(error == NULL)
    {   
        perror("MALLOC FAILED");
        return NULL;
    }
    //concatenate the error to send the client.
    strcat(error , protocol_type);
    strcat(error , " ");
    strcat(error , error_type);
    strcat(error , "\r\nServer: webserver/1.0\r\nContent-Type: text/html\r\nContent-Length: ");
    strcat(error , content_length);
    strcat(error , "\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>");
    strcat(error , error_type);
    strcat(error , "</TITLE></HEAD>\r\n<BODY><H4>");
    strcat(error , error_type);
    strcat(error , "<H4>\r\n");
    strcat(error , error_description);
    strcat(error , "\r\n</BODY></HTML>\r\n\r\n");
    return error;
}

int client_handler(void* arg)
{
    int* client_sd = (int*)arg; //cast the socketfd to int*
    int rc ; //how many chars read.
    int size = 0; // size to realloc the request.
    char buffer[256]; //temporary buffer to hold read data.
    char* request = (char*)calloc(1,sizeof(char));
    if(request == NULL)
    {
        perror("MALLOC FAILED");
        char* msg = error_handler(500,"HTTP/1.0");
        write(*client_sd,msg,strlen(msg));
        close(*client_sd);
        free(msg);
        return 0;
    }
    request_data_t* request_data = NULL;
    while(1)    //the loop reads request from the client and stores it in "request".
    {
        memset(buffer,'\0',256);   // clear the buffer  
        rc = read(*client_sd,buffer,255); 
        if(rc == 0) //case read ended
            break;
        if (rc < 0) //case read failed
            break;
        size += rc; //new size to realloc.
        request = (char*)realloc(request,(size+1)*(sizeof(char))); //reallocing the size to concatenate the request.
        if(request == NULL)
        {
            perror("MALLOC FAILED");
            char* msg = error_handler(500,request_data->protocol_type);
            write(*client_sd,msg,strlen(msg));
            close(*client_sd);
            free(msg);
            destroy_data(request_data);
            return 0;
        }
        strcat(request,buffer);
        if(strstr(request,"\r\n\r\n") != NULL)  //request ends with "\r\n\r\n"
            break;
    }
    request_data = parse_request(request);  
    free(request);  //no longer needed.
    if(request_data == NULL)    //problem occured in parse_reqeust (probably malloc).
    {
        char* msg = error_handler(500,request_data->protocol_type);
        write(*client_sd,msg,strlen(msg));
        close(*client_sd);
        free(msg);
        destroy_data(request_data);
        return 0;
    }
    if(request_data->host == NULL)  //not a valid request (error message stored in request_data->request).
    {
        write(*client_sd,request_data->request,strlen(request_data->request));
        close(*client_sd);
        destroy_data(request_data);
        return 0;
    }
    connect_server(request_data,*client_sd); 
    close(*client_sd);
    destroy_data(request_data);
    return 0;
}

void connect_server(request_data_t* request_data , int client_sd)
{
    struct sockaddr_in serv_addr;
    struct hostent* server;
    int rc ,server_sd ;
    unsigned char uc_buffer[256];
    server = gethostbyname(request_data->host);  //parse the host (get ip address).
    server_sd = socket(AF_INET , SOCK_STREAM , 0); //opening the socket to the server
    if (server_sd <0)  //case fd failed
    {
        char* msg = error_handler(500,request_data->protocol_type);
        write(client_sd,msg,strlen(msg));
        free(msg);
        return;
    }
    if (server == NULL) //case the host does not exist.
    {
        char* msg = error_handler(404,request_data->protocol_type);
        write(client_sd,msg,strlen(msg));
        free(msg);
        return;
    }
    //the next 3 lines initialize the internet socket address
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; 
    bcopy((char*)server->h_addr_list[0] , (char*)&serv_addr.sin_addr.s_addr,server->h_length);
    serv_addr.sin_port = htons(request_data->port);
    if (connect(server_sd,(const struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
    {
        char* msg = error_handler(404,request_data->protocol_type);
        write(client_sd,msg,strlen(msg));
        free(msg);
        return;
    }
    write(server_sd,request_data->request,strlen(request_data->request));   //send the request to the server
    while(1)    //the loop reads the respose from the server, and sends to client immediately.
    {
        memset(uc_buffer,'\0',256);   // clear the buffer   
        rc = read(server_sd,uc_buffer,255); 
        if (rc < 0) //case read failed
            break;
        if (rc == 0) // case there is no more chars to read
            break;
        send(client_sd, uc_buffer, rc, MSG_NOSIGNAL );
    }
}

void destroy_data(request_data_t* request_data)
{
    /*DESTROY REQUEST DATA*/
    if(request_data != NULL)
    {
        if(request_data->host != NULL)
            free(request_data->host);
        if(request_data->protocol_type != NULL)
            free(request_data->protocol_type);
        if(request_data->request != NULL)
            free(request_data->request);
        free(request_data);
    }
}
