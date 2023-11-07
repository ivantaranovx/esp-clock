
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "lwip/err.h"

#include "dnsd.h"
#include "opt.h"

#include <string.h>

#define TAG "dnsd"
#define PORT 53
#define PKT_SZ_MAX 512
#define QTYPE_A 1
#define QCLASS_IN 1
#define TTL 300

static int dns_socket;
static struct sockaddr_in saddr;
static uint8_t *pkt_buf;
static ip4_addr_t local_ip;

typedef struct __attribute__((packed))
{
    unsigned rd : 1;
    unsigned tc : 1;
    unsigned aa : 1;
    unsigned opcode : 4;
    unsigned qr : 1;
    unsigned rcode : 4;
    unsigned z : 3;
    unsigned ra : 1;
} FLAGS;

typedef struct __attribute__((packed))
{
    uint16_t id;
    FLAGS flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} HEADER;

typedef struct __attribute__((packed))
{
    uint16_t v;
} U16;

typedef struct __attribute__((packed))
{
    uint32_t v;
} U32;

static void header_format(HEADER *header)
{
    header->id = ntohs(header->id);
    header->qdcount = ntohs(header->qdcount);
    header->ancount = ntohs(header->ancount);
    header->nscount = ntohs(header->nscount);
    header->arcount = ntohs(header->arcount);
}

int get_u16(void *buf, uint16_t *v)
{
    U16 *u16 = (U16 *)buf;
    *v = ntohs(u16->v);
    return 2;
}

static char *get_name(uint8_t *buf, int *sz, uint16_t *type, uint16_t *class)
{
    int recv_len = *sz;
    *sz = 0;
    for (;;)
    {
        uint8_t l = buf[*sz];
        *sz += l + 1;
        if (l == 0)
            break;
        if (l > 63)
            return 0;
        if (*sz > recv_len)
            return 0;
    }
    if (*sz < 2)
        return 0;
    char *name = pvPortMalloc(*sz);
    if (name == 0)
        return 0;
    memset(name, 0, *sz);
    int i = 0;
    for (;;)
    {
        uint8_t l = *(buf++);
        if (l == 0)
            break;
        if (i > 0)
            name[i++] = '.';
        memcpy(&name[i], buf, l);
        buf += l;
        i += l;
    }
    buf += get_u16(buf, type);
    buf += get_u16(buf, class);
    *sz += 4;
    return name;
}

int set_name(uint8_t *buf, char *name)
{
    int len = 0;
    for (;;)
    {
        if (*name == 0)
            break;
        char *p = strchr(name, '.');
        if (p == 0)
            p = strchr(name, 0);
        int l = (int)p - (int)name;
        if ((l < 0) || (l > 63))
            break;
        buf[len++] = l;
        memcpy(&buf[len], name, l);
        len += l;
        name += l + 1;
    }
    buf[len++] = 0;
    return len;
}

int set_u16(void *buf, uint16_t v)
{
    U16 *u16 = (U16 *)buf;
    u16->v = htons(v);
    return 2;
}

int set_u32(void *buf, uint32_t v)
{
    U32 *u32 = (U32 *)buf;
    u32->v = htonl(v);
    return 4;
}

static void dns_server_task(void *pvParameters)
{
    HEADER *header = (HEADER *)pkt_buf;
    for (;;)
    {
        socklen_t socklen = sizeof(saddr);
        int pkt_len = recvfrom(dns_socket, pkt_buf, PKT_SZ_MAX, 0, (struct sockaddr *)&saddr, &socklen);
        if (pkt_len <= sizeof(HEADER))
        {
            ESP_LOGE(TAG, "recvfrom: %s", strerror(errno));
            continue;
        }
        header_format(header);
        if (header->flags.qr || (header->flags.opcode > 0) || header->flags.tc || (header->qdcount != 1))
            continue;
        pkt_len -= sizeof(HEADER);
        int rr_off = sizeof(HEADER);

        int sz = pkt_len;
        uint16_t rr_type;
        uint16_t rr_class;
        char *rr_name = get_name(&pkt_buf[rr_off], &sz, &rr_type, &rr_class);
        if (rr_name == 0)
            continue;
        if ((pkt_len == sz) && (rr_type == QTYPE_A) && (rr_class == QCLASS_IN))
        {
            ESP_LOGD(TAG, "A IN %s", rr_name);
            uint16_t id = header->id;
            memset(pkt_buf, 0, PKT_SZ_MAX);
            header->id = id;
            header->flags.qr = 1;
            header->qdcount = 1;
            header->ancount = 1;
            header_format(header);
            pkt_len = sizeof(HEADER);
            pkt_len += set_name(&pkt_buf[pkt_len], rr_name);
            pkt_len += set_u16(&pkt_buf[pkt_len], QTYPE_A);
            pkt_len += set_u16(&pkt_buf[pkt_len], QCLASS_IN);
            pkt_len += set_u16(&pkt_buf[pkt_len], 0xC000 + 12);
            pkt_len += set_u16(&pkt_buf[pkt_len], QTYPE_A);
            pkt_len += set_u16(&pkt_buf[pkt_len], QCLASS_IN);
            pkt_len += set_u32(&pkt_buf[pkt_len], TTL);
            pkt_len += set_u16(&pkt_buf[pkt_len], 4);
            pkt_len += set_u32(&pkt_buf[pkt_len], htonl(local_ip.addr));
            sendto(dns_socket, pkt_buf, pkt_len, 0, (struct sockaddr *)&saddr, sizeof(saddr));
        }
        vPortFree(rr_name);
    }
}

esp_err_t dnsd_init()
{
    esp_err_t err;
    tcpip_adapter_ip_info_t ip_info;
    ERR_RET(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
    local_ip.addr = ip_info.ip.addr;
    saddr.sin_addr.s_addr = local_ip.addr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    dns_socket = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (dns_socket < 0)
    {
        ESP_LOGE(TAG, "socket: %s", strerror(errno));
        goto err;
    }
    if (bind(dns_socket, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        ESP_LOGE(TAG, "bind: %s", strerror(errno));
        goto err;
    }
    pkt_buf = pvPortMalloc(PKT_SZ_MAX);
    if (pkt_buf == 0)
    {
        ESP_LOGE(TAG, "out of memory");
        goto err;
    }
    if (pdPASS == xTaskCreate(dns_server_task, TAG, 800, NULL, 5, NULL))
    {
        ESP_LOGI(TAG, "listen on %s", inet_ntoa(local_ip));
        return ESP_OK;
    }
err:
    if (dns_socket >= 0)
        close(dns_socket);
    if (pkt_buf != 0)
        vPortFree(pkt_buf);
    return ESP_FAIL;
}
