#!/usr/bin/python

import BaseHTTPServer
import os
import sys
import cooking_pb2

def read_recipes_from_dir(directory):
   recipe_list = []
   for f in os.listdir(directory):
      fname = os.path.join(directory, f)
      fd = open(fname, 'r')
      recipe = cooking_pb2.Recipe()
      recipe.ParseFromString(fd.read())
      recipe_list.append(recipe)
   return recipe_list

def handle_with_html(s, html_text):
   s.send_response(200)
   s.send_header("Content-type", "text/html")
   s.end_headers()
   s.wfile.write(html_text)

def handle_test(s):
   html = '<html><title>/test handler</title>Served by the /test handler</html>'
   handle_with_html(s, html)

def handle_do_stuff(s):
   print 'do_stuff'
   html = '<html><title>/do_stuff</title>Successfully \'did stuff\'</html>'
   handle_with_html(s, html)

class ZnsReqHandler(BaseHTTPServer.BaseHTTPRequestHandler):
   registered_handlers = {
      '/test': handle_test,
      '/do_stuff': handle_do_stuff,
   }

   def do_HEAD(s):
      s.send_response(200)
      s.send_header("Content-type", "text/html")
      s.end_headers()

   def handle_error(s):
      s.send_response(200)
      s.send_header("Content-type", "text/html")
      s.end_headers()
      s.wfile.write("<html><head><title>Error</title></head></html>")
      
   def do_GET(s):
      handler = s.registered_handlers.get(s.path)
      if handler is not None:
         handler(s)
      else:
         s.handle_error()

PORT_NUMBER = 5901

if __name__ == '__main__':
   print 'Starting'
   server_class = BaseHTTPServer.HTTPServer
   httpd = server_class(('', PORT_NUMBER), ZnsReqHandler)
   try:
      httpd.serve_forever()
   except KeyboardInterrupt:
      pass
   httpd.server_close()
   print 'Stopping'
