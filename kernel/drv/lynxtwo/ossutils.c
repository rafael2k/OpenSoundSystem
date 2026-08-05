#include "lynxtwo_cfg.h"
#ifdef sun

void *
__1c2n6FL_pv_ (unsigned long size)	// operator new
{
  void *tmp;
  tmp = KERNEL_MALLOC (size);

  return tmp;
}

void
__1cG__CrunKvector_des6FpvLLpF1_v_v_ (void *a, unsigned long b,
				      unsigned long c, void (*d) (void *))
{
//      cmn_err(CE_CONT, "__Crun::vector_des(%x, %u, %u, %x)\n", a, b, c, d);
}

void
__1cG__CrunKvector_des6FpvIIpF1_v_v_ (void *a, unsigned b, unsigned c,
				      void (*d) (void *))
{
//      cmn_err(CE_CONT, "__Crun::vector_des(%x, %u, %u, %x)\n", a, b, c, d);
}

#ifdef sparc
void
__1cG__CrunKvector_con6FpvLLpF1_vp2_v_ (void *a, unsigned long b,
					unsigned long c, void (*d) (void *),
					void (*e) (void *))
{
//      cmn_err(CE_CONT, "__Crun::vector_con(%x, %u, %u, %x, %x)\n", a, b, c, d, e);
}

#else
void
__1cG__CrunKvector_con6FpvIIpF1_vp2_v_ (void *a, unsigned int b,
					unsigned int c, void (*d) (void *),
					void (*e) (void *))
{
//      cmn_err(CE_CONT, "__Crun::vector_con(%x, %u, %u, %x, %x)\n", a, b, c, d, e);
}

void
__1cG__CrunKvector_con6FpvLLpF1_v3_v_ (void *a, unsigned long b,
				       unsigned long c, void (*d) (void *),
				       void (*e) (void *))
{
//      cmn_err(CE_CONT, "__Crun::vector_con(%x, %u, %u, %x, %x)\n", a, b, c, d, e);
}
#endif /* sparc */
#endif
