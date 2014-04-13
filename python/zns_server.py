#!/usr/bin/python

from __future__ import division

import BaseHTTPServer
import os
import sys
import cooking_pb2
import gs1_pb2
import nltk
import gflags
import urlparse as urlp
import protobuf_json
import json
import math

FLAGS = gflags.FLAGS
gflags.DEFINE_integer('port', '8080', 'port to serve requests on')
gflags.DEFINE_integer('max_recipes', '1000000', 'maximum recipes to keep in mem')
gflags.DEFINE_string('recipe_file', '', 'file to read recipes from')
gflags.DEFINE_string('product_file', '', 'file to read prods from')

all_recipes_list = cooking_pb2.RecipeList()
recipe_ingred_sets = {}
recipe_names_to_obj = {}
all_prods_by_gtin = {}

def try_parse_int(string):
   try:
      return int(string)
   except ValueError:
      return -1

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

def extract_ingred_name(name):
   result = []
   tags = pos_tag(name)
   for (word, pos) in tags:
      if word.isalpha() and (pos == 'NN' or pos == 'NNS' or pos == 'NNP' or pos == 'NNPS'):
         result.append(word)
   if not result:
      for (word, pos) in tags:
         if word.isalpha() and (pos == 'JJ' or pos == 'VBZ' or pos == 'PRP$'):
            result.append(word)
   return result

def map_ingred_names(recipe_list):
   recipe_ingred_sets = {}
   name_to_obj = {}
   n = 0
   for recipe in recipe_list.entries:
      n += 1
      if n > FLAGS.max_recipes:
         break
      print 'n=',n,' recipe=',recipe.name
      ingred_set = set()
      for ingredient in recipe.ingredients:
         ingred_names = extract_ingred_name(ingredient.name)
         if ingred_names is None or not ingred_names:
            continue
         for ingred_name in ingred_names:
            if ingred_name is not None:
               ingred_set.add(ingred_name)
      if len(ingred_set) > 0:
         recipe_ingred_sets[recipe.name] = ingred_set
         name_to_obj[recipe.name] = recipe
   return recipe_ingred_sets, name_to_obj

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

def read_recipes_from_file(fname):
   fd = open(fname, 'r')
   recipe_list = cooking_pb2.RecipeList()
   recipe_list.ParseFromString(fd.read())
   fd.close()
   return recipe_list

def read_prods_from_file(fname):
   fd = open(fname, 'r')
   prod_list = gs1_pb2.GS1ProductList()
   prod_list.ParseFromString(fd.read())
   fd.close()
   return prod_list

def map_prod_gtin(prod_list):
   prods_by_gtin = {}
   for prod in prod_list.entries:
      if prod.global_trade_id is None:
         continue
      prods_by_gtin[int(prod.global_trade_id)] = prod
   return prods_by_gtin

def lookup_gtin(gtin):
   product_info = all_prods_by_gtin.get(gtin)
   return product_info

def handle_with_html(s, html_text):
   s.send_response(200)
   s.send_header("Content-type", "text/html")
   s.end_headers()
   s.wfile.write(html_text)

def handle_with_json(s, json_text):
   s.send_response(200)
   s.send_header("Content-type", "application/json")
   s.end_headers()
   s.wfile.write(json_text)

def handle_prod_info(s):
   qs = urlp.parse_qs(urlp.urlparse(s.path).query)
   gtins = qs.get('gtin')
   if gtins is None or not gtins:
      handle_with_json(s, '[]')
      return
   dict_ret = []
   for gtin in gtins:
      gtin = try_parse_int(gtin)
      print 'gtin:',gtin
      product_info = lookup_gtin(gtin)
      if product_info is not None:
         print 'found:',product_info.name
         dict_ret.append(protobuf_json.pb2json(product_info))
      else:
         print 'not found'
   handle_with_json(s, json.dumps(dict_ret, indent=4))

def handle_inventory(s):
   inv_fd = open('html/inventory.html', 'r')
   inventory_html = inv_fd.read()
   inv_fd.close()
   handle_with_html(s, inventory_html)

def handle_recipe_req(s):
   qs = urlp.parse_qs(urlp.urlparse(s.path).query)
   ingred_strs = qs.get('ingred')
   count = qs.get('count')
   print 'qs:',qs

   if ingred_strs is None:
      handle_with_json(s, '[]')
      return
   if count is None:
      count = 5
   else:
      count = try_parse_int(count[0])
   if count < 0 or count > 128:
      count = 128
   print 'ingreds:', ingred_strs
   print 'count:', count

   ingred_set = set()
   for ingred_str in ingred_strs:
      ingred_names = extract_ingred_name(ingred_str)
      if ingred_names is None or not ingred_names:
         continue
      for ingred_name in ingred_names:
         if ingred_name is not None:
            ingred_set.add(ingred_name)
   if ingred_set is None:
      handle_with_json(s, '[]')
      return

   recipe_scores = {}
   for recipe in all_recipes_list.entries:
      recipe_ingred_set = recipe_ingred_sets.get(recipe.name)
      if recipe_ingred_set is None:
         continue
      recipe_score = len(recipe_ingred_set.intersection(ingred_set)) / len(recipe_ingred_set.union(ingred_set))
      recipe_scores[recipe.name] = recipe_score
   
   dicts_ret = []
   n = 0
   for recipe in sorted(recipe_scores, key=recipe_scores.get, reverse=True):
      if n > count:
         break
      print 'recipe', recipe, '\n  score', recipe_scores[recipe]
      dicts_ret.append(protobuf_json.pb2json(recipe_names_to_obj[recipe]))
      n += 1
   handle_with_json(s, json.dumps(dicts_ret, indent=4))

class ZnsReqHandler(BaseHTTPServer.BaseHTTPRequestHandler):
   registered_handlers = {
      "/recipes.json": handle_recipe_req,
      "/product_info.json": handle_prod_info,
      "/inventory": handle_inventory
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
   
   if not FLAGS.recipe_file:
      print 'Must specify --recipe_file'
      exit()
   if not FLAGS.product_file:
      print 'Must specify --product_file'
      exit()
      
   print 'Reading GS1 prods'
   all_prods = read_prods_from_file(FLAGS.product_file)
   print 'Mapping prods'
   all_prods_by_gtin = map_prod_gtin(all_prods)

   print 'Reading recipes'
   all_recipes_list = read_recipes_from_file(FLAGS.recipe_file)
   print 'Mapping ingred names'
   recipe_ingred_sets, recipe_names_to_obj = map_ingred_names(all_recipes_list)
   
   print 'Starting request handler'

   server_class = BaseHTTPServer.HTTPServer
   httpd = server_class(('', FLAGS.port), ZnsReqHandler)
   try:
      httpd.serve_forever()
   except KeyboardInterrupt:
      pass
   httpd.server_close()
   print 'Stopping'
