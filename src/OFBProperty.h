
#ifndef _FBPROPERTY_H_
#define _FBPROPERTY_H_

// OFBProperty.h
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//

#include "OFBTypes.h"
#include "OFBTime.h"
#include <vector>


namespace ofbx
{

	// forward declaration
	struct AnimationCurveNode;
	struct AnimationLayer;
	struct Object;

	class PropertyBase;
	class PropertyList;

	//! \enum FBPropertyType
	/**Property types
	*/
	enum PropertyType
	{
		ePT_unknown,	//!< unknow.
		ePT_int,		//!< int.
		ePT_int64,	//!< int 64 bits.
		ePT_uint64,	//!< unsigned int 64 bits.
		ePT_bool,		//!< bool.
		ePT_float,	//!< float.
		ePT_double,	//!< double.
		ePT_charptr,	//!< charptr.
		ePT_enum,		//!< enum.
		ePT_Time,		//!< time.
		ePT_TimeCode,	//!< timecode.
		ePT_object,	//!< object.
		ePT_event,	//!< event.
		ePT_stringlist,//!< stringlist.
		ePT_Vector4D,	//!< vector4d.
		ePT_Vector3D,	//!< vector3d.
		ePT_ColorRGB,	//!< colorrgb.
		ePT_ColorRGBA,//!< colorrgba.
		ePT_Action,	//!< action.
		ePT_Reference,//!< reference.
		ePT_TimeSpan,	//!< timespan.
		ePT_kReference,//!< kReference.
		ePT_Vector2D	//!< vector2d.
	};

	///////////////////////////////////////////////////////////////////////////////////////////////
	// PropertyList

	class PropertyList
	{
	public:

		//! a constructor
		PropertyList(Object *pParent);

		//
		bool				Add(PropertyBase *pProp);

		PropertyBase		*GetFirst() const;
		PropertyBase		*Find(const char *name) const;

		// attach nodes from stack
		int AttachAnimNodes(const AnimationLayer *pLayer);
		// detach all nodes
		void DetachAnimNodes();

	protected:

		Object				*mParent;
		PropertyBase		*mProperty;	// first property, it has pointer to the object next one
	};

	/////////////////////////////////////////////////////////////////////////////////////////
	// PropertyBase

	class PropertyBase
	{
	public:

		//! a constructor
		PropertyBase();

		bool Init(Object *_pParent, const char *name);

		virtual PropertyType GetPropertyType() const = 0;

		/**	Get the data size (number of values) for the connector.
		*	\return	Data size.
		*/
		virtual int GetDataSize() const
		{
			int size = 0;
			switch (GetPropertyType())
			{
			case ePT_double:
				size = 8;
				break;
			case ePT_bool:
			case ePT_int:
			case ePT_enum:
				size = 4;
				break;
			case ePT_Vector3D:
				size = 24;
				break;
			}
			return size;
		}
		virtual int GetDataCount() const
		{
			int count = 1;

			switch (GetPropertyType())
			{
			case ePT_Vector3D:
				count = 3;
				break;
			case ePT_Vector2D:
				count = 2;
				break;
			case ePT_Vector4D:
				count = 4;
				break;
			}
			return count;
		}

		virtual void SetData(void *pData) = 0;

		virtual bool GetData(void *buffer, int bufferSize, const OFBTime *lTime) const = 0;

		const char *GetName() const {
			return mName.raw;
		}

		bool Modified() const {
			return mIsDirty;
		}

		virtual bool IsAnimatable() { return false; }

		bool IsAnimated() const {
			return mIsAnimated;
		}

		Object *GetParent() const {
			return pParent;
		}
		PropertyBase *GetNext() const {
			return pNext;
		}


	protected:

		PropertyBase			*pNext;	// sibling element in a properties list
		Object					*pParent;

		bool					mIsDirty;		// flag when we has been change a value, and need to show changes
		bool					mIsAnimated;

		// store a property name (key)
		CStaticName				mName;

		friend class PropertyList;
	};

	
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// PropertyT

	template <class tType, PropertyType pPT> class PropertyT : public PropertyBase
	{
	public:

		//! a constructor
		PropertyT()
			: PropertyBase()
		{
		}

		// TODO: plug connection with input/output devices

		//void Retrieve(IElementProperty *pElementProperty);

		void SetData(void *buffer) override
		{
			const int size = GetBufferSize(buffer);

			if (size > 0 && GetValuePtr())
			{
				memcpy(GetValuePtr(), buffer, size);
			}

		}

		bool GetData(void *buffer, int bufferSize, const OFBTime *lTime) const override
		{
			bool lSuccess = false;

			const int size = GetDataSize();

			if (size > 0 && bufferSize >= size && GetValuePtr())
			{
				memcpy(buffer, GetValuePtr(), size);
				lSuccess = true;
			}

			return lSuccess;
		}

		tType Get() {

			tType data;
			GetData(&data, sizeof(tType), nullptr);
			return data;
		}

		inline virtual void SetPropertyValue(tType pValue)
		{
			if (GetValuePtr())
			{
				*(tType*)GetValuePtr() = pValue;
			}
		}

		//T operator () { return Get(); }

		void operator = (tType pValue) {
			SetData(&pValue);
		}

		operator tType() const {
			return mValue;
		}

		virtual PropertyType GetPropertyType() const override { return pPT; }

	protected:

		// pre-cached value
		tType						mValue;

		// for string this function should do memory allocation
		virtual void *GetValuePtr()
		{
			return (void*)&mValue;
		}
		// no memory allocation, used in read operation, only returns a pointer
		virtual void *GetValuePtr() const
		{
			return (void*)&mValue;
		}

		virtual int GetBufferSize(void *buffer)
		{
			int size = GetDataSize();
			return size;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// PropertyAnimatable

	class PropertyAnimatable : public PropertyBase
	{
	public:

		//! a constructor
		PropertyAnimatable();


		void DetachAnimationNode();

		void AttachAnimationNode(const AnimationCurveNode *pAnimNode);

		const AnimationCurveNode *GetAnimationNode();

		virtual bool IsAnimatable() override { return true; }

		//virtual bool GetData(void *buffer, size_t bufferSize, void *evaluationData) const = 0;

	protected:

		// curve node connected to property (up to three curves in a node for vector types)
		const AnimationCurveNode		*mBaseLayerNode;

	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PropertyAnimatableT

	// TODO: rules to compute quaternion rotation
	void ComputeAnimationNode(double *Data, const int DataCount, const AnimationCurveNode *pBaseNode, const OFBTime &lTime);

	template <class tType, PropertyType pPT> class PropertyAnimatableT : public PropertyAnimatable
	{
	public:

		void SetData(void *buffer) override
		{
			const int size = GetBufferSize(buffer);

			if (size > 0 && GetValuePtr())
			{
				memcpy(GetValuePtr(), buffer, size);
			}

		}

		bool GetData(void *buffer, int bufferSize, const OFBTime *pTime) const override
		{
			OFBTime lTime( (nullptr != pTime) ? pTime->Get() : GetDisplayInfo().localTime.Get());
			// TODO: evaluate animation node if connected

			bool lSuccess = false;

			void *srcptr = nullptr;

			// TODO: if property is constrained or referenced ?!

			if (mIsAnimated)
			{
				if (mCachedTime.Get() != lTime.Get())
				{
					ComputeAnimationNode( (double*)GetCachePtr(), GetDataCount(), mBaseLayerNode, lTime);
					((OFBTime*)&mCachedTime)->Set(lTime.Get());
				}

				srcptr = GetCachePtr();
			}
			else
			{
				srcptr = GetValuePtr();
			}

			const int size = GetDataSize();

			if (size > 0 && bufferSize >= size && nullptr != srcptr)
			{
				memcpy(buffer, srcptr, size);
				lSuccess = true;
			}

			return lSuccess;
		}

		void operator = (tType pValue)
		{
			SetData(&pValue);
		}

		operator tType() const {
			return mValue;
		}

		virtual PropertyType GetPropertyType() const override { return pPT; }

	protected:

		tType		mValue;

		// computed last cached value
		tType		mCachedValue;
		OFBTime		mCachedTime;

		virtual void *GetValuePtr() const
		{
			return (void*)&mValue;
		}
		virtual void *GetCachePtr() const
		{
			return (void*)&mCachedValue;
		}

		virtual int GetBufferSize(void *buffer)
		{
			int size = GetDataSize();
			return size;
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PropertyBaseEnum

	template <class tType> class PropertyBaseEnum : public PropertyT<tType, ePT_int>
	{
	public:

		//static const char *mStrings[];

	};

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// PropertyBaseString

	class PropertyBaseString : public PropertyT<const char*, ePT_charptr>
	{
	public:

		//! a constructor
		PropertyBaseString()
			: PropertyT<const char*, ePT_charptr>()
		{
				mValue = nullptr;
				mStringLength = 0;
			}

		~PropertyBaseString()
		{
			Clear();
		}

		int GetDataSize() const override
		{
			return (sizeof(char)*mStringLength);
		}

	protected:

		int		mStringLength;
		int		mLastBufferLength;

		void Clear()
		{
			if (nullptr != mValue)
			{
				delete[] mValue;
				mValue = nullptr;
			}
			mStringLength = 0;
		}

		void *GetValuePtr() override
		{
			if (mLastBufferLength > 0)
			{
				Clear();

				mValue = new char[mLastBufferLength];
				memset((void*)mValue, 0, sizeof(char)*mLastBufferLength);
				mStringLength = mLastBufferLength;

				mLastBufferLength = 0;
			}

			return (void*)mValue;
		}

		void *GetValuePtr() const override
		{
			return (void*)mValue;
		}

		int GetBufferSize(void *buffer) override
		{
			const char *dst = (const char*)buffer;
			int size = (int)strlen(dst);

			mLastBufferLength = size;

			return size;
		}
	};

	/////////////////////////////////////////////////////////////////////////////

	typedef PropertyT<int, ePT_int>					PropertyInt;
	typedef PropertyT<bool, ePT_bool>				PropertyBool;
	typedef PropertyT<double, ePT_double>			PropertyDouble;
	typedef PropertyT<OFBVector3, ePT_Vector3D>		PropertyVector3;
	typedef PropertyT<OFBColor, ePT_ColorRGB>		PropertyColor;
	typedef PropertyT<std::string, ePT_charptr>		PropertyString;
	typedef PropertyT<Object*, ePT_object>			PropertyObject;

	typedef PropertyAnimatableT<double, ePT_double>			PropertyAnimatableDouble;
	typedef PropertyAnimatableT<OFBVector3, ePT_Vector3D>	PropertyAnimatableVector3;
	typedef PropertyAnimatableT<OFBColor, ePT_ColorRGB>		PropertyAnimatableColor;

};

#endif // _FBPROPERTY_H_