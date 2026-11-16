/* Wget2's TLS through GFOpenTLS (gfopentls.library), in libwget's place
 * for ssl_none.c (scripts/wget2-common.sh lists it with libwget's files).
 * The calls are libwget's wget_ssl_* (include/wget/wget.h); the other
 * backends (ssl_gnutls.c, ssl_openssl.c, ssl_wolfssl.c) do the same over
 * their own libraries.
 *
 * TLS records go through an I/O hook (OT_SetIOHook) that reads and writes
 * Wget2's own socket descriptor (libnix's socket layer on the Amiga, which
 * is not a bsdsocket.library number OT_SetSocket could take). The socket is
 * non-blocking: when the hook would block it says which way, and
 * wget_ready_2_read()/wget_ready_2_write() wait with Wget2's timeout.
 *
 * Settings: --ca-certificate adds a PEM file to the trust store; the store
 * is GFOpenTLS's own (S:GFOpenTLS/ca-bundle.pem, ENVARC:GFOpenTLS/certs/)
 * unless --ca-directory names another folder, whose PEM files are then the
 * store. --no-check-certificate is OTV_NONE (checked, reported, not
 * enforced), --no-check-hostname OTV_NO_HOSTNAME. --secure-protocol takes
 * TLSv1_2, TLSv1_3, auto/PFS. ALPN is offered as Wget2 asks (http/1.1 on
 * this build: no HTTP/2). Sessions are kept per host while Wget2 runs, so a
 * second connection to the same host resumes. Client certificates, OCSP,
 * CRLs and HPKP pins are not offered: GFOpenTLS has no calls for them.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <wget.h>
#include "private.h"
#include "net.h"

#ifdef __amigaos__
#define OPENTLS_BASE_NAME GFWget_OpenTLSBase
#include <proto/exec.h>
#include <proto/gfopentls.h>
struct Library *GFWget_OpenTLSBase;
#else
#include <utility/hooks.h>
#include <libraries/gfopentls.h>
#include <clib/gfopentls_protos.h>
#endif

static struct config {
	const char
		*secure_protocol,
		*ca_directory,
		*ca_file,
		*cert_file,
		*key_file,
		*crl_file,
		*ocsp_server,
		*alpn;
	wget_ocsp_db
		*ocsp_cert_cache,
		*ocsp_host_cache;
	wget_tls_session_db
		*tls_session_cache;
	wget_hpkp_db
		*hpkp_cache;
	char
		ca_type,
		cert_type,
		key_type;
	bool
		check_certificate : 1,
		report_invalid_cert : 1,
		check_hostname : 1,
		print_info : 1,
		ocsp : 1,
		ocsp_date : 1,
		ocsp_stapling : 1,
		ocsp_nonce : 1;
} config = {
	.check_certificate = 1,
	.report_invalid_cert = 1,
	.check_hostname = 1,
	.ocsp_date = 1,
	.ocsp_stapling = 1,
	.ocsp_nonce = 1,
	.ca_type = WGET_SSL_X509_FMT_PEM,
	.cert_type = WGET_SSL_X509_FMT_PEM,
	.key_type = WGET_SSL_X509_FMT_PEM,
	.secure_protocol = "AUTO",
	.ca_directory = "system",
#ifdef WITH_LIBNGHTTP2
	.alpn = "h2,http/1.1",
#endif
};

static wget_tls_stats_callback *tls_stats_callback;
static void *tls_stats_ctx;
static struct OTContext *ot_ctx;
static int init;
static wget_thread_mutex mutex;

/* the sessions of this run, one per host: a later connection resumes */
#define SESSIONS 8
static struct { char *host; struct OTSession *s; } sessions[SESSIONS];
static int session_next;

struct gfwget_tls {
	struct OTConnection *conn;
	struct Hook hook;
	int fd;
	int blocked;             /* OTIO_READ or OTIO_WRITE: the way the hook would block */
	char *host;
	struct OTSession *pending;   /* TLS 1.3: the session until a ticket comes */
};

void wget_ssl_set_config_string(int key, const char *value)
{
	switch (key) {
	case WGET_SSL_SECURE_PROTOCOL: config.secure_protocol = value; break;
	case WGET_SSL_CA_DIRECTORY: config.ca_directory = value; break;
	case WGET_SSL_CA_FILE: config.ca_file = value; break;
	case WGET_SSL_CERT_FILE: config.cert_file = value; break;
	case WGET_SSL_KEY_FILE: config.key_file = value; break;
	case WGET_SSL_CRL_FILE: config.crl_file = value; break;
	case WGET_SSL_OCSP_SERVER: config.ocsp_server = value; break;
	case WGET_SSL_ALPN: config.alpn = value; break;
	default: error_printf(_("Unknown config key %d (or value must not be a string)\n"), key);
	}
}

void wget_ssl_set_config_object(int key, void *value)
{
	switch (key) {
	case WGET_SSL_OCSP_CACHE: config.ocsp_cert_cache = (wget_ocsp_db *)value; break;
	case WGET_SSL_SESSION_CACHE: config.tls_session_cache = (wget_tls_session_db *)value; break;
	case WGET_SSL_HPKP_CACHE: config.hpkp_cache = (wget_hpkp_db *)value; break;
	default: error_printf(_("Unknown config key %d (or value must not be an object)\n"), key);
	}
}

void wget_ssl_set_config_int(int key, int value)
{
	switch (key) {
	case WGET_SSL_CHECK_CERTIFICATE: config.check_certificate = (bool) value; break;
	case WGET_SSL_REPORT_INVALID_CERT: config.report_invalid_cert = (bool) value; break;
	case WGET_SSL_CHECK_HOSTNAME: config.check_hostname = (bool) value; break;
	case WGET_SSL_CA_TYPE: config.ca_type = (char) value; break;
	case WGET_SSL_CERT_TYPE: config.cert_type = (char) value; break;
	case WGET_SSL_KEY_TYPE: config.key_type = (char) value; break;
	case WGET_SSL_PRINT_INFO: config.print_info = (bool) value; break;
	case WGET_SSL_OCSP: config.ocsp = (bool) value; break;
	case WGET_SSL_OCSP_DATE: config.ocsp_date = (bool) value; break;
	case WGET_SSL_OCSP_STAPLING: config.ocsp_stapling = (bool) value; break;
	case WGET_SSL_OCSP_NONCE: config.ocsp_nonce = (bool) value; break;
	case WGET_SSL_DANE: break;
	default: error_printf(_("Unknown config key %d (or value must not be an integer)\n"), key);
	}
}

/* every *.pem / *.crt file of a folder into the trust store */
static int add_trust_dir(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d)
		return 0;
	while ((e = readdir(d))) {
		size_t l = strlen(e->d_name);
		char *path;
		if (l < 5 || (wget_strcasecmp_ascii(e->d_name + l - 4, ".pem")
			&& wget_strcasecmp_ascii(e->d_name + l - 4, ".crt")))
			continue;
		if (!(path = wget_aprintf("%s%s%s", dir,
			(dir[0] && dir[strlen(dir) - 1] != '/' && dir[strlen(dir) - 1] != ':') ? "/" : "", e->d_name)))
			continue;
		if (OT_AddTrustFile(ot_ctx, (CONST_STRPTR) path) == OTERR_OK)
			n++;
		else
			debug_printf("GFOpenTLS: %s not added to the trust store\n", path);
		xfree(path);
	}
	closedir(d);
	return n;
}

/* the TLS versions --secure-protocol asks for */
static void protocol_range(LONG *min, LONG *max)
{
	const char *p = config.secure_protocol;

	*min = OT_TLS12;
	*max = OT_TLS13;
	if (!p)
		return;
	if (!wget_strcasecmp_ascii(p, "TLSv1_2"))
		*max = OT_TLS12;
	else if (!wget_strcasecmp_ascii(p, "TLSv1_3"))
		*min = OT_TLS13;
}

void wget_ssl_init(void)
{
	wget_thread_mutex_lock(mutex);

	if (!init) {
		struct OTContextConfig cfg;
		LONG err = 0;
		bool own_dir = config.ca_directory && *config.ca_directory
			&& wget_strcasecmp_ascii(config.ca_directory, "system");

#ifdef __amigaos__
		if (!GFWget_OpenTLSBase)
			GFWget_OpenTLSBase = OpenLibrary((CONST_STRPTR) "gfopentls.library", 1);
		if (!GFWget_OpenTLSBase) {
			error_printf(_("HTTPS needs gfopentls.library (GFOpenUp part GFOpenTLS)\n"));
			wget_thread_mutex_unlock(mutex);
			return;
		}
#endif
		memset(&cfg, 0, sizeof(cfg));
		cfg.Size = sizeof(cfg);
		protocol_range(&cfg.MinVersion, &cfg.MaxVersion);
		if (own_dir || !config.check_certificate)
			cfg.Flags |= OTCF_NO_SYSTEM_TRUST;
		if (config.check_certificate && config.ca_file && *config.ca_file)
			cfg.CAFile = (STRPTR) config.ca_file;

		if (!(ot_ctx = OT_NewContext(&cfg, &err))) {
			error_printf(_("GFOpenTLS: no TLS context: %s\n"), (const char *) OT_ErrorString(err));
		} else {
			if (own_dir && config.check_certificate) {
				int n = add_trust_dir(config.ca_directory);
				debug_printf("GFOpenTLS: %d certificates from %s\n", n, config.ca_directory);
			}
			if (config.cert_file || config.key_file)
				info_printf(_("GFOpenTLS: client certificates are not supported, not sent\n"));
			init = 1;
		}
	}

	wget_thread_mutex_unlock(mutex);
}

void wget_ssl_deinit(void)
{
	int i;

	wget_thread_mutex_lock(mutex);

	if (init) {
		for (i = 0; i < SESSIONS; i++) {
			if (sessions[i].s)
				OT_FreeSession(sessions[i].s);
			xfree(sessions[i].host);
			sessions[i].s = NULL;
		}
		OT_FreeContext(ot_ctx);
		ot_ctx = NULL;
		init = 0;
	}
#ifdef __amigaos__
	if (GFWget_OpenTLSBase) {
		CloseLibrary(GFWget_OpenTLSBase);
		GFWget_OpenTLSBase = NULL;
	}
#endif

	wget_thread_mutex_unlock(mutex);
}

/* ---- the I/O hook: TLS records over Wget2's socket ---------------------- */
static LONG tls_io(struct gfwget_tls *t, struct OTIOMessage *msg)
{
	ssize_t n;

	if (msg->MethodID == OTIO_READ) {
		n = recv(t->fd, msg->Buffer, (size_t) msg->Length, 0);
		if (n >= 0)
			return (LONG) n;   /* 0: the peer closed */
	} else if (msg->MethodID == OTIO_WRITE) {
		n = send(t->fd, msg->Buffer, (size_t) msg->Length, 0);
		if (n > 0)
			return (LONG) n;
		if (n == 0) {
			t->blocked = OTIO_WRITE;
			return OTERR_WOULDBLOCK;
		}
	} else
		return OTERR_ARGS;

	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
		t->blocked = (int) msg->MethodID;
		return OTERR_WOULDBLOCK;
	}
	return OTERR_IO;
}

#ifdef __amigaos__
static ULONG tls_hook(register struct Hook *hook __asm("a0"),
                      register APTR object __asm("a2"),
                      register struct OTIOMessage *msg __asm("a1"))
{
	(void) object;
	return (ULONG) tls_io(hook->h_Data, msg);
}
#else
static ULONG tls_hook(struct Hook *hook, APTR object, APTR msg)
{
	(void) object;
	return (ULONG) tls_io(hook->h_Data, msg);
}
#endif

/* waits for the way the last call blocked: >0 ready, 0 timeout, <0 error */
static int wait_blocked(struct gfwget_tls *t, int timeout)
{
	int way = t->blocked;

	t->blocked = 0;
	if (way == OTIO_WRITE)
		return wget_ready_2_write(t->fd, timeout);
	return wget_ready_2_read(t->fd, timeout);
}

/* ---- sessions ------------------------------------------------------------ */
static struct OTSession *session_find(const char *host)
{
	int i;

	for (i = 0; i < SESSIONS; i++)
		if (sessions[i].host && !strcmp(sessions[i].host, host))
			return sessions[i].s;
	return NULL;
}

/* keeps s (taken over) for host, in place of an older one */
static void session_keep(const char *host, struct OTSession *s)
{
	int i, slot = -1;

	wget_thread_mutex_lock(mutex);
	for (i = 0; i < SESSIONS; i++)
		if (sessions[i].host && !strcmp(sessions[i].host, host))
			slot = i;
	if (slot < 0) {
		slot = session_next;
		session_next = (session_next + 1) % SESSIONS;
		xfree(sessions[slot].host);
		sessions[slot].host = wget_strdup(host);
	}
	if (sessions[slot].s)
		OT_FreeSession(sessions[slot].s);
	sessions[slot].s = s;
	wget_thread_mutex_unlock(mutex);
}

/* TLS 1.3: the session goes to the cache once its ticket has come */
static void session_check(struct gfwget_tls *t)
{
	if (t->pending && OT_SessionHasTicket(t->pending) == 1) {
		session_keep(t->host, t->pending);
		t->pending = NULL;
	}
}

static int map_error(LONG err)
{
	switch (err) {
	case OTERR_UNTRUSTED:
	case OTERR_HOSTNAME:
	case OTERR_EXPIRED:
	case OTERR_BADCERT:
	case OTERR_TRUSTSTORE:
	case OTERR_PINNED:
		return WGET_E_CERTIFICATE;
	case OTERR_NOMEM:
		return WGET_E_MEMORY;
	case OTERR_IO:
	case OTERR_CLOSED:
		return WGET_E_CONNECT;
	default:
		return WGET_E_HANDSHAKE;
	}
}

int wget_ssl_open(wget_tcp *tcp)
{
	struct gfwget_tls *t;
	struct OTSession *cached;
	const char *host;
	LONG err = 0, rc;
	long long before = 0;
	int ret;

	if (!tcp)
		return WGET_E_INVALID;
	if (!init)
		wget_ssl_init();
	if (!init)
		return WGET_E_TLS_DISABLED;

	host = tcp->ssl_hostname ? tcp->ssl_hostname : tcp->host;
	if (!host)
		return WGET_E_INVALID;
	if (!(t = wget_calloc(1, sizeof(*t))))
		return WGET_E_MEMORY;
	t->fd = tcp->sockfd;
	t->host = wget_strdup(host);
	if (!(t->conn = OT_NewConnection(ot_ctx, (CONST_STRPTR) host, &err))) {
		error_printf(_("GFOpenTLS: no connection: %s\n"), (const char *) OT_ErrorString(err));
		xfree(t->host);
		xfree(t);
		return WGET_E_MEMORY;
	}
#ifdef __amigaos__
	t->hook.h_Entry = (HOOKFUNC) (void (*)(void)) tls_hook;   /* registers a0/a2/a1 */
#else
	t->hook.h_Entry = tls_hook;
#endif
	t->hook.h_Data = t;
	OT_SetIOHook(t->conn, &t->hook);

	if (!config.check_certificate)
		OT_SetVerify(t->conn, config.report_invalid_cert ? OTV_REPORT : OTV_NONE);
	else if (!config.check_hostname || !tcp->ssl_hostname)
		OT_SetVerify(t->conn, OTV_NO_HOSTNAME);
	if (config.alpn && *config.alpn) {
		if (OT_SetALPN(t->conn, (CONST_STRPTR) config.alpn) == OTERR_OK)
			debug_printf("ALPN offering %s\n", config.alpn);
	}

	wget_thread_mutex_lock(mutex);
	if ((cached = session_find(host)))
		OT_SetSession(t->conn, cached);   /* copied: the cache keeps its own */
	wget_thread_mutex_unlock(mutex);

	tcp->ssl_session = t;
	if (tls_stats_callback)
		before = wget_get_timemillis();

	/* the handshake, waiting with Wget2's timeout whenever the socket would block */
	ret = WGET_E_SUCCESS;
	while ((rc = OT_Handshake(t->conn)) != OTERR_OK) {
		if (rc == OTERR_WOULDBLOCK) {
			int w = wait_blocked(t, tcp->connect_timeout > 0 ? tcp->connect_timeout : tcp->timeout);
			if (w > 0)
				continue;
			ret = w == 0 ? WGET_E_TIMEOUT : WGET_E_CONNECT;
		} else {
			ret = map_error(rc);
			if (ret == WGET_E_CERTIFICATE)
				error_printf(_("%s: certificate not accepted: %s\n"), host,
					(const char *) OT_GetErrorText(t->conn));
			else
				error_printf(_("%s: TLS handshake failed: %s\n"), host,
					(const char *) OT_GetErrorText(t->conn));
		}
		break;
	}

	if (ret == WGET_E_SUCCESS) {
		const char *alpn = (const char *) OT_GetALPN(t->conn);
		LONG proto = OT_GetProtocol(t->conn);
		int resumed = OT_SessionResumed(t->conn) == 1;

		debug_printf("Handshake completed%s: %s, %s\n", resumed ? " (resumed session)" : "",
			proto == OT_TLS13 ? "TLSv1.3" : "TLSv1.2", (const char *) OT_GetCipher(t->conn));
		if (!config.check_certificate && OT_GetVerifyResult(t->conn) != OTERR_OK && config.report_invalid_cert)
			info_printf(_("%s: the certificate did not check out (%s), going on as asked\n"), host,
				(const char *) OT_ErrorString(OT_GetVerifyResult(t->conn)));
		if (alpn && !strcmp(alpn, "h2"))
			tcp->protocol = WGET_PROTOCOL_HTTP_2_0;
		if (!resumed) {
			struct OTSession *s = OT_GetSession(t->conn);
			if (s) {
				if (proto == OT_TLS13 && OT_SessionHasTicket(s) != 1)
					t->pending = s;   /* the ticket comes with a later read */
				else
					session_keep(host, s);
			}
		}
		if (tls_stats_callback) {
			wget_tls_stats_data stats = {
				.hostname = host,
				.alpn_protocol = alpn,
				.version = proto == OT_TLS13 ? 5 : 4,
				.false_start = 0,
				.tfo = 0,
				.resumed = resumed,
				.tls_con = 1,
				.http_protocol = tcp->protocol,
				.cert_chain_size = (size_t) (OT_GetPeerChainLength(t->conn) > 0 ? OT_GetPeerChainLength(t->conn) : 0),
				.tls_secs = wget_get_timemillis() - before,
			};
			tls_stats_callback(&stats, tls_stats_ctx);
		}
		return WGET_E_SUCCESS;
	}

	wget_ssl_close(&tcp->ssl_session);
	return ret;
}

void wget_ssl_close(void **session)
{
	struct gfwget_tls *t;

	if (!session || !*session)
		return;
	t = *session;
	if (t->conn) {
		LONG rc;
		int tries = 0;
		/* close_notify; a peer that is slow to take it is not waited for long */
		while ((rc = OT_Close(t->conn)) == OTERR_WOULDBLOCK && tries++ < 3)
			if (wait_blocked(t, 1000) <= 0)
				break;
		OT_FreeConnection(t->conn);
	}
	if (t->pending)
		OT_FreeSession(t->pending);
	xfree(t->host);
	xfree(t);
	*session = NULL;
}

ssize_t wget_ssl_read_timeout(void *session, char *buf, size_t count, int timeout)
{
	struct gfwget_tls *t = session;
	LONG n;

	if (!t || !t->conn)
		return -1;
	if (count > 0x7fffffff)
		count = 0x7fffffff;
	for (;;) {
		n = OT_Read(t->conn, buf, (LONG) count);
		if (t->pending)
			session_check(t);
		if (n >= 0)
			return n;   /* 0: close_notify */
		if (n == OTERR_CLOSED)
			return 0;   /* the peer just closed: the end of a Connection: close body */
		if (n != OTERR_WOULDBLOCK) {
			debug_printf("GFOpenTLS read: %s\n", (const char *) OT_GetErrorText(t->conn));
			return -1;
		}
		/* a whole record may already be waiting in GFOpenTLS */
		if (OT_Pending(t->conn) > 0)
			continue;
		{
			int w = wait_blocked(t, timeout);
			if (w <= 0)
				return w;   /* 0: timeout, as the other backends */
		}
	}
}

ssize_t wget_ssl_write_timeout(void *session, const char *buf, size_t count, int timeout)
{
	struct gfwget_tls *t = session;
	LONG n;

	if (!t || !t->conn)
		return -1;
	if (count > 0x7fffffff)
		count = 0x7fffffff;
	for (;;) {
		n = OT_Write(t->conn, (CONST_APTR) buf, (LONG) count);
		if (n > 0)
			return n;
		if (n != OTERR_WOULDBLOCK) {
			debug_printf("GFOpenTLS write: %s\n", (const char *) OT_GetErrorText(t->conn));
			return -1;
		}
		{
			int w = wait_blocked(t, timeout);
			if (w <= 0)
				return w;
		}
	}
}

void wget_ssl_set_stats_callback_tls(wget_tls_stats_callback *fn, void *ctx)
{
	tls_stats_callback = fn;
	tls_stats_ctx = ctx;
}

void wget_ssl_set_stats_callback_ocsp(wget_ocsp_stats_callback *fn, void *ctx)
{
	(void) fn;
	(void) ctx;
}

static void tls_exit(void)
{
	if (mutex)
		wget_thread_mutex_destroy(&mutex);
}

INITIALIZER(tls_init)
{
	if (!mutex) {
		wget_thread_mutex_init(&mutex);
		atexit(tls_exit);
	}
}
