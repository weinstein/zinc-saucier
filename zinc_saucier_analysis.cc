#include "cooking.pb.h"
#include "file_utils.h"
#include "gs1.pb.h"
#include "gs1_utils.h"
#include "string_utils.h"

#include <algorithm>
#include <fcntl.h>
#include <gflags/gflags.h>
#include <google/protobuf/message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <iostream>
#include <memory>
#include <simstring/simstring.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using std::cerr;
using std::cin;
using std::cout;
using std::make_pair;
using std::pair;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

using google::protobuf::Message;
using google::protobuf::io::CodedInputStream;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::ZeroCopyInputStream;

DEFINE_string(simstring_dir, "/tmp", "Store simstring db files "
                                     "in this directory");
DEFINE_string(product_db_name, "simstring_product", "File prefix "
                                                    "for product simstring db");
DEFINE_string(ingredient_db_name, "simstring_ingred", "File prefix "
                                                      "for ingred db");
DEFINE_bool(create_simstring_db, true, "If true, create simstring db again. "
                                       "If false, use existing db files.");
// Required!
DEFINE_string(product_in, "", "Load products list from this file");
DEFINE_string(product_in_extra, "", "Load additional products from this file");
// Required!
DEFINE_string(recipe_in, "", "Load recipe list from this file");
DEFINE_string(recipe_in_extra, "", "Load additional recipe list "
                                   "from this file");

vector<string> GetSimstrings(const string& query, int measure, double threshold,
                            simstring::reader* dbr) {
   vector<string> res;
   dbr->retrieve(query, measure, threshold, std::back_inserter(res));
   return res;
}

bool LoadMessageFromFD(int fd, Message* protobuf) {
   struct stat infile_stat;
   if (fstat(fd, &infile_stat) < 0) {
      cerr << "Error stat-ing input file\n";
      return false;
   }
   size_t fsize = infile_stat.st_size;

   unique_ptr<ZeroCopyInputStream> zc_stream(new FileInputStream(fd));
   CodedInputStream in_stream(zc_stream.get());
   in_stream.SetTotalBytesLimit(fsize, fsize);
   if (!protobuf->ParseFromCodedStream(&in_stream)) {
      cerr << "Error parsing serialized recipes\n";
      return false;
   }
   return true;
}

void WriteSimstringDB(const string& fname, const unordered_set<string>& keys) {
   simstring::ngram_generator gen(3, false);
   simstring::writer_base<string> dbw(gen, fname);
   for (const string& prod_name : keys) {
      dbw.insert(prod_name);
   }
   dbw.close();
}

typedef pair<const zns::Recipe*, int> recipe_count;
bool compareBySecond(const recipe_count& a, const recipe_count& b) {
   return a.second > b.second;
}

int main(int argc, char** argv) {
   google::ParseCommandLineFlags(&argc, &argv, true);

   if (FLAGS_recipe_in.empty() || FLAGS_product_in.empty()) {
      cerr << "Must supply recipe and product input files. "
            << "Try --help for info\n";
      return 1;
   }

   int fd = open(FLAGS_recipe_in.c_str(), O_RDONLY);
   if (fd < 0) {
      cerr << "Error opening recipe_in\n";
      return 1;
   }
   cerr << "Loading recipes...\n";
   zns::RecipeList recipe_list;
   if (!LoadMessageFromFD(fd, &recipe_list)) {
      return 1;
   }
   cerr << "Loaded " << recipe_list.entries_size() << " recipes\n";
   close(fd);

   if (!FLAGS_recipe_in_extra.empty()) {
      fd = open(FLAGS_recipe_in_extra.c_str(), O_RDONLY);
      if (fd < 0) {
         cerr << "Error opening recipe_in_extra\n";
      } else {
         cerr << "Loading extra recipes...\n";
         zns::RecipeList extra_recipes;
         if (LoadMessageFromFD(fd, &extra_recipes)) {
            cerr << "Loaded " << extra_recipes.entries_size() << " extra recipes\n";
            recipe_list.MergeFrom(extra_recipes);
         }
      }
   }

   cerr << "Mapping recipes and ingredients...\n";
   unordered_map<string, unordered_set<const zns::Recipe*>> ingredient_to_recipes;
   unordered_map<string, const zns::Ingredient*> name_to_ingredient;
   unordered_set<string> all_ingredients;
   for (const zns::Recipe& recipe : recipe_list.entries()) {
      for (const zns::Ingredient& ingredient : recipe.ingredients()) {
         if (ingredient.name().empty()) {
            cerr << "Invalid ingredient: missing name, "
                  "from recipe: " << recipe.name() << "\n";
            continue;
         }
         string ingred_name = util::StringToLower(ingredient.name());
         name_to_ingredient.insert({ingred_name, &ingredient});
         ingredient_to_recipes[ingred_name].insert(&recipe);
         if (FLAGS_create_simstring_db) {
            all_ingredients.insert(ingred_name);
         }
      }
   }
   cerr << "Mapped " << all_ingredients.size() << " ingredient names.\n";

   const string ingred_db_name =
         FLAGS_simstring_dir.append("/").append(FLAGS_ingredient_db_name);
   if (FLAGS_create_simstring_db) {
      cerr << "Creating ingredient simstring db...\n";
      WriteSimstringDB(ingred_db_name, all_ingredients);
      all_ingredients.clear();
   }

   cerr << "Opening ingredient simstring db...\n";
   simstring::reader ingred_dbr;
   ingred_dbr.open(ingred_db_name);

   fd = open(FLAGS_product_in.c_str(), O_RDONLY);
   if (fd < 0) {
      cerr << "Error opening product_in\n";
      return 1;
   }
   cerr << "Loading products...\n";
   zns::GS1ProductList product_list;
   if (!LoadMessageFromFD(fd, &product_list)) {
      return 1;
   }
   cerr << "Loaded " << product_list.entries_size() << " products\n";
   if (!FLAGS_product_in_extra.empty()) {
      fd = open(FLAGS_product_in_extra.c_str(), O_RDONLY);
      if (fd < 0) {
         cerr << "Error opening product_in_extra\n";
      } else {
         cerr << "Loading extra products...\n";
         zns::GS1ProductList extra_products;
         if (LoadMessageFromFD(fd, &extra_products)) {
            cerr << "Loaded " << extra_products.entries_size() << " extra products\n";
            product_list.MergeFrom(extra_products);
         }
      }
   }

   cerr << "Mapping products...\n";
   unordered_map<long int, const zns::GS1Product*> gtin_to_product;
   unordered_map<string, const zns::GS1Product*> name_to_product;
   unordered_set<string> all_products;
   for (const zns::GS1Product& product : product_list.entries()) {
      if (!product.has_global_trade_id() || product.global_trade_id() < 0) {
         cerr << "Invalid product entry: missing or invalid gtin\n"
               << product.DebugString();
         continue;
      }
      if (!product.has_name() || product.name().empty()) {
         cerr << "Invalid product entry: missing name\n"
               << product.DebugString();
         continue;
      }
      string prod_name = util::StringToLower(product.name());
      gtin_to_product.insert({product.global_trade_id(), &product});
      name_to_product.insert({prod_name, &product});
      if (FLAGS_create_simstring_db) {
         all_products.insert({prod_name});
      }
   }

   const string product_db_name =
         FLAGS_simstring_dir.append("/").append(FLAGS_product_db_name);
   if (FLAGS_create_simstring_db) {
      cerr << "Creating product simstring db...\n";
      WriteSimstringDB(product_db_name, all_products);
      all_products.clear();
   }

   cerr << "Opening product simstring db...\n";
   simstring::reader product_dbr;
   product_dbr.open(product_db_name);

   double threshold = 0.6;
   vector<unordered_set<const zns::Ingredient*>> user_inventory;
   while (true) {
      string input_line;   
      cerr << ":";
      std::getline(cin, input_line);
      string query_str;
      if (input_line == ".exit" || input_line == ".quit" || input_line == ".q") {
         break;
      } else if (input_line == ".threshold") {
         cerr << "threshold=" << threshold << "\nenter new threshold:";
         std::getline(cin, input_line);
         if (!util::StringToDouble(input_line, &threshold)) {
            cerr << "invalid threshold value.\n";
         }
         continue;
      } else if (input_line == ".add") {
         while (true) {
            getline(cin, input_line);
            if (input_line == ".done") {
               break;   
            }
            if (std::all_of(input_line.begin(), input_line.end(), ::isdigit)) {
               long int gtin;
               if (!util::StringToLong(input_line, &gtin)) {
                  cerr << "Not a valid gtin: \"" << input_line << "\"\n";
                  continue;
               }
               unordered_map<long int, const zns::GS1Product*>::const_iterator iter =
                     gtin_to_product.find(gtin);
               if (iter == gtin_to_product.end()) {
                  cout << "gtin " << input_line << " not found\n";
                  continue;
               }
               const zns::GS1Product* prod = iter->second;
               cout << "Product \"" << prod->name() << "\"\n";
               if (prod->has_brand() && !prod->brand().name().empty()) {
                  cout << "Brand \"" << prod->brand().name() << "\"\n";
               }
               query_str = prod->name();
            } else {
               query_str = input_line;
            }
      
            vector<string> matching_ingred_names = GetSimstrings(
               util::StringToLower(query_str), simstring::cosine, threshold, &ingred_dbr);
            if (matching_ingred_names.empty()) {
               cout << "No matching ingredients.\n";
               continue;
            }
            unordered_set<const zns::Ingredient*> matching_ingreds;
            for (const string& ingred_name : matching_ingred_names) {
               unordered_map<string, const zns::Ingredient*>::const_iterator iter =
                     name_to_ingredient.find(ingred_name);
               if (iter == name_to_ingredient.end()) {
                  cout << "Ingredient name not mapped. This shouldn't happen.\n";
                  continue;
               }
               matching_ingreds.insert(iter->second);
            }
            if (!matching_ingreds.empty()) {
               user_inventory.push_back(matching_ingreds);
            } else {
               cerr << "No matching ingredients.\n";
            }
         }
      } else if (input_line == ".size") {
         int total_size = 0;
         for (const auto& set : user_inventory) {
            total_size += set.size();
         }
         cout << user_inventory.size() << " entries, totalling " << total_size << "\n";
      } else if (input_line == ".clear") {
         user_inventory.clear();
      } else if (input_line == ".suggest") {
         unordered_set<const zns::Recipe*> matching_recipes;
         for (const unordered_set<const zns::Ingredient*>& ingred_set : user_inventory) {
            for (const zns::Ingredient* ingred : ingred_set) {
               if (ingred->name().empty()) {
                  continue;
               }
               unordered_map<string, unordered_set<const zns::Recipe*>>::const_iterator
                  recipe_iter = ingredient_to_recipes.find(ingred->name());
               if (recipe_iter == ingredient_to_recipes.end()) {
                  cerr << "Ingredient not mapped: this should never happen\n";
                  continue;
               }
               const unordered_set<const zns::Recipe*>& recipes = recipe_iter->second;
               for (const zns::Recipe* recipe : recipes) {
                  matching_recipes.insert(recipe);
               }
            }
         }

         vector<pair<const zns::Recipe*, double>> recipe_scores;
         for (const zns::Recipe* recipe : matching_recipes) {
            double score = 0;
            for (const zns::Ingredient& ingred : recipe->ingredients()) {
               for (const unordered_set<const zns::Ingredient*>& ingred_set : user_inventory) {
                  if (ingred_set.count(&ingred)) {
                     score += 1;
                     continue;
                  }
               }
            }
            recipe_scores.push_back(make_pair(recipe, score / recipe->ingredients_size()));
         }

         std::sort(recipe_scores.begin(), recipe_scores.end(), compareBySecond);
         for (int i = 0; i < 5 && i < recipe_scores.size(); ++i) {
            const zns::Recipe* recipe = recipe_scores[i].first;
            cout << recipe->DebugString();
         }
      }
   }

   return 0;
}
