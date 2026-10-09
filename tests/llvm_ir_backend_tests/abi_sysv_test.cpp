#include <gtest/gtest.h>
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_internal.h"
#include "types.h"
#include "il.h"

BEGIN_EDG_NAMESPACE

class ABISysVTest : public ::testing::Test {
protected:
  void SetUp() override {
  }

  void TearDown() override {
  }
};

TEST_F(ABISysVTest, EmptyTypeClassifiesAsNoClass) {
  a_type ty = {};
  ty.size = 0;
  
  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(&ty, 0, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_EQ(info.num_eightbytes, 0);
}

TEST_F(ABISysVTest, LargeTypeClassifiesAsMemory) {
  a_type ty = {};
  ty.size = 32;
  
  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(&ty, 0, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_TRUE(info.pass_in_memory);
  EXPECT_EQ(info.eightbyte_classes[0], x86_64_abi_class_t::memory);
}

TEST_F(ABISysVTest, FloatTypeClassifiesAsSSE) {
  a_type ty = {};
  ty.size = 4;
  ty.kind = tk_float;
  ty.variant.float_kind = fk_float;
  
  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(&ty, 0, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_FALSE(info.pass_in_memory);
  EXPECT_EQ(info.eightbyte_classes[0], x86_64_abi_class_t::sse);
}

TEST_F(ABISysVTest, IntTypeClassifiesAsInteger) {
  a_type ty = {};
  ty.size = 4;
  ty.kind = tk_integer;
  
  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(&ty, 0, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_FALSE(info.pass_in_memory);
  EXPECT_EQ(info.eightbyte_classes[0], x86_64_abi_class_t::integer);
}

TEST_F(ABISysVTest, NullTypeReturnsError) {
  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(nullptr, 0, &info);
  EXPECT_EQ(err, llvm_gen_be_error_t::invalid_argument);
}

END_EDG_NAMESPACE
BEGIN_EDG_NAMESPACE

TEST_F(ABISysVTest, StructTypeClassifiesCorrectly) {
  a_type ty = {};
  ty.size = 12;
  ty.kind = tk_struct;
  
  a_type fty1 = {};
  fty1.size = 4;
  fty1.kind = tk_integer;
  fty1.alignment = 4;

  a_type fty2 = {};
  fty2.size = 8;
  fty2.kind = tk_float;
  fty2.variant.float_kind = fk_double;
  fty2.alignment = 8;
  
  a_field field2 = {};
  field2.type = &fty2;
  field2.offset = 8;
  field2.next = nullptr;
  field2.is_bit_field = false;

  a_field field1 = {};
  field1.type = &fty1;
  field1.offset = 0;
  field1.next = &field2;
  field1.is_bit_field = false;

  ty.variant.class_struct_union.field_list = &field1;

  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(&ty, 0, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_FALSE(info.pass_in_memory);
  EXPECT_EQ(info.eightbyte_classes[0], x86_64_abi_class_t::integer);
  EXPECT_EQ(info.eightbyte_classes[1], x86_64_abi_class_t::sse);
}

TEST_F(ABISysVTest, ArrayTypeClassifiesCorrectly) {
  a_type ty = {};
  ty.size = 8;
  ty.kind = tk_array;
  ty.variant.array.variant.number_of_elements = 2;
  
  a_type elem_ty = {};
  elem_ty.size = 4;
  elem_ty.kind = tk_float;
  elem_ty.variant.float_kind = fk_float;
  
  ty.variant.array.element_type = &elem_ty;
  
  x86_64_abi_arg_info_t info;
  llvm_gen_be_error_t err = classify_sysv_argument(&ty, 0, &info);
  
  EXPECT_EQ(err, llvm_gen_be_error_t::ok);
  EXPECT_FALSE(info.pass_in_memory);
  EXPECT_EQ(info.eightbyte_classes[0], x86_64_abi_class_t::sse);
}

END_EDG_NAMESPACE
