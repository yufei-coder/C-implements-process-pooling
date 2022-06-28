#include <iostream>
#include <string>
#include <error.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "process_pool.h"
#include "error_notification.h"
class CGI_request
{
private:
    static const int MAX_BUFFER_SIZE = 1024;
    static int process_epoll_fd;
    int client_socketf_fd;
    struct sockaddr_in client_address;
    char client_request_buffer[MAX_BUFFER_SIZE];
    int current_read_point_next_pt;

public:
    CGI_request(/* args */);
    void initial(int client_epoll_fd, int temp_client_socketf_fd, const sockaddr_in &temp_client_address);  //Initialize function.
    void process();
    ~CGI_request();
};
int CGI_request::process_epoll_fd = -1;
CGI_request::CGI_request(/* args */) // constructor
{
    client_socketf_fd=-1;
    memset(&client_address,0,sizeof(client_address));
    memset(client_request_buffer,'\0',sizeof(client_request_buffer));
    current_read_point_next_pt=NULL;
}
void CGI_request::initial(int temp_client_epoll_fd, int temp_client_socketf_fd, const sockaddr_in &temp_client_address) // Initialize function
{
    process_epoll_fd = temp_client_epoll_fd;
    client_socketf_fd = temp_client_socketf_fd;
    client_address = temp_client_address;
    memset(client_request_buffer, '\0,', sizeof(client_request_buffer));
    current_read_point_next_pt = 0;
}
void CGI_request::process() //The function process client request.
{
    int function_flag;
    int temp_current_read_point_next_pt;

    while (true)       //By loop to recieve client request.
    {
        temp_current_read_point_next_pt = current_read_point_next_pt;
        function_flag = recv(client_socketf_fd, client_request_buffer + temp_current_read_point_next_pt, sizeof(client_request_buffer + temp_current_read_point_next_pt), 0);
        if (function_flag < 0)  //An error oucrren
        {
            if (errno != EAGAIN)
            {
                del_epoll_event(process_epoll_fd, client_socketf_fd);
            }
            break;
        }
        else if(function_flag==0)   //Client close socket file descriptor, so server close relevant file descriptor.
        {
            del_epoll_event(process_epoll_fd,client_socketf_fd);
            break;
        }
        else
        {
            current_read_point_next_pt+=function_flag;
            char *buffer_current_pt;
            buffer_current_pt=strrchr(client_request_buffer,'\n');
            if(!buffer_current_pt)
            {
                break;
            }
            client_request_buffer[current_read_point_next_pt]='\0';
            if(access(client_request_buffer,F_OK))
            {
                del_epoll_event(process_epoll_fd,client_socketf_fd);
                break;
            }
            function_flag=fork();
            if (function_flag>0)
            {
                del_epoll_event(process_epoll_fd,client_socketf_fd);
                break;
            }
            else if (function_flag==-1)
            {
                del_epoll_event(process_epoll_fd,client_socketf_fd);
                break;
                /* code */
            }
            else
            {
                dup2(client_socketf_fd,STDOUT_FILENO);
                execl(client_request_buffer,client_request_buffer,(char *)NULL);
                exit(0);
            }
        }
        /* code */
    }
}
CGI_request::~CGI_request()
{
}

int main(int argc, char *argv[])
{
    using std::cout;
    using std::cin;
    using std::endl;
    if(argc<2)
    {
        cout<<"Usage: "<<basename(argv[0])<<"ip_address port"<<endl;
        return -1;
    }
    const char *bind_ip_address=argv[1];
    int bind_port_number=atoi(argv[2]);
    int process_listen_fd=socket(AF_INET,SOCK_STREAM,0);
    error_detection(process_listen_fd,"socket");
    int function_flag;
    struct sockaddr_in bind_address;
    memset(&bind_address,0,sizeof(bind_address));
    bind_address.sin_family=AF_INET;
    inet_pton(AF_INET,bind_ip_address,&bind_address.sin_addr);
    bind_address.sin_port=htons(bind_port_number);
    function_flag=bind(process_listen_fd,(sockaddr *)&bind_address,sizeof(bind_address));
    error_detection(function_flag,"bind");
    function_flag=listen(process_listen_fd,5);
    error_detection(function_flag,"listen");
    Process_Pool_Class<CGI_request> *process_pool_instance=Process_Pool_Class<CGI_request>::create_process_pool(process_listen_fd);
    if (process_pool_instance)
    {
        process_pool_instance->run_process_pool();
        delete process_pool_instance;
        /* code */
    }
    else
    {
        cout<<"Create process poll instance error."<<endl;
    }
    close(process_listen_fd);
    return 0;
}
