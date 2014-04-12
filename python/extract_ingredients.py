#!/usr/bin/python

import os
import sys
import cooking_pb2
import nltk

def pos_tag(text):
   split = nltk.word_tokenize(text)
   return nltk.pos_tag(split)

def list_from_dir(directory):
   file_list = []
   for f in os.listdir(directory):
      fname = os.path.join(directory, f)
      file_list.append(fname)
   return file_list

def pos_tag_recipe(recipe):
   tagged_ingreds = []
   for ingredient in recipe.ingredients:
      tagged_ingreds.append(pos_tag(ingredient))
   return tagged_ingreds

ingredient_names = {}
recipe_dir = sys.argv[1]
n = 0
for fname in list_from_dir(recipe_dir):
   n += 1
   fd = open(fname, 'r')
   recipe = cooking_pb2.Recipe()
   recipe.ParseFromString(fd.read())
   fd.close()
#   print recipe.name
   for ingred in recipe.ingredients:
      tags = pos_tag(ingred.name)
      for (word, pos) in tags:
         if word.isalpha() and (pos == 'NN' or pos == 'NNS' or pos == 'NNP' or pos == 'NNPS'):
            val = ingredient_names.get(word)
            if val is None:
               ingredient_names[word] = 1
            else:
               ingredient_names[word] = val + 1

for key in sorted(ingredient_names, key=ingredient_names.get):
   print key.encode('utf-8'), ingredient_names[key]
