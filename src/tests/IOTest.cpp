// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <gtest/gtest.h>
#include "esp/assets/RenderAssetInstanceCreationInfo.h"
#include "esp/core/esp.h"
#include "esp/io/JsonAllTypes.h"
#include "esp/io/URDFParser.h"
#include "esp/io/io.h"
#include "esp/io/json.h"
#include "esp/metadata/attributes/ObjectAttributes.h"

#include "configure.h"

namespace Cr = Corrade;
#include <limits>

using namespace esp::io;

using esp::metadata::attributes::AbstractObjectAttributes;
using esp::metadata::attributes::ObjectAttributes;

const std::string dataDir =
    Corrade::Utility::Directory::join(SCENE_DATASETS, "../");

TEST(IOTest, fileExistTest) {
  std::string file = FILE_THAT_EXISTS;
  bool result = exists(file);
  EXPECT_TRUE(result);

  file = "Foo.bar";
  result = exists(file);
  EXPECT_FALSE(result);
}

TEST(IOTest, fileSizeTest) {
  std::string existingFile = FILE_THAT_EXISTS;
  auto result = fileSize(existingFile);
  LOG(INFO) << "File size of " << existingFile << " is " << result;

  std::string nonexistingFile = "Foo.bar";
  result = fileSize(nonexistingFile);
  LOG(INFO) << "File size of " << nonexistingFile << " is " << result;
}

TEST(IOTest, fileRmExtTest) {
  std::string filename = "/foo/bar.jpeg";

  // rm extension
  std::string result = removeExtension(filename);
  EXPECT_EQ(result, "/foo/bar");
  EXPECT_EQ(filename, "/foo/bar.jpeg");

  std::string filenameNoExt = "/path/to/foobar";
  result = removeExtension(filenameNoExt);
  EXPECT_EQ(result, filenameNoExt);
}

TEST(IOTest, fileReplaceExtTest) {
  std::string filename = "/foo/bar.jpeg";

  // change extension
  std::string ext = ".png";
  std::string result = changeExtension(filename, ext);

  EXPECT_EQ(result, "/foo/bar.png");

  std::string filenameNoExt = "/path/to/foobar";
  result = changeExtension(filenameNoExt, ext);
  EXPECT_EQ(result, "/path/to/foobar.png");

  std::string cornerCase = "";
  result = changeExtension(cornerCase, ext);
  EXPECT_EQ(result, ".png");

  cornerCase = ".";
  result = changeExtension(cornerCase, ext);
  EXPECT_EQ(result, "..png");

  cornerCase = "..";
  result = changeExtension(cornerCase, ext);
  EXPECT_EQ(result, "...png");

  std::string cornerCaseExt = "png";  // no dot
  result = changeExtension(filename, cornerCaseExt);
  EXPECT_EQ(result, "/foo/bar.png");

  cornerCase = ".";
  result = changeExtension(cornerCase, cornerCaseExt);
  EXPECT_EQ(result, "..png");

  cornerCase = "..";
  result = changeExtension(cornerCase, cornerCaseExt);
  EXPECT_EQ(result, "...png");

  cornerCase = ".jpg";
  result = changeExtension(cornerCase, cornerCaseExt);
  EXPECT_EQ(result, ".jpg.png");
}

TEST(IOTest, tokenizeTest) {
  std::string file = ",a,|,bb|c";
  const auto& t1 = tokenize(file, ",");
  EXPECT_EQ((std::vector<std::string>{"", "a", "|", "bb|c"}), t1);
  const auto& t2 = tokenize(file, "|");
  EXPECT_EQ((std::vector<std::string>{",a,", ",bb", "c"}), t2);
  const auto& t3 = tokenize(file, ",|", 0, true);
  EXPECT_EQ((std::vector<std::string>{"", "a", "bb", "c"}), t3);
}

TEST(IOTest, parseURDF) {
  const std::string iiwaURDF = Cr::Utility::Directory::join(
      TEST_ASSETS, "URDF/kuka_iiwa/model_free_base.urdf");

  URDF::Parser parser;

  // load the iiwa test asset
  parser.parseURDF(iiwaURDF);
  auto& model = parser.getModel();
  Cr::Utility::Debug() << "name: " << model.m_name;
  EXPECT_EQ(model.m_name, "lbr_iiwa");
  Cr::Utility::Debug() << "file: " << model.m_sourceFile;
  EXPECT_EQ(model.m_sourceFile, iiwaURDF);
  Cr::Utility::Debug() << "links: " << model.m_links;
  EXPECT_EQ(model.m_links.size(), 8);
  Cr::Utility::Debug() << "root links: " << model.m_rootLinks;
  EXPECT_EQ(model.m_rootLinks.size(), 1);
  Cr::Utility::Debug() << "joints: " << model.m_joints;
  EXPECT_EQ(model.m_joints.size(), 7);
  Cr::Utility::Debug() << "materials: " << model.m_materials;
  EXPECT_EQ(model.m_materials.size(), 3);

  // test overwrite re-load
  parser.parseURDF(iiwaURDF);
}

/**
 * @brief Test basic JSON file processing
 */
TEST(IOTest, JsonTest) {
  std::string s = "{\"test\":[1,2,3,4]}";
  const auto& json = esp::io::parseJsonString(s);
  std::vector<int> t;
  esp::io::toIntVector(json["test"], &t);
  EXPECT_EQ(t[1], 2);
  EXPECT_EQ(esp::io::jsonToString(json), s);

  // test io
  auto testFilepath =
      Corrade::Utility::Directory::join(dataDir, "../io_test_json.json");
  EXPECT_TRUE(writeJsonToFile(json, testFilepath));
  const auto& loadedJson = esp::io::parseJsonFile(testFilepath);
  EXPECT_EQ(esp::io::jsonToString(loadedJson), s);
  Corrade::Utility::Directory::rm(testFilepath);

  // test basic attributes populating

  std::string attr_str =
      "{\"render mesh\": \"banana.glb\",\"join collision "
      "meshes\":false,\"mass\": 0.066,\"scale\": [2.0,2.0,2]}";

  // io::JsonGenericValue :
  esp::io::JsonDocument tmpJSON = esp::io::parseJsonString(attr_str);
  // io::JsonGenericValue :
  const esp::io::JsonGenericValue jsonDoc = tmpJSON.GetObject();

  // for function ptr placeholder
  using std::placeholders::_1;
  ObjectAttributes::ptr attributes = ObjectAttributes::create("temp");

  bool success = false;
  // test vector
  success = esp::io::jsonIntoConstSetter<Magnum::Vector3>(
      jsonDoc, "scale", std::bind(&ObjectAttributes::setScale, attributes, _1));
  EXPECT_EQ(success, true);
  EXPECT_EQ(attributes->getScale()[1], 2);

  // test double
  success = esp::io::jsonIntoSetter<double>(
      jsonDoc, "mass", std::bind(&ObjectAttributes::setMass, attributes, _1));
  EXPECT_EQ(success, true);
  EXPECT_EQ(attributes->getMass(), 0.066);

  // test bool
  success = esp::io::jsonIntoSetter<bool>(
      jsonDoc, "join collision meshes",
      std::bind(&ObjectAttributes::setJoinCollisionMeshes, attributes, _1));
  EXPECT_EQ(success, true);
  EXPECT_EQ(attributes->getJoinCollisionMeshes(), false);

  // test string
  success = esp::io::jsonIntoSetter<std::string>(
      jsonDoc, "render mesh",
      std::bind(&ObjectAttributes::setRenderAssetHandle, attributes, _1));
  EXPECT_EQ(success, true);
  EXPECT_EQ(attributes->getRenderAssetHandle(), "banana.glb");
}

// Serialize/deserialize the 7 rapidjson builtin types using
// io::addMember/readMember and assert equality.
TEST(IOTest, JsonBuiltinTypesTest) {
  rapidjson::Document d(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

  {
    int x{std::numeric_limits<int>::lowest()};
    addMember(d, "myint", x, allocator);
    int x2{0};
    readMember(d, "myint", x2);
    EXPECT_EQ(x2, x);
  }

  {
    unsigned x{std::numeric_limits<unsigned>::max()};
    addMember(d, "myunsigned", x, allocator);
    unsigned x2{0};
    readMember(d, "myunsigned", x2);
    EXPECT_EQ(x2, x);
  }

  {
    int64_t x{std::numeric_limits<int64_t>::lowest()};
    addMember(d, "myint64_t", x, allocator);
    int64_t x2{0};
    readMember(d, "myint64_t", x2);
    EXPECT_EQ(x2, x);
  }

  {
    uint64_t x{std::numeric_limits<uint64_t>::max()};
    addMember(d, "myuint64_t", x, allocator);
    uint64_t x2{0};
    readMember(d, "myuint64_t", x2);
    EXPECT_EQ(x2, x);
  }

  {
    float x{1.0 / 7};
    addMember(d, "myfloat", x, allocator);
    float x2{0};
    readMember(d, "myfloat", x2);
    EXPECT_EQ(x2, x);
  }

  {
    double x{1.0 / 13};
    addMember(d, "mydouble", x, allocator);
    double x2{0};
    readMember(d, "mydouble", x2);
    EXPECT_EQ(x2, x);
  }

  {
    bool x{true};
    addMember(d, "mybool", x, allocator);
    bool x2{false};
    readMember(d, "mybool", x2);
    EXPECT_EQ(x2, x);
  }

  // verify failure to read bool into int
  {
    int x2{0};
    EXPECT_FALSE(readMember(d, "mybool", x2));
  }

  // verify failure to read missing tag
  {
    int x2{0};
    EXPECT_FALSE(readMember(d, "my_missing_int", x2));
  }
}

// Serialize/deserialize a few stl types using io::addMember/readMember and
// assert equality.
TEST(IOTest, JsonStlTypesTest) {
  rapidjson::Document d(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

  std::string s{"hello world"};
  addMember(d, "s", s, allocator);
  std::string s2;
  readMember(d, "s", s2);
  EXPECT_EQ(s2, s);

  // test a vector of ints
  std::vector<int> vec{3, 4, 5, 6};
  addMember(d, "vec", vec, allocator);
  std::vector<int> vec2;
  readMember(d, "vec", vec2);
  EXPECT_EQ(vec2, vec);

  // test an empty vector
  std::vector<float> emptyVec{};
  addMember(d, "emptyVec", emptyVec, allocator);
  std::vector<float> emptyVec2;
  readMember(d, "emptyVec", emptyVec2);
  EXPECT_EQ(emptyVec2, emptyVec);

  // test reading a vector of wrong type
  std::vector<std::string> vec3;
  EXPECT_FALSE(readMember(d, "vec", vec3));
}

// Serialize/deserialize a few Magnum types using io::addMember/readMember and
// assert equality.
TEST(IOTest, JsonMagnumTypesTest) {
  rapidjson::Document d(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

  Magnum::Vector3 vec{1, 2, 3};
  addMember(d, "myvec", vec, allocator);
  Magnum::Vector3 vec2;
  readMember(d, "myvec", vec2);
  EXPECT_EQ(vec2, vec);

  Magnum::Quaternion quat{{1, 2, 3}, 4};
  addMember(d, "myquat", quat, allocator);
  Magnum::Quaternion quat2;
  readMember(d, "myquat", quat2);
  EXPECT_EQ(quat2, quat);

  // test reading the wrong type (wrong number of fields)
  Magnum::Quaternion quat3;
  EXPECT_FALSE(readMember(d, "myvec", quat3));

  // test reading the wrong type (wrong number of fields)
  Magnum::Vector3 vec3;
  EXPECT_FALSE(readMember(d, "myquat", vec3));

  // test reading the wrong type (array elements aren't numbers)
  std::vector<std::string> vecOfStrings{"1", "2", "3"};
  addMember(d, "myVecOfStrings", vecOfStrings, allocator);
  EXPECT_FALSE(readMember(d, "myVecOfStrings", vec3));
}

// Serialize/deserialize a few esp types using io::addMember/readMember and
// assert equality.
TEST(IOTest, JsonEspTypesTest) {
  rapidjson::Document d(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

  // add RenderAssetInstanceCreationInfo
  esp::assets::RenderAssetInstanceCreationInfo creationInfo(
      "test_filepath", Magnum::Vector3(1.f, 2.f, 3.f),
      esp::assets::RenderAssetInstanceCreationInfo::Flags(),
      "test_light_setup");
  addMember(d, "creationInfo", creationInfo, allocator);

  // AssetInfo
  esp::assets::AssetInfo assetInfo{
      esp::assets::AssetType::MP3D_MESH,
      "test_filepath2",
      esp::geo::CoordinateFrame(esp::vec3f(1.f, 0.f, 0.f),
                                esp::vec3f(0.f, 0.f, 1.f),
                                esp::vec3f(1.f, 2.f, 3.f)),
      4.f,
      true,
      false};
  addMember(d, "assetInfo", assetInfo, allocator);

  // add RenderAssetInstanceState
  esp::gfx::replay::RenderAssetInstanceState state{
      {Magnum::Vector3(1.f, 2.f, 3.f),
       Magnum::Quaternion::rotation(Magnum::Rad{1.f},
                                    Magnum::Vector3(0.f, 1.f, 0.f))},
      4};
  addMember(d, "state", state, allocator);

  // read and compare RenderAssetInstanceCreationInfo
  esp::assets::RenderAssetInstanceCreationInfo creationInfo2;
  readMember(d, "creationInfo", creationInfo2);
  EXPECT_EQ(creationInfo2.filepath, creationInfo.filepath);
  EXPECT_EQ(creationInfo2.scale, creationInfo.scale);
  EXPECT_EQ(creationInfo2.flags, creationInfo.flags);
  EXPECT_EQ(creationInfo2.lightSetupKey, creationInfo.lightSetupKey);

  // read and compare AssetInfo
  esp::assets::AssetInfo assetInfo2;
  readMember(d, "assetInfo", assetInfo2);
  EXPECT_EQ(assetInfo2.type, assetInfo.type);
  EXPECT_EQ(assetInfo2.filepath, assetInfo.filepath);
  EXPECT_EQ(assetInfo2.frame.up(), assetInfo.frame.up());
  EXPECT_EQ(assetInfo2.frame.front(), assetInfo.frame.front());
  EXPECT_EQ(assetInfo2.frame.origin(), assetInfo.frame.origin());
  EXPECT_EQ(assetInfo2.virtualUnitToMeters, assetInfo.virtualUnitToMeters);
  EXPECT_EQ(assetInfo2.requiresLighting, assetInfo.requiresLighting);
  EXPECT_EQ(assetInfo2.splitInstanceMesh, assetInfo.splitInstanceMesh);

  // read and compare RenderAssetInstanceState
  esp::gfx::replay::RenderAssetInstanceState state2;
  readMember(d, "state", state2);
  EXPECT_EQ(state2, state);
}

namespace {
// some test structs for JsonUserTypeTest below
struct MyNestedStruct {
  std::string a;
};

struct MyOuterStruct {
  MyNestedStruct nested;
  float b;
};

// Beware, toJsonValue/fromJsonValue should generally go in JsonAllTypes.h,
// not scattered in user code as done here.
inline JsonGenericValue toJsonValue(const MyNestedStruct& x,
                                    JsonAllocator& allocator) {
  JsonGenericValue obj(rapidjson::kObjectType);
  addMember(obj, "a", x.a, allocator);
  return obj;
}

bool fromJsonValue(const JsonGenericValue& obj, MyNestedStruct& x) {
  readMember(obj, "a", x.a);
  return true;
}

inline JsonGenericValue toJsonValue(const MyOuterStruct& x,
                                    JsonAllocator& allocator) {
  JsonGenericValue obj(rapidjson::kObjectType);
  addMember(obj, "nested", x.nested, allocator);
  addMember(obj, "b", x.b, allocator);
  return obj;
}

bool fromJsonValue(const JsonGenericValue& obj, MyOuterStruct& x) {
  readMember(obj, "nested", x.nested);
  readMember(obj, "b", x.b);
  return true;
}
}  // namespace

// Serialize/deserialize MyOuterStruct using io::addMember/readMember and assert
// equality.
TEST(IOTest, JsonUserTypeTest) {
  rapidjson::Document d(rapidjson::kObjectType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

  MyOuterStruct myStruct{{"hello world"}, 2.f};
  addMember(d, "myStruct", myStruct, allocator);

  MyOuterStruct myStruct2;
  readMember(d, "myStruct", myStruct2);

  EXPECT_EQ(myStruct2.nested.a, myStruct.nested.a);
  EXPECT_EQ(myStruct2.b, myStruct.b);
}
