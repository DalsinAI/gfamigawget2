"""HTTP and HTTPS servers for tests/host-wget2.sh, on free loopback ports
(written to the ports file as "http https"); the HTTPS one serves the
test CA's certificate for localhost, TLS 1.2 and 1.3, with session tickets.
  serve.py <site> <cert.pem> <key.pem> <ports file>
MIT, Copyright (c) 2026 AmiCradle Team."""
import functools
import http.server
import ssl
import sys
import threading

site, cert, key, ports = sys.argv[1:5]
Handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=site)
plain = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
tls = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ssl.TLSVersion.TLSv1_2
ctx.load_cert_chain(cert, key)
tls.socket = ctx.wrap_socket(tls.socket, server_side=True)
for s in (plain, tls):
    threading.Thread(target=s.serve_forever, daemon=True).start()
with open(ports, "w") as f:
    f.write("%d %d\n" % (plain.server_address[1], tls.server_address[1]))
threading.Event().wait()
