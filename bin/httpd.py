#!/usr/bin/env python

import SimpleHTTPServer
import SocketServer

Handler = SimpleHTTPServer.SimpleHTTPRequestHandler

PORT = 5103
httpd = SocketServer.TCPServer(("",PORT),Handler)
print "serving at port",PORT
try:
    httpd.serve_forever()
except KeyboardInterrupt:
    httpd.shutdown()

