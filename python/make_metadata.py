#!/usr/bin/python

# "Oh no! Professor will hit me! But if Zoidberg fixes it... perhaps gifts?"

from __future__ import division

import os
import sys
import cooking_pb2
import nltk
import gflags

FLAGS = gflags.FLAGS
gflags.DEFINE_string('recipe_file', '', 'file to read recipes from')
gflags.DEFINE_string('output_dir', '', 'output data and metadata here')

def try_parse_int(string):
   try:
      return int(string)
   except ValueError:
      return -1

def pos_tag(text):
   split = nltk.word_tokenize(text)
   return nltk.pos_tag(split)

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

def read_recipes_from_file(fname):
   fd = open(fname, 'r')
   recipe_list = cooking_pb2.RecipeList()
   recipe_list.ParseFromString(fd.read())
   fd.close()
   return recipe_list

def set_recipe_ids(recipe_list):
   n = 0
   for recipe in recipe_list.entries:
      recipe.iden = n
      n += 1

def write_recipes_by_id(recipe_list, output_dir):
   recipe_dir = os.path.join(output_dir, 'recipe')
   if not os.path.exists(recipe_dir):
      os.makedirs(recipe_dir)

   for recipe in recipe_list.entries:
      print recipe.name

      fname = str(recipe.iden) + '.bin'
      fd = open(os.path.join(recipe_dir, fname), 'w')
      fd.write(recipe.SerializeToString())
      fd.close()

def get_recipe_metadata(recipe_list):
   metadata = cooking_pb2.CookbookMetadata()
   all_ingreds = set()
   all_cats = set()
   for recipe in recipe_list.entries:
      print recipe.name
      recipe_metadata = metadata.recipes.add()
      recipe_metadata.iden = recipe.iden

      for category in [w.lower() for w in recipe.categories if w]:
         recipe_metadata.category_vocab.append(category)
         all_cats.add(category)

      ingred_vocab = set()      
      for ingredient in recipe.ingredients:
         ingred_names = extract_ingred_names(ingredient.name)
         for ingred_name in [w.lower() for w in ingred_names if w]:
            ingred_vocab.add(ingred_name)
            all_ingreds.add(ingred_name)
      for ingred_name in ingred_vocab:
         recipe_metadata.ingredient_vocab.append(ingred_name)

   return metadata, all_ingreds, all_cats

if __name__ == '__main__':
   print 'Starting'

   sys.argv = FLAGS(sys.argv)
   
   if not FLAGS.recipe_file:
      print 'Must specify --recipe_file'
      exit()
      
   print 'Reading recipes'
   all_recipes_list = read_recipes_from_file(FLAGS.recipe_file)

   print 'Setting recipe ids'
   set_recipe_ids(all_recipes_list)

   print 'Writing updated recipe list proto'
   fd = open(os.path.join(FLAGS.output_dir, 'all_recipes.updated.bin'), 'w')
   fd.write(all_recipes_list.SerializeToString())
   fd.close()

   print 'Writing updated recipe protos individually'
   write_recipes_by_id(all_recipes_list, FLAGS.output_dir)

   print 'Collecting recipe metadata'
   recipe_metadata, all_ingredients, all_categories = get_recipe_metadata(all_recipes_list)
   print 'Writing recipe metadata to disk'
   fd = open(os.path.join(FLAGS.output_dir, 'metadata_vocab.bin'), 'w')
   fd.write(recipe_metadata.SerializeToString())
   fd.close()

   print 'Writing vocab to disk'
   fd = open(os.path.join(FLAGS.output_dir, 'ingredients.txt'), 'w')
   for ingred in all_ingredients:
      fd.write(ingred.encode('utf-8') + '\n')
   fd.close()
   fd = open(os.path.join(FLAGS.output_dir, 'categories.txt'), 'w')
   for category in all_categories:
      fd.write(category.encode('utf-8') + '\n')
   fd.close()

   print 'Done'
   
#   print 'Starting request handler'

#   server_class = BaseHTTPServer.HTTPServer
#   httpd = server_class(('0.0.0.0', FLAGS.port), ZnsReqHandler)
#   try:
#      httpd.serve_forever()
#   except KeyboardInterrupt:
#      pass
#   httpd.server_close()
