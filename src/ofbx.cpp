
// ofbx.cpp
//
// Original OpenFBX by Mikulas Florek (https://github.com/nem0/OpenFBX)
//
// Modified by Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//	

#include "ofbx.h"
#include "miniz.h"
#include <cassert>

#include <ctype.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "OFBMath.h"

#define _USE_MATH_DEFINES // for C++  
#include <math.h> 

static double gsOrthoCameraScale = 178.0;

#define MATH_PI			3.1415926535897932384626433832795028
#define MATH_PI_DIV_180  3.1415926535897932384626433832795028 / 180.0
#define MATH_180_DIV_PI  180.0 / 3.1415926535897932384626433832795028
#define HFOV2VFOV(h, ar) (2.0 * atan((ar) * tan( (h * MATH_PI_DIV_180) * 0.5)) * MATH_180_DIV_PI) //ar : aspectY / aspectX
#define VFOV2HFOV(v, ar) (2.0 * atan((ar) * tan( (v * MATH_PI_DIV_180) * 0.5)) * MATH_180_DIV_PI) //ar : aspectX / aspectY

namespace ofbx
{

	EvaluationInfo	gDisplayInfo;

	EvaluationInfo &GetDisplayInfo()
	{
		return gDisplayInfo;
	}

struct Error
{
	Error() {}
	Error(const char* msg) { 
		s_message = msg; 
	}

	static const char* s_message;
};


const char* Error::s_message = "";


template <typename T> struct OptionalError
{
	OptionalError(Error error)
		: is_error(true)
	{
	}


	OptionalError(T _value)
		: value(_value)
		, is_error(false)
	{
	}

	OptionalError(T _value, bool _error)
		: value(_value)
			, is_error(_error)
	{}

	T getValue() const
	{
#ifdef _DEBUG
		assert(error_checked);
#endif
		return value;
	}


	bool isError()
	{
#ifdef _DEBUG
		error_checked = true;
#endif
		return is_error;
	}


private:
	T value;
	bool is_error;
#ifdef _DEBUG
	bool error_checked = false;
#endif
};


#pragma pack(1)
struct Header
{
	u8 magic[21];
	u8 reserved[2];
	u32 version;
};
#pragma pack()


struct Cursor
{
	const u8* current;
	const u8* begin;
	const u8* end;
};


template <int SIZE> static bool copyString(char (&destination)[SIZE], const char* source)
{
	const char* src = source;
	char* dest = destination;
	int length = SIZE;
	if (!src) return false;

	while (*src && length > 1)
	{
		*dest = *src;
		--length;
		++dest;
		++src;
	}
	*dest = 0;
	return *src == '\0';
}


u64 DataView::toU64() const
{
	if (is_binary)
	{
		assert(end - begin == sizeof(u64));
		return *(u64*)begin;
	}
	return atoll((const char*)begin);
}


int DataView::toInt() const
{
	if (is_binary)
	{
		assert(end - begin == sizeof(int));
		return *(int*)begin;
	}
	return atoi((const char*)begin);
}


u32 DataView::toU32() const
{
	if (is_binary)
	{
		assert(end - begin == sizeof(u32));
		return *(u32*)begin;
	}
	return (u32)atoll((const char*)begin);
}


double DataView::toDouble() const
{
	if (is_binary)
	{
		assert(end - begin == sizeof(double));
		return *(double*)begin;
	}
	return atof((const char*)begin);
}


float DataView::toFloat() const
{
	if (is_binary)
	{
		assert(end - begin == sizeof(float));
		return *(float*)begin;
	}
	return (float)atof((const char*)begin);
}


bool DataView::operator==(const char* rhs) const
{
	const char* c = rhs;
	const char* c2 = (const char*)begin;
	while (*c && c2 != (const char*)end)
	{
		if (*c != *c2) return 0;
		++c;
		++c2;
	}
	return c2 == (const char*)end && *c == '\0';
}


struct Property;
template <typename T> static bool parseArrayRaw(const Property& property, T* out, int max_size);
template <typename T> static bool parseBinaryArray(const Property& property, std::vector<T>* out);


struct Property : IElementProperty
{
	~Property() { delete next; }
	Type getType() const override { return (Type)type; }
	IElementProperty* getNext() const override { return next; }
	DataView getValue() const override { return value; }
	int getCount() const override
	{
		assert(type == ARRAY_DOUBLE || type == ARRAY_INT || type == ARRAY_FLOAT || type == ARRAY_LONG);
		if (value.is_binary)
		{
			return int(*(u32*)value.begin);
		}
		return count;
	}

	bool getValues(double* values, int max_size) const override { return parseArrayRaw(*this, values, max_size); }

	bool getValues(float* values, int max_size) const override { return parseArrayRaw(*this, values, max_size); }

	bool getValues(u64* values, int max_size) const override { return parseArrayRaw(*this, values, max_size); }

	bool getValues(i64* values, int max_size) const override { return parseArrayRaw(*this, values, max_size); }

	bool getValues(int* values, int max_size) const override { return parseArrayRaw(*this, values, max_size); }

	int count;
	u8 type;
	DataView value;
	Property* next = nullptr;
};


struct Element : IElement
{
	IElement* getFirstChild() const override { return child; }
	IElement* getSibling() const override { return sibling; }
	DataView getID() const override { return id; }
	IElementProperty* getFirstProperty() const override { return first_property; }
	IElementProperty* getProperty(int idx) const
	{
		IElementProperty* prop = first_property;
		for (int i = 0; i < idx; ++i)
		{
			if (prop == nullptr) return nullptr;
			prop = prop->getNext();
		}
		return prop;
	}

	DataView id;
	Element* child = nullptr;
	Element* sibling = nullptr;
	Property* first_property = nullptr;
};


static const Element* findChild(const Element& element, const char* id)
{
	Element* const* iter = &element.child;
	while (*iter)
	{
		if ((*iter)->id == id) return *iter;
		iter = &(*iter)->sibling;
	}
	return nullptr;
}


static IElement* resolveProperty(const Object& obj, const char* name)
{
	const Element* props = findChild((const Element&)obj.element, "Properties70");
	if (!props) return nullptr;

	Element* prop = props->child;
	while (prop)
	{
		if (prop->first_property && prop->first_property->value == name)
		{
			return prop;
		}
		prop = prop->sibling;
	}
	return nullptr;
}


static int resolveEnumProperty(const Object& object, const char* name, int default_value)
{
	Element* element = (Element*)resolveProperty(object, name);
	if (!element) return default_value;
	Property* x = (Property*)element->getProperty(4);
	if (!x) return default_value;

	return x->value.toInt();
}

static double resolveDoubleProperty(const Object& object, const char* name, const double default_value)
{
	Element* element = (Element*)resolveProperty(object, name);
	if (!element) return default_value;
	Property* x = (Property*)element->getProperty(4);
	if (!x) return default_value;

	return x->value.toDouble();
}

static OFBVector3 resolveVec3Property(const Object& object, const char* name, const OFBVector3& default_value)
{
	Element* element = (Element*)resolveProperty(object, name);
	if (!element) return default_value;
	Property* x = (Property*)element->getProperty(4);
	if (!x || !x->next || !x->next->next) return default_value;

	return {x->value.toDouble(), x->next->value.toDouble(), x->next->next->value.toDouble()};
}

static int resolveIntProperty(const Object& object, const char* name, const int default_value)
{
	Element* element = (Element*)resolveProperty(object, name);
	if (!element) return default_value;
	Property* x = (Property*)element->getProperty(4);
	if (!x) return default_value;

	return x->value.toInt();
}

static bool resolveBoolProperty(const Object& object, const char* name, const bool default_value)
{
	Element* element = (Element*)resolveProperty(object, name);
	if (!element) return default_value;
	Property* x = (Property*)element->getProperty(4);
	if (!x) return default_value;

	return (x->value.toInt() > 0);
}


Object::Object(const Scene& _scene, const IElement& _element)
	: scene(_scene)
	, element(_element)
	, is_node(false)
	, node_attribute(nullptr)
	, eval_data(0)
	, render_data(0)
	//, Selected(false)
	, mProperties(this)
{
	auto& e = (Element&)_element;
	if (e.first_property && e.first_property->next)
	{
		e.first_property->next->value.toString(name);
	}
	else
	{
		name[0] = '\0';
	}

	//Name.Init(this, "Name");
	Selected.Init(this, "Selected");

	//Name.SetPropertyValue("Object");
	Selected.SetPropertyValue(false);
}

Model::Model(const Scene& _scene, const IElement& _element)
: Object(_scene, _element)
{
	is_node = true;
	mParent = nullptr;
	mFirstChild = nullptr;

	mNext = nullptr;
	mPrev = nullptr;

	//
	RotationOrder.Init(this, "RotationOrder");

	RotationActive.Init(this, "RotationActive");
	PreRotation.Init(this, "PreRotation");
	PostRotation.Init(this, "PostRotation");

	RotationOffset.Init(this, "RotationOffset");
	RotationPivot.Init(this, "RotationPivot");

	ScalingOffset.Init(this, "ScalingOffset");
	ScalingPivot.Init(this, "ScalingPivot");

	Visibility.Init(this, "Visibility");
	VisibilityInheritance.Init(this, "Visibility Inheritance");

	Translation.Init(this, "Lcl Translation");
	Rotation.Init(this, "Lcl Rotation");
	Scaling.Init(this, "Lcl Scaling");

	GeometricTranslation.Init(this, "GeometricTranslation");
	GeometricRotation.Init(this, "GeometricRotation");
	GeometricScaling.Init(this, "GeometricScaling");

	QuaternionInterpolation.Init(this, "QuaternionInterpolation");

	Show.Init(this, "Show");
	Pickable.Init(this, "Pickable");
	Transformable.Init(this, "Transformable");

	CastsShadows.Init(this, "Casts Shadows");
	ReceiveShadows.Init(this, "Receive Shadows");

	PrimaryVisibility.Init(this, "Primary Visibility");

	// default values

	RotationOrder.SetPropertyValue(OFBRotationOrder::eEULER_XYZ);

	RotationActive = false;

	RotationOffset = Vector_Zero();
	RotationPivot = Vector_Zero();

	ScalingOffset = Vector_Zero();
	ScalingPivot = Vector_Zero();

	PreRotation = Vector_Zero();
	PostRotation = Vector_Zero();

	Visibility = true;
	VisibilityInheritance = true;

	Translation = Vector_Zero();
	Rotation = Vector_Zero();
	Scaling = Vector_One();

	GeometricTranslation = Vector_Zero();
	GeometricRotation = Vector_Zero();
	GeometricScaling = Vector_One();

	QuaternionInterpolation = false;

	Show = true;
	Pickable = true;
	Transformable = true;

	CastsShadows = true;
	ReceiveShadows = true;

	PrimaryVisibility = true;

	//
	mCacheTime = OFBTime::MinusInfinity;
}

Camera::Camera(const Scene& _scene, const IElement& _element)
: Model(_scene, _element)
{}

Light::Light(const Scene& _scene, const IElement& _element)
: Model(_scene, _element)
{
	LightType.Init(this, "LightType");
	AttenuationType.Init(this, "AttenuationType");

	Intensity.Init(this, "Intensity");
	InnerAngle.Init(this, "InnerAngle");
	OuterAngle.Init(this, "OuterAngle");
	DiffuseColor.Init(this, "DiffuseColor");

	CastShadows.Init(this, "CastShadows");
	CastLightOnObject.Init(this, "CastLightOnObject");

	// default values
	LightType.SetPropertyValue(eLightTypePoint);
	AttenuationType.SetPropertyValue(eAttenuationLinear);

	Intensity = 100.0;
	InnerAngle = 45.0;
	OuterAngle = 50.0;
	DiffuseColor = { 1.0, 1.0, 1.0 };
	CastShadows = true;
	CastLightOnObject = true;
}

ModelNull::ModelNull(const Scene& _scene, const IElement& _element)
: Model(_scene, _element)
{
}

SceneRoot::SceneRoot(const Scene& _scene, const IElement& _element)
: Model(_scene, _element)
{}

ModelSkeleton::ModelSkeleton(const Scene& _scene, const IElement& _element)
: Model(_scene, _element)
{}

static bool decompress(const u8* in, size_t in_size, u8* out, size_t out_size)
{
	mz_stream stream = {};
	mz_inflateInit(&stream);

	stream.avail_in = (int)in_size;
	stream.next_in = in;
	stream.avail_out = (int)out_size;
	stream.next_out = out;

	int status = mz_inflate(&stream, Z_SYNC_FLUSH);

	if (status != Z_STREAM_END) return false;

	return mz_inflateEnd(&stream) == Z_OK;
}


template <typename T> static OptionalError<T> read(Cursor* cursor)
{
	if (cursor->current + sizeof(T) > cursor->end) return Error("Reading past the end");
	T value = *(const T*)cursor->current;
	cursor->current += sizeof(T);
	return value;
}


static OptionalError<DataView> readShortString(Cursor* cursor)
{
	DataView value;
	OptionalError<u8> length = read<u8>(cursor);
	if (length.isError()) return Error();

	if (cursor->current + length.getValue() > cursor->end) return Error("Reading past the end");
	value.begin = cursor->current;
	cursor->current += length.getValue();

	value.end = cursor->current;

	return value;
}


static OptionalError<DataView> readLongString(Cursor* cursor)
{
	DataView value;
	OptionalError<u32> length = read<u32>(cursor);
	if (length.isError()) return Error();

	if (cursor->current + length.getValue() > cursor->end) return Error("Reading past the end");
	value.begin = cursor->current;
	cursor->current += length.getValue();

	value.end = cursor->current;

	return value;
}


static OptionalError<Property*> readProperty(Cursor* cursor)
{
	if (cursor->current == cursor->end) return Error("Reading past the end");

	std::unique_ptr<Property> prop = std::make_unique<Property>();
	prop->next = nullptr;
	prop->type = *cursor->current;
	++cursor->current;
	prop->value.begin = cursor->current;

	switch (prop->type)
	{
		case 'S':
		{
			OptionalError<DataView> val = readLongString(cursor);
			if (val.isError()) return Error();
			prop->value = val.getValue();
			break;
		}
		case 'Y': cursor->current += 2; break;
		case 'C': cursor->current += 1; break;
		case 'I': cursor->current += 4; break;
		case 'F': cursor->current += 4; break;
		case 'D': cursor->current += 8; break;
		case 'L': cursor->current += 8; break;
		case 'R':
		{
			OptionalError<u32> len = read<u32>(cursor);
			if (len.isError()) return Error();
			if (cursor->current + len.getValue() > cursor->end) return Error("Reading past the end");
			cursor->current += len.getValue();
			break;
		}
		case 'b':
		case 'c':
		case 'f':
		case 'd':
		case 'l':
		case 'i':
		{
			OptionalError<u32> length = read<u32>(cursor);
			OptionalError<u32> encoding = read<u32>(cursor);
			OptionalError<u32> comp_len = read<u32>(cursor);
			if (length.isError() | encoding.isError() | comp_len.isError()) return Error();
			if (cursor->current + comp_len.getValue() > cursor->end) return Error("Reading past the end");
			cursor->current += comp_len.getValue();
			break;
		}
		default: return Error("Unknown property type");
	}
	prop->value.end = cursor->current;
	return prop.release();
}


static void deleteElement(Element* el)
{
	if (!el) return;

	delete el->first_property;
	deleteElement(el->child);
	Element* iter = el;
	// do not use recursion to avoid stack overflow
	do
	{
		Element* next = iter->sibling;
		delete iter;
		iter = next;
	} while (iter);
}


static OptionalError<u64> readElementOffset(Cursor* cursor, u16 version)
{
	if (version >= 7500)
	{
		OptionalError<u64> tmp = read<u64>(cursor);
		if (tmp.isError()) return Error();
		return tmp.getValue();
	}

	OptionalError<u32> tmp = read<u32>(cursor);
	if (tmp.isError()) return Error();
	return tmp.getValue();
}


static OptionalError<Element*> readElement(Cursor* cursor, u32 version)
{
	OptionalError<u64> end_offset = readElementOffset(cursor, version);
	if (end_offset.isError()) return Error();
	if (end_offset.getValue() == 0) return nullptr;

	OptionalError<u64> prop_count = readElementOffset(cursor, version);
	OptionalError<u64> prop_length = readElementOffset(cursor, version);
	if (prop_count.isError() || prop_length.isError()) return Error();

	const char* sbeg = 0;
	const char* send = 0;
	OptionalError<DataView> id = readShortString(cursor);
	if (id.isError()) return Error();

	Element* element = new Element();
	element->first_property = nullptr;
	element->id = id.getValue();

	element->child = nullptr;
	element->sibling = nullptr;

	Property** prop_link = &element->first_property;
	for (u32 i = 0; i < prop_count.getValue(); ++i)
	{
		OptionalError<Property*> prop = readProperty(cursor);
		if (prop.isError())
		{
			deleteElement(element);
			return Error();
		}

		*prop_link = prop.getValue();
		prop_link = &(*prop_link)->next;
	}

	if (cursor->current - cursor->begin >= (ptrdiff_t)end_offset.getValue()) return element;

	int BLOCK_SENTINEL_LENGTH = version >= 7500 ? 25 : 13;

	Element** link = &element->child;
	while (cursor->current - cursor->begin < ((ptrdiff_t)end_offset.getValue() - BLOCK_SENTINEL_LENGTH))
	{
		OptionalError<Element*> child = readElement(cursor, version);

		if (child.isError())
		{
			deleteElement(element);
			return Error();
		}

		if (nullptr != child.getValue())
		{
			*link = child.getValue();
			link = &(*link)->sibling;
		}
	}

	if (cursor->current + BLOCK_SENTINEL_LENGTH > cursor->end)
	{
		deleteElement(element); 
		return Error("Reading past the end");
	}

	cursor->current += BLOCK_SENTINEL_LENGTH;
	return element;
}


static bool isEndLine(const Cursor& cursor)
{
	return *cursor.current == '\n';
}


static void skipInsignificantWhitespaces(Cursor* cursor)
{
	while (cursor->current < cursor->end && isspace(*cursor->current) && *cursor->current != '\n')
	{
		++cursor->current;
	}
}


static void skipLine(Cursor* cursor)
{
	while (cursor->current < cursor->end && !isEndLine(*cursor))
	{
		++cursor->current;
	}
	if (cursor->current < cursor->end) ++cursor->current;
	skipInsignificantWhitespaces(cursor);
}


static void skipWhitespaces(Cursor* cursor)
{
	while (cursor->current < cursor->end && isspace(*cursor->current))
	{
		++cursor->current;
	}
	while (cursor->current < cursor->end && *cursor->current == ';') skipLine(cursor);
}


static bool isTextTokenChar(char c)
{
	return isalnum(c) || c == '_';
}


static DataView readTextToken(Cursor* cursor)
{
	DataView ret;
	ret.begin = cursor->current;
	while (cursor->current < cursor->end && isTextTokenChar(*cursor->current))
	{
		++cursor->current;
	}
	ret.end = cursor->current;
	return ret;
}


static OptionalError<Property*> readTextProperty(Cursor* cursor)
{
	std::unique_ptr<Property> prop = std::make_unique<Property>();
	prop->value.is_binary = false;
	prop->next = nullptr;
	if (*cursor->current == '"')
	{
		prop->type = 'S';
		++cursor->current;
		prop->value.begin = cursor->current;
		while (cursor->current < cursor->end && *cursor->current != '"')
		{
			++cursor->current;
		}
		prop->value.end = cursor->current;
		if (cursor->current < cursor->end) ++cursor->current; // skip '"'
		return prop.release();
	}
	
	if (isdigit(*cursor->current) || *cursor->current == '-')
	{
		prop->type = 'L';
		prop->value.begin = cursor->current;
		if (*cursor->current == '-') ++cursor->current;
		while (cursor->current < cursor->end && isdigit(*cursor->current))
		{
			++cursor->current;
		}
		prop->value.end = cursor->current;

		if (cursor->current < cursor->end && *cursor->current == '.')
		{
			prop->type = 'D';
			++cursor->current;
			while (cursor->current < cursor->end && isdigit(*cursor->current))
			{
				++cursor->current;
			}
			if (cursor->current < cursor->end && (*cursor->current == 'e' || *cursor->current == 'E'))
			{
				// 10.5e-013
				++cursor->current;
				if (cursor->current < cursor->end && *cursor->current == '-') ++cursor->current;
				while (cursor->current < cursor->end && isdigit(*cursor->current)) ++cursor->current;
			}


			prop->value.end = cursor->current;
		}
		return prop.release();
	}
	
	if (*cursor->current == 'T' || *cursor->current == 'Y')
	{
		// WTF is this
		prop->type = *cursor->current;
		prop->value.begin = cursor->current;
		++cursor->current;
		prop->value.end = cursor->current;
		return prop.release();
	}

	if (*cursor->current == '*')
	{
		prop->type = 'l';
		++cursor->current;
		// Vertices: *10740 { a: 14.2760353088379,... }
		while (cursor->current < cursor->end && *cursor->current != ':')
		{
			++cursor->current;
		}
		if (cursor->current < cursor->end) ++cursor->current; // skip ':'
		skipInsignificantWhitespaces(cursor);
		prop->value.begin = cursor->current;
		prop->count = 0;
		bool is_any = false;
		while (cursor->current < cursor->end && *cursor->current != '}')
		{
			if (*cursor->current == ',')
			{
				if (is_any) ++prop->count;
				is_any = false;
			}
			else if (!isspace(*cursor->current) && *cursor->current != '\n') is_any = true;
			if (*cursor->current == '.') prop->type = 'd';
			++cursor->current;
		}
		if (is_any) ++prop->count;
		prop->value.end = cursor->current;
		if (cursor->current < cursor->end) ++cursor->current; // skip '}'
		return prop.release();
	}

	assert(false);
	return Error("TODO");
}


static OptionalError<Element*> readTextElement(Cursor* cursor)
{
	DataView id = readTextToken(cursor);
	if (cursor->current == cursor->end) return Error("Unexpected end of file");
	if(*cursor->current != ':') return Error("Unexpected end of file");
	++cursor->current;

	skipWhitespaces(cursor);
	if (cursor->current == cursor->end) return Error("Unexpected end of file");

	Element* element = new Element;
	element->id = id;

	Property** prop_link = &element->first_property;
	while (cursor->current < cursor->end && *cursor->current != '\n' && *cursor->current != '{')
	{
		OptionalError<Property*> prop = readTextProperty(cursor);
		if (prop.isError())
		{
			deleteElement(element);
			return Error();
		}
		if (cursor->current < cursor->end && *cursor->current == ',')
		{
			++cursor->current;
			skipWhitespaces(cursor);
		}
		skipInsignificantWhitespaces(cursor);

		*prop_link = prop.getValue();
		prop_link = &(*prop_link)->next;
	}
	
	Element** link = &element->child;
	if (*cursor->current == '{')
	{
		++cursor->current;
		skipWhitespaces(cursor);
		while (cursor->current < cursor->end && *cursor->current != '}')
		{
			OptionalError<Element*> child = readTextElement(cursor);
			if (child.isError())
			{
				deleteElement(element);
				return Error();
			}
			skipWhitespaces(cursor);

			*link = child.getValue();
			link = &(*link)->sibling;
		}
		if (cursor->current < cursor->end) ++cursor->current; // skip '}'
	}
	return element;
}


static OptionalError<Element*> tokenizeText(const u8* data, size_t size)
{
	Cursor cursor;
	cursor.begin = data;
	cursor.current = data;
	cursor.end = data + size;

	Element* root = new Element();
	root->first_property = nullptr;
	root->id.begin = nullptr;
	root->id.end = nullptr;
	root->child = nullptr;
	root->sibling = nullptr;

	Element** element = &root->child;
	while (cursor.current < cursor.end)
	{
		if (*cursor.current == ';' || *cursor.current == '\r' || *cursor.current == '\n')
		{
			skipLine(&cursor);
		}
		else
		{
			OptionalError<Element*> child = readTextElement(&cursor);
			if (child.isError())
			{
				deleteElement(root);
				return Error();
			}
			*element = child.getValue();
			if (!*element) return root;
			element = &(*element)->sibling;
		}
	}

	return root;
}


static OptionalError<Element*> tokenize(const u8* data, size_t size)
{
	Cursor cursor;
	cursor.begin = data;
	cursor.current = data;
	cursor.end = data + size;

	const Header* header = (const Header*)cursor.current;
	cursor.current += sizeof(*header);

	Element* root = new Element();
	root->first_property = nullptr;
	root->id.begin = nullptr;
	root->id.end = nullptr;
	root->child = nullptr;
	root->sibling = nullptr;

	Element** element = &root->child;
	for (;;)
	{
		OptionalError<Element*> child = readElement(&cursor, header->version);
		if (child.isError())
		{
			deleteElement(root);
			return Error();
		}
		*element = child.getValue();
		if (!*element) return root;
		element = &(*element)->sibling;
	}
}


static void parseTemplates(const Element& root)
{
	const Element* defs = findChild(root, "Definitions");
	if (!defs) return;

	std::unordered_map<std::string, Element*> templates;
	Element* def = defs->child;
	while (def)
	{
		if (def->id == "ObjectType")
		{
			Element* subdef = def->child;
			while (subdef)
			{
				if (subdef->id == "PropertyTemplate")
				{
					DataView prop1 = def->first_property->value;
					DataView prop2 = subdef->first_property->value;
					std::string key((const char*)prop1.begin, prop1.end - prop1.begin);
					key += std::string((const char*)prop1.begin, prop1.end - prop1.begin);
					templates[key] = subdef;
				}
				subdef = subdef->sibling;
			}
		}
		def = def->sibling;
	}
	// TODO
}


struct Scene;


Mesh::Mesh(const Scene& _scene, const IElement& _element)
	: Model(_scene, _element)
{
}


struct MeshImpl : Mesh
{
	MeshImpl(const Scene& _scene, const IElement& _element)
		: Mesh(_scene, _element)
		, scene(_scene)
	{
		is_node = true;
	}


	OFBMatrix getGeometricMatrix() const override
	{
		OFBVector3 translation = GeometricTranslation;
		OFBVector3 rotation = GeometricRotation;
		OFBVector3 scale = GeometricScaling;

		OFBMatrix scale_mtx = makeIdentity();
		scale_mtx.m[0] = (float)scale.x;
		scale_mtx.m[5] = (float)scale.y;
		scale_mtx.m[10] = (float)scale.z;
		OFBMatrix mtx = getRotationMatrix(rotation, OFBRotationOrder::eEULER_XYZ);
		setTranslation(translation, &mtx);

		return scale_mtx * mtx;
	}


	Type getType() const override { return Type::MESH; }


	const Geometry* getGeometry() const override { return geometry; }
	const Material* getMaterial(int index) const override { return materials[index]; }
	int getMaterialCount() const override { return (int)materials.size(); }

	bool IsStatic() const override {

		// TODO: check if there is a constant zero key
		//if (mAnimationNodes.size() > 0)
		//	return false;

		if (Translation.IsAnimated() || Rotation.IsAnimated() || Scaling.IsAnimated())
			return false;

		if (nullptr != geometry)
		{
			if (nullptr != geometry->getSkin())
				return false;
		}

		return true;
	}

	const Geometry* geometry = nullptr;
	const Scene& scene;
	std::vector<const Material*> materials;
};

////////////////////////////////////////////////////////////////////////////
// Material

Material::Material(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
	Ambient.Init(this, "AmbientColor");
	AmbientFactor.Init(this, "AmbientFactor");

	Emissive.Init(this, "EmissiveColor");
	EmissiveFactor.Init(this, "EmissiveFactor");

	Diffuse.Init(this, "DiffuseColor");
	DiffuseFactor.Init(this, "DiffuseFactor");

	TransparentColor.Init(this, "TransparentColor");
	TransparencyFactor.Init(this, "TransparencyFactor");

	Bump.Init(this, "Bump");
	NormalMap.Init(this, "NormalMap");
	BumpFactor.Init(this, "BumpFactor");
	
	Specular.Init(this, "SpecularColor");
	SpecularFactor.Init(this, "SpecularFactor");
	Shininess.Init(this, "ShininessExponent");

	Reflection.Init(this, "ReflectionColor");
	ReflectionFactor.Init(this, "ReflectionFactor");

	DisplacementColor.Init(this, "DisplacementColor");
	DisplacementFactor.Init(this, "DisplacementFactor");

	// Default values
	Ambient = { 0.2, 0.2, 0.2 };
	AmbientFactor = 1.0;

	Emissive = { 0.0, 0.0, 0.0 };
	EmissiveFactor = 1.0;

	Diffuse = { 0.8, 0.8, 0.8 };
	DiffuseFactor = 1.0;

	TransparentColor = { 0.0, 0.0, 0.0 };
	TransparencyFactor = 0.0;

	Bump = { 0.0, 0.0, 0.0 };
	NormalMap = { 0.0, 0.0, 0.0 };
	BumpFactor = 1.0;

	Specular = { 0.2, 0.2, 0.2 };
	SpecularFactor = 1.0;
	Shininess = 20.0;

	Reflection = { 0.0, 0.0, 0.0 };
	ReflectionFactor = 1.0;

	DisplacementColor = { 0.0, 0.0, 0.0 };
	DisplacementFactor = 1.0;
}


struct MaterialImpl : Material
{
	MaterialImpl(const Scene& _scene, const IElement& _element)
		: Material(_scene, _element)
	{
		for (const Texture*& tex : textures) tex = nullptr;
	}

	Type getType() const override { return Type::MATERIAL; }


	const Texture* GetTexture(Texture::TextureType type) const override { return textures[type]; }
	const Texture* textures[Texture::TextureType::COUNT];
};

/////////////////////////////////////////////////////////////////////////
// Shader

Shader::Shader(const Scene& _scene, const IElement& _element)
:Object(_scene, _element)
{}

struct ShaderImpl : Shader
{
	ShaderImpl(const Scene& _scene, const IElement& _element)
	: Shader(_scene, _element)
	{}

	Type getType() const override { return Type::SHADER; }
};

struct LimbNodeImpl : ModelSkeleton
{
	LimbNodeImpl(const Scene& _scene, const IElement& _element)
		: ModelSkeleton(_scene, _element)
	{
		is_node = true;
		
		Size.Init(this, "Size");
		Color.Init(this, "Color");

		Size = 10.0;
		Color.SetPropertyValue({ 0.85, 0.85, 0.20 });
	}
	Type getType() const override { return Type::LIMB_NODE; }

	bool Retrieve() override
	{
		return Object::Retrieve();
	}

	bool HasCustomDisplay() const override { return true; }
	void CustomModelDisplay(OFBRenderConveyer	*pConveyer) const override
	{
		// three circles for each axis

		const float radius = 1.0f;
		const float segs = 12.0f;

		float t = 0.0f;
		float maxt = 2.0f * (float)MATH_PI;
		float step = maxt / segs;
		float cos1, sin1, cos2, sin2;

		while (t < maxt)
		{
			cos1 = radius*cos(t);
			sin1 = radius*sin(t);
			t += step;
			cos2 = radius*cos(t);
			sin2 = radius*sin(t);

			pConveyer->PushLine({ cos1, sin1, 0.0 }, { cos2, sin2, 0.0 });
			pConveyer->PushLine({ cos1, 0.0, sin1 }, { cos2, 0.0, sin2 });
			pConveyer->PushLine({ 0.0, cos1, sin1 }, { 0.0, cos2, sin2 });
		}

		// TODO: draw links

		Model *pChild = Children();
		while (nullptr != pChild)
		{
			OFBVector3 v;
			//pChild->GetVector(v, eModelTranslation, false, nullptr);
			v = pChild->Translation;

			pConveyer->PushLine({ 0.0, 0.0, 0.0 }, 0.1 * v);

			pChild = pChild->GetNext();
		}
	}
};

////////////////////////////////////////////////////////////////////////
// NullImpl

struct NullImpl : ModelNull
{
	NullImpl(const Scene& _scene, const IElement& _element)
		: ModelNull(_scene, _element)
	{
		is_node = true;
		//mSize = 100.0;
		Size.Init(this, "Size");

		Size = 100.0;
	}
	Type getType() const override { return Type::NULL_NODE; }

	bool Retrieve() override
	{
		return Object::Retrieve();
	}

	bool HasCustomDisplay() const override { return true; }
	void CustomModelDisplay(OFBRenderConveyer	*pConveyer) const override
	{
		pConveyer->PushLine({ -1.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 });
		pConveyer->PushLine({ 0.0, -1.0, 0.0 }, { 0.0, 1.0, 0.0 });
		pConveyer->PushLine({ 0.0, 0.0, -1.0 }, { 0.0, 0.0, 1.0 });
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
// CameraImpl

struct CameraImpl : Camera
{
	CameraImpl(const Scene& _scene, const IElement& _element)
	: Camera(_scene, _element)
	{
		is_node = true;
		
		Color.Init(this, "Color");
		Position.Init(this, "Position");
		UpVector.Init(this, "UpVector");
		InterestPosition.Init(this, "InterestPosition");

		OpticalCenterX.Init(this, "OpticalCenterX");
		OpticalCenterY.Init(this, "OpticalCenterY");

		BackgroundColor.Init(this, "BackgroundColor");
		UseFrameColor.Init(this, "UseFrameColor");
		FrameColor.Init(this, "FrameColor");
		TurnTable.Init(this, "TurnTable");

		AspectRatioMode.Init(this, "AspectRatioMode");
		AspectWidth.Init(this, "AspectWidth");
		AspectHeight.Init(this, "AspectHeight");
		
		PixelAspectRatio.Init(this, "PixelAspectRatio");
		ApertureMode.Init(this, "ApertureMode");

		FilmOffsetX.Init(this, "FilmOffsetX");
		FilmOffsetY.Init(this, "FilmOffsetY");
		FilmWidth.Init(this, "FilmWidth");
		FilmHeight.Init(this, "FilmHeight");

		FilmAspectRatio.Init(this, "FilmAspectRatio");
		FilmSqueezeRatio.Init(this, "FilmSqueezeRatio");

		WindowWidth.Init(this, "WindowWidth");
		WindowHeight.Init(this, "WindowHeight");

		FieldOfView.Init(this, "FieldOfView");
		FieldOfViewX.Init(this, "FieldOfViewX");
		FieldOfViewY.Init(this, "FieldOfViewY");
		FocalLength.Init(this, "FocalLength");

		NearPlane.Init(this, "NearPlane");
		FarPlane.Init(this, "FarPlane");

		Target.Init(this, "LookAtProperty");
		Roll.Init(this, "Roll");

		//
		Color = { 0.8 };
		Position = Vector_Zero();
		UpVector = { 0.0, 1.0, 0.0 };
		InterestPosition = Vector_Zero();
		Roll = 0.0;
		OpticalCenterX = 0.0;
		OpticalCenterY = 0.0;
		BackgroundColor = { 0.63 };
		UseFrameColor = false;
		FrameColor = { 0.3 };

		TurnTable = 0.0;
		AspectRatioMode.SetPropertyValue(eFrameSizeWindow);
		AspectWidth = 320.0;
		AspectHeight = 200.0;
		PixelAspectRatio = 1.0;
		ApertureMode.SetPropertyValue(eApertureVertical);

		FilmOffsetX = 0.0;
		FilmOffsetY = 0.0;
		FilmWidth = 0.816;
		FilmHeight = 0.612;
		FilmAspectRatio = 1.3333333;
		FilmSqueezeRatio = 1.0;

		WindowWidth = 640;
		WindowHeight = 680;

		FieldOfView = 25.114999;
		FieldOfViewX = 40.0;
		FieldOfViewY = 40.0;
		FocalLength = 34.89327;

		NearPlane = 10.0;
		FarPlane = 4000.0;

		Target = nullptr;
		Roll = 0.0;

		//
		mCacheTime = OFBTime::MinusInfinity;
		mManualSet = false;
	}
	Type getType() const override { return Type::CAMERA; }

	// DONE:
	Model *GetTarget() const override
	{
		// resolveObjectLink(Type::NULL_NODE, Target.GetName(), 0);
		Object *pTarget = Target;
		return (Model*)pTarget;
	}

	bool GetCameraMatrix(float *pMatrix, CameraMatrixType pType, const OFBTime *pTime = nullptr) override
	{
		OFBTime lTime((nullptr != pTime) ? pTime->Get() : gDisplayInfo.localTime.Get());

		if (false == mManualSet)
		{
			if (mCacheTime.Get() != lTime.Get())
			{
				ComputeCameraMatrix(&lTime);
				mCacheTime.Set(lTime.Get());
			}
		}
		mManualSet = false;

		switch (pType)
		{
		case eProjection:
			for (int i = 0; i < 16; ++i)
				pMatrix[i] = (float)mProjection[i];
			break;
		case eModelView:
			for (int i = 0; i < 16; ++i)
				pMatrix[i] = (float)mModelView[i];
			break;
		}

		// TODO: 
		return true;
	}

	bool GetCameraMatrix(double *pMatrix, CameraMatrixType pType, const OFBTime *pTime = nullptr) override
	{
		OFBTime lTime((nullptr != pTime) ? pTime->Get() : gDisplayInfo.localTime.Get());

		if (false == mManualSet)
		{
			if (mCacheTime.Get() != lTime.Get())
			{
				ComputeCameraMatrix(&lTime);
				mCacheTime.Set(lTime.Get());
			}
		}
		mManualSet = false;

		switch (pType)
		{
		case eProjection:
			for (int i = 0; i < 16; ++i)
				pMatrix[i] = mProjection[i];
			break;
		case eModelView:
			for (int i = 0; i < 16; ++i)
				pMatrix[i] = mModelView[i];
			break;
		}

		return true;
	}

	double ComputeFieldOfView(const double focal, const double h) const override
	{
		double fov = 2.0 * atan(h / 2.0 * focal);
		return fov;
	}

	bool ComputeCameraMatrix(OFBTime *pTime = nullptr)
	{
		OFBTime lTime((nullptr != pTime) ? pTime->Get() : gDisplayInfo.localTime.Get());

		// Compute the camera position and direction.
		
		OFBVector3 lCenter;
		
		OFBVector3 lForward, lRight;

		OFBVector3 lEye; // = Position;
		GetVector(lEye, eModelTranslation, true, &lTime);
		OFBVector3 lUp = UpVector;

		if (nullptr != GetTarget())
		{
			GetTarget()->GetVector(lCenter, eModelTranslation, true, pTime);
		}
		else
		{
			// Get the direction
			OFBMatrix lGlobalRotation;
			OFBMatrix lGlobalTransform;

			GetMatrix(lGlobalTransform, eModelTransformation, true, pTime);
			OFBVector4 lRotationVector = MatrixGetRotation(lGlobalTransform);
			QuaternionToMatrix(lGlobalRotation, lRotationVector);
			

			// Get the length
			//FbxVector4 lInterestPosition(lCamera->InterestPosition.Get());
			//FbxVector4 lCameraGlobalPosition(GetGlobalPosition(lCameraNode, pTime).GetT());
			//double      lLength = (FbxVector4(lInterestPosition - lCameraGlobalPosition).Length());
			double lLength = 1.0;

			// Set the center.
			// A camera with rotation = {0,0,0} points to the X direction. So create a
			// vector in the X direction, rotate that vector by the global rotation amount
			// and then position the center by scaling and translating the resulting vector
			
			OFBVector3 frontVector = { 1.0, 0.0, 0.0 };
			OFBVector3 upVector = { 0.0, 1.0, 0.0 };

			VectorTransform33(lCenter, frontVector, lGlobalRotation);

			lCenter = lCenter * lLength;
			lCenter = lCenter + lEye;

			// Update the default up vector with the camera rotation.
			VectorTransform33(lUp, upVector, lGlobalRotation);
		}

		// Align the up vector.
		lForward = lCenter - lEye;
		VectorNormalize(lForward);
		lRight = CrossProduct(lForward, lUp);
		VectorNormalize(lRight);
		lUp = CrossProduct(lRight, lForward);
		VectorNormalize(lUp);

		// Rotate the up vector with the roll value.
		double lRadians = 0;
		double lRoll = 0.0;
		
		Roll.GetData(&lRoll, sizeof(double), &lTime);

		lRadians = lRoll * M_PI / 180.0; // FBXSDK_PI_DIV_180;
		lUp = lUp * cos(lRadians) + lRight * sin(lRadians);

		const double lNearPlane = NearPlane;
		const double lFarPlane = FarPlane;

		// Get the relevant camera settings for a perspective view.
		if (eCameraTypePerspective == ProjectionType)
		{
			//get the aspect ratio
			OFBCameraFrameSizeMode lCamAspectRatioMode = AspectRatioMode;
			double lAspectX = AspectWidth;
			double lAspectY = AspectHeight;
			double lAspectRatio = 1.333333;

			switch (lCamAspectRatioMode)
			{
			case eFrameSizeWindow:
				lAspectRatio = lAspectX / lAspectY;
				break;
			case eFrameSizeFixedRatio:
				lAspectRatio = lAspectX;

				break;
			case eFrameSizeFixedResolution:
				lAspectRatio = lAspectX / lAspectY * PixelAspectRatio;
				break;
			case eFrameSizeFixedWidthResolution:
				lAspectRatio = PixelAspectRatio / lAspectY;
				break;
			case eFrameSizeFixedHeightResolution:
				lAspectRatio = PixelAspectRatio * lAspectX;
				break;
			default:
				break;

			}

			//get the aperture ratio
			double lFilmHeight = FilmHeight;
			double lFilmWidth = FilmWidth * FilmSqueezeRatio;
			//here we use Height : Width
			double lApertureRatio = lFilmHeight / lFilmWidth;


			//change the aspect ratio to Height : Width
			lAspectRatio = 1.0 / lAspectRatio;
			/*
			//revise the aspect ratio and aperture ratio
			FbxCamera::EGateFit lCameraGateFit = lCamera->GateFit.Get();
			switch (lCameraGateFit)
			{

			case FbxCamera::eFitFill:
				if (lApertureRatio > lAspectRatio)  // the same as eHORIZONTAL_FIT
				{
					lFilmHeight = lFilmWidth * lAspectRatio;
					lCamera->SetApertureHeight(lFilmHeight);
					lApertureRatio = lFilmHeight / lFilmWidth;
				}
				else if (lApertureRatio < lAspectRatio) //the same as eVERTICAL_FIT
				{
					lFilmWidth = lFilmHeight / lAspectRatio;
					lCamera->SetApertureWidth(lFilmWidth);
					lApertureRatio = lFilmHeight / lFilmWidth;
				}
				break;
			case FbxCamera::eFitVertical:
				lFilmWidth = lFilmHeight / lAspectRatio;
				lCamera->SetApertureWidth(lFilmWidth);
				lApertureRatio = lFilmHeight / lFilmWidth;
				break;
			case FbxCamera::eFitHorizontal:
				lFilmHeight = lFilmWidth * lAspectRatio;
				lCamera->SetApertureHeight(lFilmHeight);
				lApertureRatio = lFilmHeight / lFilmWidth;
				break;
			case FbxCamera::eFitStretch:
				lAspectRatio = lApertureRatio;
				break;
			case FbxCamera::eFitOverscan:
				if (lFilmWidth > lFilmHeight)
				{
					lFilmHeight = lFilmWidth * lAspectRatio;
				}
				else
				{
					lFilmWidth = lFilmHeight / lAspectRatio;
				}
				lApertureRatio = lFilmHeight / lFilmWidth;
				break;
			case FbxCamera::eFitNone:
			default:
				break;
			}
			*/
			//change the aspect ratio to Width : Height
			lAspectRatio = 1.0 / lAspectRatio;

			double lFieldOfViewX = 0.0;
			double lFieldOfViewY = 0.0;
			
			double lFocalLength = 0.0;

			switch (ApertureMode)
			{
			case eApertureVertical:
				FieldOfView.GetData(&lFieldOfViewY, sizeof(double), &lTime);
				lFieldOfViewX = VFOV2HFOV(lFieldOfViewY, 1.0 / lApertureRatio);
				break;
			case eApertureHorizontal:
				FieldOfView.GetData(&lFieldOfViewX, sizeof(double), &lTime); //get HFOV
				lFieldOfViewY = HFOV2VFOV(lFieldOfViewX, lApertureRatio);
				break;
			case eApertureFocalLength:
				FocalLength.GetData(&lFocalLength, sizeof(double), &lTime);
				lFieldOfViewX = ComputeFieldOfView(lFocalLength, lFilmWidth);    //get HFOV
				lFieldOfViewY = HFOV2VFOV(lFieldOfViewX, lApertureRatio);
				break;
			case eApertureVertHoriz:
				FieldOfViewX.GetData(&lFieldOfViewX, sizeof(double), &lTime);
				FieldOfViewY.GetData(&lFieldOfViewY, sizeof(double), &lTime);
			}


			double lRealScreenRatio = WindowWidth / WindowHeight;
			int  lViewPortPosX = 0,
				lViewPortPosY = 0,
				lViewPortSizeX = WindowWidth,
				lViewPortSizeY = WindowHeight;
			//compute the view port
			if (lRealScreenRatio > lAspectRatio)
			{
				lViewPortSizeY = WindowHeight;
				lViewPortSizeX = (int)(lViewPortSizeY * lAspectRatio);
				lViewPortPosY = 0;
				lViewPortPosX = (int)((WindowWidth - lViewPortSizeX) * 0.5);
			}
			else
			{
				lViewPortSizeX = WindowWidth;
				lViewPortSizeY = (int)(lViewPortSizeX / lAspectRatio);
				lViewPortPosX = 0;
				lViewPortPosY = (int)((WindowHeight - lViewPortSizeY) * 0.5);
			}

			//revise the Perspective since we have film offset
			double lFilmOffsetX = FilmOffsetX.Get();
			double lFilmOffsetY = FilmOffsetY.Get();
			lFilmOffsetX = 0.0 - lFilmOffsetX / lFilmWidth * 2.0;
			lFilmOffsetY = 0.0 - lFilmOffsetY / lFilmHeight * 2.0;

			GetCameraPerspectiveMatrix(mProjection, mModelView, lFieldOfViewY, lAspectRatio, lNearPlane, lFarPlane, 
				lEye, lCenter, lUp, lFilmOffsetX, lFilmOffsetY);

		}
		else
		{
			double lPixelRatio = PixelAspectRatio;

			double lLeftPlane, lRightPlane, lBottomPlane, lTopPlane;

			double lWindowWidth = WindowWidth;
			double lWindowHeight = WindowHeight;

			if (lWindowWidth < lWindowHeight)
			{
				lLeftPlane = -gsOrthoCameraScale * lPixelRatio;
				lRightPlane = gsOrthoCameraScale * lPixelRatio;
				lBottomPlane = -gsOrthoCameraScale * lWindowHeight / lWindowWidth;
				lTopPlane = gsOrthoCameraScale * lWindowHeight / lWindowWidth;
			}
			else
			{
				lWindowWidth *= (int)lPixelRatio;
				lLeftPlane = -gsOrthoCameraScale * lWindowWidth / lWindowHeight;
				lRightPlane = gsOrthoCameraScale * lWindowWidth / lWindowHeight;
				lBottomPlane = -gsOrthoCameraScale;
				lTopPlane = gsOrthoCameraScale;
			}

			GetCameraOrthogonal(mProjection, mModelView, lLeftPlane,
				lRightPlane,
				lBottomPlane,
				lTopPlane,
				lNearPlane,
				lFarPlane,
				lEye,
				lCenter,
				lUp);
		}
		// TODO: 
		return true;
	}

	// override camera matrix
	void SetCameraMatrix(const float *pMatrix, CameraMatrixType pType) override
	{
		double *dst = nullptr;

		if (eProjection == pType)
		{
			dst = &mProjection.m[0];
		}
		else if (eModelView == pType)
		{
			dst = &mModelView.m[0];
		}

		for (int i = 0; i < 16; ++i)
			dst[i] = (double) pMatrix[i];
		mManualSet = true;
	}
	void SetCameraMatrix(const double *pMatrix, CameraMatrixType pType) override
	{
		double *dst = nullptr;

		if (eProjection == pType)
		{
			dst = &mProjection.m[0];
		}
		else if (eModelView == pType)
		{
			dst = &mModelView.m[0];
		}

		for (int i = 0; i < 16; ++i)
			dst[i] = pMatrix[i];
		mManualSet = true;
	}

	bool Retrieve() override
	{
		return Object::Retrieve();
	}

	bool HasCustomDisplay() const override { return true; }
	void CustomModelDisplay(OFBRenderConveyer	*pConveyer) const override
	{
		auto fn_constructbody = [](OFBRenderConveyer *conveyer, double size, OFBVector3 offset){
			// front
			conveyer->PushLine(offset + size * Vector_Make(0.0, 0.2, -0.1), offset + size * Vector_Make(0.0, 0.2, 0.1));
			conveyer->PushLine(offset + size * Vector_Make(0.0, -0.2, -0.1), offset + size * Vector_Make(0.0, -0.2, 0.1));
			conveyer->PushLine(offset + size * Vector_Make(0.0, 0.2, -0.1), offset + size * Vector_Make(0.0, -0.2, -0.1));
			conveyer->PushLine(offset + size * Vector_Make(0.0, 0.2, 0.1), offset + size * Vector_Make(0.0, -0.2, 0.1));

			// front to top connection
			conveyer->PushLine(offset + size * Vector_Make(0.0, 0.2, 0.1), offset + size * Vector_Make(-0.1, 0.3, 0.1));
			conveyer->PushLine(offset + size * Vector_Make(0.0, 0.2, -0.1), offset + size * Vector_Make(-0.1, 0.3, -0.1));

			// top
			conveyer->PushLine(offset + size * Vector_Make(-0.1, 0.3, 0.1)*size, offset + size * Vector_Make(-0.1, 0.3, -0.1));

			conveyer->PushLine(offset + size * Vector_Make(-0.1, 0.3, -0.1)*size, offset + size * Vector_Make(-1.0, 0.3, -0.1));
			conveyer->PushLine(offset + size * Vector_Make(-0.1, 0.3, 0.1), offset + size * Vector_Make(-1.0, 0.3, 0.1));

			conveyer->PushLine(offset + size * Vector_Make(-1.0, 0.3, -0.1), offset + size * Vector_Make(-1.0, 0.3, 0.1));
			 
			// back

			conveyer->PushLine(offset + size * Vector_Make(-1.0, 0.3, 0.1), offset + size * Vector_Make(-1.0, -0.1, 0.1));
			conveyer->PushLine(offset + size * Vector_Make(-1.0, 0.3, -0.1), offset + size * Vector_Make(-1.0, -0.1, -0.1));

			conveyer->PushLine(offset + size * Vector_Make(-1.0, -0.1, 0.1), offset + size * Vector_Make(-1.0, -0.1, -0.1));

			// bottom

			conveyer->PushLine(offset + size * Vector_Make(0.0, -0.2, 0.1), offset + size * Vector_Make(-0.9, -0.2, 0.1));
			conveyer->PushLine(offset + size * Vector_Make(0.0, -0.2, -0.1), offset + size * Vector_Make(-0.9, -0.2, -0.1));

			conveyer->PushLine(offset + size * Vector_Make(-0.9, -0.2, 0.1), offset + size * Vector_Make(-0.9, -0.2, -0.1));

			// bottom to back connection

			conveyer->PushLine(offset + size * Vector_Make(-0.9, -0.2, 0.1), offset + size * Vector_Make(-1.0, -0.1, 0.1));
			conveyer->PushLine(offset + size * Vector_Make(-0.9, -0.2, -0.1), offset + size * Vector_Make(-1.0, -0.1, -0.1));
		};

		//
		fn_constructbody(pConveyer, 1.0, { 0.0, 0.0, 0.0 });
		fn_constructbody(pConveyer, 0.5, { -0.4, 0.0, 0.2 });


		auto fn_constructcircletrim = [](OFBRenderConveyer *conveyer, double size, OFBVector3 offset, int trim) {
			//
			const int segs = 16;
			double t = 0.0;
			double maxt = 2.0 * MATH_PI;
			double step = maxt / segs;

			const double radius = 0.25;

			t += step * trim;

			double x = radius * cos(t);
			double y = radius * sin(t);

			for (int i = 0; i < 10; ++i)
			{
				t += step;

				double x2 = radius * cos(t);
				double y2 = radius * sin(t);

				conveyer->PushLine(offset + size * Vector_Make(x, y, 0.0), offset + size * Vector_Make(x2, y2, 0.0));
				conveyer->PushLine(offset + size * Vector_Make(x, y, 0.2), offset + size * Vector_Make(x2, y2, 0.2));

				conveyer->PushLine(offset + size * Vector_Make(x2, y2, 0.0), offset + size * Vector_Make(x2, y2, 0.2));

				x = x2;
				y = y2;
			}
		};

		fn_constructcircletrim(pConveyer, 1.0, { -0.5, 0.55, -0.1 }, 12);
		fn_constructcircletrim(pConveyer, 1.0, { -0.75, 0.55, -0.1 }, 2);

		//

		auto fn_constructcircle = [](OFBRenderConveyer *conveyer, double size, OFBVector3 offset) {
			const int segs = 16;
			double t = 0.0;
			double maxt = 2.0 * MATH_PI;
			double step = maxt / segs;

			const double radius = 0.25;


			double x = radius * cos(t);
			double y = radius * sin(t);

			for (int i = 0; i < segs; ++i)
			{
				t += step;

				double x2 = radius * cos(t);
				double y2 = radius * sin(t);

				conveyer->PushLine(offset + size * Vector_Make(0.0, x, y), offset + size * Vector_Make(0.0, x2, y2));
				conveyer->PushLine(offset + size * Vector_Make(0.2, x, y), offset + size * Vector_Make(0.2, x2, y2));

				conveyer->PushLine(offset + size * Vector_Make(0.0, x2, y2), offset + size * Vector_Make(0.2, x2, y2));

				x = x2;
				y = y2;
			}
		};

		// TODO: attach to camera FOV and aspect
		fn_constructcircle(pConveyer, 0.3, { 0.0, 0.0, 0.0 });
	}

protected:

	OFBMatrix		mModelView;
	OFBMatrix		mProjection;

	OFBTime			mCacheTime;
	bool			mManualSet;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// LightImpl

struct LightImpl : Light
{
	LightImpl(const Scene& _scene, const IElement &_element)
	: Light(_scene, _element)
	{}

	Type getType() const override { return Type::LIGHT;  }

	bool HasCustomDisplay() const override { return true; }
	void CustomModelDisplay(OFBRenderConveyer	*pConveyer) const override
	{

		auto fn_constructcircle = [](OFBRenderConveyer *conveyer, double size, OFBVector3 offset) {
			const int segs = 16;
			double t = 0.0;
			double maxt = 2.0 * MATH_PI;
			double step = maxt / segs;

			const double radius = 0.25;


			double x = radius * cos(t);
			double y = radius * sin(t);

			for (int i = 0; i < segs; ++i)
			{
				t += step;

				double x2 = radius * cos(t);
				double y2 = radius * sin(t);

				conveyer->PushLine(offset + size * Vector_Make(x, 0.0, y), offset + size * Vector_Make(x2, 0.0, y2));
				
				x = x2;
				y = y2;
			}
		};

		auto fn_constructcone = [](OFBRenderConveyer *conveyer, double size, double size2, double height, OFBVector3 offset) {
			const int segs = 16;
			double t = 0.0;
			double maxt = 2.0 * MATH_PI;
			double step = maxt / segs;

			const double radius = 0.25;


			double x = radius * cos(t);
			double y = radius * sin(t);

			for (int i = 0; i < segs; ++i)
			{
				t += step;

				double x2 = radius * cos(t);
				double y2 = radius * sin(t);

				conveyer->PushLine(offset + Vector_Make(size*x, 0.0, size*y), offset + Vector_Make(size*x2, 0.0, size*y2));
				conveyer->PushLine(offset + Vector_Make(size2*x, height, size2*y), offset + Vector_Make(size2*x2, height, size2*y2));

				conveyer->PushLine(offset + Vector_Make(size*x, 0.0, size*y), offset + Vector_Make(size2*x, height, size2*y));

				x = x2;
				y = y2;
			}
		};

		if (eLightTypePoint == LightType)
		{
			fn_constructcircle(pConveyer, 1.0, { 0.0, 0.0, 0.0 });

			fn_constructcircle(pConveyer, 0.9, { 0.0, 0.15, 0.0 });
			fn_constructcircle(pConveyer, 0.9, { 0.0, -0.15, 0.0 });

			fn_constructcircle(pConveyer, 0.7, { 0.0, 0.3, 0.0 });
			fn_constructcircle(pConveyer, 0.7, { 0.0, -0.3, 0.0 });
		}
		else if (eLightTypeInfinite == LightType)
		{
			fn_constructcircle(pConveyer, 0.5, { 0.0, 0.0, 0.0 });

			fn_constructcircle(pConveyer, 0.4, { 0.0, 0.15, 0.0 });
			fn_constructcircle(pConveyer, 0.4, { 0.0, -0.15, 0.0 });

			fn_constructcircle(pConveyer, 0.25, { 0.0, 0.3, 0.0 });
			fn_constructcircle(pConveyer, 0.25, { 0.0, -0.3, 0.0 });

			pConveyer->PushLine({ 0.0, -0.5, 0.0 }, { 0.0, 0.5, 0.0 });

			fn_constructcone(pConveyer, 0.01, 0.25, 0.25, { 0.0, -0.75, 0.0 });
		}
		else
		{
			pConveyer->PushLine({ 0.0, -0.75, 0.0 }, { 0.0, 0.25, 0.0 });

			fn_constructcone(pConveyer, 0.01, 0.5, -0.5, { 0.0, 0.0, 0.0 });
			fn_constructcone(pConveyer, 0.01, 0.25, 0.25, { 0.0, -1.0, 0.0 });
		}
		
	}
};

NodeAttribute::NodeAttribute(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


struct NodeAttributeImpl : NodeAttribute
{
	NodeAttributeImpl(const Scene& _scene, const IElement& _element)
		: NodeAttribute(_scene, _element)
	{
	}
	Type getType() const override { return Type::NODE_ATTRIBUTE; }
	DataView getAttributeType() const override { return attribute_type; }


	DataView attribute_type;
};


Geometry::Geometry(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


struct GeometryImpl : Geometry
{
	enum VertexDataMapping
	{
		BY_POLYGON_VERTEX,
		BY_POLYGON,
		BY_VERTEX
	};

	struct NewVertex
	{
		~NewVertex() { delete next; }

		int index = -1;
		NewVertex* next = nullptr;
	};

	std::vector<OFBVector3> vertices;
	std::vector<OFBVector3> normals;
	std::vector<OFBVector2> uvs;
	std::vector<OFBVector4> colors;
	std::vector<OFBVector3> tangents;
	std::vector<int> materials;

	const Skin* skin = nullptr;

	std::vector<int> to_old_vertices;
	std::vector<NewVertex> to_new_vertices;

	GeometryImpl(const Scene& _scene, const IElement& _element)
		: Geometry(_scene, _element)
	{
	}


	Type getType() const override { return Type::GEOMETRY; }
	int getVertexCount() const override { return (int)vertices.size(); }
	const OFBVector3* getVertices() const override { return &vertices[0]; }
	const OFBVector3* getNormals() const override { return normals.empty() ? nullptr : &normals[0]; }
	const OFBVector2* getUVs() const override { return uvs.empty() ? nullptr : &uvs[0]; }
	const OFBVector4* getColors() const override { return colors.empty() ? nullptr : &colors[0]; }
	const OFBVector3* getTangents() const override { return tangents.empty() ? nullptr : &tangents[0]; }
	const Skin* getSkin() const override { return skin; }
	const int* getMaterials() const override { return materials.empty() ? nullptr : &materials[0]; }


	void triangulate(const std::vector<int>& old_indices, std::vector<int>* indices, std::vector<int>* to_old)
	{
		assert(indices);
		assert(to_old);

		auto getIdx = [&old_indices](int i) -> int {
			int idx = old_indices[i];
			return idx < 0 ? -idx - 1 : idx;
		};

		int in_polygon_idx = 0;
		for (int i = 0; i < (int)old_indices.size(); ++i)
		{
			int idx = getIdx(i);
			if (in_polygon_idx <= 2)
			{
				indices->push_back(idx);
				to_old->push_back(i);
			}
			else
			{
				indices->push_back(old_indices[i - in_polygon_idx]);
				to_old->push_back(i - in_polygon_idx);
				indices->push_back(old_indices[i - 1]);
				to_old->push_back(i - 1);
				indices->push_back(idx);
				to_old->push_back(i);
			}
			++in_polygon_idx;
			if (old_indices[i] < 0)
			{
				in_polygon_idx = 0;
			}
		}
	}
};


Cluster::Cluster(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


struct ClusterImpl : Cluster
{
	ClusterImpl(const Scene& _scene, const IElement& _element)
		: Cluster(_scene, _element)
	{
	}

	const int* getIndices() const override { return &indices[0]; }
	int getIndicesCount() const override { return (int)indices.size(); }
	const double* getWeights() const override { return &weights[0]; }
	int getWeightsCount() const override { return (int)weights.size(); }
	OFBMatrix getTransformMatrix() const override { return transform_matrix; }
	OFBMatrix getTransformLinkMatrix() const override { return transform_link_matrix; }
	Object* getLink() const override { return link; }


	bool postprocess()
	{
		assert(skin);

		GeometryImpl* geom = (GeometryImpl*)skin->resolveObjectLinkReverse(Object::Type::GEOMETRY);
		if (!geom) return false;

		std::vector<int> old_indices;
		const Element* indexes = findChild((const Element&)element, "Indexes");
		if (indexes && indexes->first_property)
		{
			if (!parseBinaryArray(*indexes->first_property, &old_indices)) return false;
		}

		std::vector<double> old_weights;
		const Element* weights_el = findChild((const Element&)element, "Weights");
		if (weights_el && weights_el->first_property)
		{
			if (!parseBinaryArray(*weights_el->first_property, &old_weights)) return false;
		}

		if (old_indices.size() != old_weights.size()) return false;

		indices.reserve(old_indices.size());
		weights.reserve(old_indices.size());
		int* ir = old_indices.empty() ? nullptr : &old_indices[0];
		double* wr = old_weights.empty() ? nullptr : &old_weights[0];
		for (int i = 0, c = (int)old_indices.size(); i < c; ++i)
		{
			int old_idx = ir[i];
			double w = wr[i];
			GeometryImpl::NewVertex* n = &geom->to_new_vertices[old_idx];
			if (n->index == -1) continue; // skip vertices which aren't indexed.
			while (n)
			{
				indices.push_back(n->index);
				weights.push_back(w);
				n = n->next;
			}
		}

		return true;
	}


	Object* link = nullptr;
	Skin* skin = nullptr;
	std::vector<int> indices;
	std::vector<double> weights;
	OFBMatrix transform_matrix;
	OFBMatrix transform_link_matrix;
	Type getType() const override { return Type::CLUSTER; }
};

Constraint::Constraint(const Scene& _scene, const IElement& _element)
: Object(_scene, _element)
{
	Active.Init(this, "Active");
	Weight.Init(this, "Weight");

	
	//
	Active = false;
	Weight = 100.0;
}

ConstraintPosition::ConstraintPosition(const Scene& _scene, const IElement& _element)
: Constraint(_scene, _element)
{
	
	ConstrainedObject.Init(this, "Constrained Object");
	SourceObject.Init(this, "Source");

	AffectX.Init(this, "AffectX");
	AffectY.Init(this, "AffectY");
	AffectZ.Init(this, "AffectZ");

	Translation.Init(this, "Translation");

	//
	
	ConstrainedObject = nullptr;
	SourceObject = nullptr;

	AffectX = true;
	AffectY = true;
	AffectZ = true;

	Translation = Vector_Zero();
}

AnimationStack::AnimationStack(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


AnimationLayer::AnimationLayer(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


AnimationCurve::AnimationCurve(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


AnimationCurveNode::AnimationCurveNode(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}

////////////////////////////////////////////////////////////////////////////////////////
// ConstraintImpl
struct ConstraintImpl : Constraint
{
	ConstraintImpl(const Scene &_scene, const IElement &_element)
	: Constraint(_scene, _element)
	{}

	Type getType() const override { return Type::CONSTRAINT; }

	// 
	bool Evaluate(const OFBTime *pTime) override
	{
		return false;
	}
};

////////////////////////////////////////////////////////////////////////////////////////
// ConstraintPositionImpl
struct ConstraintPositionImpl : ConstraintPosition
{
	ConstraintPositionImpl(const Scene &_scene, const IElement &_element)
	: ConstraintPosition(_scene, _element)
	{
	}

	Type getType() const override { return Type::CONSTRAINT_POSITION; }

	// 
	bool Evaluate(const OFBTime *pTime) override
	{
		Object *pSrc = SourceObject;
		Object *pDst = ConstrainedObject;

		bool lAffectX = AffectX;
		bool lAffectY = AffectY;
		bool lAffectZ = AffectZ;

		OFBVector3 offset;
		Translation.GetData(&offset.x, sizeof(OFBVector3), pTime);

		if (nullptr != pSrc && nullptr != pDst)
		{
			Model *pSrcModel = (Model*)pSrc;
			Model *pDstModel = (Model*)pDst;

			OFBVector3 v;
			pSrcModel->GetVector(v, eModelTranslation, true, pTime);
			//pDstModel->SetVector(v);
		}

		return true;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
// AnimationStackImpl

struct AnimationStackImpl : AnimationStack
{
	AnimationStackImpl(const Scene& _scene, const IElement& _element)
		: AnimationStack(_scene, _element)
	{
		mLoopStart = 0;
		mLoopStop = secondsToFbxTime(4.0);
	}

	i64		getLoopStart() const override
	{
		return mLoopStart;
	}
	i64		getLoopStop() const override
	{
		return mLoopStop;
	}

	int getLayerCount() const override
	{
		return (int)mLayers.size();
	}

	const AnimationLayer* getLayer(int index) const override
	{
		if (index >= 0 && index < (int)mLayers.size())
			return mLayers[index];
		return nullptr;
		//return resolveObjectLink<AnimationLayer>(index);
	}

	// sort layers by mLayerID (re-arrange as user defined in mobu)
	bool sortLayers();

	Type getType() const override { return Type::ANIMATION_STACK; }

	i64		mLoopStart;
	i64		mLoopStop;

	std::vector<AnimationLayer*>	mLayers;
};


struct AnimationCurveImpl : AnimationCurve
{
	AnimationCurveImpl(const Scene& _scene, const IElement& _element)
		: AnimationCurve(_scene, _element)
	{
		mLastEvalTime = OFBTime::MinusInfinity;
		mLastEvalValue = 0.0f;
	}

	double Evaluate(const OFBTime &time) const override
	{
		if (mLastEvalTime.Get() == time.Get())
		{
			return mLastEvalValue;
		}
		else
		{
			((OFBTime*)&mLastEvalTime)->Set(time.Get());

			size_t count = values.size();
			float result = 0.0f;

			if (count > 0)
			{
				i64 fbx_time(time.Get());

				if (fbx_time < times[0]) fbx_time = times[0];
				if (fbx_time > times[count - 1]) fbx_time = times[count - 1];

				for (size_t i = 1; i < count; ++i)
				{
					if (times[i] >= fbx_time)
					{
						float t = float(double(fbx_time - times[i - 1]) / double(times[i] - times[i - 1]));
						result = values[i - 1] * (1 - t) + values[i] * t;
						break;
					}
				}
			}
			return result;
		}
		
	}

	int getKeyCount() const override { return (int)times.size(); }
	const i64* getKeyTime() const override { return &times[0]; }
	const float* getKeyValue() const override { return &values[0]; }
	const int *getKeyFlag() const override { return &flags[0]; }

	std::vector<i64>	times;
	std::vector<float>	values;
	std::vector<int>	flags;

	// cache
	OFBTime		mLastEvalTime;
	float		mLastEvalValue;

	Type getType() const override { return Type::ANIMATION_CURVE; }
};


Skin::Skin(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
}


struct SkinImpl : Skin
{
	SkinImpl(const Scene& _scene, const IElement& _element)
		: Skin(_scene, _element)
	{
	}

	int getClusterCount() const override { return (int)clusters.size(); }
	const Cluster* getCluster(int idx) const override { return clusters[idx]; }

	Type getType() const override { return Type::SKIN; }

	std::vector<Cluster*> clusters;
};


Texture::Texture(const Scene& _scene, const IElement& _element)
	: Object(_scene, _element)
{
	FileName.Init(this, "FileName");
	RelativeFileName.Init(this, "Relative FileName");
}


struct TextureImpl : Texture
{
	TextureImpl(const Scene& _scene, const IElement& _element)
		: Texture(_scene, _element)
	{
	}

	//DataView getRelativeFileName() const override { return relative_filename; }
	//DataView getFileName() const override { return filename; }

	DataView filename;
	DataView relative_filename;
	Type getType() const override { return Type::TEXTURE; }
};

////////////////////////////////////////////////////////////////////////////////////////
// Models root, scene root
struct Root : SceneRoot
{
	Root(const Scene& _scene, const IElement& _element)
	: SceneRoot(_scene, _element)
	{
		copyString(name, "RootNode");
		is_node = true;
		//mSize = 100.0;
	}
	Type getType() const override { return Type::ROOT; }
	/*
	const double getSize() const override
	{
		return mSize;
	}

	double		mSize;*/
};


struct Scene : IScene
{
	struct Connection
	{
		enum Type
		{
			OBJECT_OBJECT,
			OBJECT_PROPERTY,
			PROPERTY_PROPERTY
		};

		Type type;
		u64 from;
		u64 to;
		DataView srcProperty;
		DataView property;
	};

	struct ObjectPair
	{
		const Element* element;
		Object* object;
	};


	int getAnimationStackCount() const override { return (int)m_animation_stacks.size(); }
	int getMeshCount() const override { return (int)m_meshes.size(); }
	float getSceneFrameRate() const override { return m_scene_frame_rate; }

	const Object* const* getAllObjects() const override { return m_all_objects.empty() ? nullptr : &m_all_objects[0]; }


	int getAllObjectCount() const override { return (int)m_all_objects.size(); }

	int GetLightCount() const override {
		return (int)mLights.size();
	}
	const Light *GetLight(const int index) const override {
		return mLights[index];
	}

	int GetCameraCount() const override {
		return (int)mCameras.size();
	}
	const Camera *GetCamera(const int index) const override {
		return mCameras[index];
	}

	int GetMaterialCount() const override {
		return (int)mMaterials.size();
	}
	const Material *GetMaterial(const int index) const override {
		return mMaterials[index];
	}

	int GetShaderCount() const override {
		return (int)m_shaders.size();
	}
	const Shader *GetShader(const int index) const override {
		return m_shaders[index];
	}

	int GetConstraintCount() const override {
		return (int)mConstraints.size();
	}
	const Constraint *GetConstraint(const int index) const override {
		return mConstraints[index];
	}

	const AnimationStack* getAnimationStack(int index) const override
	{
		assert(index >= 0);
		assert(index < (int)m_animation_stacks.size());
		return m_animation_stacks[index];
	}


	const Mesh* getMesh(int index) const override
	{
		assert(index >= 0);
		assert(index < (int)m_meshes.size());
		return m_meshes[index];
	}


	const TakeInfo* getTakeInfo(const char* name) const override
	{
		for (const TakeInfo& info : m_take_infos)
		{
			if (info.name == name) return &info;
		}
		return nullptr;
	}

	bool PrepTakeConnections(const int takeIndex) override
	{
		AnimationStack *pStack = m_animation_stacks[takeIndex];

		
		const int layerCount = pStack->getLayerCount();

		// detach all object properties anim nodes

		for (auto iter = begin(m_all_objects); iter != end(m_all_objects); ++iter)
		{
			(*iter)->mProperties.DetachAnimNodes();

			// start a new attachments from a base layer
			//  layers are sorted under the stack

			for (int i = 0; i < layerCount; ++i)
			{
				const AnimationLayer *pLayer = pStack->getLayer(i);
				(*iter)->mProperties.AttachAnimNodes(pLayer);
			}
		}

		return true;
	}

	const IElement* getRootElement() const override { return m_root_element; }
	const Object* getRoot() const override { return m_root; }


	void destroy() override { delete this; }


	~Scene()
	{
		for (auto iter : m_object_map)
		{
			delete iter.second.object;
		}
		
		deleteElement(m_root_element);
	}


	Element* m_root_element = nullptr;
	Root* m_root = nullptr;
	float m_scene_frame_rate = -1;
	std::unordered_map<u64, ObjectPair> m_object_map;
	std::vector<Object*> m_all_objects;
	std::vector<Mesh*> m_meshes;
	std::vector<Material*>	mMaterials;
	std::vector<Shader*>	m_shaders;
	std::vector<Light*>		mLights;
	std::vector<Camera*>	mCameras;
	std::vector<Constraint*>	mConstraints;
	std::vector<AnimationStack*> m_animation_stacks;
	std::vector<Connection> m_connections;
	std::vector<u8> m_data;
	std::vector<TakeInfo> m_take_infos;
};


struct AnimationCurveNodeImpl : AnimationCurveNode
{
	AnimationCurveNodeImpl(const Scene& _scene, const IElement& _element)
		: AnimationCurveNode(_scene, _element)
	{
	}


	const Object* GetOwner() const override
	{
		return mOwner;
	}

	AnimationCurveNode *GetNext() override
	{
		return mNext;
	}

	const AnimationCurveNode *GetNext() const override
	{
		return mNext;
	}

	void LinkNext(const AnimationCurveNode *pNext) override
	{
		mNext = (AnimationCurveNode*) pNext;
	}

	OFBVector3 getNodeLocalTransform(double time) const override
	{
		u64 fbx_time = secondsToFbxTime(time);

		auto getCoord = [](const Curve& curve, i64 fbx_time) {
			if (!curve.curve) return 0.0;
			return curve.curve->Evaluate(fbx_time);
		};

		return {getCoord(curves[0], fbx_time), getCoord(curves[1], fbx_time), getCoord(curves[2], fbx_time)};
	}

	AnimationLayer *getLayer() const override
	{
		return mLayer;
	}

	int getCurveCount() const override
	{
		return mNumberOfCurves;
	}
	const AnimationCurve *getCurve(int index) const override
	{
		return curves[index].curve;
	}
	bool AttachCurve(const AnimationCurve *pCurve, const Scene::Connection *connection)
	{
		bool lSuccess = false;
		if (mNumberOfCurves < 3)
		{
			curves[mNumberOfCurves].curve = pCurve;
			curves[mNumberOfCurves].connection = connection;
			mNumberOfCurves += 1;

			lSuccess = true;
		}
		return lSuccess;
	}

	bool Evaluate(double *Data, const OFBTime pTime) const override
	{
		double *pData = Data;
		for (int i = 0; i < mNumberOfCurves; ++i)
		{
			*pData = curves[i].curve->Evaluate(pTime);
			pData += 1;
		}
		return true;
	}

	struct Curve
	{
		const AnimationCurve* curve = nullptr;
		const Scene::Connection* connection = nullptr;
	};

	AnimationLayer *mLayer = nullptr;
	AnimationCurveNode *mNext = nullptr;	// linked anim nodes under owner for different layers

	Object* mOwner = nullptr;
	DataView bone_link_property;
	Type getType() const override { return Type::ANIMATION_CURVE_NODE; }
	AnimationNodeType mode = ANIMATIONNODE_TYPE_CUSTOM;

protected:

	int mNumberOfCurves = 0;
	Curve curves[3];
};


struct AnimationLayerImpl : AnimationLayer
{
	AnimationLayerImpl(const Scene& _scene, const IElement& _element)
		: AnimationLayer(_scene, _element)
	{
		LayerID.Init(this, "mLayerID");
		Mute.Init(this, "Mute");
		Solo.Init(this, "Solo");
		Lock.Init(this, "Lock");
		Weight.Init(this, "Weight");

		LayerMode.Init(this, "LayerMode");
		LayerRotationMode.Init(this, "LayerRotationModel");

		parentLayer = nullptr;

		Mute = false;
		Solo = false;
		Lock = false;
		Weight = 100.0;
	}


	Type getType() const override { return Type::ANIMATION_LAYER; }


	int getSubLayerCount() const override
	{
		return (int)sublayers.size();
	}
	
	const AnimationLayer *getSubLayer(int index) const override
	{
		if (index >= 0 && index < (int)sublayers.size())
			return sublayers[index];

		return nullptr;
	}

	const AnimationCurveNode* getCurveNode(int index) const override
	{
		if (index >= (int)curve_nodes.size() || index < 0) return nullptr;
		return curve_nodes[index];
	}


	const AnimationCurveNode* getCurveNode(const Object& obj, const char* prop) const override
	{
		for (const AnimationCurveNodeImpl* node : curve_nodes)
		{
			if (node->bone_link_property == prop && node->GetOwner() == &obj) return node;
		}
		return nullptr;
	}

	AnimationLayer			*parentLayer;

	std::vector<AnimationLayer*> sublayers;
	std::vector<AnimationCurveNodeImpl*> curve_nodes;
};


struct OptionalError<Object*> parseTexture(const Scene& scene, const Element& element)
{
	TextureImpl* texture = new TextureImpl(scene, element);
	const Element* texture_filename = findChild(element, "FileName");
	if (texture_filename && texture_filename->first_property)
	{
		texture->filename = texture_filename->first_property->value;
	}
	const Element* texture_relative_filename = findChild(element, "RelativeFilename");
	if (texture_relative_filename && texture_relative_filename->first_property)
	{
		texture->relative_filename = texture_relative_filename->first_property->value;
	}
	return texture;
}

struct OptionalError<Object*> parseGeneric(const Scene &scene, const Element &element)
{
	Object* newGeneric = nullptr; // new ShaderImpl(scene, element);
	const Element* prop = findChild(element, "Properties70");
	
	if (prop) prop = prop->child;
	while (prop)
	{
		if (prop->id == "P" && prop->first_property)
		{
			if (prop->first_property->value == "MoBuTypeName")
			{

				if (prop->getProperty(4)->getValue() == "Shader")
				{
					// TODO: customize shader class ?!

					newGeneric = new ShaderImpl(scene, element);
					break;
				}
			}
		}
		prop = prop->sibling;
	}

	return OptionalError<Object*>(newGeneric, false);
}

template <typename T> static OptionalError<Object*> parse(const Scene& scene, const Element& element)
{
	T* obj = new T(scene, element);
	return obj;
}

static AnimationCurveNodeImpl *parseAnimationCurveNode(const Scene& scene, const Element& element)
{
	AnimationCurveNodeImpl *obj = new AnimationCurveNodeImpl(scene, element);

	return obj;
}

static OptionalError<Object*> parseCluster(const Scene& scene, const Element& element)
{
	std::unique_ptr<ClusterImpl> obj = std::make_unique<ClusterImpl>(scene, element);

	const Element* transform_link = findChild(element, "TransformLink");
	if (transform_link && transform_link->first_property)
	{
		if (!parseArrayRaw(
				*transform_link->first_property, &obj->transform_link_matrix, sizeof(obj->transform_link_matrix)))
		{
			return Error("Failed to parse TransformLink");
		}
	}
	const Element* transform = findChild(element, "Transform");
	if (transform && transform->first_property)
	{
		if (!parseArrayRaw(*transform->first_property, &obj->transform_matrix, sizeof(obj->transform_matrix)))
		{
			return Error("Failed to parse Transform");

		}
	}

	return obj.release();
}


static OptionalError<Object*> parseNodeAttribute(const Scene& scene, const Element& element)
{
	NodeAttributeImpl* obj = new NodeAttributeImpl(scene, element);
	const Element* type_flags = findChild(element, "TypeFlags");
	if (type_flags && type_flags->first_property)
	{
		obj->attribute_type = type_flags->first_property->value;
	}
	return obj;
}


static OptionalError<Object*> parseLimbNode(const Scene& scene, const Element& element)
{
	if (!element.first_property
		|| !element.first_property->next
		|| !element.first_property->next->next
		|| element.first_property->next->next->value != "LimbNode")
	{
		return Error("Invalid limb node");
	}

	LimbNodeImpl* obj = new LimbNodeImpl(scene, element);
	return obj;
}


static OptionalError<Object*> parseMesh(const Scene& scene, const Element& element)
{
	if (!element.first_property
		|| !element.first_property->next
		|| !element.first_property->next->next
		|| element.first_property->next->next->value != "Mesh")
	{
		return Error("Invalid mesh");
	}

	return new MeshImpl(scene, element);
}


static OptionalError<Object*> parseMaterial(const Scene& scene, const Element& element)
{
	MaterialImpl* material = new MaterialImpl(scene, element);
	return material;
}

static OptionalError<Object*> parseAnimationStack(AnimationStackImpl *pstack)
{
	Element *pelem = (Element*) &pstack->element;
	const Element* prop = findChild(*pelem, "Properties70");
	
	pstack->mLoopStart = 0;
	pstack->mLoopStop = secondsToFbxTime(4.0);

	if (prop) prop = prop->child;
	while (prop)
	{
		if (prop->id == "P" && prop->first_property)
		{
			if (prop->first_property->value == "LocalStart")
			{
				pstack->mLoopStart = prop->getProperty(4)->getValue().toU64();
			}
			else if (prop->first_property->value == "LocalStop")
			{
				pstack->mLoopStop = prop->getProperty(4)->getValue().toU64();
			}
		}
		prop = prop->sibling;
	}
	return pstack;
}

template<typename T> static bool parseTextArrayRaw(const Property& property, T* out, int max_size);

template <typename T> static bool parseArrayRaw(const Property& property, T* out, int max_size)
{
	if (property.value.is_binary)
	{
		assert(out);

		int elem_size = 1;
		switch (property.type)
		{
			case 'l': elem_size = 8; break;
			case 'd': elem_size = 8; break;
			case 'f': elem_size = 4; break;
			case 'i': elem_size = 4; break;
			default: return false;
		}

		const u8* data = property.value.begin + sizeof(u32) * 3;
		if (data > property.value.end) return false;

		u32 count = property.getCount();
		u32 enc = *(const u32*)(property.value.begin + 4);
		u32 len = *(const u32*)(property.value.begin + 8);

		if (enc == 0)
		{
			if ((int)len > max_size) return false;
			if (data + len > property.value.end) return false;
			memcpy(out, data, len);
			return true;
		}
		else if (enc == 1)
		{
			if (int(elem_size * count) > max_size) return false;
			return decompress(data, len, (u8*)out, elem_size * count);
		}

		return false;
	}

	return parseTextArrayRaw(property, out, max_size);
}


template <typename T> const char* fromString(const char* str, const char* end, T* val);
template <> const char* fromString<int>(const char* str, const char* end, int* val)
{
	*val = atoi(str);
	const char* iter = str;
	while (iter < end && *iter != ',') ++iter;
	if (iter < end) ++iter; // skip ','
	return (const char*)iter;
}


template <> const char* fromString<u64>(const char* str, const char* end, u64* val)
{
	*val = atol(str);
	const char* iter = str;
	while (iter < end && *iter != ',') ++iter;
	if (iter < end) ++iter; // skip ','
	return (const char*)iter;
}

template <> const char* fromString<i64>(const char* str, const char* end, i64* val)
{
	*val = atol(str);
	const char* iter = str;
	while (iter < end && *iter != ',') ++iter;
	if (iter < end) ++iter; // skip ','
	return (const char*)iter;
}

template <> const char* fromString<double>(const char* str, const char* end, double* val)
{
	*val = atof(str);
	const char* iter = str;
	while (iter < end && *iter != ',') ++iter;
	if (iter < end) ++iter; // skip ','
	return (const char*)iter;
}


template <> const char* fromString<float>(const char* str, const char* end, float* val)
{
	*val = (float)atof(str);
	const char* iter = str;
	while (iter < end && *iter != ',') ++iter;
	if (iter < end) ++iter; // skip ','
	return (const char*)iter;
}


const char* fromString(const char* str, const char* end, double* val, int count)
{
	const char* iter = str;
	for (int i = 0; i < count; ++i)
	{
		*val = atof(iter);
		++val;
		while (iter < end && *iter != ',') ++iter;
		if (iter < end) ++iter; // skip ','

		if (iter == end) return iter;

	}
	return (const char*)iter;
}


template <> const char* fromString<OFBVector2>(const char* str, const char* end, OFBVector2* val)
{
	return fromString(str, end, &val->x, 2);
}


template <> const char* fromString<OFBVector3>(const char* str, const char* end, OFBVector3* val)
{
	return fromString(str, end, &val->x, 3);
}


template <> const char* fromString<OFBVector4>(const char* str, const char* end, OFBVector4* val)
{
	return fromString(str, end, &val->x, 4);
}


template <> const char* fromString<OFBMatrix>(const char* str, const char* end, OFBMatrix* val)
{
	return fromString(str, end, &val->m[0], 16);
}


template<typename T> static void parseTextArray(const Property& property, std::vector<T>* out)
{
	const u8* iter = property.value.begin;
	for(int i = 0; i < property.count; ++i)
	{
		T val;
		iter = (const u8*)fromString<T>((const char*)iter, (const char*)property.value.end, &val);
		out->push_back(val);
	}
}


template<typename T> static bool parseTextArrayRaw(const Property& property, T* out_raw, int max_size)
{
	const u8* iter = property.value.begin;
	
	T* out = out_raw;
	while (iter < property.value.end)
	{
		iter = (const u8*)fromString<T>((const char*)iter, (const char*)property.value.end, out);
		++out;
		if (out - out_raw == max_size / sizeof(T)) return true;
	}
	return out - out_raw == max_size / sizeof(T);
}


template <typename T> static bool parseBinaryArray(const Property& property, std::vector<T>* out)
{
	assert(out);
	if (property.value.is_binary)
	{
		u32 count = property.getCount();
		int elem_size = 1;
		switch (property.type)
		{
			case 'd': elem_size = 8; break;
			case 'f': elem_size = 4; break;
			case 'i': elem_size = 4; break;
			default: return false;
		}
		int elem_count = sizeof(T) / elem_size;
		out->resize(count / elem_count);

		if (count == 0) return true;
		return parseArrayRaw(property, &(*out)[0], int(sizeof((*out)[0]) * out->size()));
	}
	else
	{
		parseTextArray(property, out);
		return true;
	}
}


template <typename T> static bool parseDoubleVecData(Property& property, std::vector<T>* out_vec)
{
	assert(out_vec);
	if (!property.value.is_binary)
	{
		parseTextArray(property, out_vec);
		return true;
	}

	if (property.type == 'd')
	{
		return parseBinaryArray(property, out_vec);
	}

	assert(property.type == 'f');
	assert(sizeof((*out_vec)[0].x) == sizeof(double));
	std::vector<float> tmp;
	if (!parseBinaryArray(property, &tmp)) return false;
	int elem_count = sizeof((*out_vec)[0]) / sizeof((*out_vec)[0].x);
	out_vec->resize(tmp.size() / elem_count);
	double* out = &(*out_vec)[0].x;
	for (int i = 0, c = (int)tmp.size(); i < c; ++i)
	{
		out[i] = tmp[i];
	}
	return true;
}


template <typename T>
static bool parseVertexData(const Element& element,
	const char* name,
	const char* index_name,
	std::vector<T>* out,
	std::vector<int>* out_indices,
	GeometryImpl::VertexDataMapping* mapping)
{
	assert(out);
	assert(mapping);
	const Element* data_element = findChild(element, name);
	if (!data_element || !data_element->first_property) 	return false;

	const Element* mapping_element = findChild(element, "MappingInformationType");
	const Element* reference_element = findChild(element, "ReferenceInformationType");

	if (mapping_element && mapping_element->first_property)
	{
		if (mapping_element->first_property->value == "ByPolygonVertex")
		{
			*mapping = GeometryImpl::BY_POLYGON_VERTEX;
		}
		else if (mapping_element->first_property->value == "ByPolygon")
		{
			*mapping = GeometryImpl::BY_POLYGON;
		}
		else if (mapping_element->first_property->value == "ByVertice" ||
					mapping_element->first_property->value == "ByVertex")
		{
			*mapping = GeometryImpl::BY_VERTEX;
		}
		else
		{
			return false;
		}
	}
	if (reference_element && reference_element->first_property)
	{
		if (reference_element->first_property->value == "IndexToDirect")
		{
			const Element* indices_element = findChild(element, index_name);
			if (indices_element && indices_element->first_property)
			{
				if (!parseBinaryArray(*indices_element->first_property, out_indices)) return false;
			}
		}
		else if (reference_element->first_property->value != "Direct")
		{
			return false;
		}
	}
	return parseDoubleVecData(*data_element->first_property, out);
}


template <typename T>
static void splat(std::vector<T>* out,
	GeometryImpl::VertexDataMapping mapping,
	const std::vector<T>& data,
	const std::vector<int>& indices,
	const std::vector<int>& original_indices)
{
	assert(out);
	assert(!data.empty());

	if (mapping == GeometryImpl::BY_POLYGON_VERTEX)
	{
		if (indices.empty())
		{
			out->resize(data.size());
			memcpy(&(*out)[0], &data[0], sizeof(data[0]) * data.size());
		}
		else
		{
			out->resize(indices.size());
			int data_size = (int)data.size();
			for (int i = 0, c = (int)indices.size(); i < c; ++i)
			{
				if(indices[i] < data_size) (*out)[i] = data[indices[i]];
				else (*out)[i] = T();
			}
		}
	}
	else if (mapping == GeometryImpl::BY_VERTEX)
	{
		//  v0  v1 ...
		// uv0 uv1 ...
		assert(indices.empty());

		out->resize(original_indices.size());

		int data_size = (int)data.size();
		for (int i = 0, c = (int)original_indices.size(); i < c; ++i)
		{
			int idx = original_indices[i];
			if (idx < 0) idx = -idx - 1;
			if(idx < data_size) (*out)[i] = data[idx];
			else (*out)[i] = T();
		}
	}
	else
	{
		assert(false);
	}
}


template <typename T> static void remap(std::vector<T>* out, const std::vector<int>& map)
{
	if (out->empty()) return;

	std::vector<T> old;
	old.swap(*out);
	int old_size = (int)old.size();
	for (int i = 0, c = (int)map.size(); i < c; ++i)
	{
		if(map[i] < old_size) out->push_back(old[map[i]]);
		else out->push_back(T());
	}
}


static OptionalError<Object*> parseAnimationCurve(const Scene& scene, const Element& element)
{
	std::unique_ptr<AnimationCurveImpl> curve = std::make_unique<AnimationCurveImpl>(scene, element);

	const Element* times = findChild(element, "KeyTime");
	const Element* values = findChild(element, "KeyValueFloat");
	const Element *flags = findChild(element, "KeyAttrFlags");

	if (times && times->first_property)
	{
		curve->times.resize(times->first_property->getCount());
		if (!times->first_property->getValues(&curve->times[0], (int)curve->times.size() * sizeof(curve->times[0])))
		{
			return Error("Invalid animation curve");
		}
	}

	if (values && values->first_property)
	{
		curve->values.resize(values->first_property->getCount());
		if (!values->first_property->getValues(&curve->values[0], (int)curve->values.size() * sizeof(curve->values[0])))
		{
			return Error("Invalid animation curve");
		}
	}

	if (flags && flags->first_property)
	{
		if (curve->values.size() == values->first_property->getCount())
		{
			curve->flags.resize(values->first_property->getCount());
			if (!values->first_property->getValues(&curve->flags[0], (int)curve->flags.size() * sizeof(curve->flags[0])))
			{
				return Error("Invalid animation curve");
			}
		}
		else if (1 == values->first_property->getCount())
		{
			int value = 0;
			if (!values->first_property->getValues(&value, sizeof(int)))
			{
				return Error("Invalid animation curve");
			}
			curve->flags.resize(curve->values.size(), value);
		}
		else
		{
			return Error("Invalid animation curve");
		}
	}

	if (curve->times.size() != curve->values.size()) return Error("Invalid animation curve");

	return curve.release();
}

static int getTriCountFromPoly(const std::vector<int>& indices, int* idx)
{
	int count = 1;
	while (indices[*idx + 1 + count] >= 0)
	{
		++count;
	}

	*idx = *idx + 2 + count;
	return count;
}


static void add(GeometryImpl::NewVertex& vtx, int index)
{
	if (vtx.index == -1)
	{
		vtx.index = index;
	}
	else if (vtx.next)
	{
		add(*vtx.next, index);
	}
	else
	{
		vtx.next = new GeometryImpl::NewVertex;
		vtx.next->index = index;
	}
}


static OptionalError<Object*> parseGeometry(const Scene& scene, const Element& element)
{
	assert(element.first_property);

	const Element* vertices_element = findChild(element, "Vertices");
	if (!vertices_element || !vertices_element->first_property) return Error("Vertices missing");

	const Element* polys_element = findChild(element, "PolygonVertexIndex");
	if (!polys_element || !polys_element->first_property) return Error("Indices missing");

	std::unique_ptr<GeometryImpl> geom = std::make_unique<GeometryImpl>(scene, element);

	std::vector<OFBVector3> vertices;
	if (!parseDoubleVecData(*vertices_element->first_property, &vertices)) return Error("Failed to parse vertices");
	std::vector<int> original_indices;
	if (!parseBinaryArray(*polys_element->first_property, &original_indices)) return Error("Failed to parse indices");

	std::vector<int> to_old_indices;
	geom->triangulate(original_indices, &geom->to_old_vertices, &to_old_indices);
	geom->vertices.resize(geom->to_old_vertices.size());

	for (int i = 0, c = (int)geom->to_old_vertices.size(); i < c; ++i)
	{
		geom->vertices[i] = vertices[geom->to_old_vertices[i]];
	}

	geom->to_new_vertices.resize(vertices.size()); // some vertices can be unused, so this isn't necessarily the same size as to_old_vertices.
	const int* to_old_vertices = geom->to_old_vertices.empty() ? nullptr : &geom->to_old_vertices[0];
	for (int i = 0, c = (int)geom->to_old_vertices.size(); i < c; ++i)
	{
		int old = to_old_vertices[i];
		add(geom->to_new_vertices[old], i);
	}

	const Element* layer_material_element = findChild(element, "LayerElementMaterial");
	if (layer_material_element)
	{
		const Element* mapping_element = findChild(*layer_material_element, "MappingInformationType");
		const Element* reference_element = findChild(*layer_material_element, "ReferenceInformationType");

		std::vector<int> tmp;

		if (!mapping_element || !reference_element) return Error("Invalid LayerElementMaterial");

		if (mapping_element->first_property->value == "ByPolygon" &&
			reference_element->first_property->value == "IndexToDirect")
		{
			geom->materials.reserve(geom->vertices.size() / 3);
			for (int& i : geom->materials) i = -1;

			const Element* indices_element = findChild(*layer_material_element, "Materials");
			if (!indices_element || !indices_element->first_property) return Error("Invalid LayerElementMaterial");

			if (!parseBinaryArray(*indices_element->first_property, &tmp)) return Error("Failed to parse material indices");

			int tmp_i = 0;
			for (int poly = 0, c = (int)tmp.size(); poly < c; ++poly)
			{
				int tri_count = getTriCountFromPoly(original_indices, &tmp_i);
				for (int i = 0; i < tri_count; ++i)
				{
					geom->materials.push_back(tmp[poly]);
				}
			}
		}
		else
		{
			if (mapping_element->first_property->value != "AllSame") return Error("Mapping not supported");
		}
	}

	const Element* layer_uv_element = findChild(element, "LayerElementUV");
	if (layer_uv_element)
	{
		std::vector<OFBVector2> tmp;
		std::vector<int> tmp_indices;
		GeometryImpl::VertexDataMapping mapping;
		if (!parseVertexData(*layer_uv_element, "UV", "UVIndex", &tmp, &tmp_indices, &mapping)) return Error("Invalid UVs");
		if (!tmp.empty())
		{
			geom->uvs.resize(tmp_indices.empty() ? tmp.size() : tmp_indices.size());
			splat(&geom->uvs, mapping, tmp, tmp_indices, original_indices);
			remap(&geom->uvs, to_old_indices);
		}
	}

	const Element* layer_tangent_element = findChild(element, "LayerElementTangents");
	if (layer_tangent_element)
	{
		std::vector<OFBVector3> tmp;
		std::vector<int> tmp_indices;
		GeometryImpl::VertexDataMapping mapping;
		if (findChild(*layer_tangent_element, "Tangents"))
		{
			if (!parseVertexData(*layer_tangent_element, "Tangents", "TangentsIndex", &tmp, &tmp_indices, &mapping)) return Error("Invalid tangets");
		}
		else
		{
			if (!parseVertexData(*layer_tangent_element, "Tangent", "TangentIndex", &tmp, &tmp_indices, &mapping))  return Error("Invalid tangets");
		}
		if (!tmp.empty())
		{
			splat(&geom->tangents, mapping, tmp, tmp_indices, original_indices);
			remap(&geom->tangents, to_old_indices);
		}
	}

	const Element* layer_color_element = findChild(element, "LayerElementColor");
	if (layer_color_element)
	{
		std::vector<OFBVector4> tmp;
		std::vector<int> tmp_indices;
		GeometryImpl::VertexDataMapping mapping;
		if (!parseVertexData(*layer_color_element, "Colors", "ColorIndex", &tmp, &tmp_indices, &mapping)) return Error("Invalid colors");
		if (!tmp.empty())
		{
			splat(&geom->colors, mapping, tmp, tmp_indices, original_indices);
			remap(&geom->colors, to_old_indices);
		}
	}

	const Element* layer_normal_element = findChild(element, "LayerElementNormal");
	if (layer_normal_element)
	{
		std::vector<OFBVector3> tmp;
		std::vector<int> tmp_indices;
		GeometryImpl::VertexDataMapping mapping;
		if (!parseVertexData(*layer_normal_element, "Normals", "NormalsIndex", &tmp, &tmp_indices, &mapping)) return Error("Invalid normals");
		if (!tmp.empty())
		{
			splat(&geom->normals, mapping, tmp, tmp_indices, original_indices);
			remap(&geom->normals, to_old_indices);
		}
	}

	return geom.release();
}


static bool isString(const Property* prop)
{
	if (!prop) return false;
	return prop->getType() == Property::STRING;
}


static bool isLong(const Property* prop)
{
	if (!prop) return false;
	return prop->getType() == Property::LONG;
}


static bool parseConnections(const Element& root, Scene* scene)
{
	assert(scene);

	const Element* connections = findChild(root, "Connections");
	if (!connections) return true;

	const Element* connection = connections->child;
	while (connection)
	{
		if (!isString(connection->first_property) )
		{
			Error::s_message = "Invalid connection";
			return false;
		}

		Scene::Connection c;
		
		if (connection->first_property->value == "OO")
		{
			if ( !isLong(connection->first_property->next)
				|| !isLong(connection->first_property->next->next) )
			{
				Error::s_message = "Invalid OO connection";
				return false;
			}

			c.type = Scene::Connection::OBJECT_OBJECT;
			c.from = connection->first_property->next->value.toU64();
			c.to = connection->first_property->next->next->value.toU64();
		}
		else 
		if (connection->first_property->value == "OP")
		{
			if (!isLong(connection->first_property->next)
				|| !isLong(connection->first_property->next->next)
				|| !connection->first_property->next->next->next)
			{
				Error::s_message = "Invalid OP connection";
				return false;
			}

			c.type = Scene::Connection::OBJECT_PROPERTY;
			c.from = connection->first_property->next->value.toU64();
			c.to = connection->first_property->next->next->value.toU64();
			c.property = connection->first_property->next->next->next->value;
		}
		else 
		if (connection->first_property->value == "PP")
		{

			if (!isLong(connection->first_property->next)
				|| !isString(connection->first_property->next->next)
				|| !connection->first_property->next->next->next // long
				|| !connection->first_property->next->next->next->next)
			{
				Error::s_message = "Invalid PP connection";
				return false;
			}

			c.type = Scene::Connection::PROPERTY_PROPERTY;
			c.from = connection->first_property->next->value.toU64();
			c.srcProperty = connection->first_property->next->next->value;
			c.to = connection->first_property->next->next->next->value.toU64();
			c.property = connection->first_property->next->next->next->next->value;
		}
		else
		{
			assert(false);
			Error::s_message = "Not supported";
			return false;
		}
		scene->m_connections.push_back(c);

		connection = connection->sibling;
	}
	return true;
}


static bool parseTakes(Scene* scene)
{
	const Element* takes = findChild((const Element&)*scene->getRootElement(), "Takes");
	if (!takes) return true;

	const Element* object = takes->child;
	while (object)
	{
		if (object->id == "Take")
		{
			if (!isString(object->first_property))
			{
				Error::s_message = "Invalid name in take";
				return false;
			}

			TakeInfo take;
			take.name = object->first_property->value;
			const Element* filename = findChild(*object, "FileName");
			if (filename)
			{
				if (!isString(filename->first_property))
				{
					Error::s_message = "Invalid filename in take";
					return false;
				}
				take.filename = filename->first_property->value;
			}
			const Element* local_time = findChild(*object, "LocalTime");
			if (local_time)
			{
				if (!isLong(local_time->first_property) || !isLong(local_time->first_property->next))
				{
					Error::s_message = "Invalid local time in take";
					return false;
				}

				take.local_time_from = fbxTimeToSeconds(local_time->first_property->value.toU64());
				take.local_time_to = fbxTimeToSeconds(local_time->first_property->next->value.toU64());
			}
			const Element* reference_time = findChild(*object, "ReferenceTime");
			if (reference_time)
			{
				if (!isLong(reference_time->first_property) || !isLong(reference_time->first_property->next))
				{
					Error::s_message = "Invalid reference time in take";
					return false;
				}

				take.reference_time_from = fbxTimeToSeconds(reference_time->first_property->value.toU64());
				take.reference_time_to = fbxTimeToSeconds(reference_time->first_property->next->value.toU64());
			}

			scene->m_take_infos.push_back(take);
		}

		object = object->sibling;
	}

	return true;
}


// http://docs.autodesk.com/FBX/2014/ENU/FBX-SDK-Documentation/index.html?url=cpp_ref/class_fbx_time.html,topicNumber=cpp_ref_class_fbx_time_html29087af6-8c2c-4e9d-aede-7dc5a1c2436c,hash=a837590fd5310ff5df56ffcf7c394787e
enum FrameRate 
{
	FrameRate_DEFAULT = 0,
	FrameRate_120 = 1,
	FrameRate_100 = 2,
	FrameRate_60 = 3,
	FrameRate_50 = 4,
	FrameRate_48 = 5,
	FrameRate_30 = 6,
	FrameRate_30_DROP = 7,
	FrameRate_NTSC_DROP_FRAME = 8,
	FrameRate_NTSC_FULL_FRAME = 9,
	FrameRate_PAL = 10,
	FrameRate_CINEMA = 11,
	FrameRate_1000 = 12,
	FrameRate_CINEMA_ND = 13,
	FrameRate_CUSTOM = 14,
};


static float getFramerateFromTimeMode(int time_mode)
{
	switch (time_mode)
	{
		case FrameRate_DEFAULT: return 1;
		case FrameRate_120: return 120;
		case FrameRate_100: return 100;
		case FrameRate_60: return 60;
		case FrameRate_50: return 50;
		case FrameRate_48: return 48;
		case FrameRate_30: return 30;
		case FrameRate_30_DROP: return 30;
		case FrameRate_NTSC_DROP_FRAME: return 29.9700262f;
		case FrameRate_NTSC_FULL_FRAME: return 29.9700262f;
		case FrameRate_PAL: return 25;
		case FrameRate_CINEMA: return 24;
		case FrameRate_1000: return 1000;
		case FrameRate_CINEMA_ND: return 23.976f;
		case FrameRate_CUSTOM: return -2;
	}
	return -1;
}


static void parseGlobalSettings(const Element& root, Scene* scene)
{
	for (ofbx::Element* settings = root.child; settings; settings = settings->sibling)
	{
		if (settings->id == "GlobalSettings")
		{
			for (ofbx::Element* props70 = settings->child; props70; props70 = props70->sibling)
			{
				if (props70->id == "Properties70")
				{
					for (ofbx::Element* time_mode = props70->child; time_mode; time_mode = time_mode->sibling)
					{
						if (time_mode->first_property && time_mode->first_property->value == "TimeMode")
						{
							ofbx::IElementProperty* prop = time_mode->getProperty(4);
							if (prop)
							{
								ofbx::DataView value = prop->getValue();
								int time_mode = *(int*)value.begin;
								scene->m_scene_frame_rate = getFramerateFromTimeMode(time_mode);
							}
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}
}


static bool parseObjects(const Element& root, Scene* scene)
{
	const Element* objs = findChild(root, "Objects");
	if (!objs) return true;

	scene->m_root = new Root(*scene, root);
	scene->m_root->id = 0;
	scene->m_object_map[0] = {&root, scene->m_root};

	const Element* object = objs->child;
	while (object)
	{
		if (!isLong(object->first_property))
		{
			Error::s_message = "Invalid";
			return false;
		}

		u64 id = object->first_property->value.toU64();
		scene->m_object_map[id] = {object, nullptr};
		object = object->sibling;
	}

	for (auto iter : scene->m_object_map)
	{
		OptionalError<Object*> obj = nullptr;

		if (iter.second.object == scene->m_root) continue;

		if (iter.second.element->id == "Geometry")
		{
			Property* last_prop = iter.second.element->first_property;
			while (last_prop->next) last_prop = last_prop->next;
			if (last_prop && last_prop->value == "Mesh")
			{
				obj = parseGeometry(*scene, *iter.second.element);
			}
		}
		else if (iter.second.element->id == "Material")
		{
			obj = parseMaterial(*scene, *iter.second.element);

			if (!obj.isError())
			{
				if (nullptr != obj.getValue())
				{
					scene->mMaterials.push_back((Material*)obj.getValue());
				}
			}
		}
		else if (iter.second.element->id == "Constraint")
		{
			IElementProperty* class_prop = iter.second.element->getProperty(2);

			if (class_prop)
			{
				if (class_prop->getValue() == "Position From Positions")
					obj = parse<ConstraintPositionImpl>(*scene, *iter.second.element);
				else
					obj = parse<ConstraintImpl>(*scene, *iter.second.element);
			}

			if (!obj.isError())
			{
				if (nullptr != obj.getValue())
				{
					scene->mConstraints.push_back( (Constraint*)obj.getValue());
				}
			}
		}
		else if (iter.second.element->id == "AnimationStack")
		{
			obj = parse<AnimationStackImpl>(*scene, *iter.second.element);
			if (!obj.isError())
			{
				AnimationStackImpl* stack = (AnimationStackImpl*)obj.getValue();
				if (nullptr != stack)
				{
					parseAnimationStack(stack);
					scene->m_animation_stacks.push_back(stack);
				}
			}
		}
		else if (iter.second.element->id == "AnimationLayer")
		{
			obj = parse<AnimationLayerImpl>(*scene, *iter.second.element);
		}
		else if (iter.second.element->id == "AnimationCurve")
		{
			obj = parseAnimationCurve(*scene, *iter.second.element);
		}
		else if (iter.second.element->id == "AnimationCurveNode")
		{
			obj = parseAnimationCurveNode(*scene, *iter.second.element);
			//obj = parse<AnimationCurveNodeImpl>(*scene, *iter.second.element);
		}
		else if (iter.second.element->id == "Deformer")
		{
			IElementProperty* class_prop = iter.second.element->getProperty(2);

			if (class_prop)
			{
				if (class_prop->getValue() == "Cluster")
					obj = parseCluster(*scene, *iter.second.element);
				else if (class_prop->getValue() == "Skin")
					obj = parse<SkinImpl>(*scene, *iter.second.element);
			}
		}
		else if (iter.second.element->id == "NodeAttribute")
		{
			obj = parseNodeAttribute(*scene, *iter.second.element);
		}
		else if (iter.second.element->id == "Model")
		{
			IElementProperty* class_prop = iter.second.element->getProperty(2);

			if (class_prop)
			{
				if (class_prop->getValue() == "Mesh")
				{
					obj = parseMesh(*scene, *iter.second.element);
					if (!obj.isError())
					{
						Mesh* mesh = (Mesh*)obj.getValue();
						scene->m_meshes.push_back(mesh);
						obj = mesh;
					}
				}
				else if (class_prop->getValue() == "LimbNode")
					obj = parseLimbNode(*scene, *iter.second.element);
				else if (class_prop->getValue() == "Null")
					obj = parse<NullImpl>(*scene, *iter.second.element);
				else if (class_prop->getValue() == "Root")
					obj = parse<NullImpl>(*scene, *iter.second.element);
				else if (class_prop->getValue() == "Camera")
				{
					obj = parse<CameraImpl>(*scene, *iter.second.element);
					if (!obj.isError())
					{
						scene->mCameras.push_back((Camera*)obj.getValue());
					}
				}
				else if (class_prop->getValue() == "Light")
				{
					obj = parse<LightImpl>(*scene, *iter.second.element);
					if (!obj.isError())
					{
						scene->mLights.push_back((Light*)obj.getValue());
					}
				}
				
			}
		}
		else if (iter.second.element->id == "Texture")
		{
			obj = parseTexture(*scene, *iter.second.element);
		}
		else if (iter.second.element->id == "MotionBuilder_Generic")
		{
			obj = parseGeneric(*scene, *iter.second.element);

			if (!obj.isError() && nullptr != obj.getValue())
			{
				if (Object::Type::SHADER == obj.getValue()->getType())
				{
					scene->m_shaders.push_back((Shader*)obj.getValue());
				}
			}
		}
		
		if (obj.isError()) return false;

		scene->m_object_map[iter.first].object = obj.getValue();
		if (obj.getValue())
		{
			scene->m_all_objects.push_back(obj.getValue());
			obj.getValue()->id = iter.first;
		}
	}

	for (const Scene::Connection& con : scene->m_connections)
	{
		Object* parent = scene->m_object_map[con.to].object;
		Object* child = scene->m_object_map[con.from].object;
		if (!child) continue;
		if (!parent) continue;

		PropertyBase *objProperty = parent->mProperties.GetFirst();

		if (Scene::Connection::OBJECT_PROPERTY == con.type)
		{
			if (con.property == "LookAtProperty")
			{
				printf("test\n");
			}

			if (Object::Type::ANIMATION_CURVE_NODE != child->getType() 
				&& Object::Type::NODE_ATTRIBUTE != child->getType() )
			{
				while (objProperty)
				{
					if (con.property == objProperty->GetName() 
						&& ePT_object == objProperty->GetPropertyType())
					{
						((PropertyObject*) objProperty)->SetPropertyValue(child);
						break;
					}
					objProperty = objProperty->GetNext();
				}
			}
		}

		objProperty = parent->mProperties.GetFirst();

		switch (child->getType())
		{
			case Object::Type::NODE_ATTRIBUTE:
				if (parent->node_attribute)
				{
					Error::s_message = "Invalid node attribute";
					return false;
				}
				parent->node_attribute = (NodeAttribute*)child;
				break;
			case Object::Type::ANIMATION_CURVE_NODE:
				if (parent->isNode())
				{
					Model *pModel = (Model*)parent;

					AnimationCurveNodeImpl* node = (AnimationCurveNodeImpl*)child;
					node->mOwner = parent;
					node->bone_link_property = con.property;

					pModel->mAnimationNodes.push_back(node);

					if (con.property == ANIMATIONNODE_TYPENAME_TRANSLATION)
						node->mode = ANIMATIONNODE_TYPE_TRANSLATION;
					else if (con.property == ANIMATIONNODE_TYPENAME_ROTATION)
						node->mode = ANIMATIONNODE_TYPE_ROTATION;
					else if (con.property == ANIMATIONNODE_TYPENAME_SCALING)
						node->mode = ANIMATIONNODE_TYPE_SCALING;
					else if (con.property == ANIMATIONNODE_TYPENAME_VISIBILITY)
						node->mode = ANIMATIONNODE_TYPE_VISIBILITY;
					else if (con.property == ANIMATIONNODE_TYPENAME_FIELDOFVIEW)
						node->mode = ANIMATIONNODE_TYPE_FIELD_OF_VIEW;

				}

				while (objProperty)
				{
					if (con.property == objProperty->GetName() && objProperty->IsAnimatable())
					{
						AnimationCurveNodeImpl* node = (AnimationCurveNodeImpl*)child;
						((PropertyAnimatable*)objProperty)->AttachAnimationNode(node);
						break;
					}
					objProperty = objProperty->GetNext();
				}
				break;
		}

		switch (parent->getType())
		{
			case Object::Type::MESH:
			{
				MeshImpl* mesh = (MeshImpl*)parent;
				switch (child->getType())
				{
					case Object::Type::GEOMETRY:
						if (mesh->geometry)
						{
							Error::s_message = "Invalid mesh";
							return false;
						}
						mesh->geometry = (Geometry*)child;
						break;
					case Object::Type::MATERIAL: mesh->materials.push_back((Material*)child); break;
				}
				break;
			}
			case Object::Type::SKIN:
			{
				SkinImpl* skin = (SkinImpl*)parent;
				if (child->getType() == Object::Type::CLUSTER)
				{
					ClusterImpl* cluster = (ClusterImpl*)child;
					skin->clusters.push_back(cluster);
					if (cluster->skin)
					{
						Error::s_message = "Invalid cluster";
						return false;
					}
					cluster->skin = skin;
				}
				break;
			}
			
			case Object::Type::MATERIAL:
			{
				MaterialImpl* mat = (MaterialImpl*)parent;
				if (child->getType() == Object::Type::TEXTURE)
				{
					Texture::TextureType type = Texture::COUNT;
					if (con.property == "NormalMap")
						type = Texture::NORMAL;
					else if (con.property == "DiffuseColor")
						type = Texture::DIFFUSE;
					if (type == Texture::COUNT) break;

					if (mat->textures[type])
					{
						Error::s_message = "Invalid material";
						return false;
					}

					mat->textures[type] = (Texture*)child;
				}
				break;
			}
			case Object::Type::GEOMETRY:
			{
				GeometryImpl* geom = (GeometryImpl*)parent;
				if (child->getType() == Object::Type::SKIN) geom->skin = (Skin*)child;
				break;
			}
			case Object::Type::CLUSTER:
			{
				ClusterImpl* cluster = (ClusterImpl*)parent;
				if (child->getType() == Object::Type::LIMB_NODE || child->getType() == Object::Type::MESH || child->getType() == Object::Type::NULL_NODE)
				{
					if (cluster->link)
					{
						Error::s_message = "Invalid cluster";
						return false;
					}

					cluster->link = child;
				}
				break;
			}
			case Object::Type::ANIMATION_STACK:
			{
				if (child->getType() == Object::Type::ANIMATION_LAYER)
				{
					AnimationLayerImpl *player = (AnimationLayerImpl*)child;
					((AnimationStackImpl*)parent)->mLayers.push_back(player);
				}
			}break;
			case Object::Type::ANIMATION_LAYER:
			{
				if (child->getType() == Object::Type::ANIMATION_CURVE_NODE)
				{
					AnimationCurveNodeImpl *pNode = (AnimationCurveNodeImpl*)child;
					pNode->mLayer = (AnimationLayerImpl*)parent;

					((AnimationLayerImpl*)parent)->curve_nodes.push_back(pNode);
				}
				else if (child->getType() == Object::Type::ANIMATION_LAYER)
				{
					AnimationLayerImpl *pChildLayer = (AnimationLayerImpl*)child;
					
					pChildLayer->parentLayer = (AnimationLayer*)parent;
					((AnimationLayerImpl*)parent)->sublayers.push_back(pChildLayer);
				}
			}
			break;
			case Object::Type::ANIMATION_CURVE_NODE:
			{
				AnimationCurveNodeImpl* node = (AnimationCurveNodeImpl*)parent;
				if (child->getType() == Object::Type::ANIMATION_CURVE)
				{

					if (false == node->AttachCurve((AnimationCurve*)child, &con) )
					{
						Error::s_message = "Invalid animation node";
						return false;
					}
				}
				break;
			}
		}
	}

	for (auto iter : scene->m_object_map)
	{
		Object* obj = iter.second.object;
		if (!obj) continue;

		//
		obj->Retrieve();

		if (Object::Type::CLUSTER == obj->getType())
		{
			if (!((ClusterImpl*)iter.second.object)->postprocess())
			{
				Error::s_message = "Failed to postprocess cluster";
				return false;
			}
		}
		else if (Object::Type::ANIMATION_STACK == obj->getType())
		{
			((AnimationStackImpl*)iter.second.object)->sortLayers();
		}
		/*
		else if (Object::Type::LIMB_NODE == obj->getType())
		{
			((LimbNodeImpl*)obj)->PrepProperties();
		}
		else if (Object::Type::NULL_NODE == obj->getType())
		{
			((NullImpl*)obj)->Retrieve();
		}
		else if (Object::Type::CAMERA == obj->getType())
		{
			((CameraImpl*)obj)->PrepProperties();
		}
		*/
		// pre-cache scene models hierarchy
		if (true == obj->isNode())
		{
			int idx = 0;
			Object *parent = obj->getParents(idx);
			while (nullptr != parent)
			{
				if (parent->isNode()) // scene->m_root != parent
				{
					//((Model*)obj)->mParent = (Model*)parent;
					//((Model*)parent)->mChildren.push_back((Model*)obj);
					((Model*)parent)->AddChild((Model*)obj);
				}
				
				idx += 1;
				parent = obj->getParents(idx);
			}
		}
	}


	return true;
}

void Model::AddChild(Model *pChild)
{
	pChild->mParent = this;

	if (nullptr == mFirstChild)
	{
		mFirstChild = pChild;
		pChild->mNext = nullptr;
		pChild->mPrev = nullptr;
	}
	else
	{
		Model *pLast = mFirstChild;
		while (nullptr != pLast->GetNext())
			pLast = pLast->GetNext();

		pLast->mNext = pChild;
		pChild->mPrev = pLast;
		pChild->mNext = nullptr;
	}
}



bool Model::evalLocal(OFBMatrix *result, const OFBVector3& translation, const OFBVector3& rotation, const OFBVector3 &scaling) const
{
	//Vec3 scaling = getLocalScaling();
	OFBVector3 rotation_pivot = RotationPivot;
	OFBVector3 scaling_pivot = ScalingPivot;
	OFBVector3 preRotation = { 0.0, 0.0, 0.0 }; // getPreRotation();
	OFBVector3 postRotation = { 0.0, 0.0, 0.0 }; // getPostRotation();
	OFBVector3 rotationOffset = RotationOffset;
	OFBVector3 scalingOffset = ScalingOffset;
	OFBRotationOrder rotation_order = OFBRotationOrder::eEULER_XYZ;
	
	bool enableRotationDOF = RotationActive;

	if (enableRotationDOF)
	{
		rotation_order = RotationOrder;
		preRotation = PreRotation;
		postRotation = PostRotation;
	}
	
	OFBMatrix s = makeIdentity();
	s.m[0] = scaling.x;
	s.m[5] = scaling.y;
	s.m[10] = scaling.z;

	OFBMatrix t = makeIdentity();
	setTranslation(translation, &t);

	OFBMatrix r = getRotationMatrix(rotation, rotation_order);

	// choose between simple and complex calculation way

	if (VectorIsZero(rotation_pivot) && VectorIsZero(scaling_pivot)
		&& VectorIsZero(preRotation) && VectorIsZero(postRotation)
		&& VectorIsZero(rotationOffset) && VectorIsZero(scalingOffset))
	{
		*result = t * r * s;
	}
	else
	{
		OFBMatrix r_pre = getRotationMatrix(preRotation, OFBRotationOrder::eEULER_XYZ);
		OFBMatrix r_post_inv = getRotationMatrix(-postRotation, OFBRotationOrder::eEULER_ZYX);

		OFBMatrix r_off = makeIdentity();
		setTranslation(rotationOffset, &r_off);

		OFBMatrix r_p = makeIdentity();
		setTranslation(rotation_pivot, &r_p);

		OFBMatrix r_p_inv = makeIdentity();
		setTranslation(-rotation_pivot, &r_p_inv);

		OFBMatrix s_off = makeIdentity();
		setTranslation(scalingOffset, &s_off);

		OFBMatrix s_p = makeIdentity();
		setTranslation(scaling_pivot, &s_p);

		OFBMatrix s_p_inv = makeIdentity();
		setTranslation(-scaling_pivot, &s_p_inv);

		// http://help.autodesk.com/view/FBX/2017/ENU/?guid=__files_GUID_10CDD63C_79C1_4F2D_BB28_AD2BE65A02ED_htm
		*result = t * r_off * r_p * r_pre * r * r_post_inv * r_p_inv * s_off * s_p * s * s_p_inv;
	}
	
	return true;
}


OFBMatrix Model::getGlobalTransform() const
{
	const Object* parent = getParents(0);
	if (!parent || !parent->isNode()) 
	{
		OFBMatrix tm;
		evalLocal(&tm, Translation, Rotation, Scaling);
		return tm;
	}

	Model *pParentModel = (Model*)parent;
	OFBMatrix tm;
	evalLocal(&tm, Translation, Rotation, Scaling);
	tm = pParentModel->getGlobalTransform() * tm;

	return tm;
}


Object* Object::resolveObjectLinkReverse(Object::Type type) const
{
	u64 id = element.getFirstProperty() ? element.getFirstProperty()->getValue().toU64() : 0;
	for (auto& connection : scene.m_connections)
	{
		if (connection.from == id && connection.to != 0)
		{
			Object* obj = scene.m_object_map.find(connection.to)->second.object;
			if (obj && obj->getType() == type) return obj;
		}
	}
	return nullptr;
}


const IScene& Object::getScene() const
{
	return scene;
}


Object* Object::resolveObjectLink(int idx) const
{
	u64 id = element.getFirstProperty() ? element.getFirstProperty()->getValue().toU64() : 0;
	for (auto& connection : scene.m_connections)
	{
		if (connection.to == id && connection.from != 0)
		{
			Object* obj = scene.m_object_map.find(connection.from)->second.object;
			if (obj)
			{
				if (idx == 0) return obj;
				--idx;
			}
		}
	}
	return nullptr;
}


Object* Object::resolveObjectLink(Object::Type type, const char* property, int idx) const
{
	u64 id = element.getFirstProperty() ? element.getFirstProperty()->getValue().toU64() : 0;
	for (auto& connection : scene.m_connections)
	{
		if (connection.to == id && connection.from != 0)
		{
			Object* obj = scene.m_object_map.find(connection.from)->second.object;
			if (obj && obj->getType() == type)
			{
				if (property == nullptr || connection.property == property)
				{
					if (idx == 0) return obj;
					--idx;
				}
			}
		}
	}
	return nullptr;
}


Object* Object::getParents(int idx) const
{
	int counter = 0;

	Object* parent = nullptr;
	for (auto& connection : scene.m_connections)
	{
		if (Scene::Connection::OBJECT_OBJECT == connection.type && connection.from == id)
		{
			Object* obj = scene.m_object_map.find(connection.to)->second.object;
			if (obj && obj->is_node)
			{
				if (counter == idx)
				{
					assert(parent == nullptr);
					parent = obj;
					break;
				}
				counter += 1;
			}
		}
	}
	return parent;
}

bool Object::Retrieve()
{
	int ivalue;
	double dvalue[4];
	char temp[64];
	// 1 - read from a templates

	if (nullptr != node_attribute)
	{
		Element *pelem = (Element*)&node_attribute->element;
		const Element* prop = findChild(*pelem, "Properties70");

		

		if (prop) prop = prop->child;
		while (prop)
		{
			if (prop->id == "P" && prop->first_property)
			{
				memset(temp, 0, sizeof(char)* 64);
				prop->first_property->getValue().toString(temp);

				PropertyBase *objProp = mProperties.Find(temp);

				if (nullptr != objProp)
				{
					switch (objProp->GetPropertyType())
					{
					case ePT_enum:
					case ePT_int:
						ivalue = prop->getProperty(4)->getValue().toInt();
						objProp->SetData(&ivalue);
						break;
					case ePT_double:
						dvalue[0] = prop->getProperty(4)->getValue().toDouble();
						objProp->SetData(dvalue);
						break;
					case ePT_ColorRGB:
					case ePT_Vector3D:
						dvalue[0] = prop->getProperty(4)->getValue().toDouble();
						dvalue[1] = prop->getProperty(5)->getValue().toDouble();
						dvalue[2] = prop->getProperty(6)->getValue().toDouble();
						objProp->SetData(dvalue);
						break;
					}

				}
			}
			prop = prop->sibling;
		}
	}

	// 2 - load from object


	const Element* props = findChild((const Element&)element, "Properties70");
	if (nullptr != props) 
	{
		Element* prop = props->child;
		while (prop)
		{
			memset(temp, 0, sizeof(char)* 64);
			prop->first_property->getValue().toString(temp);

			PropertyBase *objProp = mProperties.Find(temp);

			if (nullptr != objProp)
			{
				switch (objProp->GetPropertyType())
				{
				case ePT_enum:
				case ePT_int:
					ivalue = prop->getProperty(4)->getValue().toInt();
					objProp->SetData(&ivalue);
					break;
				case ePT_double:
					dvalue[0] = prop->getProperty(4)->getValue().toDouble();
					objProp->SetData(dvalue);
					break;
				case ePT_Vector3D:
					dvalue[0] = prop->getProperty(4)->getValue().toDouble();
					dvalue[1] = prop->getProperty(5)->getValue().toDouble();
					dvalue[2] = prop->getProperty(6)->getValue().toDouble();
					objProp->SetData(dvalue);
					break;
				}

			}
			
			prop = prop->sibling;
		}
	}
	
	
	return true;
}

IScene* load(const u8* data, int size)
{
	std::unique_ptr<Scene> scene = std::make_unique<Scene>();
	scene->m_data.resize(size);
	memcpy(&scene->m_data[0], data, size);
	OptionalError<Element*> root = tokenize(&scene->m_data[0], size);
	if (root.isError())
	{
		Error::s_message = "";
		root = tokenizeText(&scene->m_data[0], size);
		if (root.isError()) return nullptr;
	}

	scene->m_root_element = root.getValue();
	assert(scene->m_root_element);

	//if (parseTemplates(*root.getValue()).isError()) return nullptr;
	if(!parseConnections(*root.getValue(), scene.get())) return nullptr;
	if(!parseTakes(scene.get())) return nullptr;
	if(!parseObjects(*root.getValue(), scene.get())) return nullptr;
	parseGlobalSettings(*root.getValue(), scene.get());

	return scene.release();
}


const char* getError()
{
	return Error::s_message;
}

Model *FindModelByLabelName(IScene *pScene, const char *name, const ofbx::Object *pRoot)
{

	const int objectCount = pScene->getAllObjectCount();
	const Object *const *pObjects = pScene->getAllObjects();

	for (int i = 0; i < objectCount; ++i)
	{
		const Object *pObject = pObjects[i];

		if (true == pObject->isNode())
		{
			if (0 == strcmp(name, pObject->name))
			{
				return (Model*)pObject;
			}
		}
	}

	return nullptr;
}

bool AnimationStackImpl::sortLayers()
{
	std::sort(begin(mLayers), end(mLayers), [](AnimationLayer *a, AnimationLayer *b)->bool {

		AnimationLayerImpl *implA = (AnimationLayerImpl*)a;
		AnimationLayerImpl *implB = (AnimationLayerImpl*)b;

		return (implA->LayerID < implB->LayerID);
	});

	return true;
}

const int Model::GetAnimationNodeCount() const
{
	return  (int)mAnimationNodes.size();
}

const AnimationCurveNode *Model::GetAnimationNode(int index) const
{
	return mAnimationNodes[index];
}

const AnimationCurveNode *Model::FindAnimationNode(const char *key, const AnimationLayer *pLayer) const
{
	/*
	for (auto iter = begin(mAnimationNodes); iter != end(mAnimationNodes); ++iter)
	{
		AnimationCurveNodeImpl *pNode = (AnimationCurveNodeImpl*)*iter;
		
		if (0 == strcmp(pNode->name, key) && pNode->mLayer == pLayer)
		{
			return pNode;
		}
	}
	*/
	return nullptr;
}

const AnimationCurveNode *Model::FindAnimationNodeByType(const int typeId, const AnimationLayer *pLayer) const
{
	/*
	for (auto iter = begin(mAnimationNodes); iter != end(mAnimationNodes); ++iter)
	{
		AnimationCurveNodeImpl *pNode = (AnimationCurveNodeImpl*)*iter;
		if (typeId == pNode->mode && pNode->mLayer == pLayer)
		{
			return pNode;
		}
	}
	*/
	return nullptr;
}

void Model::GetMatrix(OFBMatrix &pMatrix, ModelTransformationType pWhat, bool pGlobalInfo, const OFBTime *pTime) const
{
	OFBTime lTime((nullptr != pTime) ? pTime->Get() : gDisplayInfo.localTime.Get());

	if (mCacheTime.Get() != lTime.Get())
	{
		OFBVector3	t, r, s;

		Translation.GetData(&t.x, sizeof(OFBVector3), &lTime);
		Rotation.GetData(&r.x, sizeof(OFBVector3), &lTime);
		Scaling.GetData(&s.x, sizeof(OFBVector3), &lTime);

		//mLocalCache = makeIdentity();
		evalLocal( (OFBMatrix*) &mLocalCache, t, r, s);

		// eval a new cache
		if (true == pGlobalInfo)
		{
			if (nullptr != mParent)
			{
				OFBMatrix parentTM;
				mParent->GetMatrix(parentTM, eModelTransformation, true, &lTime);

				OFBMatrix tm;// = parentTM * mLocalCache;
				MatrixMult(tm, parentTM, mLocalCache);

				// cache values
				memcpy((void*)&mGlobalCache.m[0], &tm.m[0], sizeof(OFBMatrix));
			}
			else
			{
				memcpy((void*)&mGlobalCache.m[0], &mLocalCache.m[0], sizeof(OFBMatrix));
			}
			((OFBTime*)&mCacheTime)->Set(lTime.Get());
		}
		
	}
	
	pMatrix = (true == pGlobalInfo) ? mGlobalCache : mLocalCache;
}

void Model::GetVector(OFBVector3 &pVector, ModelTransformationType pWhat, bool pGlobalInfo, const OFBTime *pTime) const
{
	if (true == pGlobalInfo)
	{
		OFBMatrix temp;
		GetMatrix(temp, eModelTransformation, pGlobalInfo, pTime);

		// TODO: calculate real rotation and scaling

		if (eModelTranslation == pWhat)
		{
			pVector.x = temp.m[12];
			pVector.y = temp.m[13];
			pVector.z = temp.m[14];
		}
		else if (eModelRotation == pWhat)
		{
			pVector = Vector_Zero();
		}
		else if (eModelScaling == pWhat)
		{
			pVector = MatrixGetScale(temp);
		}
	}
	else
	{
		if (eModelTranslation == pWhat)
			Translation.GetData(&pVector.x, sizeof(OFBVector3), pTime);
		else if (eModelRotation == pWhat)
			Rotation.GetData(&pVector.x, sizeof(OFBVector3), pTime);
		else if (eModelScaling == pWhat)
			Scaling.GetData(&pVector.x, sizeof(OFBVector3), pTime);
	}
}

void Model::GetRotation(OFBVector4 &pQuat, const OFBTime *pTime) const
{
	OFBMatrix temp;
	GetMatrix(temp, eModelTransformation, true, pTime);
	pQuat = MatrixGetRotation(temp);
}

bool Model::IsVisible(const OFBTime *pTime)
{
	bool vis = true;
	Visibility.GetData(&vis, sizeof(bool), pTime);

	if (false == Show)
		vis = false;
	else
	if (true == VisibilityInheritance)
	{
		Model *pParent = Parent();
		if (nullptr != pParent)
			vis = pParent->IsVisible(pTime);
	}

	return vis;
}

void Model::CustomModelDisplay(OFBRenderConveyer	*pConveyer) const
{

}

/////////////////////////////////////////////////////////////////////
// Object Properties





} // namespace ofbx
