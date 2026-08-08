/*
 * HalOss.cpp - LynxTWO C/C++ bridge code
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
#include <lynxtwo_cfg.h>
#include "lynxtwo.h"
#include "HalOSS.h"
}

class CMixerControl
{
public:
  class CMixerControl * m_next;
  int m_ctrl;
  int m_dev;
  int m_value;
  lynxtwo_devc *m_devc;

    CMixerControl (int id);
    CMixerControl (int id, lynxtwo_devc * devc);
    virtual ~ CMixerControl (void);

  virtual int GetMixer (void);
  virtual int SetMixer (int val);

private:
  int m_id;
};

typedef class CMixerControl CMIX, *PMIX;


CMixerControl::CMixerControl (int id)
{
  m_id = id;
  m_next = NULL;
  m_ctrl = 0;
  m_value = 0;

  m_devc = NULL;
}

CMixerControl::CMixerControl (int id, lynxtwo_devc * devc)
{
  m_id = id;
  m_next = NULL;
  m_ctrl = 0;
  m_value = 0;

  m_devc = devc;
}

int
CMixerControl::SetMixer (int value)
{
  switch (m_id)
    {
    case 1:
      if (value < 0)
	value = 0;
      if (value >= (int) NUM_SAMPLE_RATES)
	value = NUM_SAMPLE_RATES - 1;
      m_devc->rateselect = value;
      break;

    case 2:
      m_devc->ratelock = value;
      break;
    }

  return m_value = value;
}

int
CMixerControl::GetMixer (void)
{
  return m_value;
}

CMixerControl::~CMixerControl (void)
{
}

class CHalOSSMixer:public CMixerControl
{
public:
  CHalOSSMixer (int id, PHALMIXER pHalMixer, int dst, int src, int ctl,
		int chn);
   ~CHalOSSMixer (void);
  int GetMixer (void);
  int SetMixer (int val);

protected:
  int m_dst, m_src, m_ctl, m_chn;
  PHALMIXER m_pHalMixer;
};

class CMixerValue:public CMixerControl
{
public:
  CMixerValue (int id, lynxtwo_devc * devc);
   ~CMixerValue (void);
  int GetMixer (void);
  int SetMixer (int val);

private:
  int m_id;
};

typedef class CHalOSSMixer CHALMIX, *PHALMIX;

class CCalibrator:public CMixerControl
{
public:
  CCalibrator (int id);
   ~CCalibrator (void);
  int GetMixer (void);
  int SetMixer (int val);

private:
  int m_dst, m_src, m_ctl, m_chn;
  PHALMIXER m_pHalMixer;
};

int
CCalibrator::SetMixer (int value)
{
  BYTE RxBuffer[24];
  lynxtwo_devc *devc = (lynxtwo_devc *) mixer_devs[m_dev]->devc;
  PHALADAPTER pHalAdapter;

  pHalAdapter = (PHALADAPTER) devc->m_pHalAdapter;

  pHalAdapter->Get8420 ()->ReadCUBuffer (RxBuffer);

  return 0;
}

CCalibrator::CCalibrator (int id):CMixerControl (id)
{
}

CCalibrator::~CCalibrator (void)
{
}

int
CCalibrator::GetMixer (void)
{
  return 0;
}

CHalOSSMixer::CHalOSSMixer (int id, PHALMIXER pHalMixer, int dst, int src, int ctl, int chn):CMixerControl
  (id)
{
  m_dst = dst;
  m_src = src;
  m_ctl = ctl;
  m_chn = chn;
  m_pHalMixer = pHalMixer;
}

CHalOSSMixer::~CHalOSSMixer (void)
{
}

#include "lynxtwo_db.h"

int
CHalOSSMixer::GetMixer (void)
{
  ULONG value, err;

  err = m_pHalMixer->GetControl (m_dst, m_src, m_ctl, m_chn, &value);
  if (err != 0)
    {
      cmn_err (CE_WARN, "GetControl failed. Err=%d\n", err);
      return OSS_EIO;
    }

  if (m_ctl == CONTROL_PEAKMETER)
    {
      return lin32768_to_db115[value];
    }

  if (m_ctl == CONTROL_VOLUME)
    return m_value;

  return value;
}

int
CHalOSSMixer::SetMixer (int value)
{
  int err;

  m_value = value;

  if (m_ctl == CONTROL_VOLUME)
    {
      if (value < 0)
	value = 0;
      if (value > 114)
	value = 114;

      m_value = value;

      value = ((value * 1080) / 114) - 960;
    }

  err = m_pHalMixer->SetControl (m_dst, m_src, m_ctl, m_chn, value);
  if (err != 0)
    {
      cmn_err (CE_WARN,
	       "SetControl failed: dst=%d src=%d ctl=%d val=%d err=%d\n",
	       m_dst, m_src, m_ctl, value, err);
      return OSS_EIO;
    }
  return GetMixer ();
}

CMixerValue::CMixerValue (int id, lynxtwo_devc * devc):CMixerControl (id, devc)
{
  m_id = id;
}

CMixerValue::~CMixerValue (void)
{
}

int
CMixerValue::GetMixer (void)
{
  lynxtwo_devc *devc = m_devc;
  PHALADAPTER pHalAdapter;
  ULONG val;

  pHalAdapter = (PHALADAPTER) devc->m_pHalAdapter;
 
  if ((devc->deviceID == LYNX_AES16) || (devc->deviceID == LYNX_AES16_SRC))
    {
        PHAL4114 pAK4114 = pHalAdapter->Get4114 ();

        pAK4114->GetInputStatus (m_id, &val);
    }
  if ((devc->deviceID == LYNX_AES16e) || (devc->deviceID == LYNX_AES16e_SRC))
   {
        PHALAES16E pAES16E = pHalAdapter->GetAES16eDIO();

        pAES16E->GetInputStatus (m_id, &val);
   }

  return !(val & 0x00000001);	/* Error Status */
}

int
CMixerValue::SetMixer (int value)
{
  return OSS_ENOTSUP;
}

class CStereoSlider:public CHalOSSMixer
{
public:
  CStereoSlider (int id, PHALMIXER pHalMixer, int dst, int src, int ctl,
		 int chn);
  ~CStereoSlider (void);
  int GetMixer (void);
  int SetMixer (int val);
};

typedef class CStereoSlider CSTEREOSLIDER, *PSTEREOSLIDER;

CStereoSlider::CStereoSlider (int id, PHALMIXER pHalMixer, int dst, int src,
			      int ctl, int chn):
CHalOSSMixer (id, pHalMixer, dst, src, ctl, chn)
{
}

CStereoSlider::~CStereoSlider (void)
{
}

int
CStereoSlider::GetMixer (void)
{
  return m_value;
}

int
CStereoSlider::SetMixer (int value)
{
  int err;
  int left, right;

  left = value & 0xff;
  right = (value >> 8) & 0xff;

  if (left < 0)
    left = 0;
  if (left > 114)
    left = 114;
  left = (left * 65535) / 114;
  err = m_pHalMixer->SetControl (m_dst, m_src, m_ctl, m_chn, left);

  if (right < 0)
    right = 0;
  if (right > 114)
    right = 114;
  right = (right * 65535) / 114;
  err = m_pHalMixer->SetControl (m_dst + 1, m_src, m_ctl, m_chn, right);

  value = left | (right << 8);

  return m_value = value;
}

class CHalOSS
{
public:
  CHalOSS (lynxtwo_devc * devc, PHALADAPTER adap);
  ~CHalOSS (void);

  void SetGroup (int g);
  int CreateControl (int d, PMIX m, char *name, int maxval, int typ,
		     int flags);
  int CreateEnum (int d, PMIX m, char name[], int maxval, int flags);
  int CreateOnoff (int d, PMIX m, char *name, int flags);
  int CreateValue (int d, PMIX m, char *name, int mx, int flags);
  int CreateVU (int d, PMIX m, char *name, int mx);
  int CreateSlider (int d, PMIX m, char *name, int mx);
  int CreateStereoSlider (int d, PMIX m, char *name, int mx);
  int MixerCallback (int ctrl, unsigned int cmd, int value);
  int InitAdapterMixer (int dev, lynxtwo_devc * devc);
  int InitAES16AdapterMixer (int dev, lynxtwo_devc * devc);
  int InitRecMixer (int dev, lynxtwo_devc * devc);
  int InitPlayMixer (int dev, lynxtwo_devc * devc);


private:
    lynxtwo_devc * m_devc;
  PHALADAPTER m_pHalAdapter;
  PHALMIXER m_pHalMixer;

  int m_group;
  int m_nextid;

  CMixerControl *m_mixerlist;
  int m_value;
};

typedef class CHalOSS *PHALOSS;

CHalOSS::CHalOSS (lynxtwo_devc * devc, PHALADAPTER adap)
{
  m_devc = devc;
  m_nextid = 0;
  m_mixerlist = NULL;
  m_pHalAdapter = adap;
  m_pHalMixer = adap->GetMixer ();
  m_pHalMixer->SetDefaults ();
}

CHalOSS::~CHalOSS (void)
{
}

void
CHalOSS::SetGroup (int g)
{
  m_group = g;
}

EXTERN_C static int
mixer_callback (int dev, int ctrl, unsigned int cmd, int value)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) mixer_devs[dev]->devc;
  PHALOSS pOSS = (PHALOSS) devc->m_pHalOSS;
  return pOSS->MixerCallback (ctrl, cmd, value);
}

int
CHalOSS::CreateControl (int mixer_dev, PMIX m, char *name, int maxval,
			int typ, int flags)
{
  int err;

  err = mixer_ext_create_control (mixer_dev, m_group,
				  m_nextid, mixer_callback, typ,
				  name, maxval, flags);

  if (err < 0)
    return err;

  m->m_ctrl = m_nextid++;
  m->m_next = m_mixerlist;
  m->m_dev = mixer_dev;
  if (typ == MIXT_ENUM || typ == MIXT_ONOFF)
    m->m_value = maxval - 1;
  else
    m->m_value = maxval;
  m_mixerlist = m;

  return err;
}

int
CHalOSS::CreateEnum (int mixer_dev, PMIX m, char name[], int maxval,
		     int flags)
{
  if (flags == 0)
    flags = MIXF_READABLE | MIXF_WRITEABLE;

  return CreateControl (mixer_dev, m, name, maxval, MIXT_ENUM, flags);
}

int
CHalOSS::CreateOnoff (int mixer_dev, PMIX m, char *name, int flags)
{
  if (flags == 0)
    flags = MIXF_READABLE | MIXF_WRITEABLE;

  return CreateControl (mixer_dev, m, name, 1, MIXT_ONOFF, flags);
}

int
CHalOSS::CreateValue (int mixer_dev, PMIX m, char *name, int mx, int flags)
{
  return CreateControl (mixer_dev, m, name, mx, MIXT_VALUE, flags);
}

int
CHalOSS::CreateVU (int mixer_dev, PMIX m, char *name, int mx)
{
  return CreateControl (mixer_dev, m, name, mx, MIXT_MONOPEAK, MIXF_READABLE);
}

int
CHalOSS::CreateSlider (int mixer_dev, PMIX m, char *name, int mx)
{
  return CreateControl (mixer_dev, m, name, mx, MIXT_SLIDER,
			MIXF_READABLE | MIXF_WRITEABLE);
}

int
CHalOSS::CreateStereoSlider (int mixer_dev, PMIX m, char *name, int mx)
{
  int err;

  if ((err =
       CreateControl (mixer_dev, m, name, mx, MIXT_STEREOSLIDER,
		      MIXF_READABLE | MIXF_WRITEABLE)) < 0)
    return err;

  m->SetMixer (mx | (mx << 8));

  return 0;
}

int
CHalOSS::MixerCallback (int ctrl, unsigned int cmd, int value)
{
  PMIX pMIX;

  pMIX = m_mixerlist;

  while (pMIX != NULL)
    {
      if (pMIX->m_ctrl == ctrl)
	break;

      pMIX = pMIX->m_next;
    }

  if (pMIX == NULL)
    return OSS_EIO;

  if (cmd == SNDCTL_MIX_READ)
    return pMIX->GetMixer ();
  if (cmd == SNDCTL_MIX_WRITE)
    return pMIX->SetMixer (value);

  return OSS_EINVAL;
}

int
CHalOSS::InitAdapterMixer (int dev, lynxtwo_devc * devc)
{
  int group, ctl;

  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "CLOCK",
				     MIXF_FLAT)) < 0)
    return group;
  SetGroup (group);

  if ((ctl =
       CreateEnum ((int) dev, new CMixerControl (1, devc),
		   (char *) "CLOCK_RATE", (int) NUM_SAMPLE_RATES, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "8000 11025 16000 22050 32000 44100 48000 64000 88200 96000 128000 176400 192000 200000",
			 0);
  if ((ctl =
       CreateOnoff ((int) dev, new CMixerControl (2, devc),
		    (char *) "CLOCK_RATELOCK", 0)) < 0)
    return ctl;
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  (int) LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_CLOCKSOURCE, 0),
		   (char *) "CLOCK_SOURCE", 7, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "Internal Digital External header Video LStream1 LStream2",
			 0);
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_SRC_MODE, 0),
		   (char *) "CLOCK_REFERENCE", 5, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl, (char *) "Auto 13.5MHz 27MHz Word Word256",
			 0);
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_DIGITAL_FORMAT, 0),
		   (char *) "CLOCK_DIGFMT", 2, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl, (char *) "AES/EBU S/PDIF", 0);
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_SRC_MODE, 0),
		   (char *) "CLOCK_DIGMODE", 5, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "SRC_On SRC_Off:ClockSyncronous SRC_Off SRC_On:Digital_Out XMit_Only",
			 0);

/*
 * Converters
 */
  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "CONVERTERS",
				     MIXF_FLAT)) < 0)
    return group;
  SetGroup (group);
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_DITHER_TYPE, 0),
		   (char *) "DITHER", 4, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "None Triangular Shaped_Triangular Rectangular", 0);
  if ((ctl =
       CreateOnoff (dev, (CMixerControl *) new CCalibrator (1),
		    (char *) "RECALIBRATE", 0)) < 0)
    return ctl;

/*
 * Trim controls 
 */

  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "LYNXTWO_TRIM",
				     MIXF_FLAT)) < 0)
    return group;
  SetGroup (group);
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_AOUT12_TRIM, 0),
		   (char *) "TRIM_OUT1/2", 2, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl, (char *) "-10db +4db", 0);

  if (devc->model != MDL_LYNXTWO_C)
    {
      if ((ctl =
	   CreateEnum (dev,
		       (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						      LINE_ADAPTER,
						      LINE_NO_SOURCE,
						      CONTROL_AOUT34_TRIM, 0),
		       (char *) "TRIM_OUT3/4", 2, 0)) < 0)
	return ctl;
      mixer_ext_set_strings (dev, ctl, (char *) "-10db +4db", 0);
    }

  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_AIN12_TRIM, 0),
		   (char *) "TRIM_IN1/2", 2, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl, (char *) "-10db +4db", 0);
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_AIN34_TRIM, 0),
		   (char *) "TRIM_IN3/4", 2, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl, (char *) "-10db +4db", 0);

  if (devc->model == MDL_LYNXTWO_C)
    {
      if ((ctl =
	   CreateEnum (dev,
		       (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						      LINE_ADAPTER,
						      LINE_NO_SOURCE,
						      CONTROL_AIN56_TRIM, 0),
		       (char *) "TRIM_IN5/6", 2, 0)) < 0)
	return ctl;
      mixer_ext_set_strings (dev, ctl, (char *) "-10db +4db", 0);
    }

  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "LYNXTWO_FREQ",
				     MIXF_FLAT)) < 0)
    return group;
  SetGroup (group);

#define CMAX	100000000
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_1,
						   0),
		    (char *) "FQ_L/R_CLOCK", CMAX,
		    MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_2,
						   0), (char *) "FQ_DIGIN",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_3,
						   0), (char *) "FQ_EXT",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_4,
						   0), (char *) "FQ_HDR",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_5,
						   0), (char *) "FQ_VIDEO",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_6,
						   0), (char *) "FQ_LSTREAM1",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_7,
						   0), (char *) "FQ_LSTREAM2",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_8,
						   0), (char *) "FQ_PCI",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  return 0;
}

int
CHalOSS::InitAES16AdapterMixer (int dev, lynxtwo_devc * devc)
{
  int group, ctl;

  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "CLOCK",
				     MIXF_FLAT)) < 0)
    return group;
  SetGroup (group);

  if ((ctl =
       CreateEnum ((int) dev, new CMixerControl (1, devc),
		   (char *) "CLOCK_RATE", (int) NUM_SAMPLE_RATES, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "8000 11025 16000 22050 32000 44100 48000 64000 88200 96000 128000 176400 192000 200000",
			 0);
  if ((ctl =
       CreateOnoff ((int) dev, new CMixerControl (2, devc),
		    (char *) "CLOCK_RATELOCK", 0)) < 0)
    return ctl;

/* Dual Wire-in */
  if ((ctl =
       CreateOnoff (dev, (CMIX *) new CHALMIX (1, m_pHalMixer,
					       LINE_ADAPTER,
					       LINE_NO_SOURCE,
					       CONTROL_WIDEWIREIN,
					       0),
		    (char *) "DUAL-WIREIN", 0)) < 0)
    return ctl;

/* Dual Wire-out */
  if ((ctl =
       CreateOnoff (dev, (CMIX *) new CHALMIX (1, m_pHalMixer,
					       LINE_ADAPTER,
					       LINE_NO_SOURCE,
					       CONTROL_WIDEWIREOUT,
					       0),
		    (char *) "DUAL-WIREOUT", 0)) < 0)
    return ctl;


  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  (int) LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_CLOCKSOURCE_PREFERRED,
						  0),
		   (char *) "CLOCK_PREFERRED-SRC", 8, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "Internal External Header LStream Digital_in1 Digital_in2 Digital_in3 Digital_in4",
			 0);
#if 1
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_CLOCKSOURCE,
						  0),
		   (char *) "CLOCK_SOURCE", 8,
		   MIXF_READABLE | MIXF_POLL)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl,
			 (char *)
			 "Internal External Header LStream Digital_in1 Digital_in2 Digital_in3 Digital_in4",
			 0);

#endif
#if 0
// TODO
  if ((ctl =
       CreateEnum (dev,
		   (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						  LINE_ADAPTER,
						  LINE_NO_SOURCE,
						  CONTROL_CLOCKREFERENCE, 0),
		   (char *) "CLOCK_REFERENCE", 5, 0)) < 0)
    return ctl;
  mixer_ext_set_strings (dev, ctl, (char *) "Auto 13.5MHz 27MHz Word Word256",
			 0);
#endif

  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "LYNXTWO_FREQ",
				     MIXF_FLAT)) < 0)
    return group;
  SetGroup (group);

#define CMAX	100000000
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_1,
						   0),
		    (char *) "FQ_L/R_CLOCK", CMAX,
		    MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_2,
						   0), (char *) "FQ_EXT",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_3,
						   0), (char *) "FQ_HDR",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_4,
						   0), (char *) "FQ_LSTR",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_5,
						   0), (char *) "FQ_PCI",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;

  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_6,
						   0), (char *) "FQ_DI1",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_7,
						   0), (char *) "FQ_DI2",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_8,
						   0), (char *) "FQ_DI3",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_9,
						   0), (char *) "FQ_DI4",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_10,
						   0), (char *) "FQ_DI5",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_11,
						   0), (char *) "FQ_DI6",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_12,
						   0), (char *) "FQ_DI7",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
  if ((ctl =
       CreateValue (dev,
		    (CMixerControl *) new CHALMIX (1, m_pHalMixer,
						   LINE_ADAPTER,
						   LINE_NO_SOURCE,
						   CONTROL_FREQUENCY_COUNTER_13,
						   0), (char *) "FQ_DI8",
		    CMAX, MIXF_READABLE | MIXF_POLL | MIXF_HZ)) < 0)
    return ctl;
/*
 * Lock indicators
 */
  if ((group =
       mixer_ext_create_group_flags (dev, 0, (char *) "LOCK", MIXF_FLAT)) < 0)
    return group;

  SetGroup (group);

  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (0, devc), (char *) "IN1", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (1, devc), (char *) "IN2", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (2, devc), (char *) "IN3", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (3, devc), (char *) "IN4", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (4, devc), (char *) "IN5", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (5, devc), (char *) "IN6", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (6, devc), (char *) "IN7", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;
  if ((ctl =
       CreateValue ((int) dev, new CMixerValue (7, devc), (char *) "IN8", 1,
		    MIXF_READABLE | MIXF_POLL | MIXF_OKFAIL)) < 0)
    return ctl;

/*
 * SRC enable/disable
 */
  if (devc->model == MDL_AES16_SRC)
    {

      if ((group =
	   mixer_ext_create_group_flags (dev, 0, (char *) "SRC",
					 MIXF_FLAT)) < 0)
	return group;

      SetGroup (group);

      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer,
					      LINE_ADAPTER,
					      LINE_NO_SOURCE,
					      CONTROL_DIGITALIN5_SRC_ENABLE,
					      0), (char *) "IN5", 0)) < 0)
	return ctl;

      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer,
					      LINE_ADAPTER,
					      LINE_NO_SOURCE,
					      CONTROL_DIGITALIN6_SRC_ENABLE,
					      0), (char *) "IN6", 0)) < 0)
	return ctl;

      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer,
					      LINE_ADAPTER,
					      LINE_NO_SOURCE,
					      CONTROL_DIGITALIN7_SRC_ENABLE,
					      0), (char *) "IN7", 0)) < 0)
	return ctl;

      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer,
					      LINE_ADAPTER,
					      LINE_NO_SOURCE,
					      CONTROL_DIGITALIN8_SRC_ENABLE,
					      0), (char *) "IN8", 0)) < 0)
	return ctl;
    }


/*
 * SRC ratio indicators.
 */
  if (devc->model == MDL_AES16_SRC)
    {
      if ((group =
	   mixer_ext_create_group_flags (dev, 0, (char *) "SRCRATIO",
					 MIXF_FLAT)) < 0)
	return group;

      SetGroup (group);

      if ((ctl =
	   CreateValue (dev,
			(CMixerControl *) new CHALMIX (1, m_pHalMixer,
						       LINE_ADAPTER,
						       LINE_NO_SOURCE,
						       CONTROL_DIGITALIN5_SRC_RATIO,
						       0),
			(char *) "IN5", 0xFFFFFFFF,
			MIXF_READABLE | MIXF_POLL)) < 0)
	return ctl;

      if ((ctl =
	   CreateValue (dev,
			(CMixerControl *) new CHALMIX (1, m_pHalMixer,
						       LINE_ADAPTER,
						       LINE_NO_SOURCE,
						       CONTROL_DIGITALIN6_SRC_RATIO,
						       0),
			(char *) "IN6", 0xFFFFFFFF,
			MIXF_READABLE | MIXF_POLL)) < 0)
	return ctl;

      if ((ctl =
	   CreateValue (dev,
			(CMixerControl *) new CHALMIX (1, m_pHalMixer,
						       LINE_ADAPTER,
						       LINE_NO_SOURCE,
						       CONTROL_DIGITALIN7_SRC_RATIO,
						       0),
			(char *) "IN7", 0xFFFFFFFF,
			MIXF_READABLE | MIXF_POLL)) < 0)
	return ctl;

      if ((ctl =
	   CreateValue (dev,
			(CMixerControl *) new CHALMIX (1, m_pHalMixer,
						       LINE_ADAPTER,
						       LINE_NO_SOURCE,
						       CONTROL_DIGITALIN8_SRC_RATIO,
						       0),
			(char *) "IN8", 0xFFFFFFFF,
			MIXF_READABLE | MIXF_POLL)) < 0)
	return ctl;
    }

  return 0;
}

int
CHalOSS::InitRecMixer (int dev, lynxtwo_devc * devc)
{
  int group, group2 = 0, group3, ctl;
  int i;
  char tmp[10];
  char *recsrc = NULL;
  int nc;

  if (devc->model == MDL_AES16 || devc->model == MDL_AES16_SRC)
    {
      recsrc =
	(char *)
	"Di1L Di1R Di2L Di2R Di3L Di3R Di4L Di4R Di5L Di5R Di6L Di6R Di7L Di7R Di8L Di8R";
      nc = 16;
    }
  else
    {
      recsrc = (char *) "Ain1 Ain2 Ain3 Ain4 LbL LbR DinL DinR";
      nc = 8;
    }

  for (i = 0; i < 8; i++)
    {
      int n;
      if (devc->model == MDL_AES16 || devc->model == MDL_AES16_SRC)
	n = i;
      else
	n = i % 4;

      if (n == 0)
	{
	  if ((group2 = mixer_ext_create_group (dev, 0, (char *) "RECORD")) < 0)	// Dummy group
	    return group2;
	}

      //sprintf(tmp, "%d", i+1);
      sprintf (tmp, "@pcm%d", devc->input_devs[i]);
      if ((group = mixer_ext_create_group (dev, group2, tmp)) < 0)
	return group;
      SetGroup (group);

      // record sources
      if ((ctl =
	   CreateEnum (dev,
		       (CMIX *) new CHALMIX (1, m_pHalMixer,
					     LINE_RECORD_0 + i,
					     LINE_NO_SOURCE,
					     CONTROL_SOURCE_LEFT, 0),
		       (char *) "LSRC", nc, 0)) < 0)
	return ctl;
      mixer_ext_set_strings (dev, ctl, recsrc, 0);
      if ((ctl =
	   CreateEnum (dev,
		       (CMIX *) new CHALMIX (1, m_pHalMixer,
					     LINE_RECORD_0 + i,
					     LINE_NO_SOURCE,
					     CONTROL_SOURCE_RIGHT, 0),
		       (char *) "RSRC", nc, 0)) < 0)
	return ctl;
      mixer_ext_set_strings (dev, ctl, recsrc, 0);

      // Set the default sources
      m_pHalMixer->SetControl (LINE_RECORD_0 + i, LINE_NO_SOURCE,
			       CONTROL_SOURCE_LEFT, 0, n * 2);
      m_pHalMixer->SetControl (LINE_RECORD_0 + i, LINE_NO_SOURCE,
			       CONTROL_SOURCE_RIGHT, 0, n * 2 + 1);

      // VU Meter
      if ((group3 = mixer_ext_create_group (dev, group, (char *) "-")) < 0)
	return group3;
      SetGroup (group3);

      if ((ctl =
	   CreateVU (dev,
		     (CMIX *) new CHALMIX (1, m_pHalMixer, LINE_RECORD_0 + i,
					   LINE_NO_SOURCE, CONTROL_PEAKMETER,
					   LEFT), (char *) "-", 114)) < 0)
	return ctl;
      if ((ctl =
	   CreateVU (dev,
		     (CMIX *) new CHALMIX (1, m_pHalMixer, LINE_RECORD_0 + i,
					   LINE_NO_SOURCE, CONTROL_PEAKMETER,
					   RIGHT), (char *) "-", 114)) < 0)
	return ctl;

      // Mute
      if ((group3 = mixer_ext_create_group (dev, group, (char *) "MUTE")) < 0)
	return group3;
      SetGroup (group3);

      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer,
					      LINE_RECORD_0 + i / 2,
					      LINE_NO_SOURCE, CONTROL_MUTE,
					      LEFT), (char *) "L", 0)) < 0)
	return ctl;
      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer,
					      LINE_RECORD_0 + i / 2,
					      LINE_NO_SOURCE, CONTROL_MUTE,
					      RIGHT), (char *) "R", 0)) < 0)
	return ctl;
    }

  return 0;
}

int
CHalOSS::InitPlayMixer (int dev, lynxtwo_devc * devc)
{
  int group, group2, ctl;
  int i;
  char *srcs;
  int nc;

  if (devc->model == MDL_AES16 || devc->model == MDL_AES16_SRC)
    {
      srcs =
	(char *)
	"Di1L Di1R Di2L Di2R Di3L Di3R Di4L Di4R Di5L Di5R Di6L Di6R Di7L Di7R Di8L Di8R P1L P1R P2L P2R P3L P3R P4L P4R P5L P5R P6L P6R P7L P7R P8L P8R";
      nc = 32;
    }
  else
    {
      srcs =
	(char *)
	"Ain1 Ain2 Ain3 Ain4 LBIL LBIR DInL DInR L2I1 L2I2 L2I3 L2I4 L2I5 L2I6 L2I7 L2I8 P1L P1R P2L P2R P3L P3R P4L P4R P5L P5R P6L P6R P7L P7R P8L P8R";
      nc = 32;
    }

  group = 0;

  for (i = 0; i < 16; i++)
    {
      int group3;

      if (!(i % 8))
	if ((group = mixer_ext_create_group (dev, 0, (char *) "OUTPUT")) < 0)
	  return group;

      char tmp[16];
      sprintf (tmp, "A%d", i + 1);

      if ((group2 = mixer_ext_create_group (dev, group, tmp)) < 0)
	return group2;
      SetGroup (group2);

      if ((group3 = mixer_ext_create_group (dev, group2, (char *) "-")) < 0)
	return group3;
      SetGroup (group3);

      if ((ctl =
	   CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
					      LINE_NO_SOURCE, CONTROL_MUTE,
					      0), (char *) "MUTE", 0)) < 0)
	return ctl;

      if ((devc->deviceID == LYNX_AES16e) || (devc->deviceID == LYNX_AES16e_SRC))
	{
      		if ((ctl = CreateSlider (dev,
			 (CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
					       LINE_NO_SOURCE, CONTROL_VOLUME,
					       0), (char *) "VOL", 114)) < 0)
		return ctl;

      		if (i < 8)
		{
	  		oss_mixext *ent;

	  		static int colors[4] = {
	    		OSS_RGB_WHITE,
	    		OSS_RGB_RED,
	    		OSS_RGB_YELLOW,
	    		OSS_RGB_GREEN
	  		};

	  		if ((ent = mixer_find_ext (dev, ctl)) != NULL)
	    			ent->rgbcolor = colors[i % 4];
		}
	}
      if ((ctl =
	   CreateVU (dev,
		     (CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
					   LINE_NO_SOURCE, CONTROL_PEAKMETER,
					   0), (char *) "-", 114)) < 0)
	return ctl;
     
      if ((devc->deviceID == LYNX_AES16) || (devc->deviceID == LYNX_AES16_SRC))
	{
      		SetGroup (group2);
      		if ((ctl = CreateEnum (dev,
		       	(CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
				     LINE_PLAYMIX_1, CONTROL_SOURCE,
				     0), (char *) "A", nc, 0)) < 0)
		return ctl;
      		mixer_ext_set_strings (dev, ctl, srcs, 0);
      		if ((ctl = CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
				      LINE_PLAYMIX_1, CONTROL_MUTE,
				      0), (char *) "MUTEA", 0)) < 0)
		return ctl;

      		if ((ctl = CreateEnum (dev,
		       (CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
				     LINE_PLAYMIX_2, CONTROL_SOURCE,
				     0), (char *) "B", nc, 0)) < 0)
		return ctl;
      		mixer_ext_set_strings (dev, ctl, srcs, 0);
      		if ((ctl = CreateOnoff (dev,
			(CMIX *) new CHALMIX (1, m_pHalMixer, LINE_OUT_1 + i,
				      LINE_PLAYMIX_2, CONTROL_MUTE,
				      0), (char *) "MUTEB", 0)) < 0)
		return ctl;
	}
    }

  return 0;
}

static USHORT
FindAdapter (PVOID pContext, PPCI_CONFIGURATION pPCI)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) pContext;

  pPCI->usVendorID = devc->vendorID;
  pPCI->usDeviceID = devc->deviceID;

  return (HSTATUS_OK);		// Already found this
}

static USHORT
MapAdapter (PVOID pContext, PPCI_CONFIGURATION pPCI)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) pContext;

  pPCI->Base[PCI_REGISTERS_INDEX].pvAddress = (PVOID) devc->bar0virt;
  pPCI->Base[PCI_REGISTERS_INDEX].ulSize = devc->bar0size;

  pPCI->Base[AUDIO_DATA_INDEX].pvAddress = (PVOID) devc->bar1virt;
  pPCI->Base[AUDIO_DATA_INDEX].ulSize = BAR1_SIZE;

  return (HSTATUS_OK);
}

static PHALADAPTER
GetAdapter (PVOID pContext, ULONG ulAdapterNumber)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) pContext;

  if (devc == NULL)
    return (NULL);

  return (PHALADAPTER) devc->m_pHalAdapter;
}

static USHORT
UnmapAdapter (PVOID pContext, PPCI_CONFIGURATION pPCI)
{
  return HSTATUS_OK;
}

static int nblocks = 0;
#define MAXBLOCKS	1000
static PVOID blocks[MAXBLOCKS];
static int sizes[MAXBLOCKS];
static oss_dma_handle_t handles[MAXBLOCKS];
/////////////////////////////////////////////////////////////////////////////
static USHORT
AllocateMemory (oss_device_t * osdev, PVOID * pObject, PVOID * pVAddr,
		PVOID * pPAddr, ULONG ulLength, ULONG ulAddressMask)
// Allocates Non-Paged Memory and returns both the virtual address and physical address
// Allows the caller to specify an address mask so the memory is aligned as required
// by the caller
/////////////////////////////////////////////////////////////////////////////
{
  DMAADDR ulAddress;
  oss_native_word phaddr;
  oss_dma_handle_t dma_handle;

  if (ulLength < 4096)
    ulLength = 4096;

  // this code won't work for DOS as it gives addresses in SEG:OFFSET instead of linear address space

  *pObject =
    (PVOID) CONTIG_MALLOC (osdev, ulLength, MEMLIMIT_32BITS, &phaddr,
			   dma_handle);
  ulAddress = ((DMAADDR) * pObject);
  *pVAddr = (PVOID) ulAddress;	// 
  *pPAddr = (PVOID) phaddr;

  if (nblocks < MAXBLOCKS)
    {
      blocks[nblocks] = *pObject;
      sizes[nblocks] = ulLength;
      handles[nblocks] = dma_handle;
      nblocks++;
    }

  return (HSTATUS_OK);
}

/////////////////////////////////////////////////////////////////////////////
static USHORT
FreeMemory (PVOID pObject, PVOID pVAddr)
// Free memory allocated with above procedure
/////////////////////////////////////////////////////////////////////////////
{
  int i;

  for (i = 0; i < nblocks; i++)
    if (pObject == blocks[i])
      {
	CONTIG_FREE (NULL, pObject, sizes[i], handles[i]);

	blocks[i] = blocks[nblocks - 1];
	sizes[i] = sizes[nblocks - 1];
	handles[i] = handles[nblocks - 1];
	nblocks--;
	return (HSTATUS_OK);
      }

  return (HSTATUS_OK);
}

/*
 * Mixer stuff
 */

EXTERN_C static int
lynxtwo_mixer_ioctl (int dev, int audiodev, unsigned int cmd, ioctl_arg arg)
{

  if (cmd == SOUND_MIXER_READ_CAPS)
    {
      return *arg = 0;
    }

  if (cmd == SOUND_MIXER_READ_DEVMASK ||
      cmd == SOUND_MIXER_READ_RECMASK || cmd == SOUND_MIXER_READ_RECSRC ||
      cmd == SOUND_MIXER_READ_STEREODEVS)
    return *arg = 0;

  if (cmd == SOUND_MIXER_READ_DEVMASK ||
      cmd == SOUND_MIXER_READ_RECMASK || cmd == SOUND_MIXER_READ_RECSRC ||
      cmd == SOUND_MIXER_READ_STEREODEVS)
    return *arg =
      SOUND_MASK_LINE | SOUND_MASK_PCM | SOUND_MASK_MIC |
      SOUND_MASK_VOLUME | SOUND_MASK_CD;

  if (cmd == SOUND_MIXER_READ_VOLUME || cmd == SOUND_MIXER_READ_PCM ||
      cmd == SOUND_MIXER_READ_LINE || cmd == SOUND_MIXER_READ_MIC ||
      cmd == SOUND_MIXER_READ_CD || cmd == MIXER_READ (SOUND_MIXER_DIGITAL1))
    return *arg = 100 | (100 << 8);
  if (cmd == SOUND_MIXER_WRITE_VOLUME || cmd == SOUND_MIXER_WRITE_PCM ||
      cmd == SOUND_MIXER_WRITE_LINE || cmd == SOUND_MIXER_READ_MIC ||
      cmd == SOUND_MIXER_WRITE_CD ||
      cmd == MIXER_WRITE (SOUND_MIXER_DIGITAL1))
    return *arg = 100 | (100 << 8);
  if (cmd == SOUND_MIXER_PRIVATE1)
    return *arg = 0;
  return *arg = 0;
}

static mixer_driver_t lynxtwo_mixer_driver = {
  lynxtwo_mixer_ioctl
};

EXTERN_C static int
lynxtwo_adapter_mix_init (int dev)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) mixer_devs[dev]->devc;
  PHALOSS pOSS = (PHALOSS) devc->m_pHalOSS;

  return pOSS->InitAdapterMixer (dev, devc);
}

EXTERN_C static int
aes16_adapter_mix_init (int dev)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) mixer_devs[dev]->devc;
  PHALOSS pOSS = (PHALOSS) devc->m_pHalOSS;

  return pOSS->InitAES16AdapterMixer (dev, devc);
}

EXTERN_C static int
lynxtwo_rec_mix_init (int dev)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) mixer_devs[dev]->devc;
  PHALOSS pOSS = (PHALOSS) devc->m_pHalOSS;

  return pOSS->InitRecMixer (dev, devc);
}

EXTERN_C static int
lynxtwo_play_mix_init (int dev)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) mixer_devs[dev]->devc;
  PHALOSS pOSS = (PHALOSS) devc->m_pHalOSS;

  return pOSS->InitPlayMixer (dev, devc);
}

static int
lynx_hal_init_mixer (lynxtwo_devc * devc)
{
  int mixdev;
  char tmp[100];

/*
 * Adapter mixer
 */
  sprintf (tmp, "%s Adapter", devc->card_name);
  if ((mixdev = oss_install_mixer (OSS_MIXER_DRIVER_VERSION,
				   devc->osdev,
				   devc->osdev,
				   tmp,
				   &lynxtwo_mixer_driver,
				   sizeof (mixer_driver_t), devc)) >= 0)
    {
      char tmp_name[100];
      devc->adapter_mixer_dev = mixdev;
      sprintf (tmp_name, "LYNXTWO_A%d", devc->card_number);
      devc->levels = load_mixer_volumes (tmp_name, NULL, 1);
      mixer_devs[mixdev]->priority = -2;	/* Don't use as the default mixer */
      if (devc->model == MDL_AES16 || devc->model == MDL_AES16_SRC)
	mixer_ext_set_init_fn (mixdev, aes16_adapter_mix_init, 100);
      else
	mixer_ext_set_init_fn (mixdev, lynxtwo_adapter_mix_init, 30);

    }
/*
 * Record mixer
 */
  sprintf (tmp, "%s Record/Play", devc->card_name);
  if ((mixdev = oss_install_mixer (OSS_MIXER_DRIVER_VERSION,
				   devc->osdev,
				   devc->osdev,
				   tmp,
				   &lynxtwo_mixer_driver,
				   sizeof (mixer_driver_t), devc)) >= 0)
    {
      devc->input_mixer_dev = mixdev;
      mixer_devs[mixdev]->priority = -2;	/* Don't use as the default mixer */
      mixer_devs[mixdev]->caps = MIXER_CAP_LAYOUT_B | MIXER_CAP_NARROW;
      mixer_ext_set_init_fn (mixdev, lynxtwo_rec_mix_init, 100);

    }
/*
 * Play mixer
 */
  sprintf (tmp, "%s Outputs", devc->card_name);
  if ((mixdev = oss_install_mixer (OSS_MIXER_DRIVER_VERSION,
				   devc->osdev,
				   devc->osdev,
				   tmp,
				   &lynxtwo_mixer_driver,
				   sizeof (mixer_driver_t), devc)) >= 0)
    {
      devc->output_mixer_dev = mixdev;
      mixer_devs[mixdev]->priority = -2;	/* Don't use as the default mixer */
      mixer_devs[mixdev]->caps = MIXER_CAP_LAYOUT_B | MIXER_CAP_NARROW;
      mixer_ext_set_init_fn (mixdev, lynxtwo_play_mix_init, 150);

    }
  return 1;
}

int
lynxhal_interrupt (lynxtwo_devc * devc)
{
  PHALADAPTER Adap;
  //int status;

  Adap = (PHALADAPTER) devc->m_pHalAdapter;
  if (Adap == NULL)
    {
      DDB (cmn_err (CE_WARN, "lynxhal_interrupt: Adap == NULL\n"));
      return 0;
    }

#if 1
  //if ((status=Adap->SaveInterruptContext()) != HSTATUS_OK) return 0;
  if (Adap->Service (TRUE) != HSTATUS_OK)
    {
      return 0;			/* Not for me */
    }
#else
/* 2-stage interrupt handler */
  if (Adap->SaveInterruptContext () != HSTATUS_OK)
    return 0;
  //Finally call Adap->Service (FALSE) in the second stage handler.
  if (Adap->Service (FALSE) != HSTATUS_OK)
    {
      return 0;			/* Not for me */
    }
#endif
    return 1;
}

#if 0
static void
lynx_timer_callback (void *dc)
{
  lynxtwo_devc *devc = (lynxtwo_devc *) dc;
  CHalAdapter *adap = (CHalAdapter *) devc->m_pHalAdapter;

  if (devc->timeout_id == 0)
    return;

  devc->timeout_id = 0;

  adap->Update ();

  devc->timeout_id = timeout (lynx_timer_callback, (void *) devc, OSS_HZ / 4);	/* 250ms */
}
#endif

int
init_lynx_hal (lynxtwo_devc * devc)
{
  HALDRIVERINFO DrvInfo;
  PHALADAPTER Adap;
  PHALOSS pOSS;
  int err;
  oss_native_word flags;

  devc->m_pHalAdapter = NULL;
  memset (&DrvInfo, 0, sizeof (HALDRIVERINFO));

  MUTEX_ENTER(devc->mutex, flags);
  DrvInfo.pFind = FindAdapter;
  cmn_err (CE_WARN, "init_lynx_hal FindAdapter\n");
  DrvInfo.pMap = MapAdapter;
  cmn_err (CE_WARN, "init_lynx_hal MapAdapter\n");
  DrvInfo.pGetAdapter = GetAdapter;
  cmn_err (CE_WARN, "init_lynx_hal GetAdapter\n");
  DrvInfo.pUnmap = UnmapAdapter;
  cmn_err (CE_WARN, "init_lynx_hal UnmapAdapter\n");
  DrvInfo.pAllocateMemory = AllocateMemory;
  cmn_err (CE_WARN, "init_lynx_hal AllocateMem\n");
  DrvInfo.pFreeMemory = FreeMemory;
  cmn_err (CE_WARN, "init_lynx_hal FreeMem\n");
  DrvInfo.pContext = (PVOID) devc;
  cmn_err (CE_WARN, "Drvinfo block done\n");

  Adap = new CHalAdapter (&DrvInfo, 0);

  Adap->m_osdev = devc->osdev;

  if ((err = Adap->Find ()) != 0)
    {
      cmn_err (CE_WARN, "Adapter find failed, err=%d\n", err);
      return 0;
    }

  if ((err = Adap->Open (0)) != 0)
    {
      cmn_err (CE_WARN, "Adapter open failed, err=%d\n", err);
      return 0;
    }

  devc->m_pHalAdapter = (void *) Adap;
  pOSS = new CHalOSS (devc, Adap);

  devc->m_pHalOSS = pOSS;
  MUTEX_EXIT(devc->mutex, flags);
  lynx_hal_init_mixer (devc);

  Adap->EnableInterrupts ();
  Adap->SetSyncStartState (TRUE);

#if 0
  devc->timeout_id = timeout (lynx_timer_callback, (void *) devc, OSS_HZ / 4);	/* 250ms */
#endif

  return 1;
}

int
uninit_lynx_hal (lynxtwo_devc * devc)
{
  CHalAdapter *adap = (CHalAdapter *) devc->m_pHalAdapter;
  PHALOSS pOSS = (PHALOSS) devc->m_pHalOSS;

#if 0
  if (devc->timeout_id != 0)
    {
      untimeout (devc->timeout_id);
      devc->timeout_id = 0;
    }
#endif
  adap->DisableInterrupts ();
  adap->Close (0);

  delete adap;
  delete pOSS;

  return 0;
}

#ifdef sun

#if 0
void *operator
new (unsigned long size)
{
  void *tmp;
  tmp = KERNEL_MALLOC (size);

  return tmp;
}
#endif

void operator
delete (void *obj)
{
  KERNEL_FREE (obj);
}

#if 0
void
__Crun::vector_des (void *a, unsigned b, unsigned c, void (*d) (void *))
{
  cmn_err (CE_PANIC, "__Crun::vector_des() got called\n");
}

void
__Crun::vector_con (void *a, unsigned int b, unsigned int c,
		    void (*d) (void *), void (*e) (void *))
{
  cmn_err (CE_PANIC, "__Crun::vector_con() got called.\n");
}
#endif
#endif
