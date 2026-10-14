// Generated literal source-layout projection; see EXTRACTION.json for exact byte spans.
#ifndef WITH_CASE_PRESERVING_NAME
#define WITH_CASE_PRESERVING_NAME 0
#endif
#ifndef UE_FNAME_OUTLINE_NUMBER
#define UE_FNAME_OUTLINE_NUMBER 0
#endif
#ifndef WITH_METADATA
#define WITH_METADATA 0
#endif
#define COREUOBJECT_API
#define UE_DEPRECATED(...)
#if PACK_CONTROL == 4
#pragma pack(push,4)
#elif PACK_CONTROL == 1
#pragma pack(push,1)
#endif
template<typename T32BITS, typename T64BITS, int PointerSize>
struct SelectIntPointerType
{
	// nothing here are is it an error if the partial specializations fail
};

template<typename T32BITS, typename T64BITS>
struct SelectIntPointerType<T32BITS, T64BITS, 8>
{
	// Select the 64 bit type.
	typedef T64BITS TIntPointer;
};

template<typename T32BITS, typename T64BITS>
struct SelectIntPointerType<T32BITS, T64BITS, 4>
{
	// Select the 32 bit type.
	typedef T32BITS TIntPointer;
};


struct FGenericPlatformTypes
{
	//~ Unsigned base types

	// 8-bit unsigned integer
	typedef unsigned char 		uint8;

	// 16-bit unsigned integer
	typedef unsigned short int	uint16;

	// 32-bit unsigned integer
	typedef unsigned int		uint32;

	// 64-bit unsigned integer
	typedef unsigned long long	uint64;

	//~ Signed base types.

	// 8-bit signed integer
	typedef	signed char			int8;

	// 16-bit signed integer
	typedef signed short int	int16;

	// 32-bit signed integer
	typedef signed int	 		int32;

	// 64-bit signed integer
	typedef signed long long	int64;

	//~ Character types

	// An ANSI character. 8-bit fixed-width representation of 7-bit characters.
	typedef char				ANSICHAR;

	// A wide character. In-memory only. ?-bit fixed-width representation of the platform's natural wide character set. Could be different sizes on different platforms.
	typedef wchar_t				WIDECHAR;

	// An 8-bit character type. In-memory only. 8-bit representation. Should really be char8_t but making this the generic option is easier for compilers which don't fully support C++20 yet.
	enum UTF8CHAR : unsigned char {};

	// A 16-bit character type. In-memory only.  16-bit representation. Should really be char16_t but making this the generic option is easier for compilers which don't fully support C++11 yet (i.e. MSVC).
	typedef uint16				CHAR16;		

	// A 32-bit character type. In-memory only. 32-bit representation. Should really be char32_t but making this the generic option is easier for compilers which don't fully support C++11 yet (i.e. MSVC).
	[[deprecated("CHAR32 has been deprecated - please use UTF32CHAR instead.")]] // not using UE_DEPRECATED(5.6, "...") for dependency reasons
	typedef uint32				CHAR32;

	// A 32-bit character type. In-memory only. 32-bit representation.
	using UTF32CHAR = char32_t;

	// A switchable character. In-memory only. Either ANSICHAR or WIDECHAR, depending on a licensee's requirements.
	typedef WIDECHAR			TCHAR;

	// Unsigned int. The same size as a pointer.
	typedef SelectIntPointerType<uint32, uint64, sizeof(void*)>::TIntPointer UPTRINT;

	// Signed int. The same size as a pointer.
	typedef SelectIntPointerType<int32, int64, sizeof(void*)>::TIntPointer PTRINT;

	// Unsigned int. The same size as a pointer.
	typedef UPTRINT SIZE_T;

	// Signed int. The same size as a pointer.
	typedef PTRINT SSIZE_T;

	typedef int32					TYPE_OF_NULL;
	typedef decltype(nullptr)		TYPE_OF_NULLPTR;
};
#define UE_NONCOPYABLE(TypeName) \
	TypeName(TypeName&&) = delete; \
	TypeName(const TypeName&) = delete; \
	TypeName& operator=(const TypeName&) = delete; \
	TypeName& operator=(TypeName&&) = delete;


using uint32=FGenericPlatformTypes::uint32;
using uint64=FGenericPlatformTypes::uint64;
using int32=FGenericPlatformTypes::int32;
using SIZE_T=FGenericPlatformTypes::SIZE_T;

enum EClassFlags
{
	/** No Flags */
	CLASS_None				  = 0x00000000u,
	/** Class is abstract and can't be instantiated directly. */
	CLASS_Abstract            = 0x00000001u,
	/** Save object configuration only to Default INIs, never to local INIs. Must be combined with CLASS_Config */
	CLASS_DefaultConfig		  = 0x00000002u,
	/** Load object configuration at construction time. */
	CLASS_Config			  = 0x00000004u,
	/** This object type can't be saved; null it out at save time. */
	CLASS_Transient			  = 0x00000008u,
	/** This object type may not be available in certain context. (i.e. game runtime or in certain configuration). Optional class data is saved separately to other object types. (i.e. might use sidecar files) */
	CLASS_Optional            = 0x00000010u,
	/** */
	CLASS_MatchedSerializers  = 0x00000020u,
	/** Indicates that the config settings for this class will be saved to Project/User*.ini (similar to CLASS_GlobalUserConfig) */
	CLASS_ProjectUserConfig	  = 0x00000040u,
	/** Class is a native class - native interfaces will have CLASS_Native set, but not RF_MarkAsNative */
	CLASS_Native			  = 0x00000080u,
	/** Instances of this class can migrate to another server even if loaded from disk. */
	CLASS_MigratingAsset	  = 0x00000100u,
	/** Do not allow users to create in the editor. */
	CLASS_NotPlaceable        = 0x00000200u,
	/** Handle object configuration on a per-object basis, rather than per-class. */
	CLASS_PerObjectConfig     = 0x00000400u,
	
	/** Whether SetUpRuntimeReplicationData still needs to be called for this class */
	CLASS_ReplicationDataIsSetUp = 0x00000800u,
	
	/** Class can be constructed from editinline New button. */
	CLASS_EditInlineNew		  = 0x00001000u,
	/** Display properties in the editor without using categories. */
	CLASS_CollapseCategories  = 0x00002000u,
	/** Class is an interface **/
	CLASS_Interface           = 0x00004000u,
	/**  Config for this class is overridden in platform inis, reload when previewing platforms **/
	CLASS_PerPlatformConfig   = 0x00008000u,
	/** all properties and functions in this class are const and should be exported as const */
	CLASS_Const			      = 0x00010000u,

	/** Class flag indicating objects of this class need deferred dependency loading */
	CLASS_NeedsDeferredDependencyLoading = 0x00020000u,
	
	/** Indicates that the class was created from blueprint source material */
	CLASS_CompiledFromBlueprint  = 0x00040000u,

	/** Indicates that only the bare minimum bits of this class should be DLL exported/imported */
	CLASS_MinimalAPI	      = 0x00080000u,
	
	/** Indicates this class must be DLL exported/imported (along with all of it's members) */
	CLASS_RequiredAPI	      = 0x00100000u,

	/** Indicates that references to this class default to instanced. Used to be subclasses of UComponent, but now can be any UObject */
	CLASS_DefaultToInstanced  = 0x00200000u,

	/** Indicates that the parent token stream has been merged with ours. */
	CLASS_TokenStreamAssembled  = 0x00400000u,
	/** Class has component properties. */
	CLASS_HasInstancedReference= 0x00800000u,
	/** Don't show this class in the editor class browser or edit inline new menus. */
	CLASS_Hidden			  = 0x01000000u,
	/** Don't save objects of this class when serializing */
	CLASS_Deprecated		  = 0x02000000u,
	/** Class not shown in editor drop down for class selection */
	CLASS_HideDropDown		  = 0x04000000u,
	/** Class settings are saved to <AppData>/..../Blah.ini (as opposed to CLASS_DefaultConfig) */
	CLASS_GlobalUserConfig	  = 0x08000000u,
	/** Class was declared directly in C++ and has no boilerplate generated by UnrealHeaderTool */
	CLASS_Intrinsic			  = 0x10000000u,
	/** Class has already been constructed (maybe in a previous DLL version before hot-reload). */
	CLASS_Constructed		  = 0x20000000u,
	/** Indicates that object configuration will not check against ini base/defaults when serialized */
	CLASS_ConfigDoNotCheckDefaults = 0x40000000u,
	/** Class has been consigned to oblivion as part of a blueprint recompile, and a newer version currently exists. */
	CLASS_NewerVersionExists  = 0x80000000u,
};
enum EObjectFlags
{
	// Do not add new flags unless they truly belong here. There are alternatives.
	// if you change any the bit of any of the RF_Load flags, then you will need legacy serialization
	RF_NoFlags					= 0x00000000,	///< No flags, used to avoid a cast

	// This first group of flags mostly has to do with what kind of object it is. Other than transient, these are the persistent object flags.
	// The garbage collector also tends to look at these.
	RF_Public					=0x00000001,	///< Object is visible outside its package.
	RF_Standalone				=0x00000002,	///< Keep object around for editing even if unreferenced.
	RF_MarkAsNative				=0x00000004,	///< Object (UField) will be marked as native on construction (DO NOT USE THIS FLAG in HasAnyFlags() etc)
	RF_Transactional			=0x00000008,	///< Object is transactional.
	RF_ClassDefaultObject		=0x00000010,	///< This object is used as the default template for all instances of a class. One object is created for each class
	RF_ArchetypeObject			=0x00000020,	///< This object can be used as a template for instancing objects. This is set on all types of object templates
	RF_Transient				=0x00000040,	///< Don't save object.

	// This group of flags is primarily concerned with garbage collection.
	RF_MarkAsRootSet			=0x00000080,	///< Object will be marked as root set on construction and not be garbage collected, even if unreferenced (DO NOT USE THIS FLAG in HasAnyFlags() etc)
	RF_TagGarbageTemp			=0x00000100,	///< This is a temp user flag for various utilities that need to use the garbage collector. The garbage collector itself does not interpret it.

	// The group of flags tracks the stages of the lifetime of a uobject
	RF_NeedInitialization		=0x00000200,	///< This object has not completed its initialization process. Cleared when ~FObjectInitializer completes
	RF_NeedLoad					=0x00000400,	///< During load, indicates object needs loading.
	RF_KeepForCooker UE_DEPRECATED(5.6, "This flag is now obsolete as the functionality is unnecessary.")			=0x00000800,	///< Keep this object during garbage collection because it's still being used by the cooker
	RF_NeedPostLoad				=0x00001000,	///< Object needs to be postloaded.
	RF_NeedPostLoadSubobjects	=0x00002000,	///< During load, indicates that the object still needs to instance subobjects and fixup serialized component references
	RF_NewerVersionExists		=0x00004000,	///< Object has been consigned to oblivion due to its owner package being reloaded, and a newer version currently exists
	RF_BeginDestroyed			=0x00008000,	///< BeginDestroy has been called on the object.
	RF_FinishDestroyed			=0x00010000,	///< FinishDestroy has been called on the object.

	// Misc. Flags
	RF_BeingRegenerated			=0x00020000,	///< Flagged on UObjects that are used to create UClasses (e.g. Blueprints) while they are regenerating their UClass on load (See FLinkerLoad::CreateExport()), as well as UClass objects in the midst of being created
	RF_DefaultSubObject			=0x00040000,	///< Flagged on subobject templates that were created in a class constructor, and all instances created from those templates
	RF_WasLoaded				=0x00080000,	///< Flagged on UObjects that were loaded
	RF_TextExportTransient		=0x00100000,	///< Do not export object to text form (e.g. copy/paste). Generally used for sub-objects that can be regenerated from data in their parent object.
	RF_LoadCompleted			=0x00200000,	///< Object has been completely serialized by linkerload at least once. DO NOT USE THIS FLAG, It should be replaced with RF_WasLoaded.
	RF_InheritableComponentTemplate = 0x00400000, ///< Flagged on subobject templates stored inside a class instead of the class default object, they are instanced after default subobjects
	RF_DuplicateTransient		=0x00800000,	///< Object should not be included in any type of duplication (copy/paste, binary duplication, etc.)
	RF_StrongRefOnFrame			=0x01000000,	///< References to this object from persistent function frame are handled as strong ones.
	RF_NonPIEDuplicateTransient	=0x02000000,	///< Object should not be included for duplication unless it's being duplicated for a PIE session
	RF_ImmutableDefaultObject	=0x04000000,	///< Hidden version of the class default object that cannot and will never be mutated
	RF_WillBeLoaded				=0x08000000,	///< This object was constructed during load and will be loaded shortly
	RF_HasExternalPackage		=0x10000000,	///< This object has an external package assigned and should look it up when getting the outermost package
	// RF_Unused				=0x20000000,

	// RF_MirroredGarbage is mirrored in EInternalObjectFlags::Garbage because checking the internal flags is much faster for the Garbage Collector
	// while checking the object flags is much faster outside of it where the Object pointer is already available and most likely cached.
	RF_MirroredGarbage			=0x40000000,	///< Garbage from logical point of view and should not be referenced. This flag is mirrored in EInternalObjectFlags as Garbage for performance
	RF_AllocatedInSharedPage	=0x80000000,	///< Allocated from a ref-counted page shared with other UObjects
};

class FField; class FFieldClass; class UObject; class FString; template<class K,class V> class TMap;

struct FNameEntryId {
public:
	uint32 Value;
};

class FName {
public:
	FNameEntryId	ComparisonIndex;
#if !UE_FNAME_OUTLINE_NUMBER
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	uint32			Number;
#endif// ! //UE_FNAME_OUTLINE_NUMBER
#if WITH_CASE_PRESERVING_NAME
	/** Index into the Names array (used to find String portion of the string/number pair used for display) */
	FNameEntryId	DisplayIndex;
#endif // WITH_CASE_PRESERVING_NAME
};

class FThreadSafeCounter {
public:
	volatile int32 Counter;
};

class FFieldVariant {
public:
	union FFieldObjectUnion
	{
		FField* Field;
		UObject* Object;
	} Container;
};

class FFieldClass {
public:
UE_NONCOPYABLE(FFieldClass);
	/** Name of this field class */
	FName Name;
	/** Unique Id of this field class (for casting) */
	uint64 Id;
	/** Cast flags used for casting to other classes */
	uint64 CastFlags;
	/** Class flags */
	EClassFlags ClassFlags;
	/** Super of this class */
	FFieldClass* SuperClass;	
	/** Default instance of this class */
	FField* DefaultObject;
	/** Pointer to a function that can construct an instance of this class */
	FField* (*ConstructFn)(const FFieldVariant&, const FName&, EObjectFlags);
	/** Counter for generating runtime unique names */
	FThreadSafeCounter UnqiueNameIndexCounter;

};

class FField {
public:
UE_NONCOPYABLE(FField);
	/** Pointer to the class object representing the type of this FField */
	FFieldClass* ClassPrivate;

public:
	typedef FField Super;
	typedef FField ThisClass;
	typedef FField BaseFieldClass;	
	typedef FFieldClass FieldTypeClass;

	static COREUOBJECT_API FFieldClass* StaticClass();

	virtual SIZE_T GetFieldSize() const
	{
		return sizeof(FField);
	}

	/** Owner of this field */
	FFieldVariant Owner;

	/** Next Field in the linked list */
	FField* Next;

	/** Name of this field */
	FName NamePrivate;

	/** Object flags */
	EObjectFlags FlagsPrivate;

	COREUOBJECT_API virtual ~FField();
#if WITH_METADATA
private:
	/** Editor-only meta data map */
	TMap<FName, FString>* MetaDataMap;

#endif

};

extern "C" __attribute__((used)) const uint32 OfficialLayoutFacts[]={
sizeof(FField),
alignof(FField),
sizeof(FFieldClass),
alignof(FFieldClass),
sizeof(FName),
alignof(FName),
sizeof(FNameEntryId),
sizeof(FThreadSafeCounter),
sizeof(FFieldVariant),
__builtin_offsetof(FField,ClassPrivate),
__builtin_offsetof(FField,Owner),
__builtin_offsetof(FField,Next),
__builtin_offsetof(FField,NamePrivate),
__builtin_offsetof(FField,FlagsPrivate),
__builtin_offsetof(FFieldClass,Name),
__builtin_offsetof(FFieldClass,Id),
__builtin_offsetof(FFieldClass,CastFlags),
__builtin_offsetof(FFieldClass,ClassFlags),
__builtin_offsetof(FFieldClass,SuperClass),
__builtin_offsetof(FFieldClass,DefaultObject),
__builtin_offsetof(FFieldClass,ConstructFn),
__builtin_offsetof(FFieldClass,UnqiueNameIndexCounter),
sizeof(SIZE_T),
sizeof(EClassFlags),
sizeof(EObjectFlags)
};
extern "C" __attribute__((used)) SIZE_T OfficialFieldSizeBody(const FField* field){return field->FField::GetFieldSize();}

#if PACK_CONTROL == 4 || PACK_CONTROL == 1
#pragma pack(pop)
#endif
