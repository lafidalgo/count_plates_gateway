# Python 3 server example
import http.server
import socketserver

hostName = "localhost"
serverPort = 8080        

Handler = http.server.SimpleHTTPRequestHandler

with socketserver.TCPServer((hostName, serverPort), Handler) as httpd:
    print("serving at http://localhost:8080")
    httpd.serve_forever()