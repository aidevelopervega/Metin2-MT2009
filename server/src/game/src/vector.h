#ifndef __INC_METIN_II_GAME_VECTOR_H__
#define __INC_METIN_II_GAME_VECTOR_H__

extern void		Normalize(VECTOR * pV1, VECTOR * pV2);
extern float    GetDegreeFromPosition(float x, float y);
extern float    GetDegreeFromPositionXY(long sx, long sy, long ex, long ey);
extern void     GetDeltaByDegree(float fDegree, float fDistance, float *x, float *y);
extern float	GetDegreeDelta(float iDegree, float iDegree2);
extern float	DotProduct(VECTOR* pV1, VECTOR* pV2);
extern void	RotateVector(VECTOR* vec, float rotationDegrees);

#endif

