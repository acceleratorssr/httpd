#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

/*C 库函数 int isspace(int c) 检查所传的字符是否是空白字符。
标准的空白字符包括：
' '     (0x20)    space (SPC) 空格符
'\t'    (0x09)    horizontal tab (TAB) 水平制表符    
'\n'    (0x0a)    newline (LF) 换行符
'\v'    (0x0b)    vertical tab (VT) 垂直制表符
'\f'    (0x0c)    feed (FF) 换页符
'\r'    (0x0d)    carriage return (CR) 回车符*/
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void error_die(const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(uint16_t *);
void unimplemented(int);

/**********************************************************************/
/* 函数作用:解析HTTP请求，检查请求的文件路径，然后返回相应的响应
 * 参数:连接到客户端的套接字*/
/**********************************************************************/
void accept_request(int client) //原来的函数:void accept_request(void *arg)
{
    /*源代码此处有错误,*/
    //环境是一个 64 位的系统,故 intptr_t 是一个 8 字节（64位）的整数类型
    //而int 是4字节（32位）的整数类型
    // int client = (intptr_t)arg;  
    // 可将原来的void *直接改成传入int client
    /**********************************************************************/
    printf("client: %d\n", client);
    char buf[1024];             // 用于存储 HTTP 请求头
    size_t numchars;
    char method[255];           // 存储 HTTP 请求方法（GET、POST 等）
    char url[255];              // 存储请求的 URL
    char path[512];             // 存储请求的文件路径
    size_t i, j;
    struct stat st;
    char *query_string = NULL;

    // 从客户端读取一行 HTTP 请求头
    numchars = get_line(client, buf, sizeof(buf));//numchars为一行存储的字节数(不包括null)，buf最后一个字符为空字符
    printf("numchars: %zu\n", numchars);
    i = 0; j = 0;
    for(int ii=0; buf[ii]!='\0'; ii++)
    {
        printf("%c", buf[ii]);
    }
    printf("1****\n");
    //提取一个以空白字符为分隔符的子字符串（' '空格符、'\t'水平制表符、'\n'换行符、'\v'垂直制表符'、\f'换页符、'\r'回车符）
    // 解析 HTTP 请求头的第一个单词，通常是请求方法（GET、POST 等）
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';
    printf("method: \n");
    //判断字符串是否相等的函数，忽略大小写。s1和s2中的所有字母字符在比较之前都转换为小写。
    //该strcasecmp()函数对空终止字符串进行操作。函数的字符串参数应包含一个(’\0’)标记字符串结尾的空字符。
    // 检查请求方法是否为 GET 或 POST，如果不是，则返回未实现
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))//等于0即为相同，因为0是通过值，所以使用&&，有0就可以跳过执行
    {
        unimplemented(client);
        return;
    }
    printf("3****\n");

    i = 0;
    // 跳过空白字符，继续解析 URL 部分
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    printf("4****\n");
    // 跳过空白字符，继续解析 URL 部分
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
    printf("5****\n");
    // 如果请求方法是 POST，将 cgi 标志设置为 1，表示这是一个 CGI 请求
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        // 查找查询字符串的分隔符 '?'，如果找到，将其替换为 NULL 终止符，并将 query_string 指向查询字符串的起始
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            *query_string = '\0';
            query_string++;
        }
    }
    printf("6****\n");
    // 根据 URL 构建文件路径，加上 "htdocs" 前缀
    sprintf(path, "htdocs%s", url);

    // 如果路径以 '/' 结尾，追加 "index.html"
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    // 使用 stat 函数检查文件的状态，以判断文件是否存在
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))  // 读取并丢弃 HTTP 请求头
            numchars = get_line(client, buf, sizeof(buf));

        not_found(client);// 返回 404 Not Found 响应
    }
    else
    {
        // 如果文件是目录，则追加 "index.html" 并再次使用 stat 检查
        if(S_ISDIR(st.st_mode))
            strcat(path, "/index.html");

        serve_file(client, path);                       // 返回静态文件
    }
    printf("7****\n");
    close(client);
}

/**********************************************************************/
/* 通知客户端它发出的请求有问题
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* 函数作用:将文件内容输出到套接字，该函数被称为 "cat"，因为它执行类似于Unix "cat"命令的操作
 * 参数:客户端套接字的描述符
 *      要复制到套接字的文件的FILE指针。这个参数指定了要发送的文件。
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* 函数作用:使用perror()输出错误消息(用于系统错误;根据errno(表示系统调用错误)的值，
 * 并退出表示错误的程序。*/
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);//读出\n结束循环
        if (n > 0)
        {
            if (c == '\r')//\r:当前位置移到本行开头
            {
                //MSG_PEEK标志会将套接字接收队列中的可读的数据拷贝到缓冲区，但不会使套接子接收队列中的数据减少，
                //例如调用recv或read后，原本会导致套接字接收队列中的数据被读取后而减少，但是指定了MSG_PEEK标志之后，
                //可通过返回值获得可读数据长度，并且不会减少套接字接收缓冲区中的数据，所以可以供程序的其他部分继续读取。
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);//需要空字符结束所读取的字符串，所以不能保存\n
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else//发生错误时将c设置为 '\n' 并结束循环，以确保不会在缓冲区 buf 中留下未定义或错误的数据
        {
            printf("error\n");
            c = '\n';
        }    
    }
    buf[i] = '\0';//最后加上空字符
    printf("i : %d\n", i);
    return(i);
}

/**********************************************************************/
/* 函数作用:为一个HTTP响应，设置并发送响应报文
 * 参数:客户端连接的文件描述符，filename指向文件，此处没用上*/
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    for (size_t i = 0; i < strlen(buf); i++)
        printf("%c", buf[i]);
    fputc('\n', stdout);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    for (size_t i = 0; i < strlen(buf); i++)
        printf("%c", buf[i]);
    fputc('\n', stdout);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    for (size_t i = 0; i < strlen(buf); i++)
        printf("%c", buf[i]);
    fputc('\n', stdout);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 函数作用:给客户端一个404，未找到状态消息. 
 * 参数:客户端套接字*/
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 函数作用:发送一个普通文件到客户端。包括响应报文和发送回客户端的文件，并在发生错误时向客户端报告。
 * 形参:客户端套接字
 * 要发送的文件位置*/
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* 函数作用:初始化服务器，侦听指定端口上的web连接的过程。如果端口为0，
 * 则动态分配端口并修改原始端口变量以反映实际端口。
 * 参数:指向包含要连接的端口的变量的指针
 * 返回:服务器套接字*/
/**********************************************************************/
int startup(uint16_t *port)
{
    int ser_httpd = 0;
    int on = 1;
    struct sockaddr_in ser_name;

    ser_httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (ser_httpd == -1)
        error_die("socket() error");
    memset(&ser_name, 0, sizeof(ser_name));
    ser_name.sin_family = AF_INET;
    ser_name.sin_port = htons(*port);
    ser_name.sin_addr.s_addr = htonl(INADDR_ANY);
    //SOL_SOCKET为协议层，SO_REUSEADDR为对应的选项名，名单可见《TCP/IP网络编程》p140
    //SO_REUSEADDR默认值为0（flase），on设置为1（true），为true时，将Time-wait状态下的
    //套接字端口号重新分配给新的套接字端口号
    if ((setsockopt(ser_httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }
    if (bind(ser_httpd, (struct sockaddr *)&ser_name, sizeof(ser_name)) < 0)
        error_die("bind() error");
    if (*port == 0)  /* 如果输入端口号为0，即为动态分配端口 */
    {
        socklen_t namelen = sizeof(ser_name);
        // 获取系统随机分配的端口号,调用getsockname后，ser_name.sin_port即为系统随机分配的端口号
        if (getsockname(ser_httpd, (struct sockaddr *)&ser_name, &namelen) == -1)
            error_die("getsockname() error");
        *port = ntohs(ser_name.sin_port);
    }
    if (listen(ser_httpd, 5) < 0)
        error_die("listen() error");
    return(ser_httpd);
}

/**********************************************************************/
/* 函数作用:通知客户端，请求的web方法尚未实现。
 * 参数:客户端套接字*/
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];
    printf("unimplemented\n");
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;
    uint16_t port = 9190;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t  client_name_len = sizeof(client_name);

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        printf("****\n");
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept() error");
        printf("%d\n", client_sock);
        accept_request(client_sock);
    }

    close(server_sock);

    return(0);
}
