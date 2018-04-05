#pragma once


namespace ofbx
{


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed long long i64;

static_assert(sizeof(u8) == 1, "u8 is not 1 byte");
static_assert(sizeof(u32) == 4, "u32 is not 4 bytes");
static_assert(sizeof(u64) == 8, "u64 is not 8 bytes");
static_assert(sizeof(i64) == 8, "i64 is not 8 bytes");

struct Vec2
{
	double x, y;
};


union Vec3
{
	double	values[3];
	
	struct
	{
		double x, y, z;
	};
};



struct Vec4
{
	double x, y, z, w;
};


struct Matrix
{
	double m[16]; // last 4 are translation
};


struct Quat
{
	double x, y, z, w;
};


struct Color
{
	float r, g, b;
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


enum class RotationOrder {
	EULER_XYZ,
	EULER_XZY,
	EULER_YZX,
	EULER_YXZ,
	EULER_ZXY,
	EULER_ZYX,
    SPHERIC_XYZ // Currently unsupported. Treated as EULER_XYZ.
};

double fbxTimeToSeconds(i64 value);

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

struct AnimationCurveNode;
struct AnimationLayer;
struct Scene;
struct IScene;


struct Object
{
	enum class Type
	{
		ROOT,
		GEOMETRY,
		MATERIAL,
		MESH,
		TEXTURE,
		LIMB_NODE,
		NULL_NODE,
		NODE_ATTRIBUTE,
		CLUSTER,
		SKIN,
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
	Object* getParent() const;

    RotationOrder getRotationOrder() const;
	Vec3 getRotationOffset() const;
	Vec3 getRotationPivot() const;
	Vec3 getPostRotation() const;
	Vec3 getScalingOffset() const;
	Vec3 getScalingPivot() const;
	Vec3 getPreRotation() const;
	Vec3 getLocalTranslation() const;
	Vec3 getLocalRotation() const;
	Vec3 getLocalScaling() const;
	Matrix getGlobalTransform() const;
	Matrix evalLocal(const Vec3& translation, const Vec3& rotation) const;
	bool isNode() const { return is_node; }


	template <typename T> T* resolveObjectLink(int idx) const
	{
		return static_cast<T*>(resolveObjectLink(T::s_type, nullptr, idx));
	}

	u64 id;
	char name[128];
	const IElement& element;
	const Object* node_attribute;

protected:
	bool is_node;
	const Scene& scene;
};


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
	virtual DataView getFileName() const = 0;
	virtual DataView getRelativeFileName() const = 0;
};


struct Material : Object
{
	static const Type s_type = Type::MATERIAL;

	Material(const Scene& _scene, const IElement& _element);

	virtual Color getDiffuseColor() const = 0;
	virtual const Texture* getTexture(Texture::TextureType type) const = 0;
};


struct Cluster : Object
{
	static const Type s_type = Type::CLUSTER;

	Cluster(const Scene& _scene, const IElement& _element);

	virtual const int* getIndices() const = 0;
	virtual int getIndicesCount() const = 0;
	virtual const double* getWeights() const = 0;
	virtual int getWeightsCount() const = 0;
	virtual Matrix getTransformMatrix() const = 0;
	virtual Matrix getTransformLinkMatrix() const = 0;
	virtual const Object* getLink() const = 0;
};


struct Skin : Object
{
	static const Type s_type = Type::SKIN;

	Skin(const Scene& _scene, const IElement& _element);

	virtual int getClusterCount() const = 0;
	virtual const Cluster* getCluster(int idx) const = 0;
};


struct NodeAttribute : Object
{
	static const Type s_type = Type::NODE_ATTRIBUTE;

	NodeAttribute(const Scene& _scene, const IElement& _element);

	virtual DataView getAttributeType() const = 0;
};


struct Geometry : Object
{
	static const Type s_type = Type::GEOMETRY;

	Geometry(const Scene& _scene, const IElement& _element);

	virtual const Vec3* getVertices() const = 0;
	virtual int getVertexCount() const = 0;

	virtual const Vec3* getNormals() const = 0;
	virtual const Vec2* getUVs() const = 0;
	virtual const Vec4* getColors() const = 0;
	virtual const Vec3* getTangents() const = 0;
	virtual const Skin* getSkin() const = 0;
	virtual const int* getMaterials() const = 0;
};


struct Mesh : Object
{
	static const Type s_type = Type::MESH;

	Mesh(const Scene& _scene, const IElement& _element);

	virtual const Geometry* getGeometry() const = 0;
	virtual Matrix getGeometricMatrix() const = 0;
	virtual const Material* getMaterial(int idx) const = 0;
	virtual int getMaterialCount() const = 0;
};


struct AnimationStack : Object
{
	static const Type s_type = Type::ANIMATION_STACK;

	AnimationStack(const Scene& _scene, const IElement& _element);
	virtual const AnimationLayer* getLayer(int index) const = 0;
};


struct AnimationLayer : Object
{
	static const Type s_type = Type::ANIMATION_LAYER;

	AnimationLayer(const Scene& _scene, const IElement& _element);

	virtual const AnimationCurveNode* getCurveNode(int index) const = 0;
	virtual const AnimationCurveNode* getCurveNode(const Object& bone, const char* property) const = 0;
};


struct AnimationCurve : Object
{
	static const Type s_type = Type::ANIMATION_CURVE;

	AnimationCurve(const Scene& _scene, const IElement& _element);

	virtual int getKeyCount() const = 0;
	virtual const i64* getKeyTime() const = 0;
	virtual const float* getKeyValue() const = 0;
	virtual const int *getKeyFlag() const = 0;

	virtual double Evaluate(const i64 time) const = 0;
};


struct AnimationCurveNode : Object
{
	static const Type s_type = Type::ANIMATION_CURVE_NODE;

	AnimationCurveNode(const Scene& _scene, const IElement& _element);

	virtual Vec3 getNodeLocalTransform(double time) const = 0;
	virtual const Object* getBone() const = 0;
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

protected:
	virtual ~IScene() {}
};


IScene* load(const u8* data, int size);
const char* getError();

const Object *findObjectByLabelName(IScene *pScene, const char *name, const ofbx::Object *pRoot=nullptr);

} // namespace ofbx