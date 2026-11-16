/* Name and address calls for Wget2 on GFOS (see gfinet.h). The build
 * passes GFINET_NEED_SOCKADDR_STORAGE and GFINET_SIN_LEN on the Amiga.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfinet.h"

#ifdef GFINET_NEED_IN6ADDR_ANY
const struct in6_addr gfinet_in6addr_any;
#endif

/* a.b.c.d, each 0-255, nothing else (inet_pton's IPv4 form) */
static int pton4(const char *s, unsigned char *out)
{
    int part = 0, digits = 0;
    unsigned int v = 0;

    for (;; s++) {
        if (*s >= '0' && *s <= '9') {
            if (digits && v == 0) return 0;   /* no leading zeros */
            v = v * 10 + (unsigned)(*s - '0');
            if (v > 255 || ++digits > 3) return 0;
        } else if (*s == '.' || *s == '\0') {
            if (!digits || part > 3) return 0;
            out[part++] = (unsigned char)v;
            v = 0;
            digits = 0;
            if (!*s) break;
        } else
            return 0;
    }
    return part == 4;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* RFC 4291 text: groups of up to 4 hex digits, one "::", an IPv4 tail */
static int pton6(const char *s, unsigned char *out)
{
    unsigned char a[16];
    int n = 0, gap = -1, h, digits;
    unsigned int v;

    memset(a, 0, sizeof a);
    if (s[0] == ':') {
        if (s[1] != ':') return 0;
        s++;
    }
    while (*s) {
        if (*s == ':') {
            if (gap >= 0) return 0;
            gap = n;
            s++;
            continue;
        }
        v = 0;
        digits = 0;
        const char *start = s;
        while ((h = hexval(*s)) >= 0) {
            v = v * 16 + (unsigned)h;
            if (++digits > 4) return 0;
            s++;
        }
        if (*s == '.') {
            if (n > 12) return 0;
            if (!pton4(start, a + n)) return 0;
            n += 4;
            break;
        }
        if (!digits || n > 14) return 0;
        a[n++] = (unsigned char)(v >> 8);
        a[n++] = (unsigned char)v;
        if (*s == ':') {
            s++;
            if (!*s) return 0;   /* a trailing single colon */
            if (*s == ':') {
                if (gap >= 0) return 0;
                gap = n;
                s++;
            }
        } else if (*s)
            return 0;
    }
    if (gap >= 0) {
        if (n == 16) return 0;
        memmove(a + 16 - (n - gap), a + gap, (size_t)(n - gap));
        memset(a + gap, 0, (size_t)(16 - n));
    } else if (n != 16)
        return 0;
    memcpy(out, a, 16);
    return 1;
}

int inet_pton(int af, const char *src, void *dst)
{
    if (af == AF_INET) return pton4(src, dst);
#ifdef AF_INET6
    if (af == AF_INET6) return pton6(src, dst);
#endif
    errno = EAFNOSUPPORT;
    return -1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    const unsigned char *b = src;
    char t[INET6_ADDRSTRLEN];
    int i, best = -1, bestlen = 0, run, len = 0;

    if (af == AF_INET) {
        len = snprintf(t, sizeof t, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
#ifdef AF_INET6
    } else if (af == AF_INET6) {
        unsigned int w[8];
        for (i = 0; i < 8; i++) w[i] = (unsigned)b[2 * i] << 8 | b[2 * i + 1];
        /* the longest run of two or more zero groups becomes "::" */
        for (i = 0; i < 8; i += run ? run : 1) {
            for (run = 0; i + run < 8 && !w[i + run]; run++);
            if (run > bestlen && run > 1) { best = i; bestlen = run; }
        }
        if (best == 0 && (bestlen == 6 || (bestlen == 5 && w[5] == 0xffff))) {
            /* ::a.b.c.d and ::ffff:a.b.c.d */
            len = snprintf(t, sizeof t, "::%s%u.%u.%u.%u", bestlen == 5 ? "ffff:" : "",
                           b[12], b[13], b[14], b[15]);
        } else {
            for (i = 0; i < 8; i++) {
                if (i == best) {
                    len += snprintf(t + len, sizeof t - (size_t)len, "::");
                    i += bestlen - 1;
                    continue;
                }
                len += snprintf(t + len, sizeof t - (size_t)len, "%s%x",
                                (i && i != best + bestlen) ? ":" : "", w[i]);
            }
        }
#endif
    } else {
        errno = EAFNOSUPPORT;
        return NULL;
    }
    if ((socklen_t)len + 1 > size) {
        errno = ENOSPC;
        return NULL;
    }
    memcpy(dst, t, (size_t)len + 1);
    return dst;
}

const char *gai_strerror(int err)
{
    switch (err) {
    case 0: return "Success";
    case EAI_NONAME: return "Name or service not known";
    case EAI_FAMILY: return "Address family not supported (IPv4 only on this build)";
    case EAI_SERVICE: return "Service not supported (give the port as a number)";
    case EAI_MEMORY: return "Out of memory";
    case EAI_FAIL: return "The name server failed";
    case EAI_OVERFLOW: return "Buffer too small for the answer";
    default: return "Unknown getaddrinfo error";
    }
}

void freeaddrinfo(struct addrinfo *ai)
{
    while (ai) {
        struct addrinfo *next = ai->ai_next;
        free(ai);   /* one block: the entry, its address and its name */
        ai = next;
    }
}

/* one IPv4 answer: entry, address and (optionally) the canonical name in one block */
static struct addrinfo *one(const unsigned char *ip4, unsigned short port,
                            const struct addrinfo *hints, const char *canon)
{
    size_t cl = canon ? strlen(canon) + 1 : 0;
    struct addrinfo *ai = calloc(1, sizeof *ai + sizeof(struct sockaddr_in) + cl);
    struct sockaddr_in *sin;

    if (!ai) return NULL;
    sin = (struct sockaddr_in *)(ai + 1);
    sin->sin_family = AF_INET;
#ifdef GFINET_SIN_LEN
    sin->sin_len = sizeof *sin;
#endif
    sin->sin_port = htons(port);
    memcpy(&sin->sin_addr, ip4, 4);
    ai->ai_family = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : 0;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;
    ai->ai_addrlen = sizeof *sin;
    ai->ai_addr = (struct sockaddr *)sin;
    if (canon) {
        ai->ai_canonname = (char *)(sin + 1);
        memcpy(ai->ai_canonname, canon, cl);
    }
    return ai;
}

int getaddrinfo(const char *host, const char *serv, const struct addrinfo *hints, struct addrinfo **res)
{
    unsigned char ip[4];
    unsigned long port = 0;
    int flags = hints ? hints->ai_flags : 0;
    struct addrinfo *head = NULL, **tail = &head;

    *res = NULL;
    if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) {
        /* an IPv6-only request (wget2 -6): no IPv6 here */
        errno = EAFNOSUPPORT;
        return EAI_FAMILY;
    }
    if (!host && !serv)
        return EAI_NONAME;
    if (serv && *serv) {
        char *end;
        port = strtoul(serv, &end, 10);
        if (*end || port > 65535) {
            struct servent *se = getservbyname((char *)serv, NULL);
            if (!se) return EAI_SERVICE;
            port = ntohs((unsigned short)se->s_port);
        }
    }
    if (!host) {
        unsigned long a = (flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK;
        ip[0] = (unsigned char)(a >> 24); ip[1] = (unsigned char)(a >> 16);
        ip[2] = (unsigned char)(a >> 8); ip[3] = (unsigned char)a;
        *res = one(ip, (unsigned short)port, hints, NULL);
        return *res ? 0 : EAI_MEMORY;
    }
    if (pton4(host, ip)) {
        *res = one(ip, (unsigned short)port, hints, NULL);
        return *res ? 0 : EAI_MEMORY;
    }
#ifdef AF_INET6
    {
        unsigned char ip6[16];
        if (pton6(host, ip6)) {   /* an IPv6 number */
            errno = EAFNOSUPPORT;
            return EAI_FAMILY;
        }
    }
#endif
    if (flags & AI_NUMERICHOST)
        return EAI_NONAME;
    {
        struct hostent *he = gethostbyname((char *)host);
        int i;
        if (!he || he->h_addrtype != AF_INET || he->h_length != 4 || !he->h_addr_list[0])
            return EAI_NONAME;
        for (i = 0; he->h_addr_list[i]; i++) {
            struct addrinfo *ai = one((const unsigned char *)he->h_addr_list[i], (unsigned short)port,
                                      hints, (i == 0 && (flags & AI_CANONNAME)) ? he->h_name : NULL);
            if (!ai) {
                freeaddrinfo(head);
                return EAI_MEMORY;
            }
            *tail = ai;
            tail = &ai->ai_next;
        }
    }
    *res = head;
    return 0;
}

/* numbers only, as Wget2 asks (NI_NUMERICHOST | NI_NUMERICSERV); a name
   is asked of gethostbyaddr() when NI_NUMERICHOST is not given */
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

    if (!sa || salen < (socklen_t)sizeof *sin)
        return EAI_FAIL;
    if (sa->sa_family != AF_INET)
        return EAI_FAMILY;
    if (host && hostlen) {
        struct hostent *he = NULL;
        if (!(flags & NI_NUMERICHOST))
            he = gethostbyaddr((char *)&sin->sin_addr, 4, AF_INET);
        if (he && he->h_name) {
            if (strlen(he->h_name) >= (size_t)hostlen)
                return EAI_OVERFLOW;
            strcpy(host, he->h_name);
        } else if (flags & NI_NAMEREQD) {
            return EAI_NONAME;
        } else if (!inet_ntop(AF_INET, &sin->sin_addr, host, hostlen)) {
            return EAI_OVERFLOW;
        }
    }
    if (serv && servlen) {
        char b[8];
        int n = snprintf(b, sizeof b, "%u", (unsigned)ntohs(sin->sin_port));
        if (n < 0 || (socklen_t)n >= servlen)
            return EAI_OVERFLOW;
        memcpy(serv, b, (size_t)n + 1);
    }
    return 0;
}
