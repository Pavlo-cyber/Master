#Specify project name, language
project(
    'ML_LD', 
    'c', 
    default_options : ['b_colorout=always'], 
    )

#Set compiler parameters  
c_args = [
    '-mcpu=cortex-m7',
    '-g',
    '-mthumb',
    '-Wall',
    '-Werror',
    '-O2',
    '-DDEBUG',
    '-DUSE_HAL_DRIVER',
    '-DUSE_FULL_LL_DRIVER',
    '-DSTM32H743xx',
]

# Target header include list
target_include_dir = [
    'application/hardware/cmsis', 
    'application/hardware/hal', 
    'application/hardware/ll',
    'application/modules/USB_DEVICE/App',
    'application/modules/USB_DEVICE/Target',
    'application/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc',
    'application/Middlewares/ST/STM32_USB_Device_Library/Core/Inc']
# Basic source files from CMSIS, HAL, and LL libraries
stm32h7_cmsis_source_files = ['']
stm32h7_hal_source_files = [
    'application/hardware/hal/stm32h7xx_hal.c',
    'application/hardware/hal/stm32h7xx_hal_gpio.c',
    'application/hardware/hal/stm32h7xx_hal_cortex.c',
    'application/hardware/hal/stm32h7xx_hal_rcc.c',
    'application/hardware/hal/stm32h7xx_hal_rcc_ex.c',
    'application/hardware/hal/stm32h7xx_hal_pcd.c',
    'application/hardware/hal/stm32h7xx_hal_pcd_ex.c',
    'application/hardware/hal/stm32h7xx_hal_pwr_ex.c',
     'application/hardware/hal/stm32h7xx_hal_pwr.c',
    ]
stm32h7_ll_source_files = [
    'application/hardware/ll/stm32h7xx_ll_utils.c',
    'application/hardware/ll/stm32h7xx_ll_rcc.c',
    'application/hardware/hal/stm32h7xx_ll_usb.c',
    ]

stm32h7_extra_files = [
    'stm32h7xx_hal_msp.c',
    'stm32h7xx_it.c',
]

usb_device_source_file = [
    'application/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c',
    'application/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c',
    'application/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c',
    'application/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c',
    'application/modules/USB_DEVICE/App/usbd_cdc_if.c',
    'application/modules/USB_DEVICE/App/usb_device.c',
    'application/modules/USB_DEVICE/App/usbd_desc.c',
    'application/modules/USB_DEVICE/Target/usbd_conf.c',
]

project_sources = []

ldscript = join_paths(meson.source_root(), 'STM32H743ZITX_FLASH.ld')

startup_file = files('application/Startup/startup_stm32h743zitx.s')

system_files = files('application/hardware/cmsis/syscalls.c', 'application/hardware/cmsis/sysmem.c', 'application/hardware/cmsis/system_stm32h7xx.c')

link_args = [
    '-T' + ldscript,
    '-mcpu=cortex-m7',
    '-g',
    '-mthumb',
    '-Wall',
    '-Werror',
    '-O2',
    '-DDEBUG',
    '-DUSE_HAL_DRIVER',
    '-DUSE_FULL_LL_DRIVER',
    '-DSTM32H743xx',
    '-Wl,-Map=test.map', 
    '-Wl,--gc-sections', 
    ]

add_project_arguments(c_args, language : 'c')

project_sources += startup_file
project_sources += system_files
# project_sources += stm32h7_cmsis_source_files
project_sources += stm32h7_hal_source_files
project_sources +=stm32h7_ll_source_files
project_sources +=stm32h7_extra_files
project_sources += usb_device_source_file

# Module list and its boolean flags to include/exclude from build
module_list = {
    'main_module'   : true,
}

path_to_modules = 'application/modules/'
# Loop through each module and add its meson.build file
foreach module: module_list.keys()
    if module_list[module]
        subdir(path_to_modules + module)
    endif
endforeach

# Main executable
executable(
    'ML_LD.elf',
    sources: [
        project_sources
    ],
    link_depends : startup_file,
    include_directories: target_include_dir,
    c_args: c_args,

    link_args: link_args,
)