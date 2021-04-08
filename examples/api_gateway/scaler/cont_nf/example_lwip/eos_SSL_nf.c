#include <eos_pkt.h>
#include <nf_hypercall.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcp.h>
#include <lwip/stats.h>
#include <lwip/prot/tcp.h>
#include "axhttp.h"

#define ETHER_ADDR_LEN 6
#define EOS_MAX_CONNECTION 10
#define EOS_DUMMY_FILE_SIZE 0
#define EOS_FILE_NUM 1

static int conf_file_idx = 0;
static const u16_t port = 0x01BB; // 443
static u16_t tx_port; // 443
static struct ip4_addr ip, mask, gw;
static struct netif cos_if;
static vaddr_t shmem_addr;
static struct eos_ring *input_ring, *output_ring;
static int eth_copy = 0;
static int bump_alloc = 0;
/* SSL structure */
struct serverstruct *servers;
struct connstruct *usedconns;
struct connstruct *freeconns;

struct ip4_hdr {
	uint8_t  version_ihl;
	uint8_t  type_of_service;
	uint16_t total_len;
	uint16_t packet_id;
	uint16_t fragment_offset;
	uint8_t  time_to_live;
	uint8_t  next_proto_id;
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

enum echoserver_state {
	ES_NONE = 0,
	ES_ACCEPTED,
	ES_RECEIVED,
	ES_CLOSING
};

struct echoserver_struct echo_conn[EOS_MAX_CONNECTION];

struct file {
	int size;
	int curr;
};

struct file dummy_file[EOS_FILE_NUM];
struct ether_addr eth_src, eth_dst;
uint16_t ether_type;

/* init size to the file size, curr to 0 when open is called */

/* reset curr to 0 when close is called */

int
eos_dummy_read(int filedesc, char* buf, int size)
{
	int r = 0;
	struct file *f;

	f = &dummy_file[filedesc];
	if (f->curr >= f->size)	return r;
	r = min(size, f->size - f->curr);
	memset(buf, '$', r);
	f->curr += r;
	return r;
}

int
eos_dummy_open(char *filename, int flag)
{
	struct file *f;
	f = &dummy_file[0];
	f->size = EOS_DUMMY_FILE_SIZE;
	f->curr = 0;
	return 0;
}

int
eos_dummy_close(int filedesc)
{
	struct file *f;
	f = &dummy_file[filedesc];
	f->curr = 0;
	return 0;
}

static void
eos_lwip_tcp_err(void *arg,  err_t err)
{
	assert(0);
	return;
}

static void eos_lwip_tcp_close(struct tcp_pcb *tp, struct echoserver_struct *es) {
	tcp_arg(tp, NULL);
	tcp_sent(tp, NULL);
	tcp_recv(tp, NULL);
	tcp_err(tp, NULL);

	if (es != NULL) {
		es->p = NULL;
		//free(es);
	}

	tcp_close(tp);
}

static void
eos_lwip_tcp_send(struct tcp_pcb *tp, struct echoserver_struct *es)
{
	err_t wr_err = ERR_OK;

	assert(es->p);
	wr_err = tcp_write(tp, es->p->payload, es->p->len, 1);
	assert(wr_err == ERR_OK);
	//wr_err = tcp_output(tp);

	if (wr_err == ERR_OK) {
		es->p = NULL;
	} else if (wr_err == ERR_MEM){
		assert(0);
	} else {
		assert(0);
	}
}

int
eos_select(int id)
{
	struct echoserver_struct *es;

	es = &echo_conn[id];
	if (es->p == NULL) {
		return 0;
	}
	return 1;
}

int
eos_lwip_tcp_read(int id, void *buf, int len)
{
	int plen;
	struct echoserver_struct *es;

	es = &echo_conn[id];
	assert(es);
	assert(es->p);

	plen = es->p->len;
	if (plen - es->curr < len) len = plen - es->curr;
	memcpy(buf, (((char*)es->p->payload) + es->curr), len);
	es->curr += len;
	if (es->curr == plen) {
		es->p = NULL;
		es->curr = 0;
	}
	assert(es->curr <= plen);
	return len;
}

int
eos_lwip_tcp_write(int id, void *buf, int len)
{
	struct echoserver_struct *es;

	es = &echo_conn[id];
	assert(es);
	err_t wr_err = ERR_OK;
	assert(es->tp);
	wr_err = tcp_write(es->tp, buf, len, 1);
	assert(wr_err == ERR_OK);

	return len;
}

/* static void *buf[EOS_PKT_MAX_SZ]; */

/* static err_t */
/* eos_echoserver(int id) */
/* { */
/* 	int len; */

/* 	len = eos_lwip_tcp_read(id, buf, EOS_PKT_MAX_SZ); */
/* 	assert(len > 0); */

/* 	// application */

/* 	len = eos_lwip_tcp_write(id, buf, len); */
/* 	assert(len > 0); */
/* 	return ERR_OK; */
/* } */

extern int SSL_server(int id);

static err_t
eos_lwip_tcp_recv(void *arg, struct tcp_pcb *tp, struct pbuf *p, err_t err)
{
	err_t ret_err;
	struct echoserver_struct *es;
	//printc("\tin receive\n");

	es = &echo_conn[(int)arg];
	assert(es);
	if (unlikely(p == NULL)) {
		es->state = ES_CLOSING;
		assert(es->p == NULL);
		eos_lwip_tcp_close(tp, es);
	} else {
	  assert(err == ERR_OK);
	  switch (es->state) {
	  case ES_ACCEPTED:
	    es->state = ES_RECEIVED;

	  case ES_RECEIVED:
	    assert(!es->p);
	    es->p = p;
	    tcp_recved(tp, p->tot_len);
	    SSL_server(es->id);
	    //eos_echoserver(es->id);
	    break;
	  default:
	    assert(0);
	  }
	}
	return ERR_OK;

}

static err_t
eos_lwip_tcp_sent(void *arg, struct tcp_pcb *tp, u16_t len)
{
	/*struct echoserver_struct *es;

	es = (struct echoserver_struct *)arg;
	if (es->p != NULL) {
		eos_lwip_tcp_send(tp, es);
	} else {
		if (es->state == ES_CLOSING) {
			eos_lwip_tcp_close(tp, es);
		}
	}*/
	return ERR_OK;
}

extern int SSL_conn_new(int id);

static err_t
eos_lwip_tcp_accept(void *arg, struct tcp_pcb *tp, err_t err)
{
	err_t ret_err;
	struct echoserver_struct *es;

	assert(bump_alloc < EOS_MAX_CONNECTION);
	es = &echo_conn[bump_alloc];
	if (unlikely(!es)) {
		assert(0);
		return ERR_MEM;
	}

	es->state = ES_ACCEPTED;
	es->tp = tp;
	es->p = NULL;
	es->id = bump_alloc;
	es->curr = 0;
	bump_alloc++;
	
	//printc("\tin accetp\n");
	tcp_arg(tp, (void *)(es->id));
	tcp_err(tp, eos_lwip_tcp_err);
	tcp_recv(tp, eos_lwip_tcp_recv);
	tcp_sent(tp, eos_lwip_tcp_sent);
	tcp_nagle_disable(tp);
	SSL_conn_new(es->id);

	ret_err = ERR_OK;

	return ret_err;
}

static inline void
ether_addr_copy(struct ether_addr *src, struct ether_addr *dst)
{
	*dst = *src;
}

static err_t
ssl_output(struct netif *ni, struct pbuf *p, const ip4_addr_t *ip)
{
	void *pl;
	struct ether_hdr *eth_hdr;
	char *snd_pkt = NULL;
	int r, len;
	char *idx = 0;

	len     = sizeof(struct ether_hdr) + p->tot_len;
	snd_pkt = eos_pkt_allocate(input_ring, len);
	eth_hdr = (struct ether_hdr*)snd_pkt;
	idx     = snd_pkt + sizeof(struct ether_hdr);

	/* generate new ether_hdr*/
	ether_addr_copy(&eth_src, &eth_hdr->src_addr);
	ether_addr_copy(&eth_dst, &eth_hdr->dst_addr);
	eth_hdr->ether_type = ether_type;

	while(p) {
		pl = p->payload;
		memcpy(idx, pl, p->len);

		//assert(p->type != PBUF_POOL);
		idx = idx + p->len;
		p = p->next;
	}
	r = eos_pkt_send(output_ring, (void *)snd_pkt, len, tx_port);
	assert(!r);

	return ERR_OK;
}

static err_t
cos_if_init(struct netif *ni)
{
	ni->name[0] = 'c';
	ni->name[1] = 'n';
	ni->mtu     = 1500;
	ni->output  = ssl_output;

	return ERR_OK;
}

extern int SSL_init(void);

static void
init_lwip(void)
{
	lwip_init();
	IP4_ADDR(&ip, 10,10,1,2);
	IP4_ADDR(&mask, 255,255,255,0);
	IP4_ADDR(&gw, 10,10,1,2);

	netif_add(&cos_if, &ip, &mask, &gw, NULL, cos_if_init, ip4_input);
	netif_set_default(&cos_if);
	netif_set_up(&cos_if);
	netif_set_link_up(&cos_if);

	SSL_init();
}

static void
eos_create_tcp_connection()
{
	err_t ret;
	int queue = 20;
	struct tcp_pcb *new_tp, *tp;
	
	tp = tcp_new();
	//printc("\tin connection\n");
	if (tp == NULL) {
		printc("Could not create tcp connection\n");
	}
	struct ip4_addr ipa = *(struct ip4_addr*)&ip;
	ret = tcp_bind(tp, &ipa, port);
	assert(ret == ERR_OK);

	assert(tp != NULL);
	new_tp = tcp_listen_with_backlog(tp, queue);
	if (NULL == new_tp) {
		assert(0);
	}
	tcp_arg(new_tp, new_tp);
	tcp_accept(new_tp, eos_lwip_tcp_accept);
}

static char curr_pkt[EOS_PKT_MAX_SZ];

static void
cos_net_interrupt(int len, void * pkt)
{
	void *pl;
	struct pbuf *p;
	
	assert(EOS_PKT_MAX_SZ >= len);
	//pl = malloc(len);
	//pl = curr_pkt;
	//memcpy(pl, pkt, len);
	pl = pkt;
	pl = (void *)(pl + sizeof(struct ether_hdr));
	p = pbuf_alloc(PBUF_IP, (len-sizeof(struct ether_hdr)), PBUF_ROM);
	assert(p);
	p->payload = pl;
	if (cos_if.input(p, &cos_if) != ERR_OK) {
		assert(0);
	}

	assert(p);
	/* FIXME: its a hack herer */
	if (p->ref != 0) {
		pbuf_free(p);
	}

	return;
}

static void
init(void)
{
	init_lwip();
	eos_create_tcp_connection();
}

void *
ssl_get_packet(int *len, u16_t *port)
{
	int err, r = 0;
	void *pkt;

	eos_pkt_collect(input_ring, output_ring);
	pkt = eos_pkt_recv(input_ring, len, port, &err, output_ring);
	while (unlikely(!pkt)) {
		//printc("\tpacket get fail\n");
		if (err == -EBLOCK) nf_hyp_block();
		else if (err == -ECOLLET) eos_pkt_collect(input_ring, output_ring);
		pkt = eos_pkt_recv(input_ring, len, port, &err, output_ring);
	}
	if (unlikely(!eth_copy)) {
		struct ether_hdr *eth_hdr = (struct ether_hdr *)pkt;
		eth_copy = 1;
		ether_addr_copy(&eth_hdr->src_addr, &eth_dst);
		ether_addr_copy(&eth_hdr->dst_addr, &eth_src);
		ether_type = eth_hdr->ether_type;
	}
	return pkt;
}

static void
ssl_server_run() {
	int len, r;
	void *pkt;
	
	while(1) {
		pkt = ssl_get_packet(&len, &tx_port);
		assert(pkt);
		assert(len <= EOS_PKT_MAX_SZ);
		if (input_ring->cached.idx == 1) {
			output_ring->cached.cnt = EOS_PKT_PER_ENTRY + 1;
		}
		cos_net_interrupt(len, pkt);
		assert(len < EOS_PKT_MAX_SZ);
		if (input_ring->cached.idx == input_ring->cached.cnt) {
			/* the end of this batch */
			output_ring->cached.cnt = output_ring->cached.idx;
			eos_pkt_send_flush_force(output_ring);
		}
	}
}

void
cos_init(void *args) 
{
	nf_hyp_confidx_get(&conf_file_idx);
	nf_hyp_get_shmem_addr(&shmem_addr);

	input_ring  = get_input_ring((void *)shmem_addr);
	output_ring = get_output_ring((void *)shmem_addr);
	if (conf_file_idx == -1) {
		ssl_server_run();
	} else {
		init();
		printc("ssl server pre populate done\n");
		nf_hyp_checkpoint(cos_spd_id());
		ssl_server_run();
	}
}
