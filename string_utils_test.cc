#include "string_utils.h"
using namespace util;

#include <gtest/gtest.h>
#include <string>
using std::string;
#include <vector>
using std::vector;

const char kTestSplittable[] = "a,b;;d efg  hi,";
const char kInteger[] = "-12345";
const int kIntegerValue = -12345;
const char kLongInt[] = "1234567890000";
const long int kLongIntValue = 1234567890000LL;
const char kDouble[] = "-00001.234";
const double kDoubleValue = -1.234;
const char kNotNumeric[] = "hello, world!";

TEST(StringUtilsTest, testStringToNumbers) {
   int number;
   EXPECT_TRUE(StringToInt(kInteger, &number));
   EXPECT_EQ(kIntegerValue, number);

   long long_number;
   EXPECT_TRUE(StringToLong(kLongInt, &long_number));
   EXPECT_EQ(kLongIntValue, long_number);
   EXPECT_FALSE(StringToInt(kLongInt, &number));

   double dbl;
   EXPECT_TRUE(StringToDouble(kDouble, &dbl));
   EXPECT_EQ(kDoubleValue, dbl);
   EXPECT_TRUE(StringToInt(kDouble, &number));
   EXPECT_EQ((int)kDoubleValue, number);
   EXPECT_TRUE(StringToLong(kDouble, &long_number));
   EXPECT_EQ((long int)kDoubleValue, long_number);

   EXPECT_FALSE(StringToInt(kNotNumeric, &number));
   EXPECT_FALSE(StringToLong(kNotNumeric, &long_number));
   EXPECT_FALSE(StringToDouble(kNotNumeric, &dbl));
}

TEST(StringUtilsTest, testSplitString) {
   vector<string> split_on_comma = SplitString(kTestSplittable, ",");
   vector<string> actual = {"a", "b;;d efg  hi", ""};
   EXPECT_EQ(actual, split_on_comma);

   vector<string> split_on_space = SplitString(kTestSplittable, " ");
   actual = {"a,b;;d", "efg", "", "hi,"};
   EXPECT_EQ(actual, split_on_space);

   vector<string> split_on_double_semi = SplitString(kTestSplittable, ";;");
   actual = {"a,b", "d efg  hi,"};
   EXPECT_EQ(actual, split_on_double_semi);

   vector<string> split_on_a = SplitString(kTestSplittable, "a,b");
   actual = {"", ";;d efg  hi,"};
   EXPECT_EQ(actual, split_on_a);
}
