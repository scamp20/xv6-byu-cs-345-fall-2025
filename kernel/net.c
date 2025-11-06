#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

#define MAX_QUEUE 16
#define MAX_BOUND_PORTS 128

struct packet {
  uint32 srcip;
  uint16 sport;
  uint16 len;
  char *data;
};

struct udp_queue {
  struct packet pkts[MAX_QUEUE];
  int head;
  int tail;
  int count;
};

static struct udp_queue udpq[MAX_BOUND_PORTS];
static int bound_ports[MAX_BOUND_PORTS];
static int nbound = 0;

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

void
netinit(void)
{
  initlock(&netlock, "netlock");

  for (int i = 0; i < MAX_BOUND_PORTS; i++) {
    bound_ports[i] = 0;
  }

  for (int i = 0; i < MAX_BOUND_PORTS; i++) {
    udpq[i].head = 0;
    udpq[i].tail = 0;
    udpq[i].count = 0;
    memset(udpq[i].pkts, 0, sizeof(udpq[i].pkts));
  }
}

int queue_pkt(struct udp_queue *q, struct packet *p) {
  if (q->count == MAX_QUEUE)
    return -1;
  q->pkts[q->tail] = *p;
  q->tail = (q->tail + 1) % MAX_QUEUE;
  q->count++;
  return 0;
}

int dequeue_pkt(struct udp_queue *q, struct packet *p) {
  if (q->count == 0)
    return -1;  // queue empty
  *p = q->pkts[q->head];
  q->head = (q->head + 1) % MAX_QUEUE;
  q->count--;
  return 0;
}

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int port;
  argint(0, &port);

  if (port < 0 || port > 65535) return -1;

  for (int i = 0; i < nbound; i++) {
    if (bound_ports[i] == port)
      return 0; // already bound
  }

  if (nbound < MAX_BOUND_PORTS) {
    bound_ports[nbound] = port;
    nbound++;
  } else return -1; // too many ports already bound

  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  struct proc *p = myproc();
  int dport;
  uint64 src_addr, sport_addr, buf_addr;
  int maxlen;

  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf_addr);
  argint(4, &maxlen);

  // find bound port index
  int i;
  for (i = 0; i < nbound; i++) {
    if (bound_ports[i] == dport)
      break;
  }
  if (i == nbound)
    return -1; // not bound

  // get the lock since sleep will handle dropping it and picking it back up
  acquire(&netlock);

  // wait until there's something in the queue
  while (udpq[i].count == 0)
    sleep(&udpq[i], &netlock);

  // dequeue packet
  struct packet pkt;
  if (dequeue_pkt(&udpq[i], &pkt) < 0) {
    release(&netlock);
    return -1;
  }

  // give back the lock since the dequeuing is done
  release(&netlock);

  // only copy up to max length bytes
  int n = pkt.len;
  if (n > maxlen)
    n = maxlen;

  // copy the pkt.data over into the user's buffer pointed to by buf_addr
  if (copyout(p->pagetable, buf_addr, pkt.data, n) < 0) {
    kfree(pkt.data);
    return -1;
  }

  // write src and sport from pkt to user memory
  uint32 srcip = ntohl(pkt.srcip);
  uint16 sport = pkt.sport;
  if (copyout(p->pagetable, src_addr, (char *)&srcip, sizeof(srcip)) < 0 ||
      copyout(p->pagetable, sport_addr, (char *)&sport, sizeof(sport)) < 0) {
    kfree(pkt.data);
    return -1;
  }

  // free up the data now that I sent it to userspace already
  kfree(pkt.data);

  return n;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  // get ethernet frame
  struct eth *eth = (struct eth *)buf;

  // get ip header
  struct ip *ip = (struct ip *)(eth + 1);

  // only handle UDP packets
  if (ip->ip_p != IPPROTO_UDP) {
    kfree(buf);
    return;
  }

  // get udp header
  struct udp *udp = (struct udp *)(ip + 1);

  // extract udp fields
  uint16 sport = ntohs(udp->sport);
  uint16 dport = ntohs(udp->dport);
  uint16 udp_len = ntohs(udp->ulen);
  char *payload = (char *)(udp + 1);
  int payload_len = udp_len - sizeof(struct udp);

  // check if the port is bound
  int bound = 0;
  int i;
  for (i = 0; i < nbound; i++) {
    if (bound_ports[i] == dport) {
      bound = 1;
      break;
    }
  }

  if (!bound) {
    kfree(buf);
    return;
  }

  // copy payload into memory
  char *data = kalloc();
  if (!data) {
    printf("ip_rx: kalloc failed\n");
    kfree(buf);
    return;
  }
  memmove(data, payload, payload_len);

  // kfree buf since we already moved the useful data into the pkt
  kfree(buf);

  // store the header/payload data in the pkt
  struct packet pkt = {
    .srcip = ip->ip_src,
    .sport = sport,
    .len = payload_len,
    .data = data
  };

  // queue / store the pkt for later
  if (queue_pkt(&udpq[i], &pkt) < 0) {
    // queue full; drop packet
    printf("ip_rx: queue full for port %d\n", dport);
    kfree(data);
    return;
  }

  // let the recv code sleeping till data becoems available know that they can wake up and work
  wakeup(&udpq[i]);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0) {
    kfree(inbuf);
    panic("send_arp_reply");
  }
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
