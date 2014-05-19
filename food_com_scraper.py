#!/usr/bin/python

import sys
import urllib2
from BeautifulSoup import BeautifulSoup as bs, NavigableString
from fractions import Fraction

import cooking_pb2

def Cleanup(string):
   cleaner = string.replace('\t','').replace('\n','').replace('\r','')
   soup = bs(cleaner)
   cleaned = ''
   for tag in soup.contents:
      if isinstance(tag, NavigableString):
         cleaned = cleaned + tag
      else:
         cleaned = cleaned + Cleanup(tag.renderContents())
   return cleaned

def ParseTimeString(time_str):
   # looks like PT20M for 20 minutes, etc
   # PT8H for 8 hours = 8*60 minutes
   formatted = time_str.replace('PT', '')
   Hind = formatted.rfind('H')
   Mind = formatted.rfind('M')
   if Hind > 0:
      return int(formatted[:Hind]) * 60
   elif Mind > 0:
      return int(formatted[:Mind])
   else:
      return -1

def ParseAmountString(amount_str):
   amount_range = amount_str.split('-')
   if len(amount_range) == 1:
      low = high = float(ParseFractionString(amount_range[0].strip()))
   else:
      low = float(ParseFractionString(amount_range[0].strip()))
      high = float(ParseFractionString(amount_range[1].strip()))
   return [low, high]

def ParseFractionString(amount_str):
   total = 0
   for s in amount_str.split(' '):
      if s == '':
         continue
      else:
         total = total + Fraction(s)
   return float(total)

def scrapeRecipeUrl(url):
   doc = urllib2.urlopen(url).read()
   return scrapeRecipeString(doc)

def scrapeRecipeString(doc):
   recipe = cooking_pb2.Recipe()
   soup = bs(doc)

   title_text = soup.find(attrs={'itemprop': 'name'}).renderContents()
   recipe.name = title_text

   prep_time = soup.find('meta', attrs={'itemprop': 'prepTime'})
   if prep_time != None:
      recipe.prep_time_min = ParseTimeString(prep_time['content'])

   cook_time = soup.find('meta', attrs={'itemprop': 'cookTime'})
   if cook_time != None:
      recipe.cook_time_min = ParseTimeString(cook_time['content'])

   serving_div = soup.find('div', attrs={'class': 'part'})
   if serving_div != None:
      serving_amt = serving_div.find('input', attrs={'id': 'original_value'})
      if serving_amt != None and serving_amt['value'].strip() != '':
         serving_amt_range = ParseAmountString(serving_amt['value'].strip())
         recipe.serves_low = float(serving_amt_range[0])
         recipe.serves_high = float(serving_amt_range[1])
      serving_kind = serving_div.find('p', attrs={'class': 'yieldUnits-txt'})
      if serving_kind != None and serving_kind['title'].strip() != '':
         recipe.serves_unit = Cleanup(serving_kind['title'].strip())

   for category in soup.findAll('span', attrs={'itemprop': 'recipeCategory'}):
      recipe.categories.append(Cleanup(category.renderContents().strip()))

   for ingredient in soup.findAll('li', attrs={'itemprop': 'ingredients'}):
      ingredient_pb = recipe.ingredients.add()

      name = ingredient.find('span', attrs={'class': 'name'})
      if name != None:
         name_split = Cleanup(name.renderContents().strip()).split(',')
         ingredient_pb.name = name_split[0].strip()
         if len(name_split) == 2:
            ingredient_pb.prep_note = name_split[1].strip()

      amount_unit = ingredient.find('span', attrs={'class': 'amount'})
      if amount_unit != None:
         unit = amount_unit.find('span', attrs={'class': 'type'})
         if unit != None:
            ingredient_pb.unit = Cleanup(unit.renderContents().strip())

         amount = amount_unit.find('span', attrs={'class': 'value'})
         if amount != None and amount != '':
            amount_range = ParseAmountString(amount.renderContents().strip())
            ingredient_pb.amount_low = float(amount_range[0])
            ingredient_pb.amount_high = float(amount_range[1])

   instructions_div = soup.find('span', attrs={'class': 'instructions'})
   if instructions_div != None:
      for instruction in instructions_div.findAll('div', attrs={'class': 'txt'}):
         if instruction.string != None and instruction.string != '':
            recipe.instructions.append(Cleanup(instruction.string))

   return recipe

# ====== MAIN ======

f = open(sys.argv[1], 'r')
formuoli = scrapeRecipeString(f.read())
#print formuoli.SerializeToString()
out_file = open(sys.argv[2], 'w')
out_file.write(formuoli.SerializeToString())
out_file.close()
