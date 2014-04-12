#include "file_utils.h"
using util::CsvFileReader;
#include "string_utils.h"

#include "cooking.pb.h"

#include <algorithm>
#include <iostream>
using std::cerr;
using std::cout;
#include <memory>
using std::unique_ptr;
#include <unordered_map>
using std::unordered_map;
#include <unordered_set>
using std::unordered_set;
#include <string>
using std::string;
#include <vector>
using std::vector;

#include <fcntl.h>
#include <unistd.h>

#include <gflags/gflags.h>
#include <simstring/simstring.h>
#include <google/protobuf/io/zero_copy_stream.h>
using google::protobuf::io::ZeroCopyInputStream;
#include <google/protobuf/io/zero_copy_stream_impl.h>
using google::protobuf::io::FileInputStream;
#include <google/protobuf/io/coded_stream.h>
using google::protobuf::io::CodedInputStream;

DEFINE_string(proto_bin_in, "", "Load serialized products from this file");
DEFINE_string(tmp_dir, "", "Save temporary simstring db to this directory");
DEFINE_string(extra_bin_in, "", "Optionally load more products from this file");
const char kSimstringFname[] = "simstring_ingred.db";

vector<string> GetSimstrings(const string& query, int measure, double threshold,
                            simstring::reader* dbr) {
   vector<string> res;
   dbr->retrieve(query, measure, threshold, std::back_inserter(res));
   return res;
}

bool LoadRecipeListFromFD(int fd, zns::RecipeList* recipe_list) {
   struct stat infile_stat;
   if (fstat(fd, &infile_stat) < 0) {
      cerr << "Error stat-ing input file\n";
      return false;
   }
   size_t fsize = infile_stat.st_size;

   unique_ptr<ZeroCopyInputStream> zc_stream(new FileInputStream(fd));
   CodedInputStream in_stream(zc_stream.get());
   in_stream.SetTotalBytesLimit(fsize, fsize);
   if (!recipe_list->ParseFromCodedStream(&in_stream)) {
      cerr << "Error parsing serialized recipes\n";
      return false;
   }
   return true;
}

void WriteSimstringDB(const string& db_fname, const unordered_set<string>& keys) {
   simstring::ngram_generator gen(3, false);
   simstring::writer_base<string> dbw(gen, db_fname);
   for (const string& prod_name : keys) {
      dbw.insert(prod_name);
   }
   dbw.close();
}

int main(int argc, char** argv) {
   google::ParseCommandLineFlags(&argc, &argv, true);

   if (FLAGS_proto_bin_in.empty()) {
      cerr << "Must supply an input file proto_bin_in\n";
      return 1;
   }

   int fd = open(FLAGS_proto_bin_in.c_str(), O_RDONLY);
   if (fd < 0) {
      cerr << "Error opening proto_bin_in\n";
      return 1;
   }

   cerr << "Loading recipes...\n";
   zns::RecipeList recipe_list;
   if (!LoadRecipeListFromFD(fd, &recipe_list)) {
      return 1;
   }
   close(fd);

   cerr << "Loading extra recipes...\n";
   if (!FLAGS_extra_bin_in.empty()) {
      fd = open(FLAGS_extra_bin_in.c_str(), O_RDONLY);
      if (fd < 0) {
         cerr << "Error opening extra_bin_in\n";
      } else {
         cerr << "Loading supplimental recipes...\n";
         zns::RecipeList extra_list;
         if (LoadRecipeListFromFD(fd, &extra_list)) {
            recipe_list.MergeFrom(extra_list);
         }
      }
   }
   cerr << "Loaded " << recipe_list.entries().size() << " recipes.\n";

   cerr << "Mapping recipes and ingredients...\n";
   unordered_map<string, unordered_set<const zns::Recipe*>> ingredient_to_recipes;
   unordered_map<string, const zns::Ingredient*> name_to_ingredient;
   unordered_set<string> all_ingredient_names;
   for (const zns::Recipe& recipe : recipe_list.entries()) {
      for (const zns::Ingredient& ingredient : recipe.ingredients()) {
         if (ingredient.name().empty()) {
            cerr << "Invalid ingredient: missing name\n"
                  "From recipe: " << recipe.name() << "\n";
            continue;
         }
         string ingred_name = util::StringToLower(ingredient.name());
         name_to_ingredient.insert({ingred_name, &ingredient});
         ingredient_to_recipes[ingred_name].insert(&recipe);
         all_ingredient_names.insert(ingred_name);
      }
   }
   cerr << "Mapped " << all_ingredient_names.size() << " ingredients.\n";

   cerr << "Creating simstring db...\n";
   const string ss_db_fname = FLAGS_tmp_dir.append("/").append(kSimstringFname);
   WriteSimstringDB(ss_db_fname, all_ingredient_names);
   all_ingredient_names.clear();

   cerr << "Reopening simstring db...\n";
   simstring::reader dbr;
   dbr.open(ss_db_fname);

   double threshold = 0.6;
   while (true) {
      string input_line;
      cerr << ":";
      std::getline(std::cin, input_line);
      if (input_line == ".exit" || input_line == ".q") {
         break;
      } else if (input_line == ".threshold") {
         cerr << "threshold=" << threshold << "\nenter new threshold:";
         std::getline(std::cin, input_line);
         if (util::StringToDouble(input_line, &threshold)) {
            continue;
         } else {
            cerr << "invalid threshold value.\n";
            continue;
         }
      } else {
         vector<string> matches = GetSimstrings(
               util::StringToLower(input_line), simstring::cosine, threshold, &dbr);
         if (matches.empty()) {
            cout << "No good matches for: \"" << input_line << "\"\n";
            continue;
         }
         for (const string& ingred_name : matches) {
            unordered_map<string, const zns::Ingredient*>::const_iterator iter =
                  name_to_ingredient.find(ingred_name);
            if (iter == name_to_ingredient.end()) {
               cerr << "Ingredient name not mapped: this shouldn't happen\n";
               continue;
            } else {
               const zns::Ingredient* ingred = iter->second;
               cout << ingred->DebugString();
               cout << "\nRecipes using ingredient \"" << ingred->name() << "\":\n";
               unordered_map<string, unordered_set<const zns::Recipe*>>::const_iterator
                     iter = ingredient_to_recipes.find(ingred->name());
               if (iter == ingredient_to_recipes.end()) {
                  cout << "Ingredient not mapped to any recipes: this shouldn't happen\n";
                  continue;
               } else {
                  const unordered_set<const zns::Recipe*> recipes = iter->second;
                  for (const zns::Recipe* recipe : recipes) {
                     cout << recipe->DebugString();
                  }
               }
            }
         }
      }
   }

   return 0;
}
