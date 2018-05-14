
#ifndef _OFBRENDERER_H_
#define _OFBRENDERER_H_

#include "OFBTypes.h"

namespace ofbx
{

	///////////////////////////////////////////////////////////////////
	// OFBRenderConveyer

	class OFBRenderConveyer
	{
	public:
		//! a constructor
		OFBRenderConveyer();

		// custom display lines

		virtual void PushLine(const OFBVector3 &a, const OFBVector3 &b) = 0;
	};

};

#endif