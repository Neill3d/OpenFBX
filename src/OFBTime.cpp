
// OFBTime.cpp
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//

#include "OFBTime.h"

#define SECOND_LENGTH 46186158000L
#define MILLIS_LENGTH 46186158L

namespace ofbx
{


	//! Time constant: Infinity, the largest time value.
	const OFBTime OFBTime::Infinity = K_LONGLONG_MAX;

	//! Time constant: Minus Infinity, the lowest negative time value.
	const OFBTime OFBTime::MinusInfinity = K_LONGLONG_MIN;

	//! Time constant: Zero.
	const OFBTime OFBTime::Zero = 0;

	//! Time constant: Epsilon, the smallest time increment.
	const OFBTime OFBTime::Epsilon = 1;

	//! Time constant: One Second.
	const OFBTime OFBTime::OneSecond = SECOND_LENGTH;

	//! Time constant: One Minute.
	const OFBTime OFBTime::OneMinute = 60 * SECOND_LENGTH;

	//! Time constant: One Hour.
	const OFBTime OFBTime::OneHour = 3600 * SECOND_LENGTH;

	////////////////////////////////////////////////////////
	//

	OFBTime::OFBTime(kLongLong pTime)
		: mTime(pTime)
	{
	}

	OFBTime::OFBTime(int pHour, int pMinute, int pSecond, int pFrame, int pField, OFBTimeMode pTimeMode)
	{
		SetTime(pHour, pMinute, pSecond, pFrame, pField, pTimeMode);
	}

	std::string OFBTime::GetTimeString(OFBTimeMode pMode, ETimeFormats pFormat)
	{
		// TODO:
		return std::string("TODO:");
	}

	void OFBTime::SetTimeString(const char *pTime)
	{

	}

	bool OFBTime::GetTime(int &pHour, int &pMinute, int &pSecond, int &pFrame, int &pField, int &pMilliSecond, OFBTimeMode pTimeMode)
	{
		// TODO:
		return false;
	}

	kLongLong OFBTime::GetMilliSeconds()
	{
		return mTime / MILLIS_LENGTH;
	}

	void OFBTime::SetMilliSeconds(kLongLong pMilliSeconds)
	{
		mTime = pMilliSeconds * MILLIS_LENGTH;
	}

	kLongLong &OFBTime::Get()
	{
		return mTime;
	}

	const kLongLong &OFBTime::Get() const
	{
		return mTime;
	}

	void OFBTime::Set(kLongLong pTime)
	{
		mTime = pTime;
	}

	double OFBTime::GetSecondDouble()
	{
		return double(mTime) / SECOND_LENGTH;
	}

	void OFBTime::SetSecondDouble(double pTime)
	{
		mTime = kLongLong(pTime * SECOND_LENGTH);
	}

	void OFBTime::SetTime(int pHour, int pMinute, int pSecond, int pFrame, int pField, OFBTimeMode pTimeMode)
	{
		// TODO: fps according to a time mode or use default system timemode ?!
		double fps = 30.0;
		double secs = 3600.0*pHour + 60.0 * (double)pMinute + (double)pSecond + pFrame / fps;
		SetSecondDouble(secs);
	}

	kLongLong OFBTime::GetFrame(OFBTimeMode pTimeMode)
	{
		const double fps = 30.0;
		const double secs = GetSecondDouble();

		return kLongLong(secs * fps);
	}

	void OFBTime::SetFrame(kLongLong pFrames, OFBTimeMode pTimeMode)
	{
		const double fps = 30.0;
		double secs = 1.0 * double(pFrames) / fps;

		SetSecondDouble(secs);
	}


	OFBTime& OFBTime::operator=	(const OFBTime& pTime)
	{
		mTime = pTime.mTime;
		return *this;
	}
	OFBTime& OFBTime::operator+=	(const OFBTime& pTime)
	{
		mTime += pTime.mTime;
		return *this;
	}
	OFBTime& OFBTime::operator-=	(const OFBTime& pTime)
	{
		mTime -= pTime.mTime;
		return *this;
	}
	OFBTime&	OFBTime::operator*=	(const OFBTime& pTime)
	{
		mTime *= pTime.mTime;
		return *this;
	}
	OFBTime&	OFBTime::operator/=	(const OFBTime& pTime)
	{
		mTime /= pTime.mTime;
		return *this;
	}
	//@}

	//@{
	/**	Overloaded assignment operators with constants.
	*	\param	pConstant	Time to assign with operator.
	*	\return	Resulting time from operation.
	*/
	OFBTime& OFBTime::operator=	(double pConstant)
	{
		mTime = kLongLong(pConstant);
		return *this;
	}
	OFBTime& OFBTime::operator+=	(double pConstant)
	{
		mTime += kLongLong(pConstant);
		return *this;
	}
	OFBTime& OFBTime::operator-=	(double pConstant)
	{
		mTime -= kLongLong(pConstant);
		return *this;
	}
	OFBTime&	OFBTime::operator*=	(double pConstant)
	{
		mTime *= kLongLong(pConstant);
		return *this;
	}
	OFBTime&	OFBTime::operator/=	(double pConstant)
	{
		mTime /= kLongLong(pConstant);
		return *this;
	}
	//@}

	//@{
	/**	Overloaded arithmetic operators with FBTime objects.
	*	\param	pTime	Time to use in operation.
	*	\return	Result in FBTime data.
	*/
	OFBTime	OFBTime::operator- (const OFBTime &pTime)
	{
		OFBTime newTime(mTime - pTime.Get());
		return newTime;
	}
	OFBTime	OFBTime::operator+ (const OFBTime &pTime)
	{
		OFBTime newTime(mTime + pTime.Get());
		return newTime;
	}
	OFBTime	OFBTime::operator/ (const OFBTime &pTime)
	{
		OFBTime newTime(mTime / pTime.Get());
		return newTime;
	}
	OFBTime	OFBTime::operator* (const OFBTime &pTime)
	{
		OFBTime newTime(mTime * pTime.Get());
		return newTime;
	}
	//@}

	//@{
	/**	Overloaded arithmetic operators with constants.
	*	\param	pConstant	Double constant to add to time.
	*	\return	Result in FBTime data.
	*/
	OFBTime	OFBTime::operator- (double pConstant)
	{
		OFBTime newTime(mTime - kLongLong(pConstant));
		return newTime;
	}
	OFBTime	OFBTime::operator+ (double pConstant)
	{
		OFBTime newTime(mTime + kLongLong(pConstant));
		return newTime;
	}
	OFBTime	OFBTime::operator/ (double pConstant)
	{
		OFBTime newTime(mTime / kLongLong(pConstant));
		return newTime;
	}
	OFBTime	OFBTime::operator* (double pConstant)
	{
		OFBTime newTime(mTime * kLongLong(pConstant));
		return newTime;
	}
	//@}

	//@{
	/** Overloaded comparison operators.
	*	\param	pTime	Time to compare with.
	*	\return	Result of comparison.
	*/
	bool	OFBTime::operator==	(const OFBTime& pTime)
	{
		return (mTime == pTime.mTime);
	}
	bool	OFBTime::operator!=	(const OFBTime& pTime)
	{
		return (mTime != pTime.mTime);
	}
	bool	OFBTime::operator>=	(const OFBTime& pTime)
	{
		return (mTime >= pTime.mTime);
	}
	bool	OFBTime::operator<=	(const OFBTime& pTime)
	{
		return (mTime <= pTime.mTime);
	}
	bool	OFBTime::operator>	(const OFBTime& pTime)
	{
		return (mTime > pTime.mTime);
	}
	bool	OFBTime::operator<	(const OFBTime& pTime)
	{
		return (mTime < pTime.mTime);
	}

};