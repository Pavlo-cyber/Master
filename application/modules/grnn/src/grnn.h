#ifndef GRNN_H
#define GRNN_H

#include "stdint.h"

typedef enum
{
  GRNN_TEST_TYPE_HARDNESS = 0,
  GRNN_TEST_TYPE_FRACTURE = 1,
  GRNN_TEST_TYPE_STRENGTH = 2,
} GRNN_test_type_enum_t;

typedef enum
{
  GRNN_RESULT_ENUM_OK = 0,
  GRNN_RESULT_ENUM_NOK = 1, 
  GRNN_RESULT_ENUM_FS_ERROR = 2,
  GRNN_RESULT_ENUM_CSV_PARSE_ERROR = 3,
  GRNN_RESULT_MEMORY_ALLOCATION_ERROR = 4,
} GRNN_result_enum_t;

void grnn_init(float sigma1, float sigma2);

GRNN_result_enum_t grnn_train(char const* train_filename);

void grnn_test(GRNN_test_type_enum_t test_type, double* test_vector, uint32_t test_vector_size);

#endif