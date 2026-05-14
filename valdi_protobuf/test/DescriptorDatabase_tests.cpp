#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_protobuf/DescriptorDatabase.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <google/protobuf/descriptor.pb.h>

using namespace Valdi;
namespace {

static bool registerProtoInDatabase(Protobuf::DescriptorDatabase& database, ExceptionTracker& exceptionTracker) {
    std::string_view proto1 = R"(
syntax = "proto3";
package test;

enum Enum {
  VALUE_0 = 0;
  VALUE_1 = 1;
}

message MyMessage {
  string value = 1;
}

message MapMessage {
  map<string, string> stringToString = 1;
}

message ParentMessage {
  ChildMessage child_message = 1;
  ChildEnum child_enum = 2;

  message ChildMessage {
    string value = 1;
  }

  enum ChildEnum {
    VALUE_0 = 0;
    VALUE_1 = 1;
  }
}
)";

    std::string_view proto2 = R"(
syntax = "proto3";
package other_package.nested;

message MyOtherMessage {
  string value = 1;
}

)";
    if (!database.parseAndAddFileDescriptorSet("file1.proto", proto1, exceptionTracker)) {
        return false;
    }

    if (!database.parseAndAddFileDescriptorSet("directory/file2.proto", proto2, exceptionTracker)) {
        return false;
    }

    return true;
}

TEST(DescriptorDatabase, canFindFileByName) {
    SimpleExceptionTracker exceptionTracker;
    Protobuf::DescriptorDatabase database;

    ASSERT_TRUE(registerProtoInDatabase(database, exceptionTracker)) << exceptionTracker.extractError();

    google::protobuf::FileDescriptorProto fileDescriptor;

    ASSERT_TRUE(database.FindFileByName("file1.proto", &fileDescriptor));

    ASSERT_EQ("file1.proto", fileDescriptor.name());
    ASSERT_EQ("test", fileDescriptor.package());

    ASSERT_TRUE(database.FindFileByName("directory/file2.proto", &fileDescriptor));

    ASSERT_EQ("directory/file2.proto", fileDescriptor.name());
    ASSERT_EQ("other_package.nested", fileDescriptor.package());

    ASSERT_FALSE(database.FindFileByName("file3.proto", &fileDescriptor));
}

TEST(DescriptorDatabase, canFindFileContainingSymbol) {
    SimpleExceptionTracker exceptionTracker;
    Protobuf::DescriptorDatabase database;

    ASSERT_TRUE(registerProtoInDatabase(database, exceptionTracker)) << exceptionTracker.extractError();

    google::protobuf::FileDescriptorProto fileDescriptor;
    ASSERT_TRUE(database.FindFileContainingSymbol("test.MyMessage", &fileDescriptor));

    ASSERT_EQ("file1.proto", fileDescriptor.name());

    ASSERT_TRUE(database.FindFileContainingSymbol("other_package.nested.MyOtherMessage", &fileDescriptor));

    ASSERT_EQ("directory/file2.proto", fileDescriptor.name());

    ASSERT_FALSE(database.FindFileContainingSymbol("other_package.nested.MyNonExistentMessage", &fileDescriptor));
}

TEST(DescriptorDatabase, canFindAllFileNames) {
    SimpleExceptionTracker exceptionTracker;
    Protobuf::DescriptorDatabase database;

    ASSERT_TRUE(registerProtoInDatabase(database, exceptionTracker)) << exceptionTracker.extractError();

    std::vector<std::string> fileNames;
    ASSERT_TRUE(database.FindAllFileNames(&fileNames));

    ASSERT_EQ(std::vector<std::string>({"file1.proto", "directory/file2.proto"}), fileNames);
}

TEST(DescriptorDatabase, canRetrieveSymbolNames) {
    SimpleExceptionTracker exceptionTracker;
    Protobuf::DescriptorDatabase database;

    ASSERT_TRUE(registerProtoInDatabase(database, exceptionTracker)) << exceptionTracker.extractError();

    std::vector<std::string> symbolNameStrings = database.getAllSymbolNames();

    std::sort(symbolNameStrings.begin(), symbolNameStrings.end());

    ASSERT_EQ(std::vector<std::string>({
                  "other_package.nested.MyOtherMessage",
                  "test.Enum",
                  "test.MapMessage",
                  "test.MapMessage.StringToStringEntry",
                  "test.MyMessage",
                  "test.ParentMessage",
                  "test.ParentMessage.ChildEnum",
                  "test.ParentMessage.ChildMessage",
              }),
              symbolNameStrings);
}

TEST(DescriptorDatabase, canRetrievePackages) {
    SimpleExceptionTracker exceptionTracker;
    Protobuf::DescriptorDatabase database;

    ASSERT_TRUE(registerProtoInDatabase(database, exceptionTracker)) << exceptionTracker.extractError();

    ASSERT_EQ(
        Value()
            .setMapValue("name", Value("<root>"))
            .setMapValue(
                "packages",
                Value(ValueArray::make(
                    {Value()
                         .setMapValue("name", Value("test"))
                         .setMapValue("symbols",
                                      Value(ValueArray::make({Value("test.Enum"),
                                                              Value("test.MapMessage"),
                                                              Value("test.MyMessage"),
                                                              Value("test.ParentMessage")})))
                         .setMapValue(
                             "packages",
                             Value(ValueArray::make({Value()
                                                         .setMapValue("name", Value("test.MapMessage"))
                                                         .setMapValue("symbols",
                                                                      Value(ValueArray::make({Value(
                                                                          "test.MapMessage.StringToStringEntry")}))),
                                                     Value()
                                                         .setMapValue("name", Value("test.ParentMessage"))
                                                         .setMapValue("symbols",
                                                                      Value(ValueArray::make({
                                                                          Value("test.ParentMessage.ChildEnum"),
                                                                          Value("test.ParentMessage.ChildMessage"),
                                                                      })))}))),
                     Value()
                         .setMapValue("name", Value("other_package"))
                         .setMapValue("packages",
                                      Value(ValueArray::make(
                                          {Value()
                                               .setMapValue("name", Value("other_package.nested"))
                                               .setMapValue("symbols",
                                                            Value(ValueArray::make({Value(
                                                                "other_package.nested.MyOtherMessage")})))})))}))),
        database.toDebugJSON());
}

} // namespace
