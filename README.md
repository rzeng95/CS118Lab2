# CS118Lab2
CS118 Fall '15 Lab 2: Simple Window-Based Reliable Data Transfer in C

Brendan Sio 804150223  
Roland Zeng 204150508

We implemented Go-Back-N (GBN) protocol on top of UDP client-and-server skeleton code. 

### How to run:

Add files you wish to open (.text) into the same directory. We have added 'hello.txt' as an example.

In console type:
```
make
```
Open two separate terminals. In the first one, type:
```
./server [window size] [% packet loss] [% packet corruption]  
```

Where packet loss and packet corruption are decimal values greater than 0 and less than 1. Then in the second terminal, type:
```
./client hello.txt [% packet loss] [% packet corruption] 
```
By doing so, client requests _hello.txt_ from the server, which sends the contents of the file in packets. Packet loss and corruption are simulated by randomly removing packets sent and removing acknowledgements received, respectively.  

