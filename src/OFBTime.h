
#ifndef _OFBTIME_H_
#define _OFBTIME_H_

// OFBTime.h
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//

#include "OFBTypes.h"
#include <string>

namespace ofbx
{


	////////////////////////////////////////////////////////////////////////////////////
	// FBTime
	////////////////////////////////////////////////////////////////////////////////////

	//! Different time modes available.
	enum OFBTimeMode {
		eTimeModeDefault, 		//!< Default Time Mode
		eTimeMode1000Frames,		//!< 1000 : 1 millisecond
		eTimeMode120Frames, 		//!< 120
		eTimeMode100Frames,		//!< 100
		eTimeMode96Frames,		//!< 96
		eTimeMode72Frames,		//!< 72
		eTimeMode60Frames,		//!< 60
		eTimeMode5994Frames,		//!< ~59.94
		eTimeMode50Frames,		//!< 50
		eTimeMode48Frames,		//!< 48
		eTimeMode30Frames,		//!< 30 
		eTimeMode2997Frames_Drop,	//!< ~29.97 drop
		eTimeMode2997Frames,		//!< ~29.97 full
		eTimeMode25Frames,		//!< 25 
		eTimeMode24Frames,		//!< 24 
		eTimeMode23976Frames,		//!< ~23.976
		eTimeModeCustom           //!< Custom framerate
	};

	
	//! Time data structure.
	class OFBTime
	{
	private:
		kLongLong	mTime;			//!< Time member.
	public:

		//! Different time format available.
		enum ETimeFormats
		{
			eSMPTE,			//!< format as SMPTE
			eFrame,			//!< format as numeric frame
			eDefaultFormat	//!< Default Time format
		};

		/*	Constructor.
		*	\param	pTime	Time to initialize with(default=0).
		*/
		OFBTime(kLongLong pTime = 0);

		/**	Constructor.
		*	\param	pHour		Hour value.
		*	\param	pMinute		Minute value.
		*	\param	pSecond		Second value.
		*	\param	pFrame		Frame value.
		*	\param	pField		Field value.
		*	\param	pTimeMode	Time mode(default=kFBTimeModeDefault).
		*/
		OFBTime(int pHour, int pMinute, int pSecond = 0, int pFrame = 0, int pField = 0, OFBTimeMode pTimeMode = eTimeModeDefault);

		/**	Get time as a string.
		*	\param	pMode		Time mode (default=kFBTimeModeDefault) to use (call FBSystem().GetTransportFps() to the the current UI displayed mode).
		*	\param	pFormat		Format to use for the returned string(default=FBTime::eDefaultFormat).
		*	\return String value of time.
		*/
		std::string GetTimeString(OFBTimeMode pMode = eTimeModeDefault, ETimeFormats pFormat = eDefaultFormat);

		/**	Set time from string.
		*	\param	pTime	String to set time from.
		*/
		void SetTimeString(const char *pTime);

		/**	Get time (filling separate values)
		*	\retval	pHour			Hour value.
		*	\retval	pMinute			Minute value.
		*	\retval	pSecond			Second value.
		*	\retval	pFrame			Frame value.
		*	\retval	pField			Field value.
		*	\retval	pMilliSecond	MilliSecond value.
		*	\param	pTimeMode		Time mode to get time as.
		*	\return	\b true if an acceptable \e TimeMode value was chosen.
		*/
		bool		GetTime(int &pHour, int &pMinute, int &pSecond, int &pFrame, int &pField, int &pMilliSecond, OFBTimeMode pTimeMode = eTimeModeDefault);
		/**	Get milliseconds for time.
		*	\return	MilliSeconds value.
		*/
		kLongLong	GetMilliSeconds();

		/**	Set milliseconds time.
		*	\param	pMilliSeconds	MilliSeconds value.
		*/
		void		SetMilliSeconds(kLongLong pMilliSeconds);

		/**	Get time value (long)
		*	\return Time value as long.
		*/
		kLongLong&	Get();

		/**	Get time value (long)
		*	\return Time value as long.
		*/
		const kLongLong& Get() const;

		/** Set time value from a long.
		*	\param	pTime	Time value to set.
		*/
		void		Set(kLongLong pTime);

		/**	Get seconds as double.
		*	\return Seconds in double form.
		*/
		double		GetSecondDouble();

		/**	Set seconds from double.
		*	\param	pTime	Time to set seconds from.
		*/
		void		SetSecondDouble(double pTime);

		/**	Set time (from separate values)
		*	\param	pHour			Hour value.
		*	\param	pMinute			Minute value(default=0).
		*	\param	pSecond			Second value(default=0).
		*	\param	pFrame			Frame value(default=0).
		*	\param	pField			Field value(default=0).
		*	\param	pTimeMode		Time mode to get time as(default=kFBTimeModeDefault).
		*/
		void		SetTime(int pHour, int pMinute = 0, int pSecond = 0, int pFrame = 0, int pField = 0, OFBTimeMode pTimeMode = eTimeModeDefault);

		//@{
		/**	Overloaded assignment operators with FBTime objects.
		*	\param	pTime	Time to assign with operator.
		*	\return	Resulting time from operation.
		*/
		OFBTime& operator=	(const OFBTime& pTime);
		OFBTime& operator+=	(const OFBTime& pTime);
		OFBTime& operator-=	(const OFBTime& pTime);
		OFBTime&	operator*=	(const OFBTime& pTime);
		OFBTime&	operator/=	(const OFBTime& pTime);
		//@}

		//@{
		/**	Overloaded assignment operators with constants.
		*	\param	pConstant	Time to assign with operator.
		*	\return	Resulting time from operation.
		*/
		OFBTime& operator=	(double pConstant);
		OFBTime& operator+=	(double pConstant);
		OFBTime& operator-=	(double pConstant);
		OFBTime&	operator*=	(double pConstant);
		OFBTime&	operator/=	(double pConstant);
		//@}

		//@{
		/**	Overloaded arithmetic operators with FBTime objects.
		*	\param	pTime	Time to use in operation.
		*	\return	Result in FBTime data.
		*/
		OFBTime	operator- (const OFBTime &pTime);
		OFBTime	operator+ (const OFBTime &pTime);
		OFBTime	operator/ (const OFBTime &pTime);
		OFBTime	operator* (const OFBTime &pTime);
		//@}

		//@{
		/**	Overloaded arithmetic operators with constants.
		*	\param	pConstant	Double constant to add to time.
		*	\return	Result in FBTime data.
		*/
		OFBTime	operator- (double pConstant);
		OFBTime	operator+ (double pConstant);
		OFBTime	operator/ (double pConstant);
		OFBTime	operator* (double pConstant);
		//@}

		//@{
		/** Overloaded comparison operators.
		*	\param	pTime	Time to compare with.
		*	\return	Result of comparison.
		*/
		bool	operator==	(const OFBTime& pTime);
		bool	operator!=	(const OFBTime& pTime);
		bool	operator>=	(const OFBTime& pTime);
		bool	operator<=	(const OFBTime& pTime);
		bool	operator>	(const OFBTime& pTime);
		bool	operator<	(const OFBTime& pTime);
		//@}

		/**	Get the frame count.
		*	With this function, it is possible to obtain the cumulative and local frame counts.
		*	\param	pTimeMode	Time mode to get the constant (default is kFBTimeModeDefault).
		*	\return	Frames per second constant for the specified time mode.
		*/
		kLongLong	GetFrame(OFBTimeMode pTimeMode = eTimeModeDefault);

		/** Set time in frame format.
		* \param pFrames The number of frames.
		* \param pTimeMode The time mode identifier which will dictate the extraction algorithm.
		*/
		void SetFrame(kLongLong pFrames, OFBTimeMode pTimeMode = eTimeModeDefault);

		//! Time constant: Infinity, the largest time value.
		static const OFBTime Infinity;

		//! Time constant: Minus Infinity, the lowest negative time value.
		static const OFBTime MinusInfinity;

		//! Time constant: Zero.
		static const OFBTime Zero;

		//! Time constant: Epsilon, the smallest time increment.
		static const OFBTime Epsilon;

		//! Time constant: One Second.
		static const OFBTime OneSecond;

		//! Time constant: One Minute.
		static const OFBTime OneMinute;

		//! Time constant: One Hour.
		static const OFBTime OneHour;
	};

};

#endif