
#ifndef _OFBX_H_
#define _OFBX_H_

// ofbx.h
//
// Original OpenFBX by Mikulas Florek (https://github.com/nem0/OpenFBX)
//
// Modified by Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//	

#include <vector>
#include "OFBTypes.h"
#include "OFBTime.h"
#include "OFBMath.h"
#include "OFBProperty.h"

namespace ofbx
{


	//! Frame size modes.
	enum OFBCameraFrameSizeMode {
		eFrameSizeWindow,                    //!< Frame size of window.
		eFrameSizeFixedRatio,                //!< Fixed ratio.
		eFrameSizeFixedResolution,        //!< Fixed resolution.
		eFrameSizeFixedWidthResolution,    //!< Fixed width resolution.
		eFrameSizeFixedHeightResolution    //!< Fixed height resolution.
	};

	//! \enum FBCameraResolutionMode
	/**Resolution modes.*/
	enum OFBCameraResolutionMode {
		eResolutionCustom,                //!< Custom resolution mode or From Camera as a render setting.
		eResolutionD1NTSC,                //!< D1 NTSC.
		eResolutionNTSC,                    //!< NTSC.
		eResolutionPAL,                    //!< PAL.
		eResolutionD1PAL,                    //!< D1 PAL.
		eResolutionHD,                    //!< HD 1920x1080.
		eResolution640x480,                //!< 640x480.
		eResolution320x200,                //!< 320x200.
		eResolution320x240,                //!< 320x240.
		eResolution128x128,                //!< 128x128.
		eResolutionFullScreen                //!< FullScreen.
	};

	//! \enum FBCameraApertureMode
	/** Aperture modes.*/
	enum OFBCameraApertureMode {
		eApertureVertical,                //!< Vertical aperture varies.
		eApertureHorizontal,                //!< Horizontal aperture varies.
		eApertureVertHoriz,                //!< Vertical and horizontal aperture varies.
		eApertureFocalLength                //!< Focal Length aperture varies.
	};

	//! \enum FBCameraFilmBackType
	/** Filmback types.*/
	enum OFBCameraFilmBackType {
		eFilmBackCustom,                    //!< Custom Filmback.
		eFilmBack16mmTheatrical,            //!< 16mm Theatrical.
		eFilmBackSuper16mm,                //!< Super16mm.
		eFilmBack35mmAcademy,                //!< 35mm Academy.
		eFilmBack35mmTVProjection,        //!< 35mm TV Projection.
		eFilmBack35mmFullAperture,        //!< 35mm Full Aperture.
		eFilmBack35mm185Projection,        //!< 35mm 185 Projection.
		eFilmBack35mmAnamorphic,            //!< 35mm Anamorphic.
		eFilmBack70mmProjection,            //!< 70mm Projection.
		eFilmBackVistaVision,                //!< Vista Vision.
		eFilmBackDynavision,                //!< Dynavision.
		eFilmBackIMAX                        //!< IMAX.
	};


struct DataView
{
	const u8* begin = nullptr;
	const u8* end = nullptr;
	bool is_binary = true;

	bool operator!=(const char* rhs) const { return !(*this == rhs); }
	bool operator==(const char* rhs) const;

	u64 toU64() const;
	int toInt() const;
	u32 toU32() const;
	double toDouble() const;
	float toFloat() const;
	
	template <int N>
	void toString(char(&out)[N]) const
	{
		char* cout = out;
		const u8* cin = begin;
		while (cin != end && cout - out < N - 1)
		{
			*cout = (char)*cin;
			++cin;
			++cout;
		}
		*cout = '\0';
	}
};


struct IElementProperty
{
	enum Type : unsigned char
	{
		LONG = 'L',
		INTEGER = 'I',
		STRING = 'S',
		FLOAT = 'F',
		DOUBLE = 'D',
		ARRAY_DOUBLE = 'd',
		ARRAY_INT = 'i',
		ARRAY_LONG = 'l',
		ARRAY_FLOAT = 'f'
	};
	virtual ~IElementProperty() {}
	virtual Type getType() const = 0;
	virtual IElementProperty* getNext() const = 0;
	virtual DataView getValue() const = 0;
	virtual int getCount() const = 0;
	virtual bool getValues(double* values, int max_size) const = 0;
	virtual bool getValues(int* values, int max_size) const = 0;
	virtual bool getValues(float* values, int max_size) const = 0;
	virtual bool getValues(u64* values, int max_size) const = 0;
	virtual bool getValues(i64* values, int max_size) const = 0;
};


struct IElement
{
	virtual IElement* getFirstChild() const = 0;
	virtual IElement* getSibling() const = 0;
	virtual DataView getID() const = 0;
	virtual IElementProperty* getFirstProperty() const = 0;
};


//! Key tangent mode for cubic interpolation.
enum ETangentMode
{
	eTangentAuto = 0x00000100,													//!< Auto key (spline cardinal).
	eTangentTCB = 0x00000200,													//!< Spline TCB (Tension, Continuity, Bias)
	eTangentUser = 0x00000400,													//!< Next slope at the left equal to slope at the right.
	eTangentGenericBreak = 0x00000800,											//!< Independent left and right slopes.
	eTangentBreak = eTangentGenericBreak | eTangentUser,							//!< Independent left and right slopes, with next slope at the left equal to slope at the right.
	eTangentAutoBreak = eTangentGenericBreak | eTangentAuto,						//!< Independent left and right slopes, with auto key.
	eTangentGenericClamp = 0x00001000,											//!< Clamp: key should be flat if next or previous key has the same value (overrides tangent mode).
	eTangentGenericTimeIndependent = 0x00002000,								//!< Time independent tangent (overrides tangent mode).
	eTangentGenericClampProgressive = 0x00004000 | eTangentGenericTimeIndependent	//!< Clamp progressive: key should be flat if tangent control point is outside [next-previous key] range (overrides tangent mode).
};

//! Key interpolation type.
enum EInterpolationType
{
	eInterpolationConstant = 0x00000002,	//!< Constant value until next key.
	eInterpolationLinear = 0x00000004,		//!< Linear progression to next key.
	eInterpolationCubic = 0x00000008		//!< Cubic progression to next key.
};

//! Weighted mode.
enum EWeightedMode
{
	eWeightedNone = 0x00000000,						//!< Tangent has default weights of 0.333; we define this state as not weighted.
	eWeightedRight = 0x01000000,					//!< Right tangent is weighted.
	eWeightedNextLeft = 0x02000000,					//!< Left tangent is weighted.
	eWeightedAll = eWeightedRight | eWeightedNextLeft	//!< Both left and right tangents are weighted.
};

//! Key constant mode.
enum EConstantMode
{
	eConstantStandard = 0x00000000,	//!< Curve value is constant between this key and the next
	eConstantNext = 0x00000100		//!< Curve value is constant, with next key's value
};

//! Velocity mode. Velocity settings speed up or slow down animation on either side of a key without changing the trajectory of the animation. Unlike Auto and Weight settings, Velocity changes the animation in time, but not in space.
enum EVelocityMode
{
	eVelocityNone = 0x00000000,						//!< No velocity (default).
	eVelocityRight = 0x10000000,					//!< Right tangent has velocity.
	eVelocityNextLeft = 0x20000000,					//!< Left tangent has velocity.
	eVelocityAll = eVelocityRight | eVelocityNextLeft	//!< Both left and right tangents have velocity.
};

//! Tangent visibility.
enum ETangentVisibility
{
	eTangentShowNone = 0x00000000,							//!< No tangent is visible.
	eTangentShowLeft = 0x00100000,							//!< Left tangent is visible.
	eTangentShowRight = 0x00200000,							//!< Right tangent is visible.
	eTangentShowBoth = eTangentShowLeft | eTangentShowRight	//!< Both left and right tangents are visible.
};

//! FbxAnimCurveKey data indices for cubic interpolation tangent information.
enum EDataIndex
{
	eRightSlope = 0,		//!< Index of the right derivative, User and Break tangent mode (data are float).
	eNextLeftSlope = 1,		//!< Index of the left derivative for the next key, User and Break tangent mode.
	eWeights = 2,			//!< Start index of weight values, User and Break tangent break mode (data are FbxInt16 tokens from weight and converted to float).
	eRightWeight = 2,		//!< Index of weight on right tangent, User and Break tangent break mode.
	eNextLeftWeight = 3,	//!< Index of weight on next key's left tangent, User and Break tangent break mode.
	eVelocity = 4,			//!< Start index of velocity values, Velocity mode
	eRightVelocity = 4,		//!< Index of velocity on right tangent, Velocity mode
	eNextLeftVelocity = 5,	//!< Index of velocity on next key's left tangent, Velocity mode
	eTCBTension = 0,		//!< Index of Tension, TCB tangent mode (data are floats).
	eTCBContinuity = 1,		//!< Index of Continuity, TCB tangent mode.
	eTCBBias = 2			//!< Index of Bias, TCB tangent mode.
};

/**
*	Layer mode.
*/
enum FBLayerMode	{
	kFBLayerModeInvalidIndex = -1,	//!< Invalid value
	kFBLayerModeAdditive = 0,		//!< Layer value will be added to the other layers to computed the final value.
	kFBLayerModeOverride,			//!< Layer value will override the value of the other precedent layers.
	kFBLayerModeOverridePassthrough	//!< If the layer has a weigth of 75%, the precedent layers will have a combined effect of 25% on the final value. Setting the weigth to 100% is similar to setting the layer in override.
};

/**
*	Rotation mode for layer.
*/
enum FBLayerRotationMode	{
	kFBLayerRotationModeInvalidIndex = -1,		//!< Invalid value
	kFBLayerRotationModeEulerRotation = 0,	//!< The rotation will be computed component by component.
	kFBLayerRotationModeQuaternionRotation	//!< The rotation will be computed using quaternion.
};

enum AnimationNodeType
{
	ANIMATIONNODE_TYPE_CUSTOM,
	ANIMATIONNODE_TYPE_TRANSLATION,
	ANIMATIONNODE_TYPE_ROTATION,
	ANIMATIONNODE_TYPE_SCALING,
	ANIMATIONNODE_TYPE_VISIBILITY,
	ANIMATIONNODE_TYPE_FIELD_OF_VIEW
};

#define ANIMATIONNODE_TYPENAME_TRANSLATION	"Lcl Translation"
#define ANIMATIONNODE_TYPENAME_ROTATION		"Lcl Rotation"
#define ANIMATIONNODE_TYPENAME_SCALING		"Lcl Scaling"
#define ANIMATIONNODE_TYPENAME_VISIBILITY	"Visibility"
#define ANIMATIONNODE_TYPENAME_FIELDOFVIEW	"Field Of View"

//! Types of transformation vector/matrices possible.
enum ModelTransformationType {
	eModelTransformation,                   //!< Transformation.
	eModelRotation,                         //!< Rotation.
	eModelTranslation,                      //!< Translation.
	eModelScaling,                          //!< Scaling.
	eModelTransformation_Geometry,          //!< Transformation plus geometry offset  
	eModelInverse_Transformation,           //!< Inverse transformation.
	eModelInverse_Rotation,                 //!< Inverse rotation.
	eModelInverse_Translation,              //!< Inverse translation.
	eModelInverse_Scaling,                  //!< Inverse scaling.
	eModelInverse_Transformation_Geometry   //!< Inverse of transformation plus geometry offset
};

enum CameraType
{
	eCameraTypePerspective,
	eCameraTypeOrhogonal
};

//! \enum FBCameraMatrixType
/**Camera matrix types in OpenGL convention.*/
enum CameraMatrixType {
	eProjection,                      //!< Camera's Projection matrix.
	eModelView,                       //!< Camera's combined Model-View matrix.
	eModelViewProj,                   //!< Camera's combined Model-View-Projection matrix.
	eProjInverse                      //!< Camera's Projection Inverse matrix.
};

struct Object;
struct AnimationCurveNode;
struct AnimationLayer;
struct Scene;
struct IScene;

///////////////////////////////////////////////////////////////////////////////////////////
// Object

struct Object
{
	enum class Type
	{
		ROOT,
		GEOMETRY,
		MATERIAL,
		SHADER,
		MESH,
		TEXTURE,
		LIMB_NODE,
		NULL_NODE,
		CAMERA,
		LIGHT,
		NODE_ATTRIBUTE,
		CLUSTER,
		SKIN,
		CONSTRAINT,
		CONSTRAINT_POSITION,
		ANIMATION_STACK,
		ANIMATION_LAYER,
		ANIMATION_CURVE,
		ANIMATION_CURVE_NODE
	};

	Object(const Scene& _scene, const IElement& _element);

	virtual ~Object() {}
	virtual Type getType() const = 0;
	
	const IScene& getScene() const;
	Object* resolveObjectLink(int idx) const;
	Object* resolveObjectLink(Type type, const char* property, int idx) const;
	Object* resolveObjectLinkReverse(Type type) const;
	Object* getParents(int idx) const;
	
	bool isNode() const { return is_node; }


	template <typename T> T* resolveObjectLink(int idx) const
	{
		return static_cast<T*>(resolveObjectLink(T::s_type, nullptr, idx));
	}

	u64 id;
	char name[128];
	const IElement& element;
	const Object* node_attribute;	// contains some specified class properties ontop of base class

	const void *eval_data;
	const void *render_data;

	//bool Selected;
	//PropertyString			Name;
	PropertyBool			Selected;

	PropertyList			mProperties;

	// retrive properties values and connections
	virtual bool Retrieve();

	void PropertyAdd(PropertyBase *pProperty)
	{
		mProperties.Add(pProperty);
	}

protected:
	
	bool is_node;
	const Scene& scene;
};


///////////////////////////////////////////////////////////////////////////////////
// Texture

struct Texture : Object
{
	enum TextureType
	{
		DIFFUSE,
		NORMAL,

		COUNT
	};

	static const Type s_type = Type::TEXTURE;

	Texture(const Scene& _scene, const IElement& _element);

	PropertyString		FileName;
	PropertyString		RelativeFileName;

	//virtual DataView getFileName() const = 0;
	//virtual DataView getRelativeFileName() const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////
// Material

struct Material : Object
{
	static const Type s_type = Type::MATERIAL;

	Material(const Scene& _scene, const IElement& _element);

	virtual OFBColor getDiffuseColor() const = 0;
	virtual const Texture* getTexture(Texture::TextureType type) const = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////
// Shader

struct Shader : Object
{
	static const Type s_type = Type::SHADER;

	Shader(const Scene& _scene, const IElement& _element);
};

///////////////////////////////////////////////////////////////////////////////////////////
// Cluster

struct Cluster : Object
{
	static const Type s_type = Type::CLUSTER;

	Cluster(const Scene& _scene, const IElement& _element);

	virtual const int* getIndices() const = 0;
	virtual int getIndicesCount() const = 0;
	virtual const double* getWeights() const = 0;
	virtual int getWeightsCount() const = 0;
	virtual OFBMatrix getTransformMatrix() const = 0;
	virtual OFBMatrix getTransformLinkMatrix() const = 0;
	virtual const Object* getLink() const = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////
// Skin

struct Skin : Object
{
	static const Type s_type = Type::SKIN;

	Skin(const Scene& _scene, const IElement& _element);

	virtual int getClusterCount() const = 0;
	virtual const Cluster* getCluster(int idx) const = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////
// NodeAttribute

struct NodeAttribute : Object
{
	static const Type s_type = Type::NODE_ATTRIBUTE;

	NodeAttribute(const Scene& _scene, const IElement& _element);

	virtual DataView getAttributeType() const = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////
// Geometry

struct Geometry : Object
{
	static const Type s_type = Type::GEOMETRY;

	Geometry(const Scene& _scene, const IElement& _element);

	virtual const OFBVector3* getVertices() const = 0;
	virtual int getVertexCount() const = 0;

	virtual const OFBVector3* getNormals() const = 0;
	virtual const OFBVector2* getUVs() const = 0;
	virtual const OFBVector4* getColors() const = 0;
	virtual const OFBVector3* getTangents() const = 0;
	virtual const Skin* getSkin() const = 0;
	virtual const int* getMaterials() const = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Model

struct Model : Object
{
	//! a constructor
	Model(const Scene& _scene, const IElement& _element);

	//
	PropertyBool						Show;
	PropertyAnimatableVector3			Translation;
	PropertyAnimatableVector3			Rotation;
	PropertyAnimatableVector3			Scaling;

	PropertyBool							RotationActive;
	PropertyBaseEnum<OFBRotationOrder>		RotationOrder;
	PropertyVector3							RotationOffset;
	PropertyVector3							RotationPivot;
	
	PropertyVector3							ScalingOffset;
	PropertyVector3							ScalingPivot;

	PropertyVector3					PreRotation;
	PropertyVector3					PostRotation;
	

	OFBMatrix getGlobalTransform() const;
	
	Model *Parent() const {
		return mParent;
	}

	Model* Children() const {
		return mFirstChild;
	}

	Model *GetNext() const {
		return mNext;
	}
	Model *GetPrev() const {
		return mPrev;
	}

	void AddChild(Model *pChild);

	const int GetAnimationNodeCount() const;
	const AnimationCurveNode *GetAnimationNode(int index) const;

	// find nodes by name or by typeId, connected to specified layer. with multilayer or multitake it could be more than one translation node

	const AnimationCurveNode *FindAnimationNode(const char *name, const AnimationLayer *pLayer) const;
	// fast way to look for location translation, rotation, visibility, etc.
	const AnimationCurveNode *FindAnimationNodeByType(const int typeId, const	AnimationLayer *pLayer) const;

	//
	void GetMatrix(OFBMatrix &pMatrix, ModelTransformationType pWhat = eModelTransformation, bool pGlobalInfo = true, const OFBTime *pTime = nullptr) const;
	void GetVector(OFBVector3 &pVector, ModelTransformationType pWhat = eModelTranslation, bool pGlobalInfo = true, const OFBTime *pTime = nullptr) const;
	void GetRotation(OFBVector4 &pQuat, const OFBTime *pTime = nullptr) const;

	std::vector<AnimationCurveNode*>		mAnimationNodes;

protected:

	bool evalLocal(OFBMatrix *result, const OFBVector3& translation, const OFBVector3& rotation, const OFBVector3 &scaling) const;

	//
	OFBMatrix			mGlobalCache;
	OFBMatrix			mLocalCache;
	OFBTime				mCacheTime;

	//
	Model		*mParent;
	//std::vector<Model*>						mChildren;
	

	Model					*mFirstChild;

	// sibling children under a parent
	Model					*mNext;
	Model					*mPrev;
};

/////////////////////////////////////////////////////////////////////////////////////////////
//

struct Mesh : Model
{
	static const Type s_type = Type::MESH;

	Mesh(const Scene& _scene, const IElement& _element);

	PropertyVector3			GeometricTranslation;
	PropertyVector3			GeometricRotation;
	PropertyVector3			GeometricScaling;

	virtual const Geometry* getGeometry() const = 0;
	virtual OFBMatrix getGeometricMatrix() const = 0;
	virtual const Material* getMaterial(int idx) const = 0;
	virtual int getMaterialCount() const = 0;

	virtual bool IsStatic() const = 0;
};

struct ModelNull : Model
{
	static const Type s_type = Type::NULL_NODE;

	ModelNull(const Scene& _scene, const IElement &_element);

	// model null display size
	PropertyDouble			Size;
	//virtual const double getSize() const = 0;
};

// core root element, scene root
struct SceneRoot : Model
{
	static const Type s_type = Type::ROOT;

	SceneRoot(const Scene& _scene, const IElement &_element);

	// model null display size
	//PropertyDouble		Size;
	//virtual const double getSize() const = 0;
};

struct ModelSkeleton : Model
{
	static const Type s_type = Type::LIMB_NODE;

	ModelSkeleton(const Scene& _scene, const IElement &_element);

	// model null display size
	PropertyDouble		Size;
	PropertyColor		Color;
};

//////////////////////////////////////////////////////////////////////////////
// Camera

struct Camera : Model
{
	static const Type s_type = Type::CAMERA;

	Camera(const Scene& _scene, const IElement &_element);

	// camera settings

	PropertyVector3			Color;

	PropertyVector3			Position;
	PropertyVector3			UpVector;
	PropertyVector3			InterestPosition;

	PropertyDouble			OpticalCenterX;
	PropertyDouble			OpticalCenterY;

	PropertyAnimatableColor		BackgroundColor;
	PropertyBool				UseFrameColor;
	PropertyColor				FrameColor;

	PropertyDouble			TurnTable;

	PropertyBaseEnum<OFBCameraFrameSizeMode>			AspectRatioMode;
	PropertyDouble			AspectWidth;	// resolution width
	PropertyDouble			AspectHeight;	// resolution height

	PropertyDouble			PixelAspectRatio;
	PropertyBaseEnum<OFBCameraApertureMode>	ApertureMode;
	PropertyDouble			FilmOffsetX;
	PropertyDouble			FilmOffsetY;
	PropertyDouble			FilmWidth;
	PropertyDouble			FilmHeight;
	PropertyDouble			FilmAspectRatio;
	PropertyDouble			FilmSqueezeRatio;
	
	// TODO: should be updated on application resize !
	PropertyDouble			WindowWidth;
	PropertyDouble			WindowHeight;

	// TODO:
	//PropertyInt				FilmFormatIndex;

	PropertyBaseEnum<CameraType>	ProjectionType;


	PropertyAnimatableDouble	Roll;


	PropertyAnimatableDouble	FieldOfView;
	
	PropertyAnimatableDouble	FieldOfViewX;
	
	PropertyAnimatableDouble	FieldOfViewY;
	
	PropertyAnimatableDouble	FocalLength;

	PropertyDouble				NearPlane;
	PropertyDouble				FarPlane;
	
	PropertyObject				Target;

	// pre-cached or controled by transformer
	// why it's here ?!
	//Matrix		mModelView;

	virtual bool GetCameraMatrix(float *pMatrix, CameraMatrixType pType, const OFBTime *pTime = nullptr) = 0;
	virtual bool GetCameraMatrix(double *pMatrix, CameraMatrixType pType, const OFBTime *pTime = nullptr) = 0;

	// override camera matrix
	virtual void SetCameraMatrix(const float *pMatrix, CameraMatrixType pType) = 0;
	virtual void SetCameraMatrix(const double *pMatrix, CameraMatrixType pType) = 0;

	// h - horizontal image dimention
	virtual double ComputeFieldOfView(const double focal, const double h) const = 0;

	virtual Model *GetTarget() const = 0;
};

struct Light : Model
{
	static const Type s_type = Type::LIGHT;

	Light(const Scene& _scene, const IElement &_element);
};

/////////////////////////////////////////////////////////////////////////////////////////////
// Constraint

struct Constraint : Object
{
	static const Type s_type = Type::CONSTRAINT;

	Constraint(const Scene &_scene, const IElement &_element);


	// DONE: weight could be animated !!
	PropertyBool					Active;
	PropertyAnimatableDouble		Weight;
	
	virtual bool Evaluate(const OFBTime *pTime = nullptr) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Constraint Position

struct ConstraintPosition : Constraint
{
	static const Type s_type = Type::CONSTRAINT_POSITION;

	ConstraintPosition(const Scene &_scene, const IElement &_element);


	// DONE: weight could be animated !!
	PropertyBool					Active;
	PropertyAnimatableDouble		Weight;

	// TODO: should be arrays instead of single pointer !!
	PropertyObject		ConstrainedObject;
	PropertyObject		SourceObject;

	PropertyBool		AffectX;
	PropertyBool		AffectY;
	PropertyBool		AffectZ;

	PropertyAnimatableVector3		Translation;	// snap offset

};

// info for the one evaluation task
struct EvaluationInfo
{
	OFBTime		localTime;
	OFBTime		systemTime;

	bool		IsStop;	// is playing or not
};

struct AnimationStack : Object
{
	static const Type s_type = Type::ANIMATION_STACK;

	AnimationStack(const Scene& _scene, const IElement& _element);
	
	virtual i64		getLoopStart() const = 0;
	virtual i64		getLoopStop() const = 0;

	virtual int getLayerCount() const = 0;
	virtual const AnimationLayer* getLayer(int index) const = 0;
};


struct AnimationLayer : Object
{
	static const Type s_type = Type::ANIMATION_LAYER;

	AnimationLayer(const Scene& _scene, const IElement& _element);

	virtual const AnimationCurveNode* getCurveNode(int index) const = 0;
	virtual const AnimationCurveNode* getCurveNode(const Object& bone, const char* property) const = 0;

	PropertyInt		LayerID;	// rearranged order of layers, defined by users

	PropertyBool	Solo;		//!< <b>Read Write Property:</b> If true, the layer is soloed. When you solo a layer, you mute other layers that are at the same level in the hierarchy, as well as the children of those layers. Cannot be applied to the BaseAnimation Layer.
	PropertyBool	Mute;		//!< <b>Read Write Property:</b> If true, the layer is muted. A muted layer is not included in the result animation. Cannot be applied to the BaseAnimation Layer.
	PropertyBool	Lock;		//!< <b>Read Write Property:</b> If true, the layer is locked. You cannot modify keyframes on a locked layer.

	PropertyAnimatableDouble	Weight; //!< <b>Read Write Property:</b> The weight value of a layer determines how much it is present in the result animation. Takes a value from 0 (the layer is not present) to 100. The weighting of a parent layer is factored into the weighting of its child layers, if any. BaseAnimation Layer always has a Weight of 100. 

	PropertyBaseEnum<FBLayerMode>			LayerMode;	//!< <b>Read Write Property:</b> Layer mode. By default, the layer is in kFBLayerModeAdditive mode. Cannot be applied to the BaseAnimation Layer.
	PropertyBaseEnum<FBLayerRotationMode>	LayerRotationMode; //!< <b>Read Only Property:</b> Layer rotation mode. Cannot be applied to the BaseAnimation Layer.


	virtual int getSubLayerCount() const = 0;
	virtual const AnimationLayer *getSubLayer(int index) const = 0;
};


struct AnimationCurve : Object
{
	static const Type s_type = Type::ANIMATION_CURVE;

	AnimationCurve(const Scene& _scene, const IElement& _element);

	virtual int getKeyCount() const = 0;
	virtual const i64* getKeyTime() const = 0;
	virtual const float* getKeyValue() const = 0;
	virtual const int *getKeyFlag() const = 0;

	virtual double Evaluate(const OFBTime &time) const = 0;
};


struct AnimationCurveNode : Object
{
	static const Type s_type = Type::ANIMATION_CURVE_NODE;

	AnimationCurveNode(const Scene& _scene, const IElement& _element);

	virtual OFBVector3 getNodeLocalTransform(double time) const = 0;
	virtual const Object* GetOwner() const = 0;
	
	// return next anim node linked under property layers stack (in order how layers have been sorted)
	virtual AnimationCurveNode *GetNext() = 0;
	virtual const AnimationCurveNode *GetNext() const = 0;
	virtual void LinkNext(const AnimationCurveNode *pNext) = 0;

	virtual AnimationLayer *getLayer() const = 0;

	virtual int getCurveCount() const = 0;
	virtual const AnimationCurve *getCurve(int index) const = 0;

	virtual bool Evaluate(double *Data, const OFBTime pTime) const = 0;
};


struct TakeInfo
{
	DataView name;
	DataView filename;
	double local_time_from;
	double local_time_to;
	double reference_time_from;
	double reference_time_to;
};


struct IScene
{
	virtual void destroy() = 0;
	virtual const IElement* getRootElement() const = 0;
	virtual const Object* getRoot() const = 0;
	virtual const TakeInfo* getTakeInfo(const char* name) const = 0;
	virtual int getMeshCount() const = 0;
	virtual float getSceneFrameRate() const = 0;
	virtual const Mesh* getMesh(int index) const = 0;
	virtual int getAnimationStackCount() const = 0;
	virtual const AnimationStack* getAnimationStack(int index) const = 0;
	virtual const Object *const * getAllObjects() const = 0;
	virtual int getAllObjectCount() const = 0;

	virtual int GetConstraintCount() const = 0;
	virtual const Constraint *GetConstraint(const int index) const = 0;

	virtual int GetMaterialCount() const = 0;
	virtual const Material *GetMaterial(const int index) const = 0;

	virtual int GetShaderCount() const = 0;
	virtual const Shader* GetShader(const int index) const = 0;

	virtual int GetCameraCount() const = 0;
	virtual const Camera *GetCamera(const int index) const = 0;

	virtual int GetLightCount() const = 0;
	virtual const Light *GetLight(const int index) const = 0;

	// Run it On Take Change
	// we should prep properties animation nodes for a current take (Stack)
	virtual bool PrepTakeConnections(const int takeIndex) = 0;

protected:
	virtual ~IScene() {}
};

////////////////////////////////////////////////////////////////////////////////////////
// global functions

EvaluationInfo &GetDisplayInfo();

IScene* load(const u8* data, int size);
const char* getError();

Model *FindModelByLabelName(IScene *pScene, const char *name, const ofbx::Object *pRoot=nullptr);

} // namespace ofbx

#endif