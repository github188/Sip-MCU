/*
 File : UTILINL.C
*/

#include <string.h>
#include <memory.h>

INLINE_PREFIX void
set_zero(GFLOAT dst[], int L)
{
	memset(dst, 0, sizeof(GFLOAT) * L);
}

INLINE_PREFIX void
copy(const GFLOAT src[], GFLOAT dst[], int L)
{
	memcpy(dst, src, sizeof(GFLOAT) * L);
}

INLINE_PREFIX INT16
random_g729(INT16 *seed)
{
	return *seed = (INT16) ((*seed) * 31821L + 13849L);
}
