#include "cdc_wrapper.h"
#include "usbd_cdc_if.h"
#include "fatfs.h"  // Include FatFS header for file operations

CDC_WRAPPER_t CDC_WRAPPER = {0};

static const char* supported_g_code_command[] = {
    "M100", "M101", "M102"
};

extern USBD_HandleTypeDef hUsbDeviceFS;
extern volatile bool usb_data_received;
extern volatile uint32_t rx_size;
extern volatile uint32_t* rx_buffer;
extern volatile uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

void cdc_wrapper_init(command_received_callback_t command_receive_callback)
{
    // Initialize the CDC wrapper structure
    CDC_WRAPPER.command_received = command_receive_callback;
    CDC_WRAPPER.file_transfer_state = FILE_TRANSFER_IDLE;
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}

static CDC_WRAPPER_g_code_enum_t cdc_wrapper_parse_command() {
    CDC_WRAPPER_g_code_enum_t g_code = CDC_WRAPPER_G_CODE_UNSUPPORTED;

    for (int i = 0; i < sizeof(supported_g_code_command) / sizeof(supported_g_code_command[0]); i++) {
        size_t command_len = strlen(supported_g_code_command[i]);
        if (command_len <= CDC_WRAPPER.rx_packet_size && 
            strncmp((char*)UserRxBufferFS, (char*)supported_g_code_command[i], command_len) == 0) {
            g_code = (CDC_WRAPPER_g_code_enum_t)i;
            break;
        }
    }

    if (g_code != CDC_WRAPPER_G_CODE_UNSUPPORTED) {
        char *param_str = (char*)(UserRxBufferFS + strlen(supported_g_code_command[g_code]) + 1);

        while (param_str < (char*)UserRxBufferFS + CDC_WRAPPER.rx_packet_size && *param_str != '\0') {
            char param_type = *param_str;
            param_str++;

            double param_value = atof(param_str);

            switch (param_type) {
                case 'P': CDC_WRAPPER.params.monolitic_phase = param_value; break;
                case 'T': CDC_WRAPPER.params.tetragonal_phase = param_value; break;
                case 'C': CDC_WRAPPER.params.cubic_phase = param_value; break;
                case 'Z': CDC_WRAPPER.params.zro2_concentration = param_value; break;
                case 'Y': CDC_WRAPPER.params.y2o3_concentration = param_value; break;
                case 'M': CDC_WRAPPER.params.mgo_concentration = param_value; break;
                case 'F': CDC_WRAPPER.params.ceo_concentration = param_value; break;
                case 'A': CDC_WRAPPER.params.al2o3_concentration = param_value; break;
                case 'K': CDC_WRAPPER.params.tio2_concentration = param_value; break;
                case 'H': CDC_WRAPPER.params.hfo2_concentration = param_value; break;
                case 'S': CDC_WRAPPER.params.sio2_concentration = param_value; break;
                case 'O': CDC_WRAPPER.params.additional_oxides = param_value; break;
                case 'L': CDC_WRAPPER.params.sintering_temperature = param_value; break;
                case 'G': CDC_WRAPPER.params.grain_size = param_value; break;
                case 'D': CDC_WRAPPER.params.density = param_value; break;
                default: break;
            }

            while (*param_str != ' ' && *param_str != '\0') {
                param_str++;
            }
            if (*param_str == ' ') {
                param_str++;
            }
        }
    }

    return g_code;
}

void cdc_wrapper_process(void)
{
    if (!usb_data_received) {
        return;
    }

    CDC_WRAPPER.rx_packet_size += rx_size;
    FRESULT fresult = FR_OK;

    // Check for file transfer start command
    if (strncmp((char*)UserRxBufferFS, "FILE_START:", 11) == 0) {
        CDC_WRAPPER.file_transfer_state = FILE_TRANSFER_IN_PROGRESS;
        
        // Open the file in write mode
        fresult = f_open(&CDC_WRAPPER.fp, (char*)UserRxBufferFS + 11, FA_WRITE | FA_CREATE_ALWAYS);
        if (fresult != FR_OK) {
            // Handle error opening file
            CDC_Transmit_FS((uint8_t *)"ERROR: File open failed", 23);
            CDC_WRAPPER.file_transfer_state = FILE_TRANSFER_IDLE;
        } else {
            // Respond to the sender
            CDC_Transmit_FS((uint8_t *)"FILE_START_ACK", 14);
        }
    } 
    else if (CDC_WRAPPER.file_transfer_state == FILE_TRANSFER_IN_PROGRESS) {
        // Check for file transfer end command
        if (strncmp((char*)UserRxBufferFS, "FILE_END", 8) == 0) {
            fresult = f_close(&CDC_WRAPPER.fp);
            if (fresult != FR_OK) {
                // Handle error closing file
                CDC_Transmit_FS((uint8_t *)"ERROR: File close failed", 24);
            } else {
                // Respond to the sender
                CDC_Transmit_FS((uint8_t *)"FILE_END_ACK", 12);
            }
            CDC_WRAPPER.file_transfer_state = FILE_TRANSFER_IDLE;
        } else {
            // Write the received data to the file
            UINT bytes_written = 0;


            fresult = f_write(&CDC_WRAPPER.fp, UserRxBufferFS, rx_size, &bytes_written);
            if (fresult != FR_OK || bytes_written != rx_size) {
                // Handle write error
                CDC_Transmit_FS((uint8_t *)"ERROR: File write failed", 24);
                f_close(&CDC_WRAPPER.fp);  // Attempt to close the file gracefully
                CDC_WRAPPER.file_transfer_state = FILE_TRANSFER_IDLE;
            }
        }
    } 
    else if (rx_size < 64) { // Process last packet of G-code
        CDC_WRAPPER_g_code_enum_t g_code = cdc_wrapper_parse_command();

        if (g_code != CDC_WRAPPER_G_CODE_UNSUPPORTED) {
            if (CDC_WRAPPER.command_received != NULL) {
                CDC_WRAPPER.command_received(g_code, &CDC_WRAPPER.params);
            }
        }
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &UserRxBufferFS[0]);
        CDC_WRAPPER.rx_packet_size = 0;
    } 
    else {
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, (uint8_t*)rx_buffer + rx_size);
    }
    usb_data_received = false;
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}

void cdc_wrapper_transmit(double data, CDC_WRAPPER_g_code_enum_t g_code_type)
{
    sprintf((char*)CDC_WRAPPER.transmit_buffer, "M%d R%f", g_code_type + 100, data);
    CDC_Transmit_FS(CDC_WRAPPER.transmit_buffer, strlen((char*)CDC_WRAPPER.transmit_buffer));
}
