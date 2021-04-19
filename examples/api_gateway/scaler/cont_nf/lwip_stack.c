#include <string.h>

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/prot/tcp.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

// #include "axhttp.h"
#include "cont_nf.h"
#include "dpdk/rte_mbuf_core.h"

#define ETHER_ADDR_LEN 6
#define EOS_MAX_CONNECTION 10

static const u16_t port = 0x01BB;  // 443
// static u16_t tx_port;              // 443
static struct ip4_addr ip, mask, gw;
static struct netif cos_if;
// static struct cont_nf_ring *input_ring, *output_ring;
// static int eth_copy = 0;
static int bump_alloc = 0;

/* SSL structure */
struct serverstruct *servers;
struct connstruct *usedconns;
struct connstruct *freeconns;

struct ip4_hdr {
        uint8_t version_ihl;
        uint8_t type_of_service;
        uint16_t total_len;
        uint16_t packet_id;
        uint16_t fragment_offset;
        uint8_t time_to_live;
        uint8_t next_proto_id;
        uint16_t hdr_checksum;
        uint32_t src_addr;
        uint32_t dst_addr;
} __attribute__((__packed__));

struct ether_addr {
        uint8_t addr_bytes[ETHER_ADDR_LEN];
} __attribute__((__packed__));

struct ether_hdr {
        struct ether_addr dst_addr;
        struct ether_addr src_addr;
        uint16_t ether_type;
} __attribute__((__packed__));

struct echoserver_struct {
        uint8_t state;
        struct tcp_pcb *tp;
        struct pbuf *p;
        int curr;
        int id;
};

enum echoserver_state { ES_NONE = 0, ES_ACCEPTED, ES_RECEIVED, ES_CLOSING };

struct echoserver_struct echo_conn[EOS_MAX_CONNECTION];

struct ether_addr eth_src, eth_dst;
uint16_t ether_type;

static void
cont_nf_lwip_tcp_err(void *arg, err_t err) {
        UNUSED(arg);
        printf("TCP error %d\n", err);
        // assert(0);
        return;
}

static void
cont_nf_lwip_tcp_close(struct tcp_pcb *tp, struct echoserver_struct *es) {
        UNUSED(es);
        tcp_arg(tp, NULL);
        tcp_sent(tp, NULL);
        tcp_recv(tp, NULL);
        tcp_err(tp, NULL);

        tcp_close(tp);
}

static void
cont_nf_lwip_tcp_send(struct tcp_pcb *tp, struct echoserver_struct *es) {
        err_t wr_err;

        // assert(es->p);
        wr_err = tcp_write(tp, es->p->payload, es->p->len, 1);

        if (wr_err == ERR_OK) {
                es->p = NULL;
        } else if (wr_err == ERR_MEM) {
                // assert(0);
        } else {
                // assert(0);
        }
}

int
cont_nf_select(int id) {
        struct echoserver_struct *es;

        es = &echo_conn[id];
        if (es->p == NULL) {
                return 0;
        }
        return 1;
}

int
cont_nf_lwip_tcp_read(int id, void *buf, int len) {
        int plen;
        struct echoserver_struct *es;

        es = &echo_conn[id];
        // assert(es);
        // assert(es->p);

        plen = es->p->len;
        if (plen - es->curr < len)
                len = plen - es->curr;
        memcpy(buf, (((char *)es->p->payload) + es->curr), len);
        es->curr += len;
        if (es->curr == plen) {
                es->p = NULL;
                es->curr = 0;
        }
        // assert(es->curr <= plen);
        return len;
}

int
cont_nf_lwip_tcp_write(int id, void *buf, int len) {
        struct echoserver_struct *es;

        es = &echo_conn[id];
        // assert(es);
        err_t wr_err = ERR_OK;
        // assert(es->tp);
        wr_err = tcp_write(es->tp, buf, len, 1);
        if (wr_err != ERR_OK) {
                printf("Failed to write packet\n");
        }

        return len;
}

// extern int
// SSL_server(int id);

static err_t
cont_nf_lwip_tcp_recv(void *arg, struct tcp_pcb *tp, struct pbuf *p, err_t err) {
        UNUSED(err);

        // err_t ret_err;
        struct echoserver_struct *es;

        es = &echo_conn[*((int *)arg)];
        // assert(es);
        if (p == NULL) {
                printf("Remote host closed TCP connection\n");
                es->state = ES_CLOSING;
                // assert(es->p == NULL);
                cont_nf_lwip_tcp_close(tp, es);
        } else {
                // assert(err == ERR_OK);
                switch (es->state) {
                        case ES_ACCEPTED:
                                es->state = ES_RECEIVED;
                                break;
                        case ES_RECEIVED:
                                // assert(!es->p);
                                es->p = p;
                                tcp_recved(tp, p->tot_len);

                                // TODO: call cat's axTLS code
                                // SSL_server(es->id);
                                break;
                        default:
                                return ERR_ARG;
                                // assert(0);
                }
        }
        return ERR_OK;
}

static err_t
cont_nf_lwip_tcp_sent(void *arg, struct tcp_pcb *tp, u16_t len) {
        UNUSED(len);
        UNUSED(arg);
        struct echoserver_struct *es;

        es = (struct echoserver_struct *)arg;
        if (es->p != NULL) {
                cont_nf_lwip_tcp_send(tp, es);
        } else {
                if (es->state == ES_CLOSING) {
                        cont_nf_lwip_tcp_close(tp, es);
                }
        }
        return ERR_OK;
}

// extern int
// SSL_conn_new(int id);

static err_t
cont_nf_lwip_tcp_accept(void *arg, struct tcp_pcb *tp, err_t err) {
        UNUSED(arg);
        UNUSED(err);
        // err_t ret_err;
        struct echoserver_struct *es;

        // assert(bump_alloc < EOS_MAX_CONNECTION);
        es = &echo_conn[bump_alloc];
        if (!es) {
                // assert(0);
                return ERR_MEM;
        }

        printf("New TCP connection accepted\n");

        es->state = ES_ACCEPTED;
        es->tp = tp;
        es->p = NULL;
        es->id = bump_alloc;
        es->curr = 0;
        bump_alloc++;

        tcp_arg(tp, (void *)(&(es->id)));
        tcp_err(tp, cont_nf_lwip_tcp_err);
        tcp_recv(tp, cont_nf_lwip_tcp_recv);
        tcp_sent(tp, cont_nf_lwip_tcp_sent);

        // TODO: is this useful?
        tcp_nagle_disable(tp);

        // TODO: call @catherinemeadows and @bdevierno axTLS code
        // SSL_conn_new(es->id);

        return ERR_OK;
}

static inline void
ether_addr_copy(struct ether_addr *src, struct ether_addr *dst) {
        *dst = *src;
}

// static err_t
// pipe_output(struct netif *ni, struct pbuf *p, const ip4_addr_t *ip) {
//         void *pl;
//         struct ether_hdr *eth_hdr;
//         char *snd_pkt = NULL;
//         int r, len;
//         char *idx = 0;

//         // convert pbuf to mbuf
//         len = sizeof(struct ether_hdr) + p->tot_len;
//         snd_pkt = cont_nf_pkt_allocate(input_ring, len);
//         eth_hdr = (struct ether_hdr *)snd_pkt;
//         idx = snd_pkt + sizeof(struct ether_hdr);

//         /* generate new ether_hdr*/
//         ether_addr_copy(&eth_src, &eth_hdr->src_addr);
//         ether_addr_copy(&eth_dst, &eth_hdr->dst_addr);
//         eth_hdr->ether_type = ether_type;

//         while (p) {
//                 pl = p->payload;
//                 memcpy(idx, pl, p->len);

//                 // // assert(p->type != PBUF_POOL);
//                 idx = idx + p->len;
//                 p = p->next;
//         }

//         // TODO: write_packet to tx_fd

//         r = cont_nf_pkt_send(output_ring, (void *)snd_pkt, len, tx_port);
//         // assert(!r);

//         return ERR_OK;
// }

static err_t
pipe_output(struct netif *ni, struct pbuf *p, const ip4_addr_t *ip) {
        UNUSED(ni);
        UNUSED(ip);
        struct rte_mbuf packet;
        // TODO: convert p (pbuf) --> packet (mbuf)
        if (!p) {
                return ERR_ARG;
        }

        printf("Sending packet out through network\n");
        if (write_packet(&packet) == -1) {
                switch (errno) {
                        case EAGAIN:
                                // non-blocking response
                                break;
                        default:
                                perror("Couldn't write data to TX pipe");
                                return ERR_IF;
                }
        }

        return ERR_OK;
}

static err_t
tcp_if_init(struct netif *ni) {
        ni->name[0] = 'c';
        ni->name[1] = 'n';
        ni->mtu = 1500;
        ni->output = pipe_output;

        return ERR_OK;
}

// extern int
// SSL_init(void);

static void
init_lwip(void) {
        lwip_init();
        // TODO: fix IP and gateway to reflect dpdk
        IP4_ADDR(&ip, 10, 10, 1, 2);
        IP4_ADDR(&mask, 255, 255, 255, 0);
        IP4_ADDR(&gw, 10, 10, 1, 2);

        netif_add(&cos_if, &ip, &mask, &gw, NULL, tcp_if_init, ip4_input);
        netif_set_default(&cos_if);
        netif_set_up(&cos_if);
        netif_set_link_up(&cos_if);

        // TODO: call cat's axTLS initialization code
        // SSL_init();
}

int
cont_nf_create_tcp_connection() {
        err_t ret;
        int queue = 20;
        struct tcp_pcb *tp;

        tp = tcp_new();
        if (tp == NULL) {
                printf("Could not create tcp connection\n");
                return -1;
        }
        struct ip4_addr ipa = *(struct ip4_addr *)&ip;
        ret = tcp_bind(tp, &ipa, port);
        if (ret != ERR_OK)
                return -1;
        // assert(ret == ERR_OK);

        // assert(tp != NULL);
        tp = tcp_listen_with_backlog(tp, queue);
        if (tp == NULL) {
                return -1;
        }
        tcp_arg(tp, tp);
        tcp_accept(tp, cont_nf_lwip_tcp_accept);

        return 0;
}

// static char curr_pkt[PKT_MAX_SZ];

// static void
// cos_net_interrupt(int len, void *pkt) {
//         void *pl;
//         struct pbuf *p;

//         // assert(PKT_MAX_SZ >= len);
//         pl = pkt;
//         pl = (void *)(pl + sizeof(struct ether_hdr));
//         p = pbuf_alloc(PBUF_IP, (len - sizeof(struct ether_hdr)), PBUF_ROM);
//         // assert(p);
//         p->payload = pl;
//         if (cos_if.input(p, &cos_if) != ERR_OK) {
//                 // assert(0);
//         }

//         // assert(p);
//         /* FIXME: its a hack herer */
//         if (p->ref != 0) {
//                 pbuf_free(p);
//         }

//         return;
// }

int
input_mbuf_to_if(struct rte_mbuf *pkt) {
        struct pbuf *p = NULL;

        // TODO: turn rte_mbuf into pbuf struct
        if (!pkt->buf_addr) {
                return -1;
        }

        if (cos_if.input(p, &cos_if) != ERR_OK) {
                return -1;
        }

        if (!p) {
                return -1;
        }

        if (p->ref != 0) {
                pbuf_free(p);
        }

        return 0;
}

int
init_stack(void) {
        init_lwip();
        if (cont_nf_create_tcp_connection() < 0) {
                return -1;
        }
        return 0;
}

// void *
// ssl_get_packet(int *len, u16_t *port) {
//         int err, r = 0;
//         void *pkt;

//         cont_nf_pkt_collect(input_ring, output_ring);
//         pkt = cont_nf_pkt_recv(input_ring, len, port, &err, output_ring);
//         while (unlikely(!pkt)) {
//                 pkt = cont_nf_pkt_recv(input_ring, len, port, &err, output_ring);
//         }
//         if (unlikely(!eth_copy)) {
//                 struct ether_hdr *eth_hdr = (struct ether_hdr *)pkt;
//                 eth_copy = 1;
//                 ether_addr_copy(&eth_hdr->src_addr, &eth_dst);
//                 ether_addr_copy(&eth_hdr->dst_addr, &eth_src);
//                 ether_type = eth_hdr->ether_type;
//         }
//         return pkt;
// }

// static void
// ssl_server_run() {
//         int len, r;
//         void *pkt;

//         while (1) {
//                 pkt = ssl_get_packet(&len, &tx_port);
//                 cos_net_interrupt(len, pkt);
//         }
// }

// void
// cos_init(void *args) {
//         init();
//         ssl_server_run();
// }
