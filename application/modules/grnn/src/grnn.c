#include "grnn.h"

#include "csv.h"
#include "hma.h"
#include "stm32h7xx_hal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Define constants
#define MAX_ROWS (152)
#define MAX_COLS (20)

#define FILE_BUFFER_SIZE (512 * 20)
#define CONCATENATED_FILE_NAME_SIZE (50)

// Structure to represent a data frame
typedef struct {
    double data[MAX_ROWS][MAX_COLS];        //< data from dataset
    bool is_header;                         //< flag to indicate if dataframe have header

    uint16_t rows;                          //< row number, used to properly write data to structure on csv callbacks
    uint16_t cols;                          //< col number, used to properly write data to structure on csv callbacks
} DataFrame; 

typedef struct
{
  FATFS SDFatFs;
  uint32_t file_buffer[FILE_BUFFER_SIZE];

  struct csv_parser parser;
  DataFrame train_dataframe;

  uint16_t dataframe_col_number;              //< number of cols in train_dataframe structure
  uint16_t dataframe_row_number;              //< number of rows in train dataframe strcture

  double y_train[MAX_ROWS];
  double y_train_predicted[MAX_ROWS];

  char x_augmented_filename[CONCATENATED_FILE_NAME_SIZE];
  char y_augmented_filename[CONCATENATED_FILE_NAME_SIZE];

  float sigma1;
  float sigma2;
} GRNN_t;

static GRNN_t GRNN = {0};

void grnn_init(float sigma1, float sigma2)
{
  csv_init(&GRNN.parser, 0);

  // use custom heap allocation function
  GRNN.parser.free_func = vPortFree;
  GRNN.parser.realloc_func = pvPortRealloc;

  GRNN.sigma1 = sigma1;
  GRNN.sigma2 = sigma2;

  // curently used dataset with header 
  GRNN.train_dataframe.is_header = true;
}

float root_mean_squared_error(float *y_true, float *y_pred, int size) 
{
    float sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += pow(y_pred[i] - y_true[i], 2);
    }
    return sqrt(sum / size);
}

// Function to calculate the mean absolute percentage error
float mean_absolute_percentage_error(float *y_true, float *y_pred, int size) {
    float sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += fabs((y_true[i] - y_pred[i]) / y_true[i]);
    }
    return (sum / size) * 100.0;
}


// callback called on each csv filed
static void csv_field_callback (void *s, size_t i, void *user_data) 
{
  char* temp_string = pvPortMalloc(i + 1);
  
  if(temp_string == NULL)
  {
    assert_param();
  }
  else
  {
    //skip first row(first row contain lables)
    if(GRNN.train_dataframe.is_header == false)
    {
      memcpy(temp_string, s, i);
      temp_string[i] = '\0';
      GRNN.train_dataframe.data[GRNN.train_dataframe.rows][GRNN.train_dataframe.cols] = strtof(temp_string, NULL);

      GRNN.train_dataframe.cols += 1;
    }
  }

  vPortFree(temp_string);
}

// callback called on each csv row
static void csv_row_callback (int c, void *user_data) {
  // variable to memorize finale dataframe size
  if(GRNN.train_dataframe.is_header == true)
  {
    GRNN.train_dataframe.is_header = false;
  }
  else
  {
    GRNN.dataframe_col_number = GRNN.train_dataframe.cols;
    GRNN.dataframe_row_number += 1;

    // varable to read
    GRNN.train_dataframe.rows += 1;
    GRNN.train_dataframe.cols = 0;
  }
}

static GRNN_result_enum_t grnn_load_data(const char* train_filename)
{
  FRESULT fresult = FR_OK;
  GRNN_result_enum_t result_enum = GRNN_RESULT_ENUM_OK;
  FIL fp;
  
  fresult =  f_mount(&GRNN.SDFatFs, "", 1);
  if (fresult == FR_OK)
  {
    fresult = f_open(&fp, train_filename, FA_READ | FA_WRITE);
  }

  uint32_t read_byte = 0;

  while ((fresult = f_read(&fp, GRNN.file_buffer, FILE_BUFFER_SIZE,(void*) &read_byte)) == FR_OK) 
  {
    if (csv_parse(&GRNN.parser, GRNN.file_buffer, read_byte, csv_field_callback, csv_row_callback, NULL) != read_byte) 
    {
      printf("Error parsing file: %s\n", csv_strerror(csv_error(&GRNN.parser)));
      result_enum = GRNN_RESULT_ENUM_CSV_PARSE_ERROR;
    }
    // file end
    if(read_byte < FILE_BUFFER_SIZE)
    {
      break;
    }
  }

  csv_fini(&GRNN.parser, csv_field_callback, csv_row_callback, NULL);
  csv_free(&GRNN.parser);

 if(fresult ==  FR_OK)
 {
  result_enum = f_close(&fp);
 }

 if (fresult != FR_OK)
 {
  result_enum = GRNN_RESULT_ENUM_FS_ERROR;
 }

 return result_enum;
}

static double grnn_predict(double *instance_X, DataFrame* train_X, uint32_t row, uint32_t col, double *train_y, float sigma) 
{
  double result_sum = 0.0;
  double gausian_distances_sum = 0.0;

  for (int i = 0; i < row; i++) 
  {
    double distance = 0.0;

    for (int k = 0; k < col; k++) 
    {
      distance += pow(instance_X[k] - train_X->data[i][k], 2);
    }

    double gausian_distance = exp(-distance / (2 * pow(sigma, 2)));
    gausian_distances_sum += gausian_distance;
    result_sum += gausian_distance * train_y[i];
  }

  if (gausian_distances_sum < pow(10, -7)) 
  {
      gausian_distances_sum = pow(10, -7);
  }

  double result = result_sum / gausian_distances_sum;
  return result;
}

static double calculate_distance(double *instance_X, double *train_X, int col, float sigma) {
    double distance = 0.0;
    for (int k = 0; k < col; k++) {
        distance += pow(instance_X[k] - train_X[k], 2);
    }
    return exp(-distance / (2 * pow(sigma, 2)));
}

static double grnn_predict_concatenated(double *instance_X, char* train_x_filename, char* train_y_filename, uint32_t row, uint32_t col, float sigma)
{
  double result_sum = 0.0;
  double gaussian_distances_sum = 0.0;

  FIL fp1;
  FIL fp2;

  if(f_open(&fp1, train_x_filename, FA_READ) != FR_OK)
  {
    // handelr error
    assert_param(true);
  }

  if(f_open(&fp2, train_y_filename, FA_READ) != FR_OK)
  {
    // handle error
    assert_param(true);
  }

  double train_X_row[col];
  double train_y;

  for (int i = 0; i < row; i++) 
  {
    // Read a row of train_X from file
    uint32_t bytes_readed = 0;

    if (f_read(&fp1, (void*)train_X_row, sizeof(double) * col, (void*)&bytes_readed) != FR_OK) 
    {
        printf("Error reading train_X from file.\n");
        // Error indicator
    }

    // Read the corresponding train_y value
    if (f_read(&fp2, (void*)&train_y, sizeof(double), (void*)&bytes_readed) != FR_OK) 
    {
      printf("Error reading train_y from file.\n");
      // Error indicator
    }

    // Calculate Gaussian distance
    double distance = calculate_distance(instance_X, train_X_row, col, sigma);
    gaussian_distances_sum += distance;
    result_sum += distance * train_y;
  }

    if (gaussian_distances_sum < pow(10, -7)) 
    {
        gaussian_distances_sum = pow(10, -7);
    }

  return result_sum / gaussian_distances_sum;
}

double grnn_test(double* test_vector, uint32_t test_vector_size)
{
  //predict value for test_vector, use predict function

  static DataFrame df;
  // if(df == NULL)
  // {
  //   // handler error
  // }

  memcpy(df.data[0], test_vector, test_vector_size);

  double y_pred = grnn_predict(test_vector, &df, 1, test_vector_size - 1, &test_vector[test_vector_size - 1], GRNN.sigma2);

  // vPortFree(df);

  // currently used only one test vector
  double z_augmented[1][MAX_ROWS];

  double instance_x[GRNN.dataframe_col_number * 2 + 2];

  double z_aumented_sum = 0;

  for(int i = 0; i < 1; i++)
  {
    for(int j = 0; j < GRNN.dataframe_row_number; j++)
    {
      // create instance x concatenating test vector, train vecotr and they prediction value
      memcpy(instance_x, test_vector, test_vector_size * sizeof(double));
      memcpy(instance_x + test_vector_size, GRNN.train_dataframe.data[j], test_vector_size * sizeof(double));
      memcpy(instance_x + (test_vector_size) * 2, &y_pred, sizeof(double));
      memcpy(instance_x + ((test_vector_size) * 2 + 1),  &GRNN.y_train_predicted[j], sizeof(double));

      z_augmented[i][j] = grnn_predict_concatenated(instance_x, GRNN.x_augmented_filename, GRNN.y_augmented_filename, pow(GRNN.dataframe_row_number, 2), GRNN.dataframe_col_number * 2, GRNN.sigma2);
      
      z_aumented_sum += z_augmented[i][j];
    }
  }

  double y_prediction_sum = 0;
  for(int i = 0; i < GRNN.dataframe_row_number; i++)
  {
    y_prediction_sum += GRNN.y_train_predicted[i];
  }

  double final_y_predction = z_aumented_sum + y_prediction_sum;

  return final_y_predction;
}

static GRNN_result_enum_t grnn_concatenate_data(char* train_x_filename,char* train_z_filename, DataFrame* train_dataframe, uint32_t col, uint32_t row, double* y_predict)
{
  GRNN_result_enum_t result_enum = GRNN_RESULT_ENUM_OK;
  FRESULT fresult = FR_OK;
  FIL fp1, fp2;

  uint32_t byte_written = 0;

  if((f_open(&fp1, train_x_filename,  FA_READ | FA_WRITE) == FR_OK) && (f_open(&fp2, train_z_filename,  FA_READ | FA_WRITE) == FR_OK))
  {
    // files already exist
  }
  else if((f_open(&fp1, train_x_filename,  FA_READ | FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) && (f_open(&fp2, train_z_filename,  FA_READ | FA_WRITE | FA_OPEN_ALWAYS) == FR_OK))
  {
    //file create succesfully
    for (int i = 0; i < row; i++) 
    {
    // Concatenate with itself
    double* concatenated_data[MAX_COLS * 2];
    double prediction[2];
    // Concatenate with every other vector
    for (int j = 0; j < row; j++) 
    {
      // Skip if it's the same vector as the one we concatenated with itself
      memcpy(concatenated_data, train_dataframe->data[i], col * sizeof(double));
      memcpy(concatenated_data + col, train_dataframe->data[j], col * sizeof(double));

      // Write concatenated data to the file
      if(f_write(&fp1, concatenated_data, col * 2 * sizeof(double), (void*)&byte_written) != FR_OK)
      {
        return GRNN_RESULT_ENUM_NOK;
      }

      // Write corresponding y and y_predict values
      prediction[0] = y_predict[i];
      prediction[1] = y_predict[j];
      if(f_write(&fp1, prediction, 2 * sizeof(double), (void*)&byte_written) != FR_OK)
      {
        return GRNN_RESULT_ENUM_NOK;
      }

      double z = prediction[0] - prediction[1];
      if((fresult = f_write(&fp2, (void*)&z, sizeof(double), (void*)&byte_written)) != FR_OK)
      {
        return GRNN_RESULT_ENUM_NOK;
      }
    }
  }
  }
  else
  {
    result_enum = GRNN_RESULT_ENUM_FS_ERROR;
  }

  f_close(&fp1);
  f_close(&fp2);

  return result_enum;
}

static void removeExtension(char* filename) {
    // Find the position of the last occurrence of '.'
    char *dot = strrchr(filename, '.');
    
    // If '.' found, truncate the string at that position
    if (dot != NULL) 
    {
        *dot = '\0';
    }
  printf("%s", filename);
}


GRNN_result_enum_t grnn_train(const char* train_filename)
{
  GRNN_result_enum_t result_enum = GRNN_RESULT_ENUM_OK;

  if(grnn_load_data(train_filename) != GRNN_RESULT_ENUM_OK)
  {
    result_enum = GRNN_RESULT_ENUM_NOK;
  }

  for (int i = 0; i < GRNN.dataframe_row_number; i++)
  {
    // take last value from each vector it is y value
    GRNN.y_train[i] = GRNN.train_dataframe.data[i][GRNN.dataframe_col_number - 1];
  }

  for(int i = 0; i < GRNN.dataframe_row_number; i++)
  {
    // -1 because dataframe dataset containe y vlaue as last column
    GRNN.y_train_predicted[i] = grnn_predict(GRNN.train_dataframe.data[i], &GRNN.train_dataframe, GRNN.dataframe_row_number, GRNN.dataframe_col_number - 1, GRNN.y_train, GRNN.sigma1);
  }

  //concatenate each vector with each other vector for train_dataset also add to dataset y_prediction value for both concatenated vectors
  //i.e if vector dataframe[i] and dataframe[j] was concatenated result should containe dataframe[i] value, dataframe[j] value, and prediction value for both of them
  // all data should be load from file and store to file, to reduce ram used, to maximum at the same amount of time in ram there shouldnt be more than 150 vectors
  // result file should be named train_filename_concatenated.bin
  
  // create one more file which will containe vector with value of difference between prediction of 2 value,
  // i.e if for each concatenated vector before dataframe[i] and dataframe[j] there should be value predicted_y dataframe[i] - predicted_y dataframe[j]
  // store in file train_filename_y.bin
  
  // remove csv extension from file

  char filename_temp[30];
  strcpy(filename_temp, train_filename);

  removeExtension((char*)filename_temp);

  sprintf(GRNN.x_augmented_filename, "%s%s", train_filename, "_concatenated_x.bin");

  sprintf(GRNN.y_augmented_filename, "%s%s", train_filename, "_concatenated_y.bin");

  if(grnn_concatenate_data(GRNN.x_augmented_filename, GRNN.y_augmented_filename,  &GRNN.train_dataframe, GRNN.dataframe_col_number, GRNN.dataframe_row_number, GRNN.y_train_predicted) != GRNN_RESULT_ENUM_OK)
  {
    result_enum = GRNN_RESULT_ENUM_NOK;
  }

  return result_enum;
}