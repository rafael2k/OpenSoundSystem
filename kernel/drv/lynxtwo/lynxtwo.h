#define COPYING14 Copyright (C) Hannu Savolainen and Dev Mazumdar 2000-2006. All rights reserved.

#define LYNX_VENDOR_ID  0x1621
#define LYNX_LYNXTWO_A  0x0020
#define LYNX_LYNXTWO_B  0x0021
#define LYNX_LYNXTWO_C  0x0022
#define LYNX_LYNX_L22   0x0023
#define LYNX_AES16      0x0024
#define LYNX_AES16_SRC  0x0025
#define LYNX_AES16e     0x0028
#define LYNX_AES16e_SRC  0x0029

/* BAR*_SIZE taken from LynxTWO.h */
#define BAR0_SIZE       2048
#define BAR0_SIZE_AES   4096
#define BAR0_SIZE_AESe  16384
#define BAR1_SIZE       262144

#define MAX_INPUTS	8
#define MAX_OUTPUTS	8


extern int glSampleRates[14];
#define NUM_SAMPLE_RATES    (sizeof( glSampleRates ) / sizeof( int ))

typedef struct
{
  oss_device_t *osdev;
  oss_uint64_t io_base;
  oss_mutex_t mutex;
  
  void *m_pHalAdapter;
  void *m_pHalOSS;

  oss_uint64_t bar0virt, bar1virt;
  oss_uint64_t bar0addr, bar1addr;
  int vendorID, deviceID;
  char card_name[64];
  int card_number;
  int model;
#define MDL_LYNXTWO_A		1
#define MDL_LYNXTWO_B		2
#define MDL_LYNXTWO_C		3
#define MDL_L22			4
#define MDL_AES16		5
#define MDL_AES16_SRC		6
  int bar0size;

  /*
   * Mixer
   */
  int adapter_mixer_dev;
  int input_mixer_dev;
  int output_mixer_dev;
  int *levels;

  /*
   * Audio 
   */
  int num_inputs, num_outputs;
  void *input_portc[MAX_INPUTS];
  int input_devs[MAX_INPUTS];	/* Audio device numbers for inputs */
  void *output_portc[MAX_OUTPUTS];
  int first_dev;
  int ratelock;
  int rateselect;

  /*
   * Timeout for polling ChalAdapter::Update();
   */
  timeout_id_t timeout_id;
  int firstdev;			/* First audio device for this instance */
} lynxtwo_devc;
