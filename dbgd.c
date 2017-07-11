#include <hal/input.h>
#include <hal/xbox.h>
#include <hal/xbox.h>
#include <lwip/api.h>
#include <lwip/arch.h>
#include <lwip/debug.h>
#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/opt.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>
#include <lwip/timers.h>
#include <netif/etharp.h>
#include <pbkit/pbkit.h>
#include <pktdrv.h>
#include <protobuf-c/protobuf-c.h>
#include <stdlib.h>
#include <xboxkrnl/xboxkrnl.h>
#include <xboxrt/debug.h>
#include <xboxrt/string.h>

#include "dbg.pb-c.h"
#include "net.h"
#include "dbgd.h"

#define DBGD_PORT 9269

#ifndef HTTPD_DEBUG
#define HTTPD_DEBUG LWIP_DBG_OFF
#endif

static int dbgd_sysinfo(Dbg__Request *req, Dbg__Response *res);
static int dbgd_reboot(Dbg__Request *req, Dbg__Response *res);
static int dbgd_malloc(Dbg__Request *req, Dbg__Response *res);
static int dbgd_free(Dbg__Request *req, Dbg__Response *res);
static int dbgd_mem_read(Dbg__Request *req, Dbg__Response *res);
static int dbgd_mem_write(Dbg__Request *req, Dbg__Response *res);
static int dbgd_debug_print(Dbg__Request *req, Dbg__Response *res);
static int dbgd_show_debug_screen(Dbg__Request *req, Dbg__Response *res);
static int dbgd_show_front_screen(Dbg__Request *req, Dbg__Response *res);

typedef int (*dbgd_req_handler)(Dbg__Request *req, Dbg__Response *res);
static dbgd_req_handler handlers[DBG__REQUEST__TYPE__COUNT] = {
    &dbgd_sysinfo,
    &dbgd_reboot,
    &dbgd_malloc,
    &dbgd_free,
    &dbgd_mem_read,
    &dbgd_mem_write,
    &dbgd_debug_print,
    &dbgd_show_debug_screen,
    &dbgd_show_front_screen,
};

static void dbgd_serve(struct netconn *conn)
{
    Dbg__Request  *req;
    Dbg__Response res;
    err_t         err;
    size_t        res_packed_size;
    struct netbuf *inbuf;
    uint16_t      msglen;
    uint16_t      port = 0;
    uint8_t       *buf;
    uint8_t       *res_packed;
    ip_addr_t     naddr;

    netconn_peer(conn, &naddr, &port);
    debugPrint("[%s connected]\n", ip4addr_ntoa(ip_2_ip4(&naddr)));

    while (1) {
        /* Read the data from the port, blocking if nothing yet there. 
         We assume the request (the part we care about) is in one netbuf */
        err = netconn_recv(conn, &inbuf);
        if (err != ERR_OK) {
            break;
        }
        
        netbuf_data(inbuf, (void**)&buf, &msglen);
        req = dbg__request__unpack(NULL, msglen, buf);

        dbg__response__init(&res);

        if (req == NULL) {
            debugPrint("error: request was null!\n");
            netbuf_delete(inbuf);
            break;
        }

        if (req->type >= DBG__REQUEST__TYPE__COUNT) {
            /* Error! Unsupported request type */
            res.type = DBG__RESPONSE__TYPE__ERROR_UNSUPPORTED;
            break;
        } else {
            res.type = handlers[req->type](req, &res);
        }

        switch (res.type) {
        case DBG__RESPONSE__TYPE__OK:
            res.msg = "Ok";
            break;
        case DBG__RESPONSE__TYPE__ERROR_INCOMPLETE_REQUEST:
            res.msg = "Incomplete Request";
            break;
        case DBG__RESPONSE__TYPE__ERROR_UNSUPPORTED:
            res.msg = "Unsupported";
            break;
        default:
            break;
        }

        /* Send packed response */
        res_packed_size = dbg__response__get_packed_size(&res);
        res_packed = malloc(res_packed_size);
        dbg__response__pack(&res, res_packed);
        netconn_write(conn, res_packed, res_packed_size, NETCONN_COPY);
        free(res_packed);
        dbg__request__free_unpacked(req, NULL);

        /* Delete the buffer (netconn_recv gives us ownership,
         so we have to make sure to deallocate the buffer) */
        netbuf_delete(inbuf);
    }

    /* Close the connection */
    netconn_close(conn);

    debugPrint("[%s disconnected]\n", ip4addr_ntoa(ip_2_ip4(&naddr)));
}

static void dbgd_thread(void *arg)
{
    struct netconn *conn, *newconn;
    err_t err;
    LWIP_UNUSED_ARG(arg);

    conn = netconn_new(NETCONN_TCP);
    LWIP_ERROR("invalid conn", (conn != NULL), return;);

    netconn_bind(conn, NULL, DBGD_PORT);
    netconn_listen(conn);
    
    do {
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK) {
            dbgd_serve(newconn);
            netconn_delete(newconn);
        }
    } while(err == ERR_OK);

    LWIP_DEBUGF(HTTPD_DEBUG, ("dbgd_thread: netconn_accept received error %d, shutting down", err));
    netconn_close(conn);
    netconn_delete(conn);
}

void dbgd_init(void)
{
    sys_thread_new("dbgd",
                   dbgd_thread,
                   NULL,
                   DEFAULT_THREAD_STACKSIZE,
                   DEFAULT_THREAD_PRIO);
}

static int dbgd_sysinfo(Dbg__Request *req, Dbg__Response *res)
{
    static Dbg__SysInfo xb_info;

    dbg__sys_info__init(&xb_info);
    xb_info.tick_count = XGetTickCount();
    res->info = &xb_info;

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_reboot(Dbg__Request *req, Dbg__Response *res)
{
    XReboot();
    asm volatile ("jmp .");

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_malloc(Dbg__Request *req, Dbg__Response *res)
{
    if (!req->has_size)
        return DBG__RESPONSE__TYPE__ERROR_INCOMPLETE_REQUEST;

    res->address = (uint32_t)malloc(req->size);
    res->has_address = 1;

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_free(Dbg__Request *req, Dbg__Response *res)
{
    if (!req->has_address)
        return DBG__RESPONSE__TYPE__ERROR_INCOMPLETE_REQUEST;

    free((void*)req->address);

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_mem_read(Dbg__Request *req, Dbg__Response *res)
{
    if (!req->has_address || !req->has_size)
        return DBG__RESPONSE__TYPE__ERROR_INCOMPLETE_REQUEST;

    res->address = req->address;
    res->has_address = 1;

    res->size = 1; /* FIXME: add word, dword, qword support */
    res->has_size = 1;

    res->value = *((uint8_t*)(req->address));
    res->has_value = 1;

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_mem_write(Dbg__Request *req, Dbg__Response *res)
{
    if (!req->has_address || !req->has_size || !req->has_value)
        return DBG__RESPONSE__TYPE__ERROR_INCOMPLETE_REQUEST;

    *((uint8_t*)(req->address)) = (uint8_t)req->value;

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_debug_print(Dbg__Request *req, Dbg__Response *res)
{
    debugPrint("%s\n", req->msg);

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_show_debug_screen(Dbg__Request *req, Dbg__Response *res)
{
    pb_show_debug_screen();

    return DBG__RESPONSE__TYPE__OK;
}

static int dbgd_show_front_screen(Dbg__Request *req, Dbg__Response *res)
{
    pb_show_front_screen();

    return DBG__RESPONSE__TYPE__OK;
}
