
// line 1 - 54 of tutorial
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <netinet/in.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

//compile as gcc -o example_rdm example_rdm.c -L/opt/cray/libfabric/1.22.0/lib64 -I/opt/cray/libfabric/1.22.0/include  -lfabric
#define BUF_SIZE 64 
char *dst_addr = NULL;
char *port = "9228";
struct fi_info *hints, *info;
struct fid_fabric *fabric=NULL;
struct fid_domain *domain = NULL;
struct fid_ep *ep = NULL;
struct fid_av *av = NULL;
struct fid_cq *cq = NULL;
char buf[BUF_SIZE];
fi_addr_t fi_addr = FI_ADDR_UNSPEC;


static int initilize(void){
    struct fi_cq_attr cq_attr = {0};
    struct fi_av_attr av_attr = {0};
    const struct sockaddr_in *sin;
    char str_addr[INET_ADDRSTRLEN];
    int ret;

    fi_getinfo(FI_VERSION(1,22), dst_addr, port, dst_addr ? 0: FI_SOURCE,
    hints, &info); //server add and port number, FI_SOURCE flag -> source addresses, 
    //data from provider are received in info, it is the linkedlist of struct. 

    if(!dst_addr){
        sin = info->src_addr; //select first one of linkedlist
        if(!inet_ntop(sin->sin_family, &sin->sin_addr, str_addr, sizeof(str_addr))){
            printf("error converting address to string\n");
            return -1;
        }
        printf("Server started with addr %s\n", str_addr);
    }
    fi_fabric(info->fabric_attr, &fabric, NULL);
    fi_domain(fabric, info, &domain, NULL);
    fi_endpoint(domain, info, &ep, NULL);

    cq_attr.size = 128;
    cq_attr.format = FI_CQ_FORMAT_MSG;
    fi_cq_open(domain, &cq_attr, &cq, NULL);

    fi_ep_bind(ep, &cq->fid, FI_SEND|FI_RECV);

    av_attr.type = FI_AV_TABLE;
    av_attr.count = 1;

    fi_av_open(domain, &av_attr, &av, NULL);

    if(dst_addr){
        fi_av_insert(av, info->dest_addr, 1, &fi_addr, 0, NULL);
    }
    fi_ep_bind(ep, &av->fid, 0);
    
    fi_enable(ep);

    return 0;
}

static void cleanup(void)
{
    fi_close(&ep->fid);
    fi_close(&av->fid);
    fi_close(&cq->fid);
    fi_close(&domain->fid);
    fi_close(&fabric->fid);
    fi_freeinfo(info);
}

static int post_recv()
{
    int ret;
    do{
        ret = fi_recv(ep,buf, BUF_SIZE, NULL, fi_addr, NULL);
        if(ret && ret != -FI_EAGAIN){
            printf("error posting recv buffer (%d\n)", ret);
            return ret;
        }

        if(ret== -FI_EAGAIN)
            (void) fi_cq_read(cq,NULL,0);
    } while(ret);
    return 0;
}
static int post_send(void)
{
    char *msg = "Hello, server! I am your client and I will send you big number in future\0";
    int ret;
    (void) snprintf(buf,BUF_SIZE, "%s", msg);
    do{
        ret = fi_send(ep,buf,BUF_SIZE, NULL, fi_addr, NULL);
        if(ret && ret!= -FI_EAGAIN){
            printf("error posting send buffer (%d)\n", ret);
            return ret;
        }
        if(ret == -FI_EAGAIN)
            (void) fi_cq_read(cq,NULL,0);
    }
    while(ret);
}

static int wait_cq(void)
{
    struct fi_cq_err_entry comp;
    int ret;
    do{
        ret = fi_cq_read(cq, &comp, 1);
        if(ret<0 && ret!=-FI_EAGAIN){
            printf("error reading cq (%d)\n", ret);
            return ret;
        }
    }while(ret !=1);
    if(comp.flags & FI_RECV)
        printf("I received a message!\n");
    else if (comp.flags & FI_SEND)
        printf("My sent message got sent!\n");
    return 0;
}

static int run(void){
    int ret;
    if(dst_addr){
        printf("Client: send to server %s\n", dst_addr);
        post_send();
        printf("Client Send complete\n");
        wait_cq();
        printf("Client wait complete\n");
    }
    else{
        printf("Server: post buffer and wait for message from client\n");
        post_recv();
        printf("Server recv post complete\n");
        wait_cq();
        printf("This is the message I received: %s\n", buf);
    }
    return 1;
}

//line 201 - 228 of tutorial
int main (int argc, char* argv[])
{
    int ret;
    hints = fi_allocinfo(); //allocate resources using this call
    if(!hints)
        return EXIT_FAILURE;
    dst_addr = argv[optind]; //server's address
    hints->ep_attr->type = FI_EP_RDM; //request rdma endpoint type : reliable datagram message
    hints->caps = FI_MSG; //FI_MSG capability 
    hints->fabric_attr->prov_name = "tcp"; //tcp provider application
    hints->addr_format = FI_SOCKADDR_IN; //ipv4 
    hints->tx_attr->op_flags = FI_DELIVERY_COMPLETE; //used to let client know not no exist if all msg are not complete
    initilize();
    run();
    cleanup();
}

