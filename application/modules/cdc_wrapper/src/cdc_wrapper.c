#include "cdc_wrapper.h"
#include "usbd_cdc_if.h"

// send g-code command
// receive g-code command
// 
CDC_WRAPPER_t CDC_WRAPPER = {0};

static const char* supported_g_code_command[] = {
  "M100", "M101", "M102"
};

extern USBD_HandleTypeDef hUsbDeviceFS;
extern volatile bool usb_data_received;
extern volatile uint32_t rx_size;
extern volatile uint32_t* rx_buffer;
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];


void cdc_wrapper_init(command_received_callback_t command_receive_callback)
{
  // get callback to call on packet receive
  // get callback to call on file transfer
  CDC_WRAPPER.command_received = command_receive_callback;

  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}


// Function to parse G-code command and return the corresponding enum
static CDC_WRAPPER_g_code_enum_t cdc_wrapper_parse_command() {
    // Check which G-code command is received
    CDC_WRAPPER_g_code_enum_t g_code = CDC_WRAPPER_G_CODE_UNSUPPORTED;

    // Limit the loop with rx_buffer_size to prevent iterating beyond the buffer
    for (int i = 0; i < sizeof(supported_g_code_command) / sizeof(supported_g_code_command[0]); i++) {
        size_t command_len = strlen(supported_g_code_command[i]);
        if (command_len <= CDC_WRAPPER.rx_packet_size && 
            strncmp((char*)UserRxBufferFS, (char*)supported_g_code_command[i], command_len) == 0) {
            g_code = (CDC_WRAPPER_g_code_enum_t)i;
            break;
        }
    }

    // If the command is supported, parse its parameters
    if (g_code != CDC_WRAPPER_G_CODE_UNSUPPORTED) 
    {
        // The command string starts with "M100", "M101", or "M102", so skip past that
        char *param_str = (char*)(UserRxBufferFS + strlen(supported_g_code_command[g_code]) + 1);

        while (param_str < (char*)UserRxBufferFS + CDC_WRAPPER.rx_packet_size && *param_str != '\0') 
        {
            char param_type = *param_str;
            param_str++;  // Move to the value part

            double param_value = atof(param_str);  // Convert the string to a float

            // Process or store each parameter value based on the type
            switch (param_type) {
                case 'T':
                    CDC_WRAPPER.params.tetragonal_phase = param_value;
                    break;
                case 'C':
                    CDC_WRAPPER.params.cubic_phase = param_value;
                    break;
                case 'Z':
                    CDC_WRAPPER.params.zro2_concentration = param_value;
                    break;
                case 'Y':
                    CDC_WRAPPER.params.y2o3_concentration = param_value;
                    break;
                case 'M':
                    CDC_WRAPPER.params.mgo_concentration = param_value;
                    break;
                case 'F':
                    CDC_WRAPPER.params.ceo_concentration = param_value;
                    break;
                case 'A':
                    CDC_WRAPPER.params.al2o3_concentration = param_value;
                    break;
                case 'K':
                    CDC_WRAPPER.params.tio2_concentration = param_value;
                    break;
                case 'H':
                    CDC_WRAPPER.params.hfo2_concentration = param_value;
                    break;
                case 'S':
                    CDC_WRAPPER.params.sio2_concentration = param_value;
                    break;
                case 'O':
                    CDC_WRAPPER.params.additional_oxides = param_value;
                    break;
                case 'L':
                    CDC_WRAPPER.params.sintering_temperature = param_value;
                    break;
                case 'G':
                    CDC_WRAPPER.params.grain_size = param_value;
                    break;
                case 'D':
                    CDC_WRAPPER.params.density = param_value;
                    break;
                default:
                    // Handle unknown parameters if necessary
                    break;
            }

            // Move to the next parameter
            while (*param_str != ' ' && *param_str != '\0') 
            {
                param_str++;  // Skip over the number part to the next parameter
            }
            if (*param_str == ' ') 
            {
                param_str++;  // Skip the space
            }
        }
    }

    return g_code;
}

void cdc_wrapper_process(void)
{
  if(usb_data_received != true)
  {
    return;
  }

  CDC_WRAPPER.rx_packet_size += rx_size;
  
  // last packet
  if(rx_size < 64)
  {
    // parse received command 
    CDC_WRAPPER_g_code_enum_t g_code = cdc_wrapper_parse_command();

    // in case we support this command call the callback which will process it
    if (g_code != CDC_WRAPPER_G_CODE_UNSUPPORTED)
    {
      if(CDC_WRAPPER.command_received != NULL)
      {
        CDC_WRAPPER.command_received(g_code, &CDC_WRAPPER.params);
      }
    }
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &UserRxBufferFS[0]);
    CDC_WRAPPER.rx_packet_size = 0;
  }
  else
  {
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx_buffer + rx_size);
  }
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}


// transfer g-code command which is currently supported
// M-100, M-101, M-102
void cdc_wrapper_transmit(double data, CDC_WRAPPER_g_code_enum_t g_code_type)
{
  sprintf((char*)CDC_WRAPPER.transmit_buffer, "M%d R%f", g_code_type + 100, data);
  CDC_Transmit_FS(CDC_WRAPPER.transmit_buffer, strlen((char*)CDC_WRAPPER.transmit_buffer));
}
