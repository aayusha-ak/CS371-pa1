/*
# Copyright 2025 University of Kentucky
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
*/

/* 
Please specify the group members here

# Student #1: Aayusha Kandel
# Student #2:
# Student #3: 

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX_EVENTS 64
#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;

/*
 * This structure is used to store per-thread data in the client
 */
typedef struct {
    int epoll_fd;        /* File descriptor for the epoll instance, used for monitoring events on the socket. */
    int socket_fd;       /* File descriptor for the client socket connected to the server. */
    long long total_rtt; /* Accumulated Round-Trip Time (RTT) for all messages sent and received (in microseconds). */
    long total_messages; /* Total number of messages sent and received. */
    float request_rate;  /* Computed request rate (requests per second) based on RTT and total messages. */
} client_thread_data_t;

/*
 * This function runs in a separate client thread to handle communication with the server
 */
void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct epoll_event client_event, events[MAX_EVENTS];
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP"; /* Send 16-Bytes message every time */
    char recv_buf[MESSAGE_SIZE];
    struct timeval start, end;

    // Hint 1: register the "connected" client_thread's socket in the its epoll instance
    // Hint 2: use gettimeofday() and "struct timeval start, end" to record timestamp, which can be used to calculated RTT.

    /* TODO:
     * It sends messages to the server, waits for a response using epoll,
     * and measures the round-trip time (RTT) of this request-response.
     */
    // Register this thread's socket with its epoll instance
    memset(&client_event, 0, sizeof(client_event)); // clear event struct
    client_event.events = EPOLLIN; //notify when socket is readable
    client_event.data.fd = data->socket_fd; // identify which fd triggered event

    //Add socket to the thread's epoll instance
    if (epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, data->socket_fd, &client_event) == -1) {
        perror("epoll_ctl - client_thread");
        return NULL;
    }

    //Initialize varibles for RTT and throughput calculations
    data->total_rtt = 0;
    data->total_messages = 0;
    data->request_rate = 0.0f;


    // Request Loop
    // - send message
    // - wait for server response using epoll
    // - Recieve the echo
    // - Measure RTT
    for (int i = 0; i < num_requests; i++) {
        //
        if (gettimeofday(&start, NULL) == -1) {
            perror("gettimeofday start");
            break;
        }

        // send message to the server
        ssize_t sent = send(data->socket_fd, send_buf, MESSAGE_SIZE, 0);
        if (sent < 0) {
            perror("send()");
            break;

        } else if (sent != MESSAGE_SIZE) { // if not all were sent, treat as error
            fprintf(stderr, "short send\n");
            break;
        }

        //Block until server responds
        int nfds = epoll_wait(data->epoll_fd, events, 1, -1); //epoll_wait() returns when socket becomes readable
        if (nfds < 0) {
            perror("epoll_wait()");
            break;
        }

        //checking if event is for the socket and is readable
        if (events[0].data.fd == data->socket_fd &&
            (events[0].events & EPOLLIN)) {

            //Recieve the echoed message from the server
            ssize_t n = recv(data->socket_fd, recv_buf, MESSAGE_SIZE, 0);
            if (n <= 0) {
                if (n < 0) perror("recv"); //error
                break; // n = 0 - close connection
            }

            // Record timestamp right after receiving
            if (gettimeofday(&end, NULL) == -1) {
                perror("gettimeofday end");
                break;
            }

            //compute RRT in microseconds.
            long long rtt_us =
                (long long)(end.tv_sec - start.tv_sec) * 1000000LL +
                (long long)(end.tv_usec - start.tv_usec);

            //Add up RTT and message count - for throughput
            data->total_rtt += rtt_us;
            data->total_messages += 1;
        }
    }
     
    /* TODO:
     * The function exits after sending and receiving a predefined number of messages (num_requests). 
     * It calculates the request rate based on total messages and RTT
     */

    if (data->total_rtt > 0 && data->total_messages > 0) {

        //casting double so dividion happens in floating-point
        double total_sec = (double)data->total_rtt / 1000000.0; //converitng to seconds

        //message per sec = (# of successdul request/response cycles) /(total time in seonds)
        data->request_rate = (float)(data->total_messages / total_sec); 
    } else {
        data->request_rate = 0.0f; // RTT is not valid
    }

    return NULL;
}


/*
 * This function orchestrates multiple client threads to send requests to a server,
 * collect performance data of each threads, and compute aggregated metrics of all threads.
 */
void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;

    /* TODO:
     * Create sockets and epoll instances for client threads
     * and connect these sockets of client threads to the server
     */
    
    // Hint: use thread_data to save the created socket and epoll instance for each thread
     //You will pass the thread_data to pthread_create() as below
    //for (int i = 0; i < num_client_threads; i++) {
      //  pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    //}

    // initialize server address for all clients threads
    // the addresss is resued when connecting each thread's socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    //convert ip address of server into binary
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton()");
        return;
    }

    // Create sockets, connect, create epoll instances, start threads
    for (int i = 0; i < num_client_threads; i++) {

        //Socket
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("socket()");
            return;
        }

        //Connect thread's socket to the server
        //each thread maintains its own independent connection
        if (connect(sock_fd, (struct sockaddr *)&server_addr,
                    sizeof(server_addr)) < 0) {
            perror("connect()");
            close(sock_fd);
            return;
        }

        // create an epoll instance for thread
        // each thread uses its own epoll fd to monitor its socket
        int epfd = epoll_create1(0);
        if (epfd < 0) {
            perror("epoll_create())");
            close(sock_fd);
            return;
        }

        //Store per-thread resources in thread_data[]
        //This struct is passed to client_thread_func()
        thread_data[i].socket_fd = sock_fd;
        thread_data[i].epoll_fd = epfd;
        thread_data[i].total_rtt = 0;
        thread_data[i].total_messages = 0;
        thread_data[i].request_rate = 0.0f;

        //Launch the client thread
        //Each thread runs client_thread_func() independenlty
        if (pthread_create(&threads[i], NULL, client_thread_func,
                           &thread_data[i]) != 0) {
            perror("pthread_create()");
            close(sock_fd);
            close(epfd);
            return;
        }
    }

    /* TODO:
     * Wait for client threads to complete and aggregate metrics of all client threads
     */

     // wait for all threads to finish sending requests
     for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    
    long long total_rtt = 0; //total RTT across all threads
    long total_messages = 0; // total # of messages sent and received
    float total_request_rate = 0.0f; //sum of pre-thread request rates

    //After all the threads finish compute: 
    for (int i = 0; i < num_client_threads; i++) {
        total_rtt += thread_data[i].total_rtt; // total RRT across all threads
        total_messages += thread_data[i].total_messages; //total # of message sent and received
        total_request_rate += thread_data[i].request_rate; // sum of pre-thread request rate

        
        close(thread_data[i].socket_fd);// close all the threads socket
        close(thread_data[i].epoll_fd); // close all the threads epoll instance
    }

    //print Average RRT and Total request rate 
    if (total_messages > 0) {
        printf("Average RTT: %lld us\n", total_rtt / total_messages);
    } else {
        printf("Average RTT: N/A (no messages)\n");
    }
    printf("Total Request Rate: %f messages/s\n", total_request_rate);

    //printf("Average RTT: %lld us\n", total_rtt / total_messages);
    //printf("Total Request Rate: %f messages/s\n", total_request_rate);
}



void run_server() {

    /* TODO:
     * Server creates listening socket and epoll instance.
     * Server registers the listening socket to epoll
     */
    
    /* Server's run-to-completion event loop */

    // TCP listening socket
    //client attempts to connect to this socket
    int listeningSocket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket_fd < 0) {
        perror("socket()");
        return;
    }
    // epoll instance 
    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0 ) {
        perror("epoll_create1()");
        return;
    }

    //Set up server address - IP + port#
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address)); //clear the struct to avoid garbage
    address.sin_family = AF_INET; //IPv4 socket
    address.sin_port = htons(server_port); // convert server port into network byte order
    address.sin_addr.s_addr = INADDR_ANY; //accepts connections from any local IP

    //Bind the socket to the  chosen IP + port
    int binding_status = bind(listeningSocket_fd,
                             (struct sockaddr *)&address,
                              sizeof(address));

    if (binding_status < 0) {
        perror("bind()");
        close(listeningSocket_fd);
        return;
    }

    // Mark socket as listening  - open for TCP connections
    int listening_status = listen(listeningSocket_fd, SOMAXCONN); // SOMAXCONN - max # of pending connections the OS allows
    if (listening_status < 0) {
        perror("Listen()");
        return;
    }

    // Register listening socket with epoll
    struct epoll_event ev;
    ev.events = EPOLLIN; // notfies when client is reaching out
    ev.data.fd = listeningSocket_fd; //identify which fd triggered event
    
    
    int epoll_status = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listeningSocket_fd, &ev);
    if (epoll_status < 0) {
        perror("epoll_ctl()");
        return;
    }


    // Array for epoll_wait() to store events
    struct epoll_event events[MAX_EVENTS];

    //buffer for receiving client messages
    char buffer[MESSAGE_SIZE];

    while (1) {
        /* TODO:
         * Server uses epoll to handle connection establishment with clients
         * or receive the message from clients and echo the message back
         */
     
        int ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (ready_fds < 0) {
            perror("epoll_wait");
            break; 
        }

        //process each ready file descriptor 
        for (int i = 0; i < ready_fds; i++) {
            
            int fd = events[i].data.fd;// File descriptor that triggered this event

            // Case 1: Listening socket is ready - new client attempting to connect
            if (fd == listeningSocket_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                //accept new client connection
                int client_fd = accept(listeningSocket_fd,
                                       (struct sockaddr *)&client_addr,
                                       &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

                printf("Accepted new client: fd = %d\n", client_fd);

                // Register new client socket to epoll
                struct epoll_event client_ev;
                client_ev.events = EPOLLIN; // notifies when client sends data or disconnects
                client_ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
                    perror("epoll_ctl : client_fd");
                    close(client_fd);
                }

            } else {

                
                // Existing client sent data
                int n = recv(fd, buffer, MESSAGE_SIZE, 0);
                if (n <= 0) { // n  = 0 - client closed connection
                    // error occured
                    if (n < 0) {
                        perror("recv");
                    }
                    printf("Client disconnected: fd = %d\n", fd);

                    //Remove client form epoll 
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd); //close socket
                } else {
                    // n > 0 - data recieved
                    // Echo message back to client
                    send(fd, buffer, n, 0);
                    printf("Received %d bytes from client fd=%d, echoing back\n", n, fd);
                }
            }
        }
    }
    
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "server") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);

        run_server();
    } else if (argc > 1 && strcmp(argv[1], "client") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        if (argc > 4) num_client_threads = atoi(argv[4]);
        if (argc > 5) num_requests = atoi(argv[5]);

        run_client();
    } else {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
}

    return 0;
}
