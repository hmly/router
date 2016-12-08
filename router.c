#define _BSD_SOURCE  /* In conjunction with unistd.h */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <float.h>
#include <time.h> 
#include <sys/time.h>

#define DEBUG     0
#define MAX_NODES 20         /* Number of routers */
#define MSGLEN    1024       /* For hold request and router id */
#define INF       99999

#define PING_INT   1000000   /* 1 sec */ 
#define FLOOD_INT  10000000
#define MSG_INT    10000000

#define PING     0
#define ROUTABLE 1
#define MESSAGE  2

/* For routing table computation */
typedef struct {
    int src;
    int src_port;
    int dst;
    int dst_port;
    int cost;
    int conn;
} Link;

typedef struct {
    int dst;
    int cost;
} LSRouter;

typedef struct {
    int src;
    int dst;
    int cost;
    int src_port;
    int dst_port;
    int conn;
} RoutingTableEntry;

typedef struct {
    int len;
    RoutingTableEntry tableContent[MAX_NODES * MAX_NODES];
} RoutingTable;

/* Header for LSP */
typedef struct  {
    int id;
    int type;
} Hdr;

/* Linked state packet */
typedef struct {
    int id;
    int seq;
    int ttl;
    int len;
    int dst;
    int nexthop;
    char buf[MSGLEN];
    LSRouter table[MAX_NODES];
    Link links[MAX_NODES];
    RoutingTable routingtable;
    struct timeval send_time;
    Hdr header;
} LSP;

typedef struct {
    int id;
    int port;
    int link_cnt;
    LSP self_lsp;
    Link links[MAX_NODES];
    RoutingTable routingtable;
} Router;

/* Global vars */
int sock;
int graph[MAX_NODES][MAX_NODES];
Router router;

/* Config self router */
void config_router(Router *router, char *host, char *port) {
    Link *link;

    link = &(router->links[router->link_cnt++]);
    link->src = router->id;
    link->src_port = router->port;
    link->dst = atoi(host);
    link->dst_port = atoi(port);
    link->cost = INF;
}

/* Calculate the transmission time in ms */
int delay(struct timeval t1, struct timeval t2) {
    int d;
    d = (t2.tv_sec - t1.tv_sec) * 1000000;
    d += t2.tv_usec - t1.tv_usec;
    return d;
}

/* Dijkstra algorithm - compute shortest path from src to dst 
   source: http://scanftree.com/Data_Structure/dijkstra's-algorithm
   flag: 0, find shortest path to dst and returns next hop
         1, compute shortest path to every other nodes and display table */
int dijkstra(int G[MAX_NODES][MAX_NODES], int n, int startnode, int endnode, int flag) {
    int cost[MAX_NODES][MAX_NODES], distance[MAX_NODES], pred[MAX_NODES];
    int pred2[MAX_NODES];
    int visited[MAX_NODES], count, mindistance, nextnode, i, j, k;

    for (i=0; i < n; i++) {
        for (j=0; j < n; j++) {
            if (G[i][j] == 0)
                cost[i][j] = INF;
            else
                cost[i][j] = G[i][j];
        }
    }
    for (i=0; i < n; i++) {
        distance[i] = cost[startnode][i];
        pred[i] = startnode;
        visited[i] = 0;
    }
    nextnode = 0;  /* Initialize var with dummy value */
    distance[startnode] = 0;
    visited[startnode] = 1;
    count = 1;
    while (count < n-1) {
        mindistance = INF;
        for (i=0; i < n; i++) {
            if (distance[i] < mindistance && !visited[i]) {
                mindistance = distance[i];
                nextnode = i;
            }
        }
        visited[nextnode] = 1;
        for (i=0; i < n; i++) {
            if (!visited[i])
                if (mindistance+cost[nextnode][i] < distance[i]) {
                    distance[i] = mindistance + cost[nextnode][i];
                    pred[i] = nextnode;
                }
            count++;
        }
    }
 
    /* Print a table of distance from src to other nodes; routing table */ 
    if (flag) {
        printf("------------------------\n");
        printf("dst\tcost\tpath\n");
        for (i=0; i < n; i++) {
            if (i != startnode) {
                printf("%d\t%d\t%d", i, distance[i], i);
                j = i;
                do {
                    j = pred[j];
                    printf(" <- %d", j);
                } while(j != startnode);
                printf("\n");
            }
        }
        printf("------------------------\n");
    } else {
        for (i=0, k=0; i < n; i++) {
            if (i != startnode && i == endnode) {
                j = i;
                do {
                    j = pred[j];
                    pred2[k++] = j;
                } while(j != startnode);
            }
        }
        /* Get the next-hop router */
        if (k == 1)
            return endnode;
        else
            return pred2[k-2];
    }
    return -1;
}

/* Init self routing table */
void init_routingtable(Router *router) {
    int i;
    Link *link;
    RoutingTableEntry *entry;
    router->routingtable.len = router->link_cnt;

    for (i=0; i < router->link_cnt; i++) {
        link = &(router->links[i]);
        entry = &(router->routingtable.tableContent[i]);
        entry->src = link->src;
        entry->dst = link->dst;
        entry->cost = link->cost;
        entry->src_port = link->src_port;
        entry->dst_port = link->dst_port;
    }
}

/* DEBUG: Print self routing table with costs */
void print_routingtable(Router *router) {
    int i;
    printf("<<< table of costs >>>\n");
    printf("src\tdest\tcost\tsrc-port\tout-port\n");
    for (i=0; i < router->routingtable.len; i++) {
        printf("%d\t%d\t%d\t%d\t\t%d\n", 
            router->routingtable.tableContent[i].src,
            router->routingtable.tableContent[i].dst,
            router->routingtable.tableContent[i].cost,
            router->routingtable.tableContent[i].src_port,
            router->routingtable.tableContent[i].dst_port);
    }
}

/* Convert routing table to an adjacency matrix */
void to_adjmatrix(Router *router, int graph[][MAX_NODES], int alive_rt[], int rt_cnt) {
    int i, j, k;

    /* Set all costs to infinity */
    for (i=0; i < MAX_NODES; i++) {
        for (j=0; j < MAX_NODES; j++) {
            graph[i][j] = INF;
        }
    }

    /* Update costs for existing routers */
    for (i=0; i < router->routingtable.len; i++) {
        graph[router->routingtable.tableContent[i].src][router->routingtable.tableContent[i].dst] = router->routingtable.tableContent[i].cost;
    }

    /* Update cost of down routers to infinity */
    for (i=0; i < router->link_cnt; i++) {
        for (j=0; j < rt_cnt; j++) {
            if (alive_rt[j] == router->links[i].dst) break;
            /* No match, so ith router is down */
            if (j == rt_cnt-1 && alive_rt[j] != router->links[i].dst) {
                for (k=0; k < MAX_NODES; k++) {
                    graph[router->links[i].dst][k] = INF;  /* horizontal */
                    graph[k][router->links[i].dst] = INF;  /* vertical */
                }
            }
        }
    }
}

/* Update routing table by adding new routers discovered in the network and update existing routers */
void update_routingtable(Router *router, LSP *table) {
    int i, j, flag;

    for (i=0, flag=0; i < table->routingtable.len; i++) {
        for (j=0; j < router->routingtable.len; j++) {
            /* Router already in table */
            if (table->routingtable.tableContent[i].src == router->routingtable.tableContent[j].src && 
                table->routingtable.tableContent[i].dst == router->routingtable.tableContent[j].dst) {
                if (table->routingtable.tableContent[i].cost < router->routingtable.tableContent[j].cost)
                    router->routingtable.tableContent[j].cost = table->routingtable.tableContent[i].cost;
                flag = 1;
                break;
            }
        }
        /* Router not in table */
        if (!flag) {
            router->routingtable.tableContent[router->routingtable.len].src = table->routingtable.tableContent[i].src;
            router->routingtable.tableContent[router->routingtable.len].dst = table->routingtable.tableContent[i].dst;
            router->routingtable.tableContent[router->routingtable.len].cost = table->routingtable.tableContent[i].cost;
            router->routingtable.tableContent[router->routingtable.len].src_port = table->routingtable.tableContent[i].src_port;
            router->routingtable.tableContent[router->routingtable.len++].dst_port = table->routingtable.tableContent[i].dst_port;
        }
        flag = 0;  /* Reset flag */
    }
}

/* Update costs in routing table with the transmission time */
void update_routingtable_cost(Router *router, struct timeval t1, struct timeval t2, int host) {
    int i, trans_time;
    trans_time = delay(t1, t2);

    for (i=0; i < router->routingtable.len; i++) {
        if (router->routingtable.tableContent[i].dst == host) {
            router->routingtable.tableContent[i].cost = trans_time;
        }
    }
}

/* DEBUG: Print the adjacency matrix */
void print_mat(int graph[][MAX_NODES]) {
    int i, j;
    printf("<<< adjacency matrix >>>\n");
    for (i=0; i < MAX_NODES; i++) {
        for (j=0; j < MAX_NODES; j++) {
            printf("%6d ", graph[i][j]);
        }
        printf("\n");
    }
}

/* Listen and handle all incoming packets */
void *incoming(void *ptr) {
    int i, rn, rt_upd;
    int alive_rt[MAX_NODES * MAX_NODES];
    struct sockaddr_in self, other;
    struct timeval start, end;
    socklen_t sock_len;
    LSP buf_lsp;

    /* Set socket attribs */
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = INADDR_ANY;
    self.sin_port = htons(router.port);
    other.sin_family = AF_INET;
    other.sin_addr.s_addr = INADDR_ANY;

    /* Set time */
    rt_upd = 0;
    rn = 0;
    gettimeofday(&start, NULL);

    while (1) {
        if (recvfrom(sock, &buf_lsp, sizeof(LSP), 0, (struct sockaddr *) &self, &sock_len) < 0) {
            printf("error recv packet...\n");
        }
        switch(buf_lsp.header.type) {
            case PING:
                printf("< ping router %d\n", buf_lsp.id);

                alive_rt[rn++] = buf_lsp.id;
                gettimeofday(&end, NULL);
                update_routingtable_cost(&router, buf_lsp.send_time, end, buf_lsp.id);
                break;
            case ROUTABLE:
                if (DEBUG) {
                    printf("< routable router %d\n", buf_lsp.id);
                }
                rt_upd++;
                update_routingtable(&router, &buf_lsp);
                if (rt_upd == MAX_NODES * router.link_cnt) {
                    gettimeofday(&start, NULL);
                    to_adjmatrix(&router, graph, alive_rt, rn);
                    dijkstra(graph, MAX_NODES, router.id, -1, 1);  /* Print distance vector */
                    if (DEBUG) {
                        print_mat(graph);
                        print_routingtable(&router);
                    }
                    rn = 0;      /* Reset count */
                    rt_upd = 0;  /* Reset table update count */
                }
                break;
            case MESSAGE:
                printf("< msg router %d\n", buf_lsp.id);
                if (buf_lsp.dst == router.id) {
                    printf(">>> %d <<<\n", buf_lsp.id);
                }
                else {
                    buf_lsp.nexthop = dijkstra(graph, MAX_NODES, router.id, buf_lsp.dst, 0);
                    for (i=0; i < router.link_cnt; i++) {
                        if (router.links[i].dst == buf_lsp.nexthop) {
                            other.sin_port = htons(router.links[i].dst_port);
                        }
                    }
                    /* Send msg to final dest or next hop */
                    if (sendto(sock, &buf_lsp, sizeof(LSP), 0, (struct sockaddr *) &other, sizeof(struct sockaddr)) < 0) {
                        printf("cannot send msg to router %d\n", router.links[i].dst);
                    }
                    printf("> msg router %d\n", buf_lsp.nexthop);
                }
                break;
            default:
                printf("invalid packet type... %d\n", buf_lsp.header.type);
                break;
        }
    }
    return NULL;
}

/* Periodically send ping to neighbor routers */
void *ping(void *ptr) {
    int i;
    struct sockaddr_in other;
    struct timeval tv;

    /* Set lsp packet */
    router.self_lsp.header.id = router.id;
    router.self_lsp.header.type = PING;

    other.sin_family = AF_INET;
    other.sin_addr.s_addr = INADDR_ANY;

    while (1) {
        usleep(PING_INT);

        for (i=0; i < router.link_cnt; i++) {
            gettimeofday(&tv, NULL);
            router.self_lsp.send_time = tv;
            other.sin_port = htons(router.links[i].dst_port);

            if (sendto(sock, &router.self_lsp, sizeof(LSP), 0, (struct sockaddr *) &other, sizeof(struct sockaddr)) < 0) {
                printf("cannot send ping to router %d\n", router.links[i].dst);
            }
            printf("> ping router %d\n", router.links[i].dst);
        }
    }
    return NULL;
}

/* Periodically flood network with routing tables */
void *flooding(void *ptr) {
    int i, j;
    struct sockaddr_in other;
    LSP lsp;

    /* Set lsp header */
    lsp.header.id = router.id;
    lsp.header.type = ROUTABLE;

    other.sin_family = AF_INET;
    other.sin_addr.s_addr = INADDR_ANY;
    lsp.id = router.id;

    while (1) {
        usleep(FLOOD_INT);
        lsp.routingtable = router.routingtable;

        /* Flood N-1 times for N nodes in network */
        for (i=0; i < MAX_NODES-1; i++) {
            for (j=0; j < router.link_cnt; j++) {
                other.sin_port = htons(router.links[j].dst_port);

                if (sendto(sock, &lsp, sizeof(LSP), 0, (struct sockaddr *) &other, sizeof(struct sockaddr)) < 0) {
                    printf("cannot send routable to router %d\n", router.links[i].dst);
                }
                if (DEBUG) {
                    printf("> routable router %d\n", router.links[j].dst);
                }
            }
        }
    }
    return NULL;
}

void *msg(void *ptr) {
    int i, dst_id;
    char buf[MSGLEN];
    struct sockaddr_in other;
    LSP lsp;

    /* Set lsp header */
    lsp.header.id = router.id;
    lsp.header.type = MESSAGE;

    other.sin_family = AF_INET;
    other.sin_addr.s_addr = INADDR_ANY;    

    while(1) {
        usleep(MSG_INT);
        if (scanf("%s", buf) != 1) {
            perror("scanf");
        }
        if (strcmp(buf, "msg") == 0) {
            printf("send msg to ?\n");
            if (scanf("%d", &dst_id) != 1) {
                perror("scanf");
            }

            lsp.id = router.id;
            lsp.dst = dst_id;
            lsp.nexthop = dijkstra(graph, MAX_NODES, lsp.id, lsp.dst, 0);

            if (graph[router.id][lsp.nexthop] == INF) {
                printf("> msg router %d is unreachable / router is down\n", lsp.nexthop);
            }
            else {
                /* Find nexthop port */
                for (i=0; i < router.link_cnt; i++) {
                    if (router.links[i].dst == lsp.nexthop) {
                        other.sin_port = htons(router.links[i].dst_port);
                    }
                }
                if (sendto(sock, &lsp, sizeof(LSP), 0, (struct sockaddr *) &other, sizeof(struct sockaddr)) < 0) {
                    printf("cannot send msg to router %d\n", router.links[i].dst);
                }
                printf("> msg router %d\n", lsp.dst);
            }
        }
    }
    return NULL;
}

/* Initialize router and create pthreads for router operations */
int main(int argc, char **argv) {
    int i;
    struct sockaddr_in self;
    pthread_t t_incoming, t_ping, t_flooding, t_msg;

    /* ./router id port host1 port1 host2 port2 ... ; argc will always be odd */
    if (argc < 3 && argc % 2 != 0) {
        printf("Usage: %s id port host1 port1 host2 port2 ...\n", argv[0]);
        exit(1);
    }

    /* Init router */
    router.id = atoi(argv[1]);
    router.port = atoi(argv[2]);
    router.link_cnt = 0;

    /* Init neighbor routers */
    for (i=3; i < argc-1; i+=2)
        config_router(&router, argv[i], argv[i+1]);

    /* Create udp socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("failed to create udp socket");
        exit(1);
    }
    self.sin_family = AF_INET;
    self.sin_addr.s_addr = INADDR_ANY;
    self.sin_port = htons(router.port);

    /* Bind socket */
    if (bind(sock, (struct sockaddr *) &self, sizeof(self))) {
        perror("failed to bind datagram socket");
        exit(1);
    }

    /* Init LSP for self router */
    router.self_lsp.id = router.id;
    router.self_lsp.len = router.link_cnt;
    init_routingtable(&router);
    printf("*** Router %d is online ***\n", router.id);
    sleep(MAX_NODES - router.id - 1);

    /* Create threads for execute/listen requests */
    pthread_create(&t_incoming, NULL, incoming, NULL);
    pthread_create(&t_ping, NULL, ping, NULL);
    pthread_create(&t_flooding, NULL, flooding, NULL);
    pthread_create(&t_msg, NULL, msg, NULL);
    pthread_join(t_incoming, NULL);
    pthread_join(t_ping, NULL);
    pthread_join(t_flooding, NULL);
    pthread_join(t_msg, NULL);

    close(sock);
    return 0;
}
