
#include "OFBProperty.h"
#include "ofbx.h"

// OFBProperty.cpp
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//

namespace ofbx
{
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Property Base

	PropertyBase::PropertyBase()
	{
		pParent = nullptr;
		pNext = nullptr;

		mIsAnimated = false;
		mIsDirty = false;
	}

	bool PropertyBase::Init(Object *_pParent, const char *name)
	{
		bool lSuccess = false;
		if (nullptr != _pParent)
		{
			pParent = _pParent;
			memset(mName.raw, 0, sizeof(char)* 32);
			sprintf_s(mName.raw, sizeof(char)* 32, "%s", name);

			pParent->PropertyAdd(this);
			lSuccess = true;
		}

		return lSuccess;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Property Animatable

	PropertyAnimatable::PropertyAnimatable()
		: PropertyBase()
	{
		mBaseLayerNode = nullptr;
		mIsAnimated = false;
	}

	// TODO: could be many, animation node per stack layer

	void PropertyAnimatable::DetachAnimationNode()
	{
		mBaseLayerNode = nullptr;
		mIsAnimated = false;
	}

	void PropertyAnimatable::AttachAnimationNode(const AnimationCurveNode *pAnimNode)
	{
		if (nullptr == mBaseLayerNode)
		{
			mBaseLayerNode = pAnimNode;
			if (nullptr != mBaseLayerNode)
				mIsAnimated = true;
		}
		else
		{
			AnimationCurveNode *pLast = (AnimationCurveNode*)mBaseLayerNode;

			while (nullptr != pLast->GetNext())
			{
				pLast = pLast->GetNext();
			}
			pLast->LinkNext(pAnimNode);
		}
	}

	const AnimationCurveNode *PropertyAnimatable::GetAnimationNode()
	{
		return mBaseLayerNode;
	}

	void ComputeAnimationNode(double *Data, const int DataCount, const AnimationCurveNode *pBaseNode, const OFBTime &lTime)
	{
		pBaseNode->Evaluate(Data, lTime);

		const AnimationCurveNode *pNext = pBaseNode->GetNext();
		while (nullptr != pNext)
		{
			AnimationLayer *pLayer = pNext->getLayer();

			if (false == pLayer->Mute)
			{
				// 
				double weight;
				pLayer->Weight.GetData(&weight, sizeof(double), &lTime);
				weight *= 0.01;

				double temp[3];
				pNext->Evaluate(temp, lTime);

				for (int i = 0; i < DataCount; ++i)
				{
					Data[i] += weight * temp[i];
				}
			}
			
			pNext = pNext->GetNext();
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	// Property List

	bool PropertyList::Add(PropertyBase *pProp)
	{
		if (nullptr == mProperty)
		{
			mProperty = pProp;
			mProperty->pNext = nullptr;
		}
		else
		{
			// list prop to a last list item
			PropertyBase *prop = mProperty;
			while (nullptr != prop->GetNext())
			{
				prop = prop->GetNext();
			}
			prop->pNext = pProp;
		}
		return true;
	}

	PropertyList::PropertyList(Object *pParent)
		: mParent(pParent)
	{
		mProperty = nullptr;
	}

	PropertyBase		*PropertyList::GetFirst() const
	{
		return mProperty;
	}

	PropertyBase		*PropertyList::Find(const char *name) const
	{
		PropertyBase *prop = mProperty;

		while (nullptr != prop)
		{
			if (0 == strcmp(prop->GetName(), name))
			{
				return prop;
			}
			prop = prop->GetNext();
		}

		return nullptr;
	}

	void PropertyList::DetachAnimNodes()
	{
		PropertyBase *prop = mProperty;

		while (nullptr != prop)
		{
			if (true == prop->IsAnimatable() )
			{
				PropertyAnimatable *pAnimatable = (PropertyAnimatable*)prop;
				pAnimatable->DetachAnimationNode();
			}
			prop = prop->GetNext();
		}
	}

	int PropertyList::AttachAnimNodes(const AnimationLayer *pLayer)
	{
		int count = 0;

		PropertyBase *prop = mProperty;

		while (nullptr != prop)
		{
			if (true == prop->IsAnimatable() )
			{
				PropertyAnimatable *pAnimatable = (PropertyAnimatable*)prop;
				
				const AnimationCurveNode *pNode = pLayer->getCurveNode(*mParent, prop->GetName());

				if (nullptr != pNode)
				{
					pAnimatable->AttachAnimationNode(pNode);
					count += 1;
				}

			}
			prop = prop->GetNext();
		}

		return count;
	}

};