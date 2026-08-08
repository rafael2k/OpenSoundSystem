/*
 * OssAudio.cpp - LynxTWO Audio code
 *
 * Copyright (C) Hannu Savolainen and Dev Mazumdar 2003
 */

#include <StdAfx.h>
#include "HalAdapter.h"
#include "HalDevice.h"
#include "LynxTWO.h"
#include "resource.h"

extern "C"
{
#include "lynxtwo_cfg.h"
#include "lynxtwo.h"
#include "HalOSS.h"

#ifdef USE_LICENSING
#include "../private/licensing/licensing.h"
#endif
}

int glSampleRates[NUM_SAMPLE_RATES] =
  { 8000, 11025, 16000, 22050, 32000, 44100, 48000,
  64000, 88200, 96000, 128000, 176400, 192000, 200000
};

#define FMT_MASK (AFMT_S32_NE|AFMT_S16_NE|AFMT_S8)
EXTERN_C static void lynxtwo_reset (int dev);

/***********************************
 * Audio routines 
 ***********************************/

class lynxtwo_portc
{
public:
  dmap_p m_dmap;
  adev_p m_adev;

    lynxtwo_portc (int n, int dev, lynxtwo_devc * devc);
   ~lynxtwo_portc (void);

  int Open (int mode, int open_fals);
  void Close (int mode);
  int SetSpeed (int arg);
  int SetFormat (int arg);
  int SetChannels (int arg);
  void SetTrigger (int state);
  int SyncStartPrepare (void);
  int SyncStartTrigger (void);
  int Ioctl (unsigned int cmd, ioctl_arg arg);

  int OutputBlock (oss_native_word ptr, int count, int fragsize,
		   int intrflag);
  int StartInput (oss_native_word ptr, int count, int fragsize, int intrflag);

  int PrepareForInput (int bsize, int bcount);
  int PrepareForOutput (int bsize, int bcount);
  int device_mode;
  int m_audio_dev;
protected:
  int bits, channels, speed;
  int fmtbytes, fmtbits;
  int open_mode;
  lynxtwo_devc *m_devc;

  int m_nDevice;
  int m_State;
  int m_Prepared;

  PHALADAPTER m_pHalAdapter;
  PHALWAVEDMADEVICE m_pHD;
  PHALDMA m_pHDMA;

private:
    friend USHORT InterruptCallback (ULONG ulReason, PVOID pvContext1,
				     PVOID pvContext2);
  USHORT ServiceInterrupt (ULONG ulReason, PVOID pvContext);

};

lynxtwo_portc::lynxtwo_portc (int n, int dev, lynxtwo_devc * devc)
{
  m_devc = devc;

  m_pHalAdapter = (PHALADAPTER) devc->m_pHalAdapter;
  m_pHD = NULL;

  m_audio_dev = dev;
  open_mode = 0;
  speed = 48000;
  channels = 2;
  fmtbytes = 2;
  fmtbits = 16;
  bits = AFMT_S16_NE;

  m_nDevice = n;
  m_State = MODE_STOP;
}

lynxtwo_portc::~lynxtwo_portc (void)
{
}

USHORT lynxtwo_portc::ServiceInterrupt (ULONG ulReason, PVOID pvContext)
{
  switch (ulReason)
    {
    case kReasonWaveEmpty:
      break;

    case kReasonWave:
      //cmn_err((CE_WARN,"W"));
      //cmn_err((CE_WARN,"kReasonWave\n"));

      PHALWAVEDEVICE pHD;
      ULONG
	ulBytesToProcess,
	ulCircularBufferSize;

      pHD = (PHALWAVEDEVICE) pvContext;

      //cmn_err((CE_WARN,"d%ld ", pHD->GetDeviceNumber() ));

      // Get the amount of free space on device
      //cmn_err((CE_WARN,"GetTransferSize\n"));
      pHD->GetTransferSize (&ulBytesToProcess, &ulCircularBufferSize);
      //cmn_err((CE_WARN,"R%ld ", ulBytesToProcess ));

      if (pHD->IsRecord ())
	{
#if 0
	  pHD->TransferAudio (m_pRecordBuffer, ulBytesToProcess,
			      &ulBytesTransfered);
	  if (ulBytesToProcess != ulBytesTransfered)
	    cmn_err (CE_WARN, "Did not transfer all the audio! %lu %lu\n",
		     ulBytesToProcess, ulBytesTransfered);
	  m_WaveRecordFile.WriteBlock ((HPSTR) m_pRecordBuffer,
				       ulBytesTransfered);
	  //cmn_err((CE_WARN,"W%ld ", ulBytesTransfered ));
#endif
	}
      else
	{
	  if (device_mode == PCM_ENABLE_INPUT)
	    oss_audio_inputintr (m_audio_dev, 0);
	  else
	    oss_audio_outputintr (m_audio_dev, 1);
	}

      break;

    case kReasonDMAEmpty:
      break;

    case kReasonDMABufferComplete:
      PHALWAVEDMADEVICE pHDD;
      ULONG
	ulSize;

      pHDD = (PHALWAVEDMADEVICE) pvContext;

      if (m_audio_dev < 0 || m_audio_dev >= num_audio_engines)
	{
	  cmn_err (CE_WARN, "Bad audio engine %d\n", m_audio_dev);
	  break;
	}

      ulSize = 0;
      if (device_mode == PCM_ENABLE_INPUT)
	{
	  oss_audio_inputintr (m_audio_dev, 0);
	}
      else
	{
	  oss_audio_outputintr (m_audio_dev, 1);
	}

      break;
    case kReasonMIDI:
      cmn_err (CE_WARN, "kReasonMIDI\n");
      break;
    case kReasonDMALimit:
      cmn_err (CE_NOTE,
	       "ServiceInterrupt: Invalid ulReason=kReasonDMALimit\n");
      return (HSTATUS_INVALID_PARAMETER);
      break;
    default:
      cmn_err (CE_NOTE, "ServiceInterrupt: Invalid ulReason %x\n", ulReason);
      return (HSTATUS_INVALID_PARAMETER);
    }

  return HSTATUS_OK;
}

USHORT
InterruptCallback (ULONG ulReason, PVOID pvContext1, PVOID pvContext2)
{
  lynxtwo_portc *
    portc = (lynxtwo_portc *) pvContext1;
  return portc->ServiceInterrupt (ulReason, pvContext2);
}

int
lynxtwo_portc::Open (int mode, int open_flags)
{
  oss_native_word flags;

  if ((mode & ~device_mode))	/* This open mode is not possible */
    {
      return OSS_EIO;
    }

  MUTEX_ENTER_IRQDISABLE (m_devc->mutex, flags);
  if (open_mode != 0)
    {
      MUTEX_EXIT_IRQRESTORE (m_devc->mutex, flags);
      return OSS_EBUSY;
    }

  open_mode = mode;
  m_Prepared = FALSE;

  m_pHD = m_pHalAdapter->GetWaveDMADevice (m_nDevice);
  m_pHDMA = m_pHD->GetDMA ();

  // m_pHD->EnableSyncStart();
  m_pHD->RegisterCallback (InterruptCallback, (PVOID) this, (PVOID) m_pHD);
  MUTEX_EXIT_IRQRESTORE (m_devc->mutex, flags);

  return 0;
}

void
lynxtwo_portc::Close (int mode)
{
  oss_native_word flags;

  lynxtwo_reset (m_audio_dev);
  MUTEX_ENTER_IRQDISABLE (m_devc->mutex, flags);
  open_mode = 0;
  MUTEX_EXIT_IRQRESTORE (m_devc->mutex, flags);
}

int
lynxtwo_portc::SetSpeed (int arg)
{
  int i, best = 2, dif, bestdif = 0x7fffffff;

  if (arg == 0)
    arg = speed;

  if (m_devc->ratelock)
    {
      arg = glSampleRates[m_devc->rateselect];
    }

  if (m_devc->model == MDL_AES16 || m_devc->model == MDL_AES16_SRC)
    {
      if (arg < 32000)
	arg = 32000;
      if (arg > 192000)
	arg = 192000;
    }

  for (i = 0; i < NUM_SAMPLE_RATES; i++)
    {
      if (arg == glSampleRates[i])	/* Exact match */
	{
	  speed = arg;
	  return speed;
	}

      dif = arg - glSampleRates[i];
      if (dif < 0)
	dif *= -1;
      if (dif <= bestdif)
	{
	  best = i;
	  bestdif = dif;
	}

    }

  speed = glSampleRates[best];
  return speed;
}

int
lynxtwo_portc::SetChannels (int arg)
{
  if ((arg == 0))
    return channels;
  if (arg < 1)
    arg = 1;
  if (arg > 2)
    arg = 2;
  channels = arg;

  return channels;
}

int
lynxtwo_portc::SetFormat (int arg)
{
  if (arg == 0)
    return bits;

  if (arg != AFMT_S16_NE && arg != AFMT_S32_NE && arg != AFMT_U8)
    arg = AFMT_S16_NE;

  switch (arg)
    {
    case AFMT_U8:
      fmtbits = 8;
      fmtbytes = 1;
      break;
    case AFMT_S16_NE:
      fmtbits = 16;
      fmtbytes = 2;
      break;
    case AFMT_S32_NE:
      fmtbits = 32;
      fmtbytes = 4;
      break;
    }

  if (m_pHD->SetFormat (WAVE_FORMAT_PCM, channels, speed, fmtbits,
			channels * fmtbytes) != HSTATUS_OK)
    {
      switch (bits)
	{
	case AFMT_U8:
	  fmtbits = 8;
	  fmtbytes = 1;
	  break;
	case AFMT_S16_NE:
	  fmtbits = 16;
	  fmtbytes = 2;
	  break;
	case AFMT_S32_NE:
	  fmtbits = 32;
	  fmtbytes = 4;
	  break;
	}
      return bits;
    }

  bits = arg;

  return bits;
}

int
lynxtwo_portc::Ioctl (unsigned int cmd, ioctl_arg arg)
{
  char *srcs;
  PHALMIXER mix;
  ULONG value;
  int n;

  if (m_devc->model == MDL_AES16 || m_devc->model == MDL_AES16_SRC)
    {
      srcs = (char *) "Di1 Di2 Di3 Di4 Di5 Di6 Di7 Di8";
    }
  else
    {
      srcs = (char *) "Ain1/2 Ain3/4 Lb Dig";
    }

  switch (cmd)
    {
    case SNDCTL_DSP_GET_RECSRC:
      if (device_mode != PCM_ENABLE_INPUT)
	return OSS_EINVAL;

      n = m_nDevice - WAVE_RECORD0_DEVICE;
      mix = m_pHalAdapter->GetMixer ();
      mix->GetControl (LINE_RECORD_0 + n, LINE_NO_SOURCE, CONTROL_SOURCE_LEFT,
		       0, &value);
      return *arg = value / 2;
      break;

    case SNDCTL_DSP_SET_RECSRC:
      if (device_mode != PCM_ENABLE_INPUT)
	return OSS_EINVAL;

      value = *arg * 2;
      n = m_nDevice - WAVE_RECORD0_DEVICE;
      mix = m_pHalAdapter->GetMixer ();
      mix->SetControl (LINE_RECORD_0 + n, LINE_NO_SOURCE, CONTROL_SOURCE_LEFT,
		       0, value);
      mix->SetControl (LINE_RECORD_0 + n, LINE_NO_SOURCE,
		       CONTROL_SOURCE_RIGHT, 0, value + 1);
      mixer_devs[m_devc->input_mixer_dev]->modify_counter++;
      return *arg = value / 2;
      break;

    case SNDCTL_DSP_GET_RECSRC_NAMES:
      if (device_mode != PCM_ENABLE_INPUT)
	return OSS_EINVAL;

      return oss_encode_enum ((oss_mixer_enuminfo *) arg, srcs, 0);
      break;
    }

  return OSS_EINVAL;
}

void
lynxtwo_portc::SetTrigger (int state)
{
  if (device_mode & PCM_ENABLE_OUTPUT)
    if (state & PCM_ENABLE_OUTPUT)
      {
	if (m_Prepared && m_State == MODE_STOP)
	  {
	    if (m_pHD->Start ())
	      cmn_err (CE_WARN, "Starting play device %d failed\n",
		       m_nDevice);
	    m_State = MODE_RUNNING;
	    m_Prepared = FALSE;
	  }
      }
    else
      {
	if (m_State == MODE_RUNNING)
	  {
	    if (m_pHD->Stop ())
	      cmn_err (CE_WARN, "Stopping play device %d failed\n",
		       m_nDevice);
	    m_State = MODE_STOP;
	    m_Prepared = FALSE;
	  }
      }
  if (device_mode & PCM_ENABLE_INPUT)
    if (state & PCM_ENABLE_INPUT)
      {
	if (m_Prepared && m_State == MODE_STOP)
	  {
	    if (m_pHD->Start ())
	      cmn_err (CE_WARN, "Starting record device %d failed\n",
		       m_nDevice);
	    m_State = MODE_RUNNING;
	    m_Prepared = FALSE;
	  }
      }
    else
      {
	if (m_State == MODE_RUNNING)
	  {
	    if (m_pHD->Stop ())
	      cmn_err (CE_WARN, "Stopping record device %d failed\n",
		       m_nDevice);
	    m_State = MODE_STOP;
	    m_Prepared = FALSE;
	  }
      }
}

int
lynxtwo_portc::OutputBlock (oss_native_word ptr, int count, int fragsize,
			    int intrflag)
{
  int c;

  c = count / 4;

  m_pHDMA->AddEntry ((unsigned char *) ptr, c, TRUE);
  return 0;
}

int
lynxtwo_portc::StartInput (oss_native_word ptr, int count, int fragsize,
			   int intrflag)
{
  int c;

  c = count / 4;

  m_pHDMA->AddEntry ((unsigned char *) ptr, c, TRUE);
  return 0;
}

int
lynxtwo_portc::SyncStartPrepare (void)
{
  m_pHD->EnableSyncStart ();
  return 0;
}

int
lynxtwo_portc::SyncStartTrigger (void)
{
  if (m_pHD->Start ())
    cmn_err (CE_WARN, "Starting play device %d failed\n", m_nDevice);
  m_State = MODE_RUNNING;
  m_Prepared = FALSE;
  return 0;
}

int
lynxtwo_portc::PrepareForInput (int bsize, int bcount)
{
  if (m_State != MODE_STOP)
    return 0;

  if (m_pHD->SetFormat (WAVE_FORMAT_PCM, channels, speed, fmtbits,
			channels * fmtbytes) != HSTATUS_OK)
    {
      cmn_err (CE_NOTE,
	       "Current sample format not supported (c=%d, s=%d, b=%d)\n",
	       channels, speed, fmtbits);
      return OSS_EIO;
    }
  m_pHD->SetOverrunIE (TRUE);
  // m_pHD->SetDualMono(TRUE); What is this?
  m_pHD->SetDMASingle (TRUE);
  m_pHD->SetInterruptSamples (m_dmap->fragment_size / 4);
  m_pHD->SetLimitIE (FALSE);

  m_Prepared = TRUE;

  return 0;
}

int
lynxtwo_portc::PrepareForOutput (int bsize, int bcount)
{

  if (m_State != MODE_STOP)
    return 0;

  m_pHD->SetFormat (WAVE_FORMAT_PCM, channels, speed, fmtbits,
		    channels * fmtbytes);
  m_pHD->SetOverrunIE (TRUE);
  // m_pHD->SetDualMono(TRUE); What is this?
  m_pHD->SetDMASingle (TRUE);
  m_pHD->SetInterruptSamples (m_dmap->fragment_size / 4);
  m_pHD->SetLimitIE (FALSE);

  m_Prepared = TRUE;

  return 0;
}

EXTERN_C static int
lynxtwo_set_rate (int dev, int arg)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

  return portc->SetSpeed (arg);
}

EXTERN_C static short
lynxtwo_set_channels (int dev, short arg)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

  return portc->SetChannels (arg);
}

EXTERN_C static unsigned int
lynxtwo_set_format (int dev, unsigned int arg)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

  return portc->SetFormat (arg);
}

EXTERN_C static int
lynxtwo_ioctl (int dev, unsigned int cmd, ioctl_arg arg)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

  return portc->Ioctl (cmd, arg);
}

EXTERN_C static void
lynxtwo_trigger (int dev, int state)
{
  lynxtwo_portc *portc;

  if (dev < 0 || dev >= num_audio_engines)
    {
      cmn_err (CE_WARN, "lynxtwo_trigger: Bad audio engine %d\n", dev);
      return;
    }

  portc = (lynxtwo_portc *) audio_engines[dev]->portc;

  if (portc == NULL)
    {
      cmn_err (CE_WARN, "lynxtwo_trigger: portc==NULL\n");
      return;
    }

  portc->SetTrigger (state);
}

EXTERN_C static void
lynxtwo_reset (int dev)
{
  lynxtwo_trigger (dev, 0);
}

EXTERN_C static void lynxtwo_close (int dev, int mode);

EXTERN_C static int
lynxtwo_open (int dev, int mode, int open_flags)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

#ifdef USE_LICENSING
  extern int options_data;
  if (!(options_data & OPT_LYNX))
    return OSS_EBUSY;
#endif
  return portc->Open (mode, open_flags);
}

EXTERN_C static void
lynxtwo_close (int dev, int mode)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

#ifdef USE_LICENSING
  extern int options_data;
  if (!(options_data & OPT_LYNX))
    return;
#endif

  portc->Close (mode);
}

EXTERN_C static void
lynxtwo_output_block (int dev, oss_native_word ptr, int count, int fragsize,
		      int intrflag)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;
  portc->OutputBlock (ptr, count, fragsize, intrflag);
}

EXTERN_C static void
lynxtwo_start_input (int dev, oss_native_word ptr, int count, int fragsize,
		     int intrflag)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;
  portc->StartInput (ptr, count, fragsize, intrflag);
}

EXTERN_C static int
lynxtwo_prepare_for_output (int dev, int bsize, int bcount)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;
  return portc->PrepareForOutput (bsize, bcount);
}

EXTERN_C static int
lynxtwo_prepare_for_input (int dev, int bsize, int bcount)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;
  return portc->PrepareForInput (bsize, bcount);
}

EXTERN_C static int
lynxtwo_check_output (int dev)
{
  return OSS_EIO;
}

EXTERN_C static int
lynxtwo_sync_control (int dev, int event, int mode)
{
  lynxtwo_portc *portc = (lynxtwo_portc *) audio_engines[dev]->portc;

  if (event == SYNC_ATTACH)
    {
      return portc->SyncStartPrepare ();
    }

  if (event == SYNC_TRIGGER)
    {
      return portc->SyncStartTrigger ();
    }

  return OSS_EIO;
}

#if 0
EXTERN_C static int
lynxtwo_get_buffer_pointer (int dev, dmap_t * dmap, int direction)
{
}
#endif

static audiodrv_t lynxtwo_audio_driver = {
  lynxtwo_open,
  lynxtwo_close,
  lynxtwo_output_block,
  lynxtwo_start_input,
  lynxtwo_ioctl,
  lynxtwo_prepare_for_input,
  lynxtwo_prepare_for_output,
  lynxtwo_reset,
  NULL,
  NULL,
  NULL,
  NULL,
  lynxtwo_trigger,
  lynxtwo_set_rate,
  lynxtwo_set_format,
  lynxtwo_set_channels,
  NULL,
  NULL,
  NULL,
  lynxtwo_check_output,
  NULL,				/* lynxtwo_alloc_buffer */
  NULL,				/* lynxtwo_free_buffer */
  NULL,
  NULL,
  NULL,				/* lynxtwo_get_buffer_pointer */
  NULL,
  lynxtwo_sync_control
};

static void
install_output (lynxtwo_devc * devc, char *name)
{
  int adev, n;
  char tmp_name[1024];
  lynxtwo_portc *portc;
  int caps = ADEV_NOINPUT | ADEV_NOVIRTUAL;

  n = devc->num_outputs++;

  portc = new lynxtwo_portc (WAVE_PLAY0_DEVICE + n, num_audio_engines, devc);

  devc->output_portc[n] = portc;

  sprintf (tmp_name, "%s %s", devc->card_name, name);

  if ((adev = oss_install_audiodev (OSS_AUDIO_DRIVER_VERSION,
				    devc->osdev,
				    devc->osdev,
				    tmp_name,
				    &lynxtwo_audio_driver,
				    sizeof (audiodrv_t),
				    caps, FMT_MASK, devc, -1)) < 0)
    {
      adev = -1;
      return;
    }
  else
    {
      int i;

      portc->m_audio_dev = adev;

      if (devc->first_dev == -1)
	devc->first_dev = adev;
      audio_engines[adev]->devc = devc;
      audio_engines[adev]->portc = portc;
      audio_engines[adev]->rate_source = devc->first_dev;
      audio_engines[adev]->min_rate = 8000;
      audio_engines[adev]->max_rate = 200000;
      //audio_engines[adev]->min_block = 4096;
      //audio_engines[adev]->max_block = 4096;
      for (i = 0; i < (int) NUM_SAMPLE_RATES; i++)
	audio_engines[adev]->rates[i] = glSampleRates[i];
      audio_engines[adev]->nrates = NUM_SAMPLE_RATES;
      portc->m_adev = audio_engines[adev];
      portc->m_dmap = audio_engines[adev]->dmap_out;
      portc->device_mode = PCM_ENABLE_OUTPUT;
    }
}

static void
install_input (lynxtwo_devc * devc, char *name)
{
  int adev, n;
  char tmp_name[1024];
  lynxtwo_portc *portc;
  int caps = ADEV_NOOUTPUT | ADEV_NOVIRTUAL;

  n = devc->num_inputs++;

  portc = new lynxtwo_portc (n, num_audio_engines, devc);

  devc->input_portc[n] = portc;

  sprintf (tmp_name, "%s %s", devc->card_name, name);

  if ((adev = oss_install_audiodev (OSS_AUDIO_DRIVER_VERSION,
				    devc->osdev,
				    devc->osdev,
				    tmp_name,
				    &lynxtwo_audio_driver,
				    sizeof (audiodrv_t),
				    caps, FMT_MASK, devc, -1)) < 0)
    {
      adev = -1;
      return;
    }
  else
    {
      int i;

      portc->m_audio_dev = adev;

      if (devc->first_dev == -1)
	devc->first_dev = adev;
      devc->input_devs[n] = adev;
      audio_engines[adev]->devc = devc;
      audio_engines[adev]->portc = portc;
      audio_engines[adev]->rate_source = devc->first_dev;
      audio_engines[adev]->min_rate = 8000;
      audio_engines[adev]->max_rate = 200000;
      for (i = 0; i < (int) NUM_SAMPLE_RATES; i++)
	audio_engines[adev]->rates[i] = glSampleRates[i];
      audio_engines[adev]->nrates = NUM_SAMPLE_RATES;
      portc->m_adev = audio_engines[adev];
      portc->m_dmap = audio_engines[adev]->dmap_in;
      portc->device_mode = PCM_ENABLE_INPUT;
    }
}

static void
install_inputs (lynxtwo_devc * devc)
{
  int i;
  char name[32];

  for (i = 0; i < MAX_INPUTS; i++)
    {
      sprintf (name, "Record %d", i + 1);
      install_input (devc, name);
    }
}

static void
install_outputs (lynxtwo_devc * devc)
{
  int i;
  char name[32];

  for (i = 0; i < MAX_OUTPUTS; i++)
    {
      sprintf (name, "Play %d", i + 1);
      install_output (devc, name);
    }
}

int
init_lynx_audio (lynxtwo_devc * devc)
{
  extern int lynxtwo_init_order;

  devc->firstdev = num_audio_engines;

  if (lynxtwo_init_order == 0)
    {
      install_outputs (devc);
      install_inputs (devc);
    }
  else
    {
      install_inputs (devc);
      install_outputs (devc);
    }

  return 1;
}

int
uninit_lynx_audio (lynxtwo_devc * devc)
{
  int i;
  lynxtwo_portc *portc;

  for (i = 0; i < devc->num_outputs; i++)
    {
      portc = (lynxtwo_portc *) devc->output_portc[i];
//      delete portc;
      devc->output_portc[i] = NULL;
    }
  devc->num_outputs = 0;

  for (i = 0; i < devc->num_inputs; i++)
    {
      portc = (lynxtwo_portc *) devc->input_portc[i];
//      delete portc;
      devc->input_portc[i] = NULL;
    }
  devc->num_inputs = 0;

  return 1;
}
