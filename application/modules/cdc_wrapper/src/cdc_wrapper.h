#ifndef CDC_WRAPPER_H
#define CDC_WRAPPER_H

#include "stdint.h"
#include "stdbool.h"

#define CDC_WRAPPER_TRANSMIT_BUFFER_SIZE (256u)

typedef enum
{
  CDC_WRAPPER_G_CODE_UNSUPPORTED = -1,
  CDC_WRAPPER_G_CODE_M100 = 0,
  CDC_WRAPPER_G_CODE_M101 = 1,
  CDC_WRAPPER_G_CODE_M102 = 2,
  CDC_WRAPPER_G_CODE_M_103 = 3, /// g_code used on prototype state to start datasets transfer
  CDC_WRAPPER_G_CODE_M_104 = 4, /// g_code used on prototype state to end datasets transfer
} CDC_WRAPPER_g_code_enum_t;



typedef struct 
{
  double tetragonal_phase;        //< Tetragonal phase, %.
  double cubic_phase;             //< Cubic phase, %.
  double zro2_concentration;     //< ZrO2, mol%.
  double y2o3_concentration;     //< Y2O3, mol%.
  double mgo_concentration;      //< MgO, mol%.
  double ceo_concentration;       //< CeO, mol%.
  double al2o3_concentration;     //< Al2O3, mol%.
  double tio2_concentration;      //< TiO2, mol%.
  double hfo2_concentration;      //< HfO2, mol%.
  double sio2_concentration;      //< SiO2, mol%.
  double additional_oxides;       // Additional oxides, mol%.
  double sintering_temperature;   // Sintering temperature, °C.
  double grain_size;
  double density;
// •	ST<value>: 
// •	G<value>: Grain size, µm.
// •	D<value>: Density, g/cm³.
} CDC_WRAPPER_g_code_parameters_t;

typedef void (*command_received_callback_t)(CDC_WRAPPER_g_code_enum_t g_code, CDC_WRAPPER_g_code_parameters_t* params);

typedef struct 
{
  uint8_t transmit_buffer[CDC_WRAPPER_TRANSMIT_BUFFER_SIZE];
  command_received_callback_t command_received;
  CDC_WRAPPER_g_code_parameters_t params;
  uint32_t rx_packet_size;
} CDC_WRAPPER_t;


void cdc_wrapper_init(command_received_callback_t command_receive_callback);


void cdc_wrapper_process(void);


void cdc_wrapper_transmit(double data, CDC_WRAPPER_g_code_enum_t g_code_type);


#endif