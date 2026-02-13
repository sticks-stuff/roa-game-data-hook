#pragma once

class GMLInstanceBase;
class GMLInstance;

enum
{
	GML_TYPE_REAL = 0,
	GML_TYPE_STRING = 1,
	GML_TYPE_ARRAY = 2,
	GML_TYPE_POINTER = 3,
	GML_TYPE_UNDEFINED = 5,
	GML_TYPE_OBJECT = 6,
	GML_TYPE_INT32 = 7,
	GML_TYPE_VEC4 = 8,
	GML_TYPE_VEC44 = 9,
	GML_TYPE_INT64 = 10,
	GML_TYPE_ACCESSOR = GML_TYPE_INT64 | GML_TYPE_STRING,
	GML_TYPE_NULL = 12,
	GML_TYPE_BOOL = 13,
	GML_TYPE_ITERATOR = 14,
};

struct GMLStringRef
{
	const char* string;
	int refCountGML;
	int size;

	GMLStringRef(const char* str)
	{
		string = str;
		size = std::strlen(str);
		refCountGML = 1;
	}

	void free()
	{
		if (string != NULL)
		{
			delete[] string;
		}
	}
};

#pragma pack(push, 4)
struct GMLVar
{
	union
	{
		int valueInt32; // int32
		long long valueInt64; // int64
		double valueReal; // number
		GMLStringRef* valueString; // string
		void* valueArray; // array
		void* valuePointer; // ptr
	};
	int flags; // Not sure what this is used for
	int	type;

	inline void setReal(double value)
	{
		freeValue();
		type = GML_TYPE_REAL;
		valueReal = value;
	}

	inline void setInt32(int value)
	{
		freeValue();
		type = GML_TYPE_INT32;
		valueInt32 = value;
	}

	inline void setInt64(long long value)
	{
		freeValue();
		type = GML_TYPE_INT64;
		valueInt64 = value;
	}

	inline void setBool(bool value)
	{
		freeValue();
		type = GML_TYPE_BOOL;
		valueReal = value ? 1 : 0;
	}

	inline void setString(const char* value)
	{
		freeValue();
		type = GML_TYPE_STRING;
		valueString = new GMLStringRef(_strdup(value));
	}

	inline void setUndefined()
	{
		freeValue();
		type = GML_TYPE_UNDEFINED;
		valuePointer = NULL;
	}

	double getReal()
	{
		switch (type)
		{
		case GML_TYPE_REAL:
		case GML_TYPE_BOOL:
			return valueReal;
		case GML_TYPE_INT32:
			return (double)valueInt32;
		case GML_TYPE_INT64:
			return (double)valueInt64;
		default:
			return 0;
		}
	}

	const char* getCString()
	{
		if (type == GML_TYPE_STRING)
		{
			if (valueString->string != NULL)
			{
				return valueString->string;
			}
		}
		return "";
	}

	inline std::string getString()
	{
		return std::string(getCString());
	}

	GMLVar()
	{
		type = GML_TYPE_UNDEFINED;
		valuePointer = NULL;
	}

	GMLVar(double value) { setReal(value); }
	GMLVar(float value) { setReal(value); }
	GMLVar(int value) { setInt32(value); }
	GMLVar(long long value) { setInt64(value); }
	GMLVar(bool value) { setReal(value ? 1 : 0); }

	GMLVar(const char* value)
	{
		type = GML_TYPE_STRING;
		valueString = new GMLStringRef(_strdup(value));
	}

	GMLVar(std::string value)
	{
		type = GML_TYPE_STRING;
		valueString = new GMLStringRef(_strdup(value.c_str()));
	}

	std::string toString()
	{
		switch (type)
		{
		case GML_TYPE_REAL:
		case GML_TYPE_BOOL:
			return std::to_string(valueReal);
		case GML_TYPE_INT32:
			return std::to_string(valueInt32);
		case GML_TYPE_INT64:
			return std::to_string(valueInt64);
		case GML_TYPE_POINTER:
			return "*" + std::to_string((int64_t)valuePointer);
		case GML_TYPE_UNDEFINED:
			return "undefined";
		case GML_TYPE_STRING:
			return getString();
		case GML_TYPE_ARRAY:
			return "array";
		default:
			return "unkonwn";
		}
	}

	inline bool truthy()
	{
		// Returns whether the value casts to true in GML
		return getReal() > 0.5;
	}

	inline void freeValue()
	{
		if (type == GML_TYPE_STRING)
		{
			valueString->free();
			delete valueString;
			type = GML_TYPE_UNDEFINED;
			valuePointer = NULL;
		}
	}

};

struct CInstanceBase
{
	virtual ~CInstanceBase() = 0;

	GMLVar* m_YYVars;
};

struct YYObjectBase : CInstanceBase {};

struct CInstance : YYObjectBase
{
	GMLVar ToRValue() const;

	GMLVar* GetRefMember(
		const char* MemberName
	);

	GMLVar* GetRefMember(
		const std::string& MemberName
	);

	const GMLVar* GetRefMember(
		const char* MemberName
	) const;

	const GMLVar* GetRefMember(
		const std::string& MemberName
	) const;

	GMLVar GetMember(
		const char* MemberName
	) const;

	GMLVar GetMember(
		const std::string& MemberName
	) const;

	int32_t GetMemberCount() const;

	static CInstance* FromInstanceID(
		int32_t InstanceID
	);
};

using PFUNC_YYGMLScript = GMLVar & (*)(
	CInstance* Self,
	CInstance* Other,
	GMLVar& Result,
	int ArgumentCount,
	GMLVar* Arguments[]
	);

using PFUNC_YYGML = void(*)(
	CInstance* Self,
	CInstance* Other
	);

using PFUNC_RAW = void(*)();

// opaque structs
struct CBackGM;
struct CViewGM;
struct CPhysicsWorld;
struct RTile;
struct YYRoomTiles;
struct YYRoomInstances;
struct CLayerEffectInfo;

// Represents a GameMaker layer.
struct CLayer;

// Represents a layer element. Can either be an instance, or a sprite.
struct CLayerElementBase;

// Represents a layer element. Refers to an instance.
struct CLayerInstanceElement;

// Represents a layer element. Refers to a sprite.
struct CLayerSpriteElement;

using FNVariable = bool(*)(
	CInstance* Instance,
	int Index,
	GMLVar* Value
	);

struct RVariableRoutine
{
	const char* m_Name;
	FNVariable m_GetVariable;
	FNVariable m_SetVariable;
	bool m_CanBeSet;
};

struct RToken
{
	int m_Kind;
	unsigned int m_Type;
	int m_Ind;
	int m_Ind2;
	GMLVar m_Value;
	int m_ItemNumber;
	RToken* m_Items;
	int m_Position;
};

struct YYGMLFuncs
{
	const char* m_Name;
	union
	{
		PFUNC_YYGMLScript m_ScriptFunction;
		PFUNC_YYGML m_CodeFunction;
		PFUNC_RAW m_RawFunction;
	};
	void* m_FunctionVariables; // YYVAR
};

struct CCode
{
	int (**_vptr$CCode)(void);
	CCode* m_Next;
	int m_Kind;
	int m_Compiled;
	const char* m_Str;
	RToken m_Token;
	GMLVar m_Value;
	void* m_VmInstance;
	void* m_VmDebugInfo;
	char* m_Code;
	const char* m_Name;
	int m_CodeIndex;
	YYGMLFuncs* m_Functions;
	bool m_Watch;
	int m_Offset;
	int m_LocalsCount;
	int m_ArgsCount;
	int m_Flags;
	YYObjectBase* m_Prototype;

	const char* GetName() const { return this->m_Name; }
};

template <typename T>
struct OLinkedList
{
	T* m_First;
	T* m_Last;
	int32_t m_Count;
};

// Newer struct, later renamed to LinkedList - OLinkedList is used in older x86 games, 
	// and causes misalingment due to alignment changing from 8-bytes in x64 to 4-bytes in x86.
template <typename T>
struct LinkedList
{
	T* m_First;
	T* m_Last;
	int32_t m_Count;
	int32_t m_DeleteType;
};

struct YYRoom
{
	// The name of the room
	uint32_t m_NameOffset;
	// The caption of the room, legacy variable, used pre-GMS
	uint32_t m_Caption;
	// The width of the room
	int32_t m_Width;
	// The height of the room
	int32_t m_Height;
	// Speed of the room
	int32_t m_Speed;
	// Whether the room is persistent (UMT marks it as a bool, but it seems to be int32_t)
	int32_t m_Persistent;
	// The background color
	int32_t m_Color;
	// Whether to show the background color
	int32_t m_ShowColor;
	// Creation code of the room
	uint32_t m_CreationCode;
	int32_t m_EnableViews;
	uint32_t pBackgrounds;
	uint32_t pViews;
	uint32_t pInstances;
	uint32_t pTiles;
	int32_t m_PhysicsWorld;
	int32_t m_PhysicsWorldTop;
	int32_t m_PhysicsWorldLeft;
	int32_t m_PhysicsWorldRight;
	int32_t m_PhysicsWorldBottom;
	float m_PhysicsGravityX;
	float m_PhysicsGravityY;
	float m_PhysicsPixelToMeters;
};

// Note: this is not how GMLVar store arrays
template <typename T>
struct CArrayStructure
{
	int32_t Length;
	T* Array;
};

struct CLayerElementBase
{
	int32_t m_Type;
	int32_t m_ID;
	bool m_RuntimeDataInitialized;
	const char* m_Name;
	CLayer* m_Layer;
	union
	{
		CLayerInstanceElement* m_InstanceFlink;
		CLayerSpriteElement* m_SpriteFlink;
		CLayerElementBase* m_Flink;
	};
	union
	{
		CLayerInstanceElement* m_InstanceBlink;
		CLayerSpriteElement* m_SpriteBlink;
		CLayerElementBase* m_Blink;
	};
};

struct CLayerSpriteElement : CLayerElementBase
{
	int32_t m_SpriteIndex;
	float m_SequencePosition;
	float m_SequenceDirection;
	float m_ImageIndex;
	float m_ImageSpeed;
	int32_t m_SpeedType;
	float m_ImageScaleX;
	float m_ImageScaleY;
	float m_ImageAngle;
	uint32_t m_ImageBlend;
	float m_ImageAlpha;
	float m_X;
	float m_Y;
};

struct CLayerInstanceElement : CLayerElementBase
{
	int32_t m_InstanceID;
	CInstance* m_Instance;
};

struct CLayer
{
	int32_t m_Id;
	int32_t m_Depth;
	float m_XOffset;
	float m_YOffset;
	float m_HorizontalSpeed;
	float m_VerticalSpeed;
	bool m_Visible;
	bool m_Deleting;
	bool m_Dynamic;
	const char* m_Name;
	GMLVar m_BeginScript;
	GMLVar m_EndScript;
	bool m_EffectEnabled;
	bool m_EffectPendingEnabled;
	GMLVar m_Effect;
	CLayerEffectInfo* m_InitialEffectInfo;
	int32_t m_ShaderID;
	LinkedList<CLayerElementBase> m_Elements;
	CLayer* m_Flink;
	CLayer* m_Blink;
	void* m_GCProxy;
};

using CHashMapHash = uint32_t;

template<typename TKey, typename TValue>
struct CHashMapElement
{
	TValue m_Value;
	TKey m_Key;
	CHashMapHash m_Hash;
};

template <typename TKey, typename TValue, int TInitialMask>
struct CHashMap
{
private:
	// Typed functions for calculating hashes
	static CHashMapHash CHashMapCalculateHash(
		int Key
	)
	{
		return (Key * -0x61c8864f + 1) & INT_MAX;
	}

	static CHashMapHash CHashMapCalculateHash(
		YYObjectBase* Key
	)
	{
		return ((static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(Key)) >> 6) * 7 + 1) & INT_MAX;
	}

	static CHashMapHash CHashMapCalculateHash(
		void* Key
	)
	{
		return ((static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(Key)) >> 8) + 1) & INT_MAX;
	};

	static CHashMapHash CHashMapCalculateHash(
		const char* Key
	)
	{
		// https://github.com/jwerle/murmurhash.c - Licensed under MIT
		size_t len = strlen(Key);
		uint32_t c1 = 0xcc9e2d51;
		uint32_t c2 = 0x1b873593;
		uint32_t r1 = 15;
		uint32_t r2 = 13;
		uint32_t m = 5;
		uint32_t n = 0xe6546b64;
		uint32_t h = 0;
		uint32_t k = 0;
		uint8_t* d = (uint8_t*)Key; // 32 bit extract from 'key'
		const uint32_t* chunks = NULL;
		const uint8_t* tail = NULL; // tail - last 8 bytes
		int i = 0;
		int l = len / 4; // chunk length

		chunks = (const uint32_t*)(d + l * 4); // body
		tail = (const uint8_t*)(d + l * 4); // last 8 byte chunk of `key'

		// for each 4 byte chunk of `key'
		for (i = -l; i != 0; ++i)
		{
			// next 4 byte chunk of `key'
			k = chunks[i];

			// encode next 4 byte chunk of `key'
			k *= c1;
			k = (k << r1) | (k >> (32 - r1));
			k *= c2;

			// append to hash
			h ^= k;
			h = (h << r2) | (h >> (32 - r2));
			h = h * m + n;
		}

		k = 0;

		// remainder
		switch (len & 3) { // `len % 4'
		case 3: k ^= (tail[2] << 16);
		case 2: k ^= (tail[1] << 8);

		case 1:
			k ^= tail[0];
			k *= c1;
			k = (k << r1) | (k >> (32 - r1));
			k *= c2;
			h ^= k;
		}

		h ^= len;

		h ^= (h >> 16);
		h *= 0x85ebca6b;
		h ^= (h >> 13);
		h *= 0xc2b2ae35;
		h ^= (h >> 16);

		return h;
	}

public:
	int32_t m_CurrentSize;
	int32_t m_UsedCount;
	int32_t m_CurrentMask;
	int32_t m_GrowThreshold;
	CHashMapElement<TKey, TValue>* m_Elements;
	void(*m_DeleteValue)(TKey* Key, TValue* Value);

	bool GetContainer(
		TKey Key,
		CHashMapElement<TKey, TValue>*& Value
	)
	{
		CHashMapHash value_hash = CHashMapCalculateHash(Key);
		int32_t ideal_position = static_cast<int>(value_hash & m_CurrentMask);

		for (
			// Start at the ideal element (the value is probably not here though)
			CHashMapElement<TKey, TValue>& current_element = this->m_Elements[ideal_position];
			// Continue looping while the hash isn't 0 (meaning we reached the end of the map)
			current_element.m_Hash != 0;
			// Go to the next position
			current_element = this->m_Elements[(++ideal_position) & this->m_CurrentMask]
			)
		{
			if (current_element.m_Key != Key)
				continue;

			Value = &current_element;
			return true;
		}

		return false;
	}

	bool GetValue(
		TKey Key,
		TValue& Value
	)
	{
		// Try to get the container
		CHashMapElement<TKey, TValue>* object_container = nullptr;
		if (!this->GetContainer(
			Key,
			object_container
		))
		{
			return false;
		}

		Value = object_container->m_Value;
		return true;
	}
};

struct CRoomInternal
{
	// CBackGM* m_Backgrounds[8];
	bool m_EnableViews;
	bool m_ClearScreen;
	bool m_ClearDisplayBuffer;
	CViewGM* m_Views[8];
	const char* m_LegacyCode;
	CCode* m_CodeObject;
	bool m_HasPhysicsWorld;
	int32_t m_PhysicsGravityX;
	int32_t m_PhysicsGravityY;
	float m_PhysicsPixelToMeters;
	OLinkedList<CInstance> m_ActiveInstances;
	LinkedList<CInstance> m_InactiveInstances;
	CInstance* m_MarkedFirst;
	CInstance* m_MarkedLast;
	int32_t* m_CreationOrderList;
	int32_t m_CreationOrderListSize;
	YYRoom* m_WadRoom;
	void* m_WadBaseAddress;
	CPhysicsWorld* m_PhysicsWorld;
	int32_t m_TileCount;
	CArrayStructure<RTile> m_Tiles;
	YYRoomTiles* m_WadTiles;
	YYRoomInstances* m_WadInstances;
	const char* m_Name;
	bool m_IsDuplicate;
	LinkedList<CLayer> m_Layers;
	CHashMap<int32_t, CLayer*, 7> m_LayerLookup;
	CHashMap<int32_t, CLayerElementBase*, 7> m_LayerElementLookup;
	CLayerElementBase* m_LastElementLookedUp;
	CHashMap<int32_t, CLayerInstanceElement*, 7> m_InstanceElementLookup;
	int32_t* m_SequenceInstanceIDs;
	int32_t m_SequenceInstanceIdCount;
	int32_t m_SequenceInstanceIdMax;
	int32_t* m_EffectLayerIDs;
	int32_t m_EffectLayerIdCount;
	int32_t m_EffectLayerIdMax;
};

struct CRoom
{
	int32_t m_LastTile;
	CRoom* m_InstanceHandle;
	const char* m_Caption;
	int32_t m_Speed;
	int32_t m_Width;
	int32_t m_Height;
	bool m_Persistent;
	uint32_t m_Color;
	bool m_ShowColor;
private:

	// Last confirmed use in 2023.8, might be later even
	struct
	{
		CBackGM* Backgrounds[8];
		CRoomInternal Internals;
	} WithBackgrounds;

	// 2024.6 (first confirmed use) has Backgrounds removed.
	// CRoomInternal cannot be properly aligned (due to bool having 1-byte alignment),
	// so GetMembers() crafts the pointer manually instead of having a defined struct here.

public:
	CRoomInternal& GetMembers();
};
#pragma pack(pop)