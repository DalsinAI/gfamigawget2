/* Name and address calls for Wget2 on GFOS, our own (amiga/gfinet.c):
 * getaddrinfo, freeaddrinfo, gai_strerror, getnameinfo, inet_pton, inet_ntop.
 * libnix's socket layer has a getaddrinfo, but its struct addrinfo is the
 * one its own build saw; these are compiled with the same headers as Wget2,
 * so the fields always agree. Names go through gethostbyname()
 * (bsdsocket.library's resolver), numbers are read here. IPv4: an IPv6
 * request answers EAI_FAMILY, and Wget2 goes on with IPv4.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef GFWGET_GFINET_H
#define GFWGET_GFINET_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifndef GFINET_NO_ARPA
#include <arpa/inet.h>
#endif

#ifdef GFINET_NEED_SOCKADDR_STORAGE
/* as large as any address, aligned for any of them; the 4.4BSD length and
   family bytes first, as struct sockaddr has them on the Amiga */
struct sockaddr_storage {
    unsigned char ss_len;
    unsigned char ss_family;
    char __ss_pad1[6];
    long long __ss_align;
    char __ss_pad2[112];
};
#endif
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
#ifndef IN6_IS_ADDR_LINKLOCAL
#define IN6_IS_ADDR_LINKLOCAL(a) \
    (((const unsigned char *)(a))[0] == 0xfe && (((const unsigned char *)(a))[1] & 0xc0) == 0x80)
#endif
#ifndef AI_PASSIVE
#define AI_PASSIVE 0x1
#endif
#ifndef AI_CANONNAME
#define AI_CANONNAME 0x2
#endif
#ifndef AI_NUMERICHOST
#define AI_NUMERICHOST 0x4
#endif
#ifndef EAI_NONAME
#define EAI_NONAME (-2)
#define EAI_FAIL (-4)
#define EAI_FAMILY (-6)
#define EAI_SERVICE (-8)
#define EAI_MEMORY (-10)
#endif

#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0x400
#endif
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0x20
#endif
#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif
#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST 0x1
#define NI_NUMERICSERV 0x2
#define NI_NAMEREQD 0x8
#endif
#ifndef EAI_OVERFLOW
#define EAI_OVERFLOW (-12)
#endif

#define getaddrinfo gfinet_getaddrinfo
#define freeaddrinfo gfinet_freeaddrinfo
#define gai_strerror gfinet_gai_strerror
#define getnameinfo gfinet_getnameinfo
#define inet_pton gfinet_inet_pton
#define inet_ntop gfinet_inet_ntop
#ifdef GFINET_NEED_IN6ADDR_ANY
#define in6addr_any gfinet_in6addr_any
extern const struct in6_addr gfinet_in6addr_any;
#endif

int getaddrinfo(const char *host, const char *serv, const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);
const char *gai_strerror(int err);
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags);
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif
