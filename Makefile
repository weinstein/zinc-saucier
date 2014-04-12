

CURL_CFLAGS = $(shell curl-config --cflags)
CURL_LIBS = $(shell curl-config --libs)
GFLAGS_LIBS = -lgflags
JSONCPP_LIBS = -ljsoncpp
PROTOBUF_LIBS = -lprotobuf -lpthread
GTEST_SRC = /usr/src/gtest/src/gtest-all.cc
GTEST_MAIN = /usr/src/gtest/src/gtest_main.cc

PROTOC = protoc --cpp_out=./ --python_out=./python

COMPILE = g++ -c --std=c++0x -g -O0 $(CURL_CFLAGS)
LINK = g++ --std=c++0x -g -O0
LINK_FLAGS = $(CURL_LIBS) $(GFLAGS_LIBS) $(PROTOBUF_LIBS) $(JSONCPP_LIBS)

gtest.o: $(GTEST_SRC)
	$(COMPILE) -I/usr/src/gtest $(GTEST_SRC) -o gtest.o

gtest_main.o: $(GTEST_MAIN)
	$(COMPILE) -I/usr/src/gtest,/usr/include $(GTEST_MAIN) -o gtest_main.o

GTEST_LIBS = gtest.o gtest_main.o


EXEC = http_session_test json_utils_test reddit_agent_test file_utils_test string_utils_test gtin_csv_to_proto gs1_search_tool ingredient_search_tool zinc_saucier

http_session.o: http_session.cc
	$(COMPILE) http_session.cc

http_session_test.o: http_session_test.cc
	$(COMPILE) http_session_test.cc

http_session_test: http_session_test.o http_session.o $(GTEST_LIBS)
	$(LINK) http_session_test.o http_session.o $(GTEST_LIBS) $(LINK_FLAGS) -o http_session_test

reddit_types.pb.h: reddit_types.proto
	$(PROTOC) reddit_types.proto

reddit_types.pb.cc: reddit_types.proto
	$(PROTOC) reddit_types.proto

reddit_constants.o: reddit_constants.cc
	$(COMPILE) reddit_constants.cc

json_utils.o: json_utils.cc reddit_types.pb.h
	$(COMPILE) json_utils.cc

json_utils_test.o: json_utils_test.cc reddit_types.pb.h
	$(COMPILE) json_utils_test.cc

json_utils_test: json_utils.o json_utils_test.o reddit_types.pb.o reddit_constants.o $(GTEST_LIBS)
	$(LINK) json_utils.o json_utils_test.o reddit_types.pb.o reddit_constants.o $(GTEST_LIBS) $(LINK_FLAGS) -o json_utils_test

string_utils.o: string_utils.cc
	$(COMPILE) string_utils.cc

string_utils_test.o: string_utils_test.cc
	$(COMPILE) string_utils_test.cc

string_utils_test: string_utils_test.o string_utils.o $(GTEST_LIBS)
	$(LINK) string_utils_test.o string_utils.o $(GTEST_LIBS) $(LINK_FLAGS) -o string_utils_test

file_utils.o: file_utils.cc
	$(COMPILE) file_utils.cc

file_utils_test.o: file_utils_test.cc
	$(COMPILE) file_utils_test.cc

file_utils_test: file_utils_test.o file_utils.o string_utils.o $(GTEST_LIBS)
	$(LINK) file_utils.o file_utils_test.o string_utils.o $(GTEST_LIBS) $(LINK_FLAGS) -o file_utils_test

reddit_agent.o: reddit_agent.cc reddit_types.pb.h
	$(COMPILE) reddit_agent.cc

reddit_agent_test.o: reddit_agent_test.cc reddit_types.pb.h
	$(COMPILE) reddit_agent_test.cc

reddit_agent_test: reddit_agent_test.o reddit_agent.o reddit_constants.o json_utils.o file_utils.o string_utils.o http_session.o reddit_types.pb.o $(GTEST_LIBS)
	$(LINK) reddit_agent_test.o reddit_agent.o reddit_constants.o json_utils.o file_utils.o string_utils.o http_session.o reddit_types.pb.o $(GTEST_LIBS) $(LINK_FLAGS) -o reddit_agent_test

cooking.pb.h: cooking.proto
	$(PROTOC) cooking.proto

cooking.pb.cc: cooking.proto
	$(PROTOC) cooking.proto

python/cooking_pb2.py: cooking.proto
	$(PROTOC) cooking.proto

gs1.pb.h: gs1.proto
	$(PROTOC) gs1.proto

gs1.pb.cc: gs1.proto
	$(PROTOC) gs1.proto

gs1_utils.o: gs1_utils.cc gs1.pb.h
	$(COMPILE) gs1_utils.cc

gtin_csv_to_proto.o: gtin_csv_to_proto.cc gs1.pb.h
	$(COMPILE) gtin_csv_to_proto.cc

gtin_csv_to_proto: gtin_csv_to_proto.o gs1.pb.o gs1_utils.o file_utils.o string_utils.o
	$(LINK) gtin_csv_to_proto.o gs1.pb.o gs1_utils.o file_utils.o string_utils.o $(LINK_FLAGS) -o gtin_csv_to_proto

ingredient_search_tool.o: ingredient_search_tool.cc cooking.pb.h
	$(COMPILE) ingredient_search_tool.cc

ingredient_search_tool: ingredient_search_tool.o cooking.pb.o file_utils.o string_utils.o
	$(LINK) ingredient_search_tool.o cooking.pb.o file_utils.o string_utils.o $(LINK_FLAGS) -o ingredient_search_tool

gs1_search_tool.o: gs1_search_tool.cc gs1.pb.h
	$(COMPILE) gs1_search_tool.cc

gs1_search_tool: gs1_search_tool.o gs1.pb.o gs1_utils.o file_utils.o string_utils.o
	$(LINK) gs1_search_tool.o gs1.pb.o gs1_utils.o file_utils.o string_utils.o $(LINK_FLAGS) -o gs1_search_tool

zinc_saucier.o: zinc_saucier.cc gs1.pb.h cooking.pb.h
	$(COMPILE) zinc_saucier.cc

zinc_saucier: zinc_saucier.o gs1.pb.o cooking.pb.o file_utils.o string_utils.o perceptron.o
	$(LINK) zinc_saucier.o gs1.pb.o cooking.pb.o file_utils.o string_utils.o perceptron.o $(LINK_FLAGS) -o zinc_saucier

perceptron.o: perceptron.cc
	$(COMPILE) perceptron.cc

clean:
	rm -f *.o
	rm -f *.pb.h *.pb.cc
	rm -f $(EXEC)
