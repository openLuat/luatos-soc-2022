
#include "mem_map.h"

/* Entry Point */
ENTRY(Reset_Handler)

/* Specify the memory areas */
MEMORY
{
  ASMB_AREA(rwx)              : ORIGIN = 0x00000000, LENGTH = 0x010000      /* 64KB */
  MSMB_AREA(rwx)              : ORIGIN = 0x00400000, LENGTH = 0x140000      /* 1.25MB */
#ifdef __LUATOS__
  FLASH_AREA(rx)              : ORIGIN = 0x00824000, LENGTH = 2212K         /* 2212K */
#else
  FLASH_AREA(rx)              : ORIGIN = 0x00824000, LENGTH = 2944K         /* 2944K */
#endif
}

/* Define output sections */
SECTIONS
{
  . = AP_FLASH_LOAD_ADDR;
  .vector :
  {
    KEEP(*(.isr_vector))
  } >FLASH_AREA
  .cache : ALIGN(128)
  {
    Image$$UNLOAD_NOCACHE$$Base = .;
    *cache.o(.text*) 
  } >FLASH_AREA
  
  .load_bootcode 0x0 :
  {
    . = ALIGN(4);
    Load$$LOAD_BOOTCODE$$Base = LOADADDR(.load_bootcode);
    Image$$LOAD_BOOTCODE$$Base = .;
    KEEP(*(.mcuVector))
    *(.ramBootCode)
    *qspi.o(.text*)
    *flash.o(.text*)
    . = ALIGN(4);
  } >ASMB_AREA AT>FLASH_AREA

    Image$$LOAD_BOOTCODE$$Length = SIZEOF(.load_bootcode);

  .load_ap_piram_asmb 0x1400 :
  {
   . = ALIGN(4);
   Load$$LOAD_AP_PIRAM_ASMB$$Base = LOADADDR(.load_ap_piram_asmb);
   Image$$LOAD_AP_PIRAM_ASMB$$Base = .;
   *(.psPARamcode)
   *(.platPARamcode)
   *memset.o(.text*)
   *memcpy-armv7m.o(.text*)
   . = ALIGN(4);
  } >ASMB_AREA AT>FLASH_AREA

  Image$$LOAD_AP_PIRAM_ASMB$$Length = SIZEOF(.load_ap_piram_asmb);

  .load_ap_firam_asmb : ALIGN(4)
  {
   . = ALIGN(4);
   Load$$LOAD_AP_FIRAM_ASMB$$Base = LOADADDR(.load_ap_firam_asmb);
   Image$$LOAD_AP_FIRAM_ASMB$$Base = .;
   *(.psFARamcode)
   *(.platFARamcode)
   . = ALIGN(4);
  } >ASMB_AREA AT>FLASH_AREA

  Image$$LOAD_AP_FIRAM_ASMB$$Length = SIZEOF(.load_ap_firam_asmb);

  .load_ap_rwdata_asmb : ALIGN(4)
  {
   . = ALIGN(4);
   Load$$LOAD_AP_FDATA_ASMB$$RW$$Base = LOADADDR(.load_ap_rwdata_asmb);
   Image$$LOAD_AP_FDATA_ASMB$$RW$$Base = .;
   *(.platFARWData)
   . = ALIGN(4);
  } >ASMB_AREA AT>FLASH_AREA
  Image$$LOAD_AP_FDATA_ASMB$$Length = SIZEOF(.load_ap_rwdata_asmb);
  
  .load_ps_rwdata_asmb : ALIGN(4)
  {
    Load$$LOAD_PS_FDATA_ASMB$$RW$$Base = LOADADDR(.load_ps_rwdata_asmb);
    Image$$LOAD_PS_FDATA_ASMB$$RW$$Base = .;
    *(.psFARWData)
    . = ALIGN(4);
  } >ASMB_AREA AT>FLASH_AREA
  Image$$LOAD_PS_FDATA_ASMB$$RW$$Length = SIZEOF(.load_ps_rwdata_asmb);
  
  .load_ap_zidata_asmb (NOLOAD):
  {
   . = ALIGN(4);
   Image$$LOAD_AP_FDATA_ASMB$$ZI$$Base = .;
   *(.platFAZIData)
   . = ALIGN(4);
   Image$$LOAD_AP_FDATA_ASMB$$ZI$$Limit = .;
   
   Image$$LOAD_PS_FDATA_ASMB$$ZI$$Base = .;
   *(.psFAZIData)
   . = ALIGN(4);
   Image$$LOAD_PS_FDATA_ASMB$$ZI$$Limit = .;
   
   *(.exceptCheck)
  } >ASMB_AREA

  .load_rrcmem 0xB000 (NOLOAD):
  {
    *(.rrcMem)
  } >ASMB_AREA

  .load_flashmem 0xC000 (NOLOAD):
  {
    *(.apFlashMem)
  } >ASMB_AREA

  .load_ap_piram_msmb MSMB_START_ADDR :
  {
    . = ALIGN(4);
    Load$$LOAD_AP_PIRAM_MSMB$$Base = LOADADDR(.load_ap_piram_msmb);
    Image$$LOAD_AP_PIRAM_MSMB$$Base = .;
    *(.psPMRamcode)
    *(.platPMRamcode)
    *(.platPMRamcodeFCLK)
    *(.recordNodeRO)
    . = ALIGN(4);
  } >MSMB_AREA AT>FLASH_AREA

  Image$$LOAD_AP_PIRAM_MSMB$$Length = SIZEOF(.load_ap_piram_msmb);

  .load_ap_firam_msmb : ALIGN(4)
  {
    . = ALIGN(4);
    Load$$LOAD_AP_FIRAM_MSMB$$Base = LOADADDR(.load_ap_firam_msmb);
    Image$$LOAD_AP_FIRAM_MSMB$$Base = .;
    *(.ramCode2)
    *(.upRamCode)
    *(.psFMRamcode)
    *(.platFMRamcode)
    . = ALIGN(4);
  } >MSMB_AREA AT>FLASH_AREA

  Image$$LOAD_AP_FIRAM_MSMB$$Length = SIZEOF(.load_ap_firam_msmb);

  .load_apos : ALIGN(4)
  {
    . = ALIGN(4);
    Load$$LOAD_APOS$$Base = LOADADDR(.load_apos);
    Image$$LOAD_APOS$$Base = .;
    *event_groups.o(.text*)
    *heap_6.o(.text*)
    *tlsf.o(.text*)
    *list.o(.text*)
    *queue.o(.text*)
    *tasks.o(.text*)
    *timers.o(.text*)
    *port.o(.text*)
    *port_asm.o(.text*)
    *cmsis_os2.o(.text*)
    . = ALIGN(4);
  } >MSMB_AREA AT>FLASH_AREA

    Image$$LOAD_APOS$$Length = SIZEOF(.load_apos);

  .load_dram_bsp : ALIGN(4)
  {
    . = ALIGN(4);
    Load$$LOAD_DRAM_BSP$$Base = LOADADDR(.load_dram_bsp);
    Image$$LOAD_DRAM_BSP$$Base = .;
    *bsp_spi.o(.data*)
    *flash.o(.data*)
    *flash_rt.o(.data*)
    *gpr.o(.data*)
    *apmu.o(.data*)
    *apmuTiming.o(.data*)
    *bsp.o(.data*)
    *plat_config.o(.data*)
    *system_ec618.o(.data*)
    *unilog.o(.data*)
    *pad.o(.data*)
    *ic.o(.data*)
    *ec_main.o(.data*)
    *slpman.o(.data*)
    *bsp_usart.o(.data*)
    *bsp_lpusart.o(.data*)
    *timer.o(.data*)
    *dma.o(.data*)
    *adc.o(.data*)
    *wdt.o(.data*)
    *usb_device.o(.data*)
    *uart_device.o(.data*)
    *clock.o(.data*)
    *hal_adc.o(.data*)
    *hal_adcproxy.o(.data*)
    *hal_alarm.o(.data*)
    *exception_process.o(.data*)
    *exception_dump.o(.data*)
    . = ALIGN(4);
  } >MSMB_AREA AT>FLASH_AREA

    Image$$LOAD_DRAM_BSP$$Length = SIZEOF(.load_dram_bsp);

  .load_dram_bsp_zi (NOLOAD):
  {
    . = ALIGN(4);
    Image$$LOAD_DRAM_BSP$$ZI$$Base = .;
    *bsp_spi.o(.bss*)
    *flash.o(.bss*)
    *flash_rt.o(.bss*)
    *gpr.o(.bss*)
    *apmu.o(.bss*)
    *apmuTiming.o(.bss*)
    *bsp.o(.bss*)
    *plat_config.o(.bss*)
    *system_ec618.o(.bss*)
    *unilog.o(.bss*)
    *pad.o(.bss*)
    *ic.o(.bss*)
    *ec_main.o(.bss*)
    *slpman.o(.bss*)
    *bsp_usart.o(.bss*)
    *bsp_lpusart.o(.bss*)
    *timer.o(.bss*)
    *dma.o(.bss*)
    *adc.o(.bss*)
    *wdt.o(.bss*)
    *usb_device.o(.bss*)
    *uart_device.o(.bss*)
    *clock.o(.bss*)
    *hal_adc.o(.bss*)
    *hal_trim.o(.bss*)
    *hal_adcproxy.o(.bss*)
    *hal_alarm.o(.bss*)
    *exception_process.o(.bss*)
    *exception_dump.o(.bss*)
    *(.recordNodeZI)
    . = ALIGN(4);
  Image$$LOAD_DRAM_BSP$$ZI$$Limit = .;
  } >MSMB_AREA

  .unload_slpmem (NOLOAD):
  {
    *(.sleepmem)
  } >MSMB_AREA

  .load_dram_shared : ALIGN(4)
  {
    . = ALIGN(4);
    Load$$LOAD_DRAM_SHARED$$Base = LOADADDR(.load_dram_shared);
    Image$$LOAD_DRAM_SHARED$$Base = .;
    *(.data*)
    . = ALIGN(4);
  } >MSMB_AREA AT>FLASH_AREA

  Image$$LOAD_DRAM_SHARED$$Length = SIZEOF(.load_dram_shared);

  .load_dram_shared_zi (NOLOAD):
  {
    . = ALIGN(4);
    Image$$LOAD_DRAM_SHARED$$ZI$$Base = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    *(.stack)               /* stack should be 4 byte align */
    Image$$LOAD_DRAM_SHARED$$ZI$$Limit = .;
    *(.USB_NOINIT_DATA_BUF)
  } >MSMB_AREA


  PROVIDE(end_ap_data = . );
  PROVIDE(start_up_buffer = up_buf_start);
  .load_up_buffer start_up_buffer(NOLOAD):
  {
    *(.catShareBuf)
    Image$$LOAD_UP_BUFFER$$Limit = .;
  } >MSMB_AREA

  PROVIDE(end_up_buffer = . );
  heap_size = start_up_buffer - end_ap_data;
  ASSERT(heap_size>=min_heap_size_threshold,"ap use too much ram, heap less than min_heap_size_threshold!")
  ASSERT(end_up_buffer<=MSMB_APMEM_END_ADDR,"ap use too much ram, exceed to MSMB_APMEM_END_ADDR")

  .text :
  {
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.vfpll_veneer)
    *(.v4_bx)
    *(.init*)
    *(.fini*)
    *(.iplt)
    *(.igot.plt)
    *(.rel.iplt)
  } >FLASH_AREA

  .preinit_fun_array :
  {
      . = ALIGN(4);
      __preinit_fun_array_start = .;
      KEEP (*(SORT(.preinit_fun_array.*)))
      KEEP (*(.preinit_fun_array*))
      __preinit_fun_array_end = .;
      . = ALIGN(4);
  } > FLASH_AREA   
  .init_fun_array :
  {
      . = ALIGN(4);
      __init_fun_array_start = .;
      KEEP (*(SORT(.init_fun_array.*)))
      KEEP (*(.init_fun_array*))
      __init_fun_array_end = .;
      . = ALIGN(4);
  } > FLASH_AREA
  
  .task_fun_array :
  {
      . = ALIGN(4);
      __task_fun_array_start = .;
      KEEP (*(SORT(.task_fun_array.*)))
      KEEP (*(.task_fun_array*))
      __task_fun_array_end = .;
      . = ALIGN(4);
  } > FLASH_AREA

  PROVIDE(totalFlashLimit = .);

  .unload_cpaon CP_AONMEMBACKUP_START_ADDR (NOLOAD):
  {

  } >MSMB_AREA

  .load_xp_sharedinfo XP_SHAREINFO_BASE_ADDR (NOLOAD):
  {
  *(.shareInfo)
  } >MSMB_AREA
  
  .load_dbg_area XP_DBGRESERVED_BASE_ADDR (NOLOAD):
  {
  *(.resetFlag)
  } >MSMB_AREA
  
  .unload_xp_ipcmem IPC_SHAREDMEM_START_ADDR (NOLOAD):
  {

  } >MSMB_AREA

}

GROUP(
    libgcc.a
    libc.a
    libm.a
 )