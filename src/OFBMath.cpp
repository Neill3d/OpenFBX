
// OFBMath.cpp
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//

#include "OFBMath.h"
#include <math.h>
#include <assert.h>
#include <algorithm>

#define TO_RAD  3.1415926535897932384626433832795028 / 180.0

namespace ofbx
{
	const OFBVector3 Vector_Zero()
	{
		return { 0.0, 0.0, 0.0 };
	}

	const OFBVector3 Vector_One()
	{
		return{ 1.0, 1.0, 1.0 };
	}

	void setTranslation(const OFBVector3& t, OFBMatrix* mtx)
	{
		mtx->m[12] = t.x;
		mtx->m[13] = t.y;
		mtx->m[14] = t.z;
	}


	OFBVector3 operator-(const OFBVector3& v)
	{
		return{ -v.x, -v.y, -v.z };
	}


	OFBMatrix operator*(const OFBMatrix& lhs, const OFBMatrix& rhs)
	{
		OFBMatrix res;
		for (int j = 0; j < 4; ++j)
		{
			for (int i = 0; i < 4; ++i)
			{
				double tmp = 0;
				for (int k = 0; k < 4; ++k)
				{
					tmp += lhs.m[i + k * 4] * rhs.m[k + j * 4];
				}
				res.m[i + j * 4] = tmp;
			}
		}
		return res;
	}


	OFBMatrix makeIdentity()
	{
		return{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	}


	OFBMatrix rotationX(double angle)
	{
		OFBMatrix m = makeIdentity();
		double c = cos(angle);
		double s = sin(angle);

		m.m[5] = m.m[10] = c;
		m.m[9] = -s;
		m.m[6] = s;

		return m;
	}


	OFBMatrix rotationY(double angle)
	{
		OFBMatrix m = makeIdentity();
		double c = cos(angle);
		double s = sin(angle);

		m.m[0] = m.m[10] = c;
		m.m[8] = s;
		m.m[2] = -s;

		return m;
	}


	OFBMatrix rotationZ(double angle)
	{
		OFBMatrix m = makeIdentity();
		double c = cos(angle);
		double s = sin(angle);

		m.m[0] = m.m[5] = c;
		m.m[4] = -s;
		m.m[1] = s;

		return m;
	}


	OFBMatrix getRotationMatrix(const OFBVector3& euler, OFBRotationOrder order)
	{
		OFBMatrix rx = rotationX(euler.x * TO_RAD);
		OFBMatrix ry = rotationY(euler.y * TO_RAD);
		OFBMatrix rz = rotationZ(euler.z * TO_RAD);
		switch (order) {
		default:
		case OFBRotationOrder::eSPHERIC_XYZ:
			assert(false);
		case OFBRotationOrder::eEULER_XYZ:
			return rz * ry * rx;
		case OFBRotationOrder::eEULER_XZY:
			return ry * rz * rx;
		case OFBRotationOrder::eEULER_YXZ:
			return rz * rx * ry;
		case OFBRotationOrder::eEULER_YZX:
			return rx * rz * ry;
		case OFBRotationOrder::eEULER_ZXY:
			return ry * rx * rz;
		case OFBRotationOrder::eEULER_ZYX:
			return rx * ry * rz;
		}
	}

	double	QuaternionNorm(const OFBVector4 &q)
	{
		return (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
	}


	void QuaternionToMatrix(OFBMatrix &m, const OFBVector4 &rotation)
	{
		double wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;
		double s = 2.0 / QuaternionNorm(rotation);  // 4 mul 3 add 1 div
		x2 = rotation[0] * s;    y2 = rotation[1] * s;    z2 = rotation[2] * s;
		xx = rotation[0] * x2;   xy = rotation[0] * y2;   xz = rotation[0] * z2;
		yy = rotation[1] * y2;   yz = rotation[1] * z2;   zz = rotation[2] * z2;
		wx = rotation[3] * x2;   wy = rotation[3] * y2;   wz = rotation[3] * z2;

		m(0, 0) = 1.0 - (yy + zz);
		m(1, 0) = xy - wz;
		m(2, 0) = xz + wy;

		m(0, 1) = xy + wz;
		m(1, 1) = 1.0 - (xx + zz);
		m(2, 1) = yz - wx;

		m(0, 2) = xz - wy;
		m(1, 2) = yz + wx;
		m(2, 2) = 1.0 - (xx + yy);
	}


	double fbxTimeToSeconds(i64 value)
	{
		return double(value) / 46186158000L;
	}


	i64 secondsToFbxTime(double value)
	{
		return i64(value * 46186158000L);
	}


	OFBVector3 operator*(const OFBVector3& v, float f)
	{
		return{ v.x * f, v.y * f, v.z * f };
	}

	OFBVector3 operator-(const OFBVector3& a, const OFBVector3& b)
	{
		return{ a.x - b.x, a.y - b.y, a.z - b.z };
	}

	OFBVector3 operator+(const OFBVector3& a, const OFBVector3& b)
	{
		return{ a.x + b.x, a.y + b.y, a.z + b.z };
	}

	bool VectorIsZero(const OFBVector3 &v)
	{
		return (0.0 == v.x && 0.0 == v.y && 0.0 == v.z);
	}

	double DotProduct(const OFBVector3 &v1, const OFBVector3 &v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	OFBVector3 CrossProduct(const OFBVector3 &left, const OFBVector3 &right)
	{
		OFBVector3	dest;

		dest.x = left.y * right.z -
			left.z * right.y;
		dest.y = left.z * right.x -
			left.x * right.z;
		dest.z = left.x * right.y -
			left.y * right.x;
		return dest;
	}

	double VectorLength(const OFBVector3 &v)
	{
		double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return len;
	}

	void VectorNormalize(OFBVector3	&v)
	{
		double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

		if (len != 0.0)
		{
			len = 1.0 / len;
			v.x *= len;
			v.y *= len;
			v.z *= len;
		}
	}

	void VectorTransform(OFBVector3 &res, const OFBVector3 &in1, const OFBMatrix &in2)
	{
		res.x = DotProduct(in1, { in2.m[0], in2.m[4], in2.m[8] }) + in2.m[12];
		res.y = DotProduct(in1, { in2.m[1], in2.m[5], in2.m[9] }) + in2.m[13];
		res.z = DotProduct(in1, { in2.m[2], in2.m[6], in2.m[10] }) + in2.m[14];
	}

	void VectorTransform33(OFBVector3 &res, const OFBVector3 &in1, const OFBMatrix &in2)
	{
		res.x = DotProduct(in1, { in2.m[0], in2.m[4], in2.m[8] });
		res.y = DotProduct(in1, { in2.m[1], in2.m[5], in2.m[9] });
		res.z = DotProduct(in1, { in2.m[2], in2.m[6], in2.m[10] });
	}

	OFBVector3 MatrixGetColumn(const OFBMatrix &mat, const int index)
	{
		return{ mat.m[0+index], mat.m[4+index], mat.m[8+index] };
	}

	OFBVector3 MatrixGetScale(const OFBMatrix &m)
	{
		return{ VectorLength(MatrixGetColumn(m, 0)), VectorLength(MatrixGetColumn(m, 1)),
			VectorLength(MatrixGetColumn(m, 2)) };
	}

	double Sign(double value)
	{
		return (value >= 0.0) ? 1.0 : -1.0;
	}

	OFBVector4 MatrixGetRotation(const OFBMatrix &m)
	{
		OFBVector3 s = MatrixGetScale(m);

		// Normalize Scale from Matrix4x4
		double m00 = m(0, 0) / s.x;
		double m01 = m(0, 1) / s.y;
		double m02 = m(0, 2) / s.z;
		double m10 = m(1, 0) / s.x;
		double m11 = m(1, 1) / s.y;
		double m12 = m(1, 2) / s.z;
		double m20 = m(2, 0) / s.x;
		double m21 = m(2, 1) / s.y;
		double m22 = m(2, 2) / s.z;

		OFBVector4 q;
		q.w = sqrt(std::max(0.0, 1.0 + m00 + m11 + m22)) * 0.5;
		q.x = sqrt(std::max(0.0, 1.0 + m00 - m11 - m22)) * 0.5;
		q.y = sqrt(std::max(0.0, 1.0 - m00 + m11 - m22)) * 0.5;
		q.z = sqrt(std::max(0.0, 1.0 - m00 - m11 + m22)) * 0.5;
		q.x *= Sign(q.x * (m21 - m12));
		q.y *= Sign(q.y * (m02 - m20));
		q.z *= Sign(q.z * (m10 - m01));

		// q.Normalize()
		double qMagnitude = 1.0 / sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
		q.w *= qMagnitude;
		q.x *= qMagnitude;
		q.y *= qMagnitude;
		q.z *= qMagnitude;

		return q;
	}

	void LookAt(OFBMatrix &Matrix, const OFBVector3& Eye, const OFBVector3& Center, const OFBVector3& Up)
	{
		OFBVector3 X, Y, Z;

		Z = Eye - Center;
		//VectorSub(Eye, Center, Z);
		VectorNormalize(Z);

		Y = Up;
		X = CrossProduct(Y, Z);
		Y = CrossProduct(Z, X);

		VectorNormalize(X);
		VectorNormalize(Y);

		Matrix = makeIdentity();
		Matrix(0, 0) = X[0];
		Matrix(1, 0) = X[1];
		Matrix(2, 0) = X[2];
		Matrix(3, 0) = -DotProduct(X, Eye);
		Matrix(0, 1) = Y[0];
		Matrix(1, 1) = Y[1];
		Matrix(2, 1) = Y[2];
		Matrix(3, 1) = -DotProduct(Y, Eye);
		Matrix(0, 2) = Z[0];
		Matrix(1, 2) = Z[1];
		Matrix(2, 2) = Z[2];
		Matrix(3, 2) = -DotProduct(Z, Eye);
	}

	// ported from nv_math
	OFBMatrix & frustum(OFBMatrix& M, const double l, const double r, const double b,
		const double t, const double n, const double f)
	{
		M(0,0) = (2.0*n) / (r - l);
		M(1,0) = 0.0;
		M(2,0) = 0.0;
		M(3,0) = 0.0;

		M(0,1) = 0.0;
		M(1,1) = (2.0*n) / (t - b);
		M(2,1) = 0.0;
		M(3,1) = 0.0;

		M(0,2) = (r + l) / (r - l);
		M(1,2) = (t + b) / (t - b);
		M(2,2) = -(f + n) / (f - n);
		M(3,2) = -1.0;

		M(0,3) = 0.0;
		M(1,3) = 0.0;
		M(2,3) = -(2.0*f*n) / (f - n);
		M(3,3) = 0.0;
		return M;
	}

	OFBMatrix & perspective(OFBMatrix& M, const double fovy, const double aspect, const double n, const double f)
	{
		double xmin, xmax, ymin, ymax;

		ymax = n * tanf(fovy * TO_RAD * 0.5);
		ymin = -ymax;

		xmin = ymin * aspect;
		xmax = ymax * aspect;

		return frustum(M, xmin, xmax, ymin, ymax, n, f);
	}

	OFBMatrix & ortho(OFBMatrix & M, const double left,
		const double right,
		const double bottom,
		const double top,
		const double n,
		const double f)
	{
		M(0,0) = 2.0 / (right - left);
		M(0,1) = 0.0;
		M(0,2) = 0.0;
		M(0,3) = -(right + left) / (right - left);
		M(1,0) = 0.0;
		M(1,1) = 2.0 / (top - bottom);
		M(1,2) = 0.0;
		M(1,3) = -(top + bottom) / (top - bottom);
		M(2,0) = 0.0;
		M(2,1) = 0.0;
		M(2,2) = -2.0 / (f - n);
		M(2,3) = -(f + n) / (f - n);
		M(3,0) = 0.0;
		M(3,1) = 0.0;
		M(3,2) = 0.0;
		M(3,3) = 1.0;
		return M;
	}

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
		double  pFilmOffsetY)
	{
		perspective(proj, pFieldOfViewY, pAspect, pNearPlane, pFarPlane);
		LookAt(modelView, pEye, pCenter, pUp);
	}

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
		const OFBVector3 &lUp)
	{
		ortho(proj, lLeftPlane, lRightPlane, lBottomPlane, lTopPlane, lNearPlane, lFarPlane);
		LookAt(modelView, lEye, lCenter, lUp);
	}
	

	OFBMatrix &MatrixMult(OFBMatrix &dest, const OFBMatrix &M1, const OFBMatrix &M2)
	{
		
		const double*  L;
		const double*  R;

		L = &M1.m[0];
		R = &M2.m[0];

		double* D = &dest.m[0];
		for (int i = 0; i<4; i++, D += 4, R += 4)
		{
			D[0] = L[0] * R[0] + L[4] * R[1] + L[8] * R[2] + L[12] * R[3];
			D[1] = L[1] * R[0] + L[5] * R[1] + L[9] * R[2] + L[13] * R[3];
			D[2] = L[2] * R[0] + L[6] * R[1] + L[10] * R[2] + L[14] * R[3];
			D[3] = L[3] * R[0] + L[7] * R[1] + L[11] * R[2] + L[15] * R[3];
		}
		return dest;
	}


};