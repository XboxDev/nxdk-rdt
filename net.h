#ifndef NET_H
#define NET_H

extern struct netif nforce_netif, *g_pnetif;

int net_init(void);
void net_shutdown(void);

#endif
