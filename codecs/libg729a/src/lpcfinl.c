/*
 File : LPCFINL.C
*/

#include <math.h>

/*----------------------------------------------------------------------------
* lsf_lsp - convert from lsf[0..M-1 to lsp[0..M-1]
*----------------------------------------------------------------------------
*/
INLINE_PREFIX void lsf_lsp(
    const GFLOAT lsf[],          /* input :  lsf */
    GFLOAT lsp[],          /* output: lsp */
    int m
)
{
    for (; m; --m, ++lsp, ++lsf)
        *lsp = (GFLOAT)cos((double)*lsf);
}

/*----------------------------------------------------------------------------
* lsp_lsf - convert from lsp[0..M-1 to lsf[0..M-1]
*----------------------------------------------------------------------------
*/
INLINE_PREFIX void lsp_lsf(
    const GFLOAT lsp[],          /* input :  lsp coefficients */
    GFLOAT lsf[],          /* output:  lsf (normalized frequencies */
    int m
)
{
    for (; m; --m, ++lsf, ++lsp)
        *lsf = (GFLOAT)acos((double)*lsp);
}
