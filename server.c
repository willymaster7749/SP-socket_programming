#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

// perror: print error message
#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int step;
    int type; // 0 for read, 1 for write
    // you don't need to change this.
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // initial 中初始化。size of open file descriptor table, size of request list

// 判斷輸入格式
char who[16];
int num;
int typerror;

// check whether is locked
int r_locked[21] = {0};
int w_locked[21] = {0};

fd_set read_set;

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id; //customer id //902001-902020
    int adultMask;
    int childrenMask;
} Order;

int handle_read(request* reqP) {
    char buf[512];
    read(reqP->conn_fd, buf, sizeof(buf));
    memcpy(reqP->buf, buf, strlen(buf));
    reqP -> buf_len = strlen(reqP -> buf);
    return 0;   
}

void cut_client(request *instance){
    printf("close connection with fd: %d\n", instance -> conn_fd);
    close(instance -> conn_fd);
    free_request(instance);
    return;
}

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
    struct flock lock;
    lock.l_type = type; /* F_RDLCK, F_WRLCK, F_UNLCK */ 
    lock.l_start = offset; /* byte offset, relative to l_whence */ 
    lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */ 
    lock.l_len = len; /* #bytes (0 means to EOF) */
    return(fcntl(fd, cmd, &lock));
}

#define read_lock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
#define write_lock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
#define unlock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))


int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    FD_ZERO(&read_set);

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // open the record file
    int record_fd;
    if((record_fd = openat(AT_FDCWD, "./preorderRecord", O_RDWR)) == -1){
        fprintf(stderr, "Cannot open ./preorderRecord\n");
        exit(1);
    }

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    
    while (1) {

        // scan the available client(including server socket itself)
        FD_ZERO(&read_set);
        for(int i = 0; i < maxfd; i++){
            if(requestP[i].conn_fd != -1){
                FD_SET(requestP[i].conn_fd, &read_set);
            }
        }

        // call select to find who is ready
        int ready_count = select(maxfd, &read_set, NULL, NULL, NULL);

        // someone want to connect
        if(FD_ISSET(svr.listen_fd, &read_set)){
            // Check new connection
            clilen = sizeof(cliaddr);
            // 連結 client，回傳 client's socket 位置 (fd)。後面兩個參數是「空的 socket 資料結構」
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                }
                ERR_EXIT("accept");
            }
            requestP[conn_fd].step = 1;
            requestP[conn_fd].conn_fd = conn_fd;
            // 從那個奇怪的 structure 獲得 client 的 ip (?)
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);

            // printing instruction to this new member client
            char *instruction = "Please enter the id (to check how many masks can be ordered):\n";    
            write(requestP[conn_fd].conn_fd, instruction, strlen(instruction));   
        }
        
        // deal with every ready client
        for(int i = 0; i < maxfd; i++){
            if( i != svr.listen_fd && FD_ISSET(i, &read_set)){
                if(requestP[i].step == 1){
                    int ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                    if (ret < 0) {
                        fprintf(stderr, "bad request from %s\n", requestP[i].host);
                        cut_client(&requestP[i]);
                    }
                    // checking the validity of client's input
                    int client_input = atoi(requestP[i].buf);
                    if(client_input < 902001 || client_input > 902020){
                        char *instruction = "Operation failed.\n";    
                        write(requestP[i].conn_fd, instruction, strlen(instruction));
                        cut_client(&requestP[i]);
                        continue;
                    }

                    // deal with file lock
                    #ifdef READ_SERVER 
                    off_t offset = sizeof(Order) * (client_input - 902001);
                    int return_value;
                    if(w_locked[client_input - 902001] != 0){
                        char *instruction = "Locked.\n";    
                        write(requestP[i].conn_fd, instruction, strlen(instruction));
                        cut_client(&requestP[i]);
                        continue;
                    } else if(read_lock(record_fd, offset, SEEK_SET, sizeof(Order)) < 0 ){
                        char *instruction = "Locked.\n";    
                        write(requestP[i].conn_fd, instruction, strlen(instruction));
                        cut_client(&requestP[i]);
                        continue;
                    }else{
                        r_locked[client_input - 902001]++;
                    }
                    #else
                    off_t offset = sizeof(Order) * (client_input - 902001);
                    if(w_locked[client_input - 902001] != 0 || r_locked[client_input - 902001] != 0){
                        char *instruction = "Locked.\n";    
                        write(requestP[i].conn_fd, instruction, strlen(instruction));
                        cut_client(&requestP[i]);
                        continue;
                    }else if(write_lock(record_fd, offset, SEEK_SET, sizeof(Order)) < 0){
                        char *instruction = "Locked.\n";    
                        write(requestP[i].conn_fd, instruction, strlen(instruction));
                        cut_client(&requestP[i]);
                        continue;
                    } else{
                        w_locked[client_input - 902001]++;
                    }
                    #endif

                    requestP[i].id = client_input;
                    if(lseek(record_fd, sizeof(Order) * (client_input - 902001), SEEK_SET) == -1){
                        printf("Cannot seek record file.\n");
                        exit(1);
                    }
                    Order *buff = (Order *)malloc(sizeof(Order));
                    if(read(record_fd, buff, sizeof(Order)) > 0){
                        char instruction[1024];
                        sprintf(instruction,"You can order %d adult mask(s) and %d children mask(s).\n",
                                buff -> adultMask, buff -> childrenMask);
                        write(requestP[i].conn_fd, instruction, strlen(instruction));   
                    } else {
                        printf("Read Error!\n");
                        exit(1);
                    }

                    #ifdef READ_SERVER 
                    // 關閉且清空連線紀錄、鎖
                    offset = sizeof(Order) * (client_input - 902001);
                    r_locked[client_input - 902001]--;
                    unlock(record_fd, offset, SEEK_SET, sizeof(Order));
                    cut_client(&requestP[i]);
                    continue;
                    #endif

                    requestP[i].step = 2;
                    sprintf(buf,"Please enter the mask type (adult or children) and number of mask you would like to order:\n");
                    write(requestP[i].conn_fd, buf, strlen(buf)); 
                } else if(requestP[i].step == 2){
                    read(requestP[i].conn_fd, buf, sizeof(buf));
                    num = -1;
                    off_t offset;
                    sscanf(buf, "%s %d", who, &num);
                    if((strcmp(who, "adult") != 0 && strcmp(who, "children") != 0) || num == -1){
                        strcpy(buf, "Operation failed.\n");
                        write(requestP[i].conn_fd, buf, strlen(buf));
                        offset = sizeof(Order) * (requestP[i].id - 902001);
                        w_locked[requestP[i].id - 902001]--;
                        unlock(record_fd, offset, SEEK_SET, sizeof(Order));
                        cut_client(&requestP[i]);
                    } else{
                        Order *buff = (Order *)malloc(sizeof(Order));
                        lseek(record_fd, sizeof(Order) * (requestP[i].id - 902001), SEEK_SET);
                        read(record_fd, buff, sizeof(Order));
                        typerror = -1;
                        if(strcmp(who, "adult") == 0){
                            if(buff -> adultMask - num >= 0 && num > 0){
                                buff -> adultMask -= num;
                            }else{
                                typerror = 1;
                            }
                        } else{
                            if(buff -> childrenMask - num >= 0 && num > 0){
                                buff -> childrenMask -= num;
                            }else{
                                typerror = 1;
                            }
                        }
                        if(typerror == 1){
                            strcpy(buf, "Operation failed.\n");
                            write(requestP[i].conn_fd, buf, strlen(buf));
                            offset = sizeof(Order) * (requestP[i].id - 902001);
                            w_locked[requestP[i].id - 902001]--;
                            unlock(record_fd, offset, SEEK_SET, sizeof(Order));
                            cut_client(&requestP[i]);
                        }else{
                            lseek(record_fd, sizeof(Order) * (requestP[i].id - 902001), SEEK_SET);
                            write(record_fd, buff, sizeof(Order));
                            sprintf(buf, "Pre-order for %d successed, %d %s mask(s) ordered.\n", requestP[i].id, num, who);
                            write(requestP[i].conn_fd, buf, strlen(buf));
                            offset = sizeof(Order) * (requestP[i].id - 902001);
                            w_locked[requestP[i].id - 902001]--;
                            unlock(record_fd, offset, SEEK_SET, sizeof(Order));
                            cut_client(&requestP[i]);
                        }
                    }
                }

            }
        }   
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP -> step = 0;
    reqP -> type = 0;
}

static void free_request(request* reqP) {
    // if (reqP->filename != NULL) {
    //     free(reqP->filename);
    //     reqP->filename = NULL;
    // }
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
