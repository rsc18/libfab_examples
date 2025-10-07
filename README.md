# Libfabrics example codes  
This repo contains sample codes of libfabric applications.
The code is adopted from libfabric tutorial **HOTI'22**. 

### Build in Aurora
```bash
make all
```
### Start the server
```bash
FI_PROVIDER=cxi ./bin/example_cxi_rdm
```
Expected Output:
```bash
Server CXI address: fi_addr_cxi://0x0000000000053e00
Initillization done!! Server: post buffer and wait for message from client
```
### Start the client
Note that we got Server CXI address as fi_addr_cxi://0x0000000000053e00.
So, run the program with following argument for client on **another terminal**.

```bash 
FI_PROVIDER=cxi ./bin/example_cxi_rdm 0x0000000000053e00
```

Expected Output:
```bash
On Client side:
Client using server address: 0x0000000000053e00
Initillization done!! My sent message got sent!

On Server side:
Server CXI address: fi_addr_cxi://0x0000000000053e00
Initillization done!! Server: post buffer and wait for message from client
I received a message!
This is the message I received: Hello, server! I am your client and I will send you big number.
```
