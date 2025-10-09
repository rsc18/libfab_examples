#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>   // REQUIRED for fi_getname()
#include <mpi.h>
//compile as gcc -o example_rdm example_rdm.c -L/opt/cray/libfabric/1.22.0/lib64 -I/opt/cray/libfabric/1.22.0/include  -lfabric

#define BUF_SIZE 64

char *dst_addr = NULL;
struct fi_info *hints, *info;
struct fid_fabric *fabric = NULL;
struct fid_domain *domain = NULL;
struct fid_ep *ep = NULL;
struct fid_av *av = NULL;
struct fid_cq *cq = NULL;
char buf[BUF_SIZE];
fi_addr_t fi_addr = FI_ADDR_UNSPEC;
static fi_addr_t fi_dest = FI_ADDR_UNSPEC;
int msg = 0;


static uint64_t initilize(int rank) {
    struct fi_cq_attr cq_attr = {0};
    struct fi_av_attr av_attr = {0};
    int ret;
    fi_getinfo(FI_VERSION(1, 22), NULL, NULL, 0, hints, &info);
    fi_fabric(info->fabric_attr, &fabric, NULL);
    fi_domain(fabric, info, &domain, NULL);
    fi_endpoint(domain, info, &ep, NULL);

    cq_attr.size = 128;
    cq_attr.format = FI_CQ_FORMAT_MSG;
    fi_cq_open(domain, &cq_attr, &cq, NULL);

    fi_ep_bind(ep, &cq->fid, FI_SEND | FI_RECV);

    av_attr.type = FI_AV_TABLE;
    av_attr.count = 1;

    fi_av_open(domain, &av_attr, &av, NULL);
    fi_ep_bind(ep, &av->fid, 0);
    fi_enable(ep);

    
        size_t addrlen = 0;
        fi_getname(&ep->fid, NULL, &addrlen);
        void *local_addr = malloc(addrlen);
        fi_getname(&ep->fid, local_addr, &addrlen);

        // Print full CXI address
        uint64_t *addr64 = (uint64_t *) local_addr;
        printf("Rank [%d] CXI address: fi_addr_cxi://0x%016" PRIx64 "\n", rank, *addr64);
        fflush(stdout);
   
       return *addr64;
}

static void cleanup(void) {
    fi_close(&ep->fid);
    fi_close(&av->fid);
    fi_close(&cq->fid);
    fi_close(&domain->fid);
    fi_close(&fabric->fid);
    fi_freeinfo(info);
}

static int post_recv() {
    int ret;
    ret = fi_recv(ep, buf, BUF_SIZE, NULL, fi_addr, NULL);
    return ret;
}

static int post_send(int rank, int counter) {
    // snprintf(buf, BUF_SIZE, "Msg from rank %d, counter=%d", rank, counter);
    int ret = fi_send(ep, buf, BUF_SIZE, NULL, fi_dest, NULL);
    return ret;
}

static int wait_cq(int rank) {
    struct fi_cq_msg_entry comp;
    int ret;
    do {
        ret = fi_cq_read(cq, &comp, 1);
        if (ret == -FI_EAVAIL) {
            struct fi_cq_err_entry ee = {0};
            fi_cq_readerr(cq, &ee, 0);
            fprintf(stderr, "CQ error: %s (prov_errno=%d)\n",
                    fi_strerror(ee.err), ee.prov_errno);
            exit(EXIT_FAILURE);
        }
    } while (ret != 1);

    // if (comp.flags & FI_RECV)
    //     printf("[%d] I received a message!\n", rank);
    // else if (comp.flags & FI_SEND)
    //     printf("[%d] My sent message got sent!\n",rank);
    // else
    //     printf("NOthing!!!|n");
    return 0;
}

//static int run(void) {
//    int ret;
//    if (dst_addr) {
//        post_send();
//        wait_cq();
//    } else {
//        printf("Server: post buffer and wait for message from client\n");
//        post_recv();
//        wait_cq();
//        printf("This is the message I received: %s\n", buf);
//    }
//    return 1;
//}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, n;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n);

    int ret;
    hints = fi_allocinfo(); //allocate resources using this call
    if (!hints)
        return EXIT_FAILURE;
    dst_addr = argv[1]; //server's address
    hints->ep_attr->type = FI_EP_RDM; //request rdma endpoint type : reliable datagram message
    hints->caps = FI_MSG; //FI_MSG capability
    hints->fabric_attr->prov_name = "cxi"; // provider name
    hints->addr_format = FI_ADDR_CXI;
    hints->ep_attr->protocol = FI_PROTO_CXI;
    hints->domain_attr->mr_mode = FI_MR_ENDPOINT;
    hints->tx_attr->op_flags = FI_DELIVERY_COMPLETE; //used to let client know not no exist if all msg are not complete

    uint64_t node_addr= initilize(rank);
    //printf("Initillization done!! ");
    
    //share address between ranks
    uint64_t *all_addrs = (uint64_t *) malloc(n*sizeof(uint64_t));
    MPI_Allgather(&node_addr,1,MPI_UINT64_T, all_addrs, 1, MPI_UINT64_T, MPI_COMM_WORLD);



    if(rank==0){
        printf("local CXI addr[0]: 0x%016" PRIx64 "\n", all_addrs[0]);
        printf("local CXI addr[1]: 0x%016" PRIx64 "\n",  all_addrs[1]);
        printf("My CXI addr: 0x%016" PRIx64 "\n",node_addr);
    }

    buf[0]='0';
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank==0) {
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "0x%016" PRIx64, all_addrs[1]);
        uint64_t next_node_addr = strtoull(addr_str, NULL, 0);
        fi_av_insert(av, &next_node_addr, 1, &fi_dest, 0, NULL);

        for (int i = 0; i < 5; i++) {
            post_send(rank, i);
            wait_cq(rank);  // send completion
            // prepare for next iteration
            post_recv();
            wait_cq(rank);  // receive completion (posted earlier)
            printf("[%d] This is the message I received: %s\n", rank, buf);
            sprintf(buf, "%d", atoi(buf)+1);
        }
    }
    else if (rank==1) {
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "0x%016" PRIx64, all_addrs[0]);
        uint64_t next_node_addr = strtoull(addr_str, NULL, 0);
        fi_av_insert(av, &next_node_addr, 1, &fi_dest, 0, NULL);

        for (int i = 0; i < 5; i++) {
            post_recv();            
            wait_cq(rank);  // receive completion (posted earlier)
            printf("[%d] This is the message I received: %s\n", rank, buf);
            sprintf(buf, "%d", atoi(buf)+1);
            post_send(rank, i);
            wait_cq(rank);  // send completion
        }
    }
   // run();
    cleanup();
    MPI_Finalize();
}
