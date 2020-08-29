CREATED BY: Tal Tabak
DESCRIPTION:
This program implements a proxy server which can take care of multiple requests and handle them simulataneously by using threadpool.
the server takes the request and checks some constarints which defined earlier , such as filtered sites etc.
1.
 Server creates pool of threads, threads wait for jobs
2.
Server accept a new connection from a client (aka a new socket fd).
3.
Server dispatch a job - call dispatch with the main. 
negotiation function and fd as a parameter. 
dispatch will add work_t item to the queue.   
4.
 When there will be an available thread, it will takes a job from the queue and run the negotiation function.

PROGRAM FILES:
    threadpool.c -An implementaion of a threadpool, queue of threads, each thread is working on a different job with access to the same data.
    proxyServer.c -An implementaion of a proxy server, gets client requests , checks validation, sends the requests to the server and returns responses to the client.    

REMARKS:
   Workspace: Visual Studio Code

Input: <port> <pool-size> <max-number-of-request> <filter> ,  in cmd line

The implementaion of the program is done with helpful structures (request_data and proxy_data) which holds fields of 
    the data parsed from the request and the data parsed from the cmd line. 
    