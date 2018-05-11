
#ifndef _OFBMATH_H_
#define _OFBMATH_H_

// OFBMath.h
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//	

#include "OFBTypes.h"

namespace ofbx
{
	
	enum class OFBRotationOrder {
		eEULER_XYZ,
		eEULER_XZY,
		eEULER_YZX,
		eEULER_YXZ,
		eEULER_ZXY,
		eEULER_ZYX,
		eSPHERIC_XYZ // Currently unsupported. Treated as EULER_XYZ.
	};


	// Return value is 1 when f is positive or zero, -1 when f is negative.
	double Sign(double value);

	// vector3

	const OFBVector3 Vector_Zero();
	const OFBVector3 Vector_One();

	bool VectorIsZero(const OFBVector3 &v);

	double DotProduct(const OFBVector3 &v1, const OFBVector3 &v2);
	OFBVector3 CrossProduct(const OFBVector3 &left, const OFBVector3 &right);

	double VectorLength(const OFBVector3 &v);
	void VectorNormalize(OFBVector3	&v);

	void VectorTransform(OFBVector3 &res, const OFBVector3 &in1, const OFBMatrix &in2);
	void VectorTransform33(OFBVector3 &res, const OFBVector3 &in1, const OFBMatrix &in2);

	void setTranslation(const OFBVector3& t, OFBMatrix* mtx);
	OFBVector3 operator-(const OFBVector3& v);
	OFBVector3 operator*(const OFBVector3& v, float f);
	OFBVector3 operator-(const OFBVector3& a, const OFBVector3& b);
	OFBVector3 operator+(const OFBVector3& a, const OFBVector3& b);

	// quat
	double	QuaternionNorm(const OFBVector4 &q);

	// conversions
	void QuaternionToMatrix(OFBMatrix &m, const OFBVector4 &rotation);
	
	// matrix 4x4

	OFBMatrix makeIdentity();
	OFBMatrix rotationX(double angle);
	OFBMatrix rotationY(double angle);
	OFBMatrix rotationZ(double angle);
	OFBMatrix getRotationMatrix(const OFBVector3& euler, OFBRotationOrder order);

	OFBVector3 MatrixGetScale(const OFBMatrix &m);
	OFBVector3 MatrixGetColumn(const OFBMatrix &m, const int index);
	OFBVector4 MatrixGetRotation(const OFBMatrix &m);

	OFBMatrix operator * (const OFBMatrix&, const OFBMatrix&);

	OFBMatrix &MatrixMult(OFBMatrix &dest, const OFBMatrix &M1, const OFBMatrix &M2);

	// camera matrix

	void GetCameraPerspectiveMatrix(
		OFBMatrix &proj,
		OFBMatrix &modelView,
		double pFieldOfViewY,
		double pAspect,
		double pNearPlane,
		double pFarPlane,
		const OFBVector3& pEye,
		const OFBVector3& pCenter,
		const OFBVector3& pUp,
		double  pFilmOffsetX,
		double  pFilmOffsetY);

	void GetCameraOrthogonal(
		OFBMatrix &proj,
		OFBMatrix &modelView, 
		double lLeftPlane,
		double lRightPlane,
		double lBottomPlane,
		double lTopPlane,
		double lNearPlane,
		double lFarPlane,
		const OFBVector3 &lEye,
		const OFBVector3 &lCenter,
		const OFBVector3 &lUp);

	// time convertion

	double fbxTimeToSeconds(i64 value);
	i64 secondsToFbxTime(double value);
};

#endif