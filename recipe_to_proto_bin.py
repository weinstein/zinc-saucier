#!/usr/bin/python

import os
import sys
import cooking_pb2

recipe_list = cooking_pb2.RecipeList()
for f in os.listdir(sys.argv[1]):
   if f.endswith(sys.argv[2]):
      fname = os.path.join(sys.argv[1], f)
      fd = open(fname, 'r')
      recipe = recipe_list.entries.add()
      recipe.ParseFromString(fd.read())

      for ingredient in recipe.ingredients:
         name_split = ingredient.name.split(',')
         ingredient.name = name_split[0]
         if len(name_split) == 2:
            ingredient.prep_note = name_split[1].strip()

out_file = open(sys.argv[3], 'w')
out_file.write(recipe_list.SerializeToString())
out_file.close()
