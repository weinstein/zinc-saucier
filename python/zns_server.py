#!/usr/bin/python

import BaseHTTPServer
import os
import sys
import cooking_pb2
import nltk
import gflags
import urlparse as urlp
FLAGS = gflags.FLAGS

gflags.DEFINE_integer('port', '8080', 'port to serve requests on')
gflags.DEFINE_string('recipe_dir', '', 'directory to read recipes from')

ingred_names_to_recipes = {}

def pos_tag(text):
   split = nltk.word_tokenize(text)
   return nltk.pos_tag(split)

def list_from_dir(directory):
   file_list = []
   for f in os.listdir(directory):
      fname = os.path.join(directory, f)
      file_list.append(fname)
   return file_list

def recipe_from_file(fname):
   fd = open(fname, 'r')
   recipe = cooking_pb2.Recipe()
   recipe.ParseFromString(fd.read())
   fd.close()
   return recipe

def map_ingred_names(src_dir):
   name_to_recipes = {}
   n = 0
   for fname in list_from_dir(src_dir):
      if n > 200:
         break
      recipe = recipe_from_file(fname)
      for ingredient in recipe.ingredients:
         tags = pos_tag(ingredient.name)
         for (word, pos) in tags:
            if word.isalpha() and (pos == 'NN' or pos == 'NNS' or pos == 'NNP' or pos == 'NNPS'):
               matching_recipes = name_to_recipes.get(word)
               if matching_recipes is None:
                  name_to_recipes[word] = [recipe]
               else:
                  name_to_recipes[word].append(recipe)
      n += 1
   return name_to_recipes

def read_recipes_from_dir(directory):
   recipe_list = []
   for f in os.listdir(directory):
      fname = os.path.join(directory, f)
      fd = open(fname, 'r')
      recipe = cooking_pb2.Recipe()
      fd.close()
      recipe.ParseFromString(fd.read())
      recipe_list.append(recipe)
   return recipe_list

def handle_with_html(s, html_text):
   s.send_response(200)
   s.send_header("Content-type", "text/html")
   s.end_headers()
   s.wfile.write(html_text)

def handle_recipe_req(s):
   qs = urlp.parse_qs(urlp.urlparse(s.path).query)
   ingred_strs = qs['ingreds']
   print 'ingreds:', ingred_strs
   if ingred_strs is None:
      return
   ingred_set = set()
   for ingred_str in ingred_strs:
      ingred_set.add(ingred_str)
   recipe_matches = set()
   recipe_name_to_obj = {}
   for ingred_str in ingred_strs:
      recipes = ingred_names_to_recipes.get(ingred_str)
      if recipes is None:
         continue
      for recipe in recipes:
         recipe_matches.add(recipe.name)
         recipe_name_to_obj[recipe.name] = recipe
   recipe_scores = {}
   for recipe in recipe_matches:
      recipe_ingred_set = set()
      for ingred in recipe_name_to_obj[recipe].ingredients:
         tags = pos_tag(ingred.name)
         for (word, pos) in tags:
            if word.isalpha() and (pos == 'NN' or pos == 'NNS' or pos == 'NNP' or pos == 'NNPS'):
               recipe_ingred_set.add(word)
      recipe_score = len(recipe_ingred_set.intersection(ingred_set)) / len(recipe_ingred_set) / len(ingred_set)
      recipe_scores[recipe] = recipe_score
   
   html_text = '<html><title>Results</title>'
   n = 0
   for recipe in sorted(recipe_scores, key=recipe_scores.get, reverse=True):
      if n > 25:
         break
      html_text += '<div id=' + str(n) + '>'
      html_text += str(recipe_name_to_obj[recipe])
      html_text += '</div><br>'
      n += 1
   html_text += '</html>'
   handle_with_html(s, html_text)

class ZnsReqHandler(BaseHTTPServer.BaseHTTPRequestHandler):
   registered_handlers = {
      "/recipes": handle_recipe_req,
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
      path = urlp.urlparse(s.path).path
      handler = s.registered_handlers.get(path)
      if handler is not None:
         handler(s)
      else:
         s.handle_error()

if __name__ == '__main__':
   print 'Starting'

   sys.argv = FLAGS(sys.argv)
   
   if not FLAGS.recipe_dir:
      print 'Must specify --recipe_dir'
      exit()
      
   print 'Mapping ingredient names'
   ingred_names_to_recipes = map_ingred_names(FLAGS.recipe_dir)
   
   print 'Starting request handler'
   server_class = BaseHTTPServer.HTTPServer
   httpd = server_class(('', FLAGS.port), ZnsReqHandler)
   try:
      httpd.serve_forever()
   except KeyboardInterrupt:
      pass
   httpd.server_close()
   print 'Stopping'
