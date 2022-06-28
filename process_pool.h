#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <assert.h>
class Process_information_Class // The class stores information for process.
{
private:
    pid_t pid_number;
    int communicate_pipefd[2];
    /* data */
public:
    Process_information_Class();
    Process_information_Class(pid_t temp_pid_number, int *communicate_pipefd_pt);
    ~Process_information_Class();
};
Process_information_Class::Process_information_Class()
{
    pid_number = -1;
    communicate_pipefd[0] = -1;
    communicate_pipefd[1] = -1;
}
Process_information_Class::Process_information_Class(pid_t temp_pid_number, int *communicate_pipefd_pt)
{
    pid_number = temp_pid_number;
    communicate_pipefd[0] = communicate_pipefd_pt[0];
    communicate_pipefd[1] = communicate_pipefd_pt[1];
}
Process_information_Class::~Process_information_Class()
{
}
static int signal_pipefd[2]; // The signal_pipefd used to process signal,the aim is to achieve a unified signal source.
int setnonblocking(int fd)   // Set file descriptor nonblock flag;
{
    int old_fd_flag = fcntl(fd, F_GETFL);
    int new_fd_flag = old_fd_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_fd_flag);
    return old_fd_flag;
}

void add_epoll_event(int epoll_fd, int fd) // Add file descriptor event to epoll interest list;
{
    struct epoll_event events;
    events.data.fd = fd;
    events.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
    setnonblocking(fd);
}
void del_epoll_event(int epoll_fd, int fd) // Delete file descriptor from epoll interest list;
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}
void handler_send_signal(int signal_number)
{
    int temp_signal = signal_number;
    int temp_errno = errno;
    send(signal_pipefd[1], (char *)&temp_signal, 1, 0);
    errno = temp_errno;
}
void add_signal(int signal_number, void (*handler_send_signal)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler_send_signal;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    sigaction(signal_number, &sa, NULL);
}
template <typename type>
class Process_Pool_Class
{
private:
    static Process_Pool_Class<type> *process_pool_instance; // The process pool instance;
    static const int MAX_PROCESS_LIMIT = 16;                // The maximum number of child process per process pool;
    static const int MAX_NUMBER_CLIENT_PER_PROCESS = 10000; // The maximum number of client per process;
    static const int MAX_EVENT_PER_EPOLL = 10002;           // The maximum number of events processed per epoll;
    int max_current_process_count;                          // Current process number in process pool;
    int process_order_number;                               // The sequential number of child process in process pool;
    int process_epoll_fd;                                   // Each process has itself epoll file descriptor;
    int process_listen_fd;
    int process_stop; // Each process has itself stop flag;
    Process_information_Class *child_process_information_pt;

private:
    Process_Pool_Class(int temp_process_listen_fd, int temp_current_process_count=8);
    void setup_signal_pipe_and_epoll();
    void run_parent();
    void run_child();
    /* data */
public:
    // Process_Pool_Class(/* args */);
    // The static function ensure that programme create at more one process pool;
    static Process_Pool_Class<type> *create_process_pool(int temp_process_listen_fd, int temp_current_process_count = 8);
    void run_process_pool();
    ~Process_Pool_Class();
};
template <typename type>
Process_Pool_Class<type> *Process_Pool_Class<type>::create_process_pool(int temp_process_listen_fd, int temp_current_process_count = 8)
    {
        if (!process_pool_instance)
        {
            process_pool_instance = new Process_Pool_Class<type>(temp_process_listen_fd, temp_current_process_count);
        }
        return process_pool_instance;
    }
template <typename type> // Constructor
Process_Pool_Class<type>::Process_Pool_Class(int temp_process_listen_fd, int temp_current_process_count) : process_listen_fd(temp_process_listen_fd), current_process_count(temp_current_process_count), process_order_number(-1), process_stop(false)
{
    assert(temp_current_process_count > 0 && temp_current_process_count <= MAX_PROCESS_NUMBER);
    child_process_information_pt = new Process_information_Class[temp_current_process_count];
    assert(child_process_information);
    for (size_t i = 0; i < temp_current_process_count; i++) // The loop create temp_current_process_count processes, and build pipeline with parent process.
    {
        int function_flag;
        function_flag = socketpair(AF_UNIX, SOCK_STREAM, 0, child_process_information_pt[i].communicate_pipefd);
        assert(function_flag == 0);
        setnonblocking(child_process_information_pt[i].communicate_pipefd[0]);
        child_process_information_pt[i].pid_number = fork(); // Constructor use 'fork' to create child process.
        assert(child_process_information_pt[i].pid_number >= 0);
        if (child_process_information_pt[i].pid_number > 0)
        {
            close(child_process_information_pt[i].communicate_pipefd[0]);
            continue;
        }
        else
        {
            close(child_process_information_pt[i].communicate_pipefd[1]);
            process_order_number = i;
            break;
        }

        /* code */
    }
}

template <typename type> // Initialize static process pool instance process_pool_instance;
Process_Pool_Class<type> *Process_Pool_Class<type>::process_pool_instance = NULL;

template <typename type>                                     // Unified event sources
void Process_Pool_Class<type>::setup_signal_pipe_and_epoll() //
{
    int function_flag;
    process_epoll_fd = epoll_create(5);
    assert(process_epoll_fd >= 0);
    function_flag = socketpair(AF_UNIX, SOCK_STREAM, 0, signal_pipefd); // Create pipeline to send signal.
    assert(function_flag == 0);
    setnonblocking(signal_pipefd[0]);
    add_epoll_event(process_epoll_fd, signal_pipefd[0]); // Add event to epoll interest list.
    add_signal(SIGCHLD, handler_send_signal);
    add_signal(SIGTERM, handler_send_signal);
    add_signal(SIGINT, handler_send_signal);
    sigaction(SIGPIPE, SIG_IGN);
}
template <typename type>
void Process_Pool_Class<type>::run_process_pool() // The Process_order_number value is -1 in parent process, and the value is a positive number in child parent.
{
    if (process_order_number == -1)
        run_parent();
    else
        run_child();
}
template <typename type>
void Process_Pool_Class<type>::run_child()
{
    setup_signal_pipe_and_epoll();
    // Each child process finds a pipeline that communicates with the parent process through the process pool variable 'process_order_number'.
    int temp_pipefd = child_process_information_pt[process_order_number].communicate_pipefd[0];
    // The child need to listen pipeline file descriptor 'temp_pipe_fd', because the parent process will notify the child process by it that a new connect is comming.
    add_epoll_event(process_epoll_fd, temp_pipefd);
    struct epoll_event ready_events[MAX_EVENT_PER_EPOLL]; // Epoll_event array aim to store epoll ready events.
    type *client_data = new type[MAX_NUMBER_CLIENT_PER_PROCESS];
    assert(client);
    int epoll_ready_number = 0;
    int function_flag = -1;
    while (!process_stop)
    {
        epoll_ready_number = epoll_wait(process_epoll_fd, ready_events, MAX_EVENT_PER_EPOLL, -1);
        if ((epoll_ready_number < 0) && (errno != EINTR))
        {
            printf("Child process %d epoll_wait failure.\n", getpid());
            break;
        }
        for (size_t i = 0; i < epoll_ready_number; i++) // Epoll ready events loop.
        {
            int temp_socket = ready_events[i].data.fd;
            if (temp_socket == temp_pipefd && ready_events[i].events & EPOLLIN) // If this statement is true, the parent process has a new connect to reach.
            {
                int accept_parent_data;
                int function_flag;
                int client_fd
                function_flag = recv(temp_pipefd, (char *)&accept_parent_data,sizeof(accept_parent_data),0);
                if(function_flag<=0)
                {
                    continue;
                }
                struct sockaddr_in client_address;
                socklen_t client_address_length=sizeof(client_address);
                client_fd=accept(process_listen_fd,&client_address,&client_address_length);
                if (client_fd<0)
                {
                    printf("Accept failure, error information:%s.\n",strerror(errno));
                    continue;
                }
                setnonblocking(client_fd);
                add_epoll_event(process_epoll_fd,client_fd);
                //client_data[client_fd].init();
            }
            else if (temp_socket==signal_pipefd[0]&&ready_events[i].events&EPOLLIN)
            {
                char pending_signal[1024];
                int function_flag;
                function_flag=recv(temp_socket,pending_signal,sizeof(pending_signal),0);
                if (funcntion_flag<=0)
                {
                    continue;
                    /* code */
                }
                for (size_t i = 0; i < function_flag; i++)
                {
                    switch (pending_signal[i])
                    {
                    case SIGCHLD:
                    {
                        pid_t quit_child_process_identifier;
                        int quit_child_process_state;
                        while ((quit_child_process_identifier=waitpid(-1,&quit_child_process_state,WNOHANG)>0))
                        {
                            continue;
                            /* code */
                        }
                        break;
                    }
                    case SIGTERM:
                    case SIGINT:
                    {
                        process_stop=true;
                        break;
                    }
                    default:
                        break;
                    }
                    /* code */
                }
                /* code */
            }
            else if (ready_events[i].events&EPOLLIN)
            {
                //client_data[temp_socket].process;
                /* code */
            }
            else
            {
                continue;
            }
            /* code */
        }
        /* code */
    }
    delete [] client_data;
    client_data=NULL;
    close(process_epoll_fd);
    close(child_process_information_pt[process_order_number].communicate_pipefd[0]);
    //close(process_listen_fd);
}
template <typename type>
void Process_Pool_Class<type>::run_parent()
{
    setup_signal_pipe_and_epoll();
    add_epoll_event(process_epoll_fd, process_listen_fd);
    struct epoll_event parent_ready_events[2];
    int current_child_process_sequence_number = 0;
    int new_connect_signal = 1;
    int epoll_ready_number = 0;
    int function_flag = -1;
    while (!process_stop)
    {
        epoll_ready_number = epoll_wait(process_epoll_fd, parent_ready_events, 2, -1);
        if (epoll_ready_number < 0 && errno != EINTR)
        {
            printf("Parent process epoll_wait failure.\n");
            break;
        }
        for (size_t i = 0; i < epoll_ready_number; i++)
        {
            int temp_socket;
            temp_socket = parent_ready_events[i].data.fd;
            if (temp_socket == process_listen_fd) // A new connect comes in, assign it to a child process, by Round Robin algorithm.
            {
                int j = current_child_process_sequence_number;
                do
                {
                    if (child_process_information_pt[j].pid_number != -1)
                    {
                        break;
                    }
                    j = (j + 1) % max_current_process_count;
                    /* code */
                } while (j != current_child_process_sequence_number);
                if (child_process_information_pt[j].pid_number == -1)
                {
                    process_stop = true;
                    break;
                }
                current_child_process_sequence_number = (j + 1) % max_current_process_count;
                send(child_process_information_pt[j].communicate_pipefd[1], (char *)&new_connect_signal, sizeof(new_connect_signal), 0);
                printf("Send new connect signal to %d child process, process identifier %d.\n", j, child_process_information_pt[j].pid_number);
            }
            //
            else if ((temp_socket == signal_pipefd[0]) && (parent_ready_events[j].events & EPOLLIN))
            {
                char pending_signal[1024];
                int function_flag;
                function_flag = recv(signal_pipefd[0], pending_signal, sizeof(pending_signal), 0);
                if (function_flag < 0)
                {
                    continue;
                }

                for (size_t i = 0; i < function_flag; i++)
                {
                    switch (pending_signal[i])
                    {
                    case SIGCHLD:
                    {
                        pid_t quit_child_process_identifier;
                        int quit_child_state;
                        while ((quit_child_process_identifier = waitpid(-1, &quit_child_state, WNOHANG)) > 0)
                        {
                            for (size_t i = 0; i < max_current_process_count; i++)
                            {
                                if (child_process_information_pt[i] == quit_child_process_identifier)
                                {
                                    printf("Child process %d quit, identifier %d.\n", i, quit_child_process_identifier);
                                    close(child_process_information_pt[i].communicate_pipefd[0]);
                                    child_process_information_pt[i].pid_number = -1;
                                }
                                /* code */
                            }
                            /* code */
                        }
                        process_stop = true;
                        for (size_t i = 0; i < max_current_process_count; i++)
                        {
                            if (child_process_information_pt[i].pid_number != -1)
                            {
                                process_stop = false;
                            }
                            /* code */
                        }
                        break;
                    }

                    case SIGTERM:
                    case SIGINT:
                    {
                        printf("Kill all child process.\n");
                        for (size_t i = 0; i < max_current_process_count; i++)
                        {
                            int quit_child_process_identifier = child_process_information[i].pid_number;
                            if (quit_child_process_identifier != -1)
                            {
                                kill(quit_child_process_identifier, SIGTERM);
                            }
                            /* code */
                        }
                        break;
                    }
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(process_epoll_fd);
    
    //close(process_listen_fd);
}
template <typename type>
Process_Pool_Class<type>::~Process_Pool_Class()
{
    delete[] child_process_information_pt;
}
#endif