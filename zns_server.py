#!/usr/bin/python

# "Oh no! Professor will hit me! But if Zoidberg fixes it... perhaps gifts?"

from __future__ import division

import BaseHTTPServer
import os
import sys
import cooking_pb2
import nltk
import gflags
import urlparse as urlp
import protobuf_json
import json
import math
import pylru
import perceptron
import numpy as np
import time

FLAGS = gflags.FLAGS
gflags.DEFINE_integer('port', '8080', 'port to serve requests on')
gflags.DEFINE_integer('cache_size', '1024', 'keep a cache of the LRU recipes')
gflags.DEFINE_string('recipe_metadata', '', 'file to load metadata proto')
gflags.DEFINE_string('recipe_dir', '', 'dir containing recipe files <id>.bin')
gflags.DEFINE_string('ingredient_vocab', '', 'file to load ingred vocab from')
gflags.DEFINE_string('category_vocab', '', 'file to load category vocab from')

metadata = cooking_pb2.CookbookMetadata()
ingredient_vocab = {}
category_vocab = {}
recipe_cache = None

bender_png = None
inventory_html = None
recipe_info_html = None
last_read_time = {}

def try_parse_int(string):
   try:
      return int(string)
   except ValueError:
      return -1

def pos_tag(text):
   split = nltk.word_tokenize(text)
   return nltk.pos_tag(split)

def read_recipe(fname):
   fd = open(fname, 'r')
   if not fd:
      return None
   recipe = cooking_pb2.Recipe()
   recipe.ParseFromString(fd.read())
   fd.close()
   return recipe

def read_recipe_with_id(iden):
   if recipe_cache is not None and (iden in recipe_cache):
      recipe = recipe_cache[iden]
      return recipe
   else:
      recipe = read_recipe(os.path.join(FLAGS.recipe_dir, str(iden) + '.bin'))
      recipe_cache[iden] = recipe
      return recipe

def extract_ingred_names(name):
   result = []
   tags = pos_tag(name)
   for (word, pos) in tags:
      if word.isalpha() and (pos == 'NN' or pos == 'NNS' or pos == 'NNP' or pos == 'NNPS'):
         result.append(word)
   if not result:
      for (word, pos) in tags:
         if word.isalpha() and (pos == 'JJ' or pos == 'VBZ' or pos == 'PRP$'):
            result.append(word)
   if not result:
      if name:
         result.append(name)
   return result

def read_metadata(fname):
   fd = open(fname, 'r')
   if not fd:
      return None
   meta = cooking_pb2.CookbookMetadata()
   meta.ParseFromString(fd.read())
   fd.close()
   return meta

def read_vocab(fname):
   vocab = set()
   with open(fname, 'r') as f:
      for line in f.readlines():
         vocab.add(line.strip().lower())
   return vocab

def read_file(fname):
   fd = open(fname, 'r')
   data = fd.read()
   fd.close()
   return data

def handle_with_html(s, html_text):
   s.send_response(200)
   s.send_header("Content-type", "text/html")
   s.end_headers()
   s.wfile.write(html_text)

def handle_with_png(s, png_data):
   s.send_response(200)
   s.send_header("Content-type", "image/png")
   s.end_headers()
   s.wfile.write(png_data)

def handle_with_json(s, json_text):
   s.send_response(200)
   s.send_header("Content-type", "application/json")
   s.end_headers()
   s.wfile.write(json_text)

#def handle_prod_info(s):
#   qs = urlp.parse_qs(urlp.urlparse(s.path).query)
#   gtins = qs.get('gtin')
#   if gtins is None or not gtins:
#      handle_with_json(s, '[]')
#      return
#   dict_ret = []
#   for gtin in gtins:
#      gtin = try_parse_int(gtin)
#      print 'gtin:',gtin
#      product_info = lookup_gtin(gtin)
#      if product_info is not None:
#         print 'found:',product_info.name
#         dict_ret.append(protobuf_json.pb2json(product_info))
#      else:
#         print 'not found'
#   handle_with_json(s, json.dumps(dict_ret, indent=4))

def handle_recipe_info_json(s):
   qs = urlp.parse_qs(urlp.urlparse(s.path).query)
   recipe_id = qs.get('id')
   if recipe_id is None or not recipe_id:
      handle_with_json(s, '{}')
      return
   recipe_id = try_parse_int(recipe_id[0])
   if recipe_id >= 0 and recipe_id < len(metadata.recipes):
      recipe = read_recipe_with_id(recipe_id)
      pb_dict = protobuf_json.pb2json(recipe)
      handle_with_json(s, json.dumps(pb_dict, indent=4))
   else:
      handle_with_json(s, '{}')

def read_file_if_modified(fname, default=None):
   last_read = last_read_time.get(fname)
   if not last_read or os.path.getmtime(fname) >= last_read:
      print 'rereading stale file', fname
      last_read_time[fname] = time.time()
      return read_file(fname)
   else:
      return default

def handle_recipe_info(s):
   global recipe_info_html
   recipe_info_html = read_file_if_modified('html/recipe_info.html', default=recipe_info_html)
   handle_with_html(s, recipe_info_html)

def handle_bender_png(s):
   global bender_png
   bender_png = read_file_if_modified('img/bender.png', default=bender_png)
   handle_with_png(s, bender_png)

def handle_inventory(s):
   global inventory_html
   inventory_html = read_file_if_modified('html/inventory.html', default=inventory_html)
   handle_with_html(s, inventory_html)

def ingred_name_to_feature(name):
   ret = ingredient_vocab.get(name)
   if ret is None:
      return None
   else:
      return ret + len(category_vocab)

def cat_name_to_feature(name):
   return category_vocab.get(name)

def num_features():
   return len(category_vocab) + len(ingredient_vocab) + 1

def get_feature_vec(recipe):
   features = np.zeros(num_features())
   for ingredient in recipe.ingredient_vocab:
      feature_num = ingred_name_to_feature(ingredient.strip().lower())
      if feature_num is not None:
         features[feature_num] = 1
   for category in recipe.category_vocab:
      feature_num = cat_name_to_feature(category.strip().lower())
      if feature_num is not None:
         features[feature_num] = 1
      else:
         print 'category', category, 'not found'
   features[num_features()-1] = 1
   return features

def handle_serendipity(s):
   qs = urlp.parse_qs(urlp.urlparse(s.path).query)
   likes = qs.get('like')
   dislikes = qs.get('dislike')
   max_count = qs.get('count')
   print 'qs:',qs
   if likes is None or not likes:
      if dislikes is None or not dislikes:
         handle_with_json(s, '[]')
         return
   if max_count is None:
      max_count = 5
   else:
      max_count = try_parse_int(max_count[0])
   if max_count < 0 or max_count > 128:
      max_count = 128
   print 'max_count', max_count

   likes_set = set()
   if likes:
      likes_set = set([try_parse_int(rid) for rid in likes])
   dislikes_set = set()
   if dislikes:
      dislikes_set = set([try_parse_int(rid) for rid in dislikes])

   samples = [get_feature_vec(metadata.recipes[rid]) for rid in likes_set] + [get_feature_vec(metadata.recipes[rid]) for rid in dislikes_set]
   labels = [True] * len(likes_set) + [False] * len(dislikes_set)
   n = len(samples)
   max_epochs = n
   tol = 0.01
   clf = perceptron.Perceptron(num_features(), 1)
   inds = range(0, n)
   for epoch in range(0, max_epochs):
      correct = 0
      np.random.shuffle(inds)
      for ind in inds:
         if clf.give_training_data(samples[ind], labels[ind]):
            correct += 1
      acc = correct / n
      print 'epoch', epoch
      print '  acc', acc
      clf.epoch_tick()
      if 1 - acc < tol:
         break

   recipe_scores = {}
   for recipe in metadata.recipes:
      recipe_score = clf.get_activation(get_feature_vec(recipe))
      recipe_scores[recipe.iden] = recipe_score

   dicts_ret = []
   count = 1
   for recipe_id in sorted(recipe_scores, key=recipe_scores.get, reverse=True):
      if count > max_count:
         break
      if recipe_id in likes_set or recipe_id in dislikes_set:
         continue
      recipe = read_recipe_with_id(recipe_id)
      print 'Recipe', recipe.name
      print '  score', recipe_scores[recipe_id]
      dicts_ret.append(protobuf_json.pb2json(recipe))
      count += 1
   handle_with_json(s, json.dumps(dicts_ret, indent=4))

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
      ingred_names = extract_ingred_names(ingred_str)
      ingred_set = ingred_set.union(set([w.lower() for w in ingred_names if w]))

   if ingred_set is None or not ingred_set:
      handle_with_json(s, '[]')
      return

   recipe_scores = {}
   for recipe in metadata.recipes:
      recipe_ingred_set = set([w.lower() for w in recipe.ingredient_vocab if w])
      if recipe_ingred_set:
         recipe_score = len(recipe_ingred_set.intersection(ingred_set))**2 / len(recipe_ingred_set)
         recipe_scores[recipe.iden] = recipe_score
   
   dicts_ret = []
   n = 1
   for recipe_id in sorted(recipe_scores, key=recipe_scores.get, reverse=True):
      if n > count:
         break
      recipe = read_recipe_with_id(recipe_id)
      print 'recipe', recipe.name, '\n  score', recipe_scores[recipe_id]
      pb_dict = protobuf_json.pb2json(recipe)
      dicts_ret.append(pb_dict)
      n += 1
   handle_with_json(s, json.dumps(dicts_ret, indent=4))

class ZnsReqHandler(BaseHTTPServer.BaseHTTPRequestHandler):
   registered_handlers = {
      "/recipes.json": handle_recipe_req,
#      "/product_info.json": handle_prod_info,
      "/recipe_info.json": handle_recipe_info_json,
      "/": handle_inventory,
      "/recipe_info": handle_recipe_info,
      "/bender.png": handle_bender_png,
      "/serendipity.json": handle_serendipity,
   }

   def do_HEAD(s):
      s.send_response(200)
      s.send_header("Content-type", "text/html")
      s.end_headers()

   def handle_error(s):
      s.send_response(404)
      s.send_header("Content-type", "text/html")
      s.end_headers()
      s.wfile.write("error")
      
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
   if not FLAGS.recipe_metadata:
      print 'Must specify --recipe_metadata'
      exit()
   if not FLAGS.ingredient_vocab:
      print 'Must specify --ingredient_vocab'
      exit()
   if not FLAGS.category_vocab:
      print 'Must specify --category_vocab'
      exit()

   print 'Reading recipe metadata'
   metadata = read_metadata(FLAGS.recipe_metadata)
   print 'Reading vocabularies'
   for n, w in enumerate(list(read_vocab(FLAGS.ingredient_vocab))):
      ingredient_vocab[w] = n
   for n, w in enumerate(list(read_vocab(FLAGS.category_vocab))):
      category_vocab[w] = n

   recipe_cache = pylru.lrucache(FLAGS.cache_size)

   last_read_time = {}

   print 'Starting request handler'
   server_class = BaseHTTPServer.HTTPServer
   httpd = server_class(('0.0.0.0', FLAGS.port), ZnsReqHandler)
   try:
      httpd.serve_forever()
   except KeyboardInterrupt:
      pass
   httpd.server_close()
   print 'Stopping'
