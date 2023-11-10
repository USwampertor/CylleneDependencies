// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"

namespace carb
{
namespace typeinfo
{

/**
 * All supported Types.
 */
enum class TypeKind
{
    eNone,
    eBuiltin, ///! int, float etc.
    ePointer, ///! int*, const float* etc.
    eConstantArray, ///! int[32]
    eFunctionPointer, ///! float (*) (char, int)
    eRecord, ///! class, struct
    eEnum, ///! enum
    eUnknown, ///! Unresolved type. Type could be unsupported or not registered in the plugin.
    eCount
};

/**
 * Type hash is unique type identifier. (FNV-1a 64 bit hash is used).
 */
typedef uint64_t TypeHash;

/**
 * Type info common to all types.
 */
struct Type
{
    TypeHash hash; ///! Hashed type name string.
    const char* name; ///! Type name (C++ canonical name).
    size_t size; ///! Size of a type. Generally it is equal to sizeof(Type::name). Could be 0 for some types (void,
                 /// function).
};

/**
 * Get a type info for defined type.
 *
 * Defining a type is not mandatory, but can be convenient to extract type name, hash and size from a type.
 * All builtins are already predefined. Use CARB_TYPE_INFO to define own types.
 *
 * Usage example:
 *     printf(getType<unsigned int>().name); // prints "unsigned int";
 */
template <class T>
constexpr Type getType();

/**
 * Type link used as a reference to any other type.
 *
 * If kind is TypeKind::eUnknown -> type is nullptr, hash is valid
 * If kind is TypeKind::eNone -> type is nullptr, hash is 0. That means link points to nothing.
 * For any other kind -> type points to actual class. E.g. for TypeKind::eRecord type points to RecordType.
 *
 * Usage example:
 *     TypeLink type = ...;
 *     if (type.is<PointerType>())
 *         TypeLink pointedType = type.getAs<PointerType>()->pointee;
 */
struct TypeLink
{
    TypeHash hash = 0;
    TypeKind kind = TypeKind::eNone;
    void* type = nullptr;

    template <typename T>
    bool is() const
    {
        return T::kKind == kind;
    }

    template <typename T>
    const T* getAs() const
    {
        if (is<T>())
            return static_cast<const T*>(type);
        return nullptr;
    }
};


/**
 * Helper class to store const ranges in array. Supports C++11 range-based for loop.
 */
template <class T>
class Range
{
public:
    Range() = default;

    Range(const T* begin, size_t size) : m_begin(begin), m_size(size)
    {
    }

    const T* begin() const
    {
        return m_begin;
    }

    const T* end() const
    {
        return m_begin + m_size;
    }

    size_t size() const
    {
        return m_size;
    }

    const T& operator[](size_t idx) const
    {
        CARB_ASSERT(idx < m_size);
        return m_begin[idx];
    }

private:
    const T* m_begin;
    size_t m_size;
};


/**
 * Attribute is a tag that is used to convey additional (meta) information about fields or records.
 *
 * The idea is that you can associate some data of particular type with a field or record.
 * Use AttributeDesc to specify data pointer, data type and size. Data will be copied internally, so it should be fully
 * copyable.
 *
 * struct MyAttribute { int min, max };
 * MyAttribute attr1 = { 0, 100 };
 * AttributeDesc desc = { "", &attr1, sizeof(MyAttribute), CARB_HASH_STRING("carb::my::MyAttribute") };
 *
 * ... pass desc into some of ITypeInfo registration functions, for example in FieldDesc.
 * then you can use it as:
 *
 * const RecordType* r = ...;
 * const Attribute& a = r->fields[0]->attributes[0];
 * if (a.isType<MyAttribute>())
 *     return a.getValue<MyAttrubte>();
 *
 * You can also pass nullptr as data and only use annotation string. Thus have custom string attribute.
 */
class Attribute
{
public:
    const char* annotation = nullptr; //!< Stores whole annotation string as is.
    const void* data = nullptr; //!< Pointer to the data constructed from attribute expression.
    TypeLink type; //!< Type of the attribute data.

    template <class T>
    bool isType() const
    {
        return getType<T>().hash == type.hash;
    }

    template <class T>
    const T& getValue() const
    {
        CARB_ASSERT(isType<T>() && "type mismatch");
        return *static_cast<const T*>(data);
    }
};

/**
 * Attribute descriptor used to specify field and record attributes.
 */
struct AttributeDesc
{
    const char* annotation; ///! Annotation string as is (recommended). It is not used anywhere internally, so any
                            /// information could be stored.
    void* data; ///! Pointer to a data to copy. Can be nullptr.
    size_t dataSize; ///! Size of data to copy.
    TypeHash type; ///! Type of data. Is ignored if data is nullptr or 0 size.
};


/**
 * Builtin type. E.g. float, int, double, char etc.
 *
 * All builtin types are automatically registered and defined (see CARB_TYPE_INFO below).
 */
struct BuiltinType
{
    Type base;

    static constexpr TypeKind kKind = TypeKind::eBuiltin;
};

/**
 * Pointer type. E.g. int*, const float* const*.
 */
struct PointerType
{
    Type base;
    TypeLink pointee; ///! The type it points to.

    static constexpr TypeKind kKind = TypeKind::ePointer;
};

/**
 * Represents the canonical version of C arrays with a specified constant size.
 * Example: int x[204];
 */
struct ConstantArrayType
{
    Type base;
    TypeLink elementType; ///! The type of array element.
    size_t arraySize; ///! The size of array.

    static constexpr TypeKind kKind = TypeKind::eConstantArray;
};

/**
 * Function pointer type.
 *
 * Special type which describes pointer to a function.
 */
struct FunctionPointerType
{
    Type base;
    TypeLink returnType; ///! Qualified return type of a function.
    Range<TypeLink> parameters; ///! Function parameters represented as qualified types.

    static constexpr TypeKind kKind = TypeKind::eFunctionPointer;
};

struct FunctionPointerTypeDesc
{
    TypeHash returnType;
    Range<TypeHash> parameters;
};

struct FieldExtra;

/**
 * Represents a field in a record (class or struct).
 */
struct Field
{
    TypeLink type; ///! Qualified type of a field.
    uint32_t offset; ///! Field offset in a record. Only fields with valid offset are supported.
    const char* name; ///! Field name.
    FieldExtra* extra; ///! Extra information available for some fields. Can be nullptr.
    Range<Attribute> attributes; ///! Field attributes.


    //////////// Access ////////////

    template <class T>
    bool isType() const
    {
        return getType<T>().hash == type.hash;
    }

    template <class T>
    bool isTypeOrElementType() const
    {
        if (isType<T>())
            return true;

        const ConstantArrayType* arrayType = type.getAs<ConstantArrayType>();
        return arrayType && (arrayType->elementType.hash == getType<T>().hash);
    }

    template <class T, typename S>
    void setValue(volatile S& instance, T const& value) const
    {
        memcpy((char*)&instance + offset, &value, sizeof(T));
    }

    template <class T, typename S>
    void setValueChecked(volatile S& instance, T const& value) const
    {
        if (isType<T>())
            setValue(instance, value);
        else
            CARB_ASSERT(false && "type mismatch");
    }

    template <class T, typename S>
    T& getRef(S& instance) const
    {
        return *(T*)((char*)&instance + offset);
    }

    template <class T, typename S>
    const T& getRef(const S& instance) const
    {
        return getRef(const_cast<S&>(instance));
    }

    template <class T, typename S>
    T getValue(const S& instance) const
    {
        return *(T*)((const char*)&instance + offset);
    }

    template <class T, typename S>
    T getValueChecked(const S& instance) const
    {
        if (isType<T>())
            return getValue<T>(instance);
        CARB_ASSERT(false && "type mismatch");
        return T{};
    }

    template <class T, typename S>
    T* getPtr(S& instance) const
    {
        return reinterpret_cast<T*>((char*)&instance + offset);
    }

    template <class T, typename S>
    const T* getPtr(const S& instance) const
    {
        return getPtr(const_cast<S&>(instance));
    }

    template <class T, typename S>
    T* getPtrChecked(S& instance) const
    {
        if (isTypeOrElementType<T>())
            return getPtr(instance);
        CARB_ASSERT(false && "type mismatch");
        return nullptr;
    }

    template <class T, typename S>
    const T* getPtrChecked(const S& instance) const
    {
        return getPtrChecked(const_cast<S&>(instance));
    }
};

/**
 * Field extra information.
 *
 * If Field is a function pointer it stores function parameter names.
 */
struct FieldExtra
{
    Range<const char*> functionParameters; ///! Function parameter names
};

/**
 * Field descriptor used to specify fields.
 *
 * The main difference from the Field is that type is specified using a hash, which plugin automatically resolves into
 * TypeLink during this record registration or later when type with this hash is registered.
 */
struct FieldDesc
{
    TypeHash type;
    uint32_t offset;
    const char* name;
    Range<AttributeDesc> attributes;
    Range<const char*> extraFunctionParameters;
};

/**
 * Represents a record (struct or class) as a collection of fields
 */
struct RecordType
{
    Type base;
    Range<Field> fields;
    Range<Attribute> attributes; ///! Record attributes.

    static constexpr TypeKind kKind = TypeKind::eRecord;
};

/**
 * Represents single enum constant.
 */
struct EnumConstant
{
    const char* name;
    uint64_t value;
};

/**
 * Represents a enum type (enum or enum class).
 */
struct EnumType
{
    Type base;
    Range<EnumConstant> constants;

    static constexpr TypeKind kKind = TypeKind::eEnum;
};


/**
 * ITypeInfo plugin interface.
 *
 * Split into 2 parts: one for type registration, other for querying type information.
 *
 * Registration follows the same principle for every function starting with word "register":
 * If a type (of the same type kind) with this name is already registered:
 * 1. It returns it instead
 * 2. Warning is logged.
 * 3. The content is checked and error is logged for any mismatch.
 *
 * The order of type registering is not important. If registered type contains TypeLink inside (fields of a struct,
 * returned type of function etc.) it will be lazily resolved when appropriate type is registered.
 *
 * Every registration function comes in 2 flavors: templated version and explicit (with "Ex" postfix). The first one
 * works only if there is appropriate getType<> specialization, thus CARB_TYPE_INFO() with registered type
 * should be defined before.
 *
 */
struct ITypeInfo
{
    CARB_PLUGIN_INTERFACE("carb::typeinfo::ITypeInfo", 1, 0)

    /**
     * Get a type registered in the plugin by name/type hash. Empty link can be returned.
     */
    TypeLink(CARB_ABI* getTypeByName)(const char* name);
    TypeLink(CARB_ABI* getTypeByHash)(TypeHash hash);

    /**
     * Get a record type registered in the plugin by name/type hash. Can return nullptr.
     */
    const RecordType*(CARB_ABI* getRecordTypeByName)(const char* name);
    const RecordType*(CARB_ABI* getRecordTypeByHash)(TypeHash hash);

    /**
     * Get a number of all record types registered in the plugin.
     */
    size_t(CARB_ABI* getRecordTypeCount)();

    /**
     * Get all record types registered in the plugin. The count of elements in the array is
     * ITypeInfo::getRecordTypeCount().
     */
    const RecordType* const*(CARB_ABI* getRecordTypes)();

    /**
     * Get a enum type registered in the plugin by name/type hash. Can return nullptr.
     */
    const EnumType*(CARB_ABI* getEnumTypeByName)(const char* name);
    const EnumType*(CARB_ABI* getEnumTypeByHash)(TypeHash hash);

    /**
     * Get a pointer type registered in the plugin by name/type hash. Can return nullptr.
     */
    const PointerType*(CARB_ABI* getPointerTypeByName)(const char* name);
    const PointerType*(CARB_ABI* getPointerTypeByHash)(TypeHash hash);

    /**
     * Get a constant array type registered in the plugin by name/type hash. Can return nullptr.
     */
    const ConstantArrayType*(CARB_ABI* getConstantArrayTypeByName)(const char* name);
    const ConstantArrayType*(CARB_ABI* getConstantArrayTypeByHash)(TypeHash hash);

    /**
     * Get a function pointer type registered in the plugin by name/type hash. Can return nullptr.
     */
    const FunctionPointerType*(CARB_ABI* getFunctionPointerTypeByName)(const char* name);
    const FunctionPointerType*(CARB_ABI* getFunctionPointerTypeByHash)(TypeHash hash);

    /**
     * Register a new record type.
     */
    template <class T>
    const RecordType* registerRecordType(const Range<FieldDesc>& fields = {},
                                         const Range<AttributeDesc>& attributes = {});

    const RecordType*(CARB_ABI* registerRecordTypeEx)(const char* name,
                                                      size_t size,
                                                      const Range<FieldDesc>& fields,
                                                      const Range<AttributeDesc>& attributes);

    /**
     * Register a new enum type.
     */
    template <class T>
    const EnumType* registerEnumType(const Range<EnumConstant>& constants);

    const EnumType*(CARB_ABI* registerEnumTypeEx)(const char* name, size_t size, const Range<EnumConstant>& constants);

    /**
     * Register a new pointer type.
     */
    template <class T>
    const PointerType* registerPointerType(TypeHash pointee);

    const PointerType*(CARB_ABI* registerPointerTypeEx)(const char* name, size_t size, TypeHash pointee);

    /**
     * Register a new constant array type.
     */
    template <class T>
    const ConstantArrayType* registerConstantArrayType(TypeHash elementType, size_t arraySize);

    const ConstantArrayType*(CARB_ABI* registerConstantArrayTypeEx)(const char* name,
                                                                    size_t size,
                                                                    TypeHash elementType,
                                                                    size_t arraySize);

    /**
     * Register a new function pointer type.
     */
    template <class T>
    const FunctionPointerType* registerFunctionPointerType(TypeHash returnType, Range<TypeHash> parameters = {});

    const FunctionPointerType*(CARB_ABI* registerFunctionPointerTypeEx)(const char* name,
                                                                        size_t size,
                                                                        TypeHash returnType,
                                                                        Range<TypeHash> parameters);
};


template <class T>
inline const RecordType* ITypeInfo::registerRecordType(const Range<FieldDesc>& fields,
                                                       const Range<AttributeDesc>& attributes)
{
    const Type type = getType<T>();
    return this->registerRecordTypeEx(type.name, type.size, fields, attributes);
}

template <class T>
inline const EnumType* ITypeInfo::registerEnumType(const Range<EnumConstant>& constants)
{
    const Type type = getType<T>();
    return this->registerEnumTypeEx(type.name, type.size, constants);
}

template <class T>
inline const PointerType* ITypeInfo::registerPointerType(TypeHash pointee)
{
    const Type type = getType<T>();
    return this->registerPointerTypeEx(type.name, type.size, pointee);
}

template <class T>
inline const ConstantArrayType* ITypeInfo::registerConstantArrayType(TypeHash elementType, size_t arraySize)
{
    const Type type = getType<T>();
    return this->registerConstantArrayTypeEx(type.name, type.size, elementType, arraySize);
}

template <class T>
inline const FunctionPointerType* ITypeInfo::registerFunctionPointerType(TypeHash returnType, Range<TypeHash> parameters)
{
    const Type type = getType<T>();
    return this->registerFunctionPointerTypeEx(type.name, type.size, returnType, parameters);
}

} // namespace typeinfo
} // namespace carb


/**
 * Convenience macro to define type.
 *
 * Use it like that:
 * CARB_TYPE_INFO(int*)
 * CARB_TYPE_INFO(np::MyClass)
 *
 * NOTE: it is important to also pass full namespace path to your type (to capture it in the type name).
 */
#define CARB_TYPE_INFO(T)                                                                                              \
    namespace carb                                                                                                     \
    {                                                                                                                  \
    namespace typeinfo                                                                                                 \
    {                                                                                                                  \
    template <>                                                                                                        \
    constexpr carb::typeinfo::Type getType<T>()                                                                        \
    {                                                                                                                  \
        return { CARB_HASH_STRING(#T), #T, sizeof(T) };                                                                \
    }                                                                                                                  \
    }                                                                                                                  \
    }

/**
 * Predefined builtin types
 */
CARB_TYPE_INFO(bool)
CARB_TYPE_INFO(char)
CARB_TYPE_INFO(unsigned char)
CARB_TYPE_INFO(short)
CARB_TYPE_INFO(unsigned short)
CARB_TYPE_INFO(int)
CARB_TYPE_INFO(unsigned int)
CARB_TYPE_INFO(long)
CARB_TYPE_INFO(unsigned long)
CARB_TYPE_INFO(long long)
CARB_TYPE_INFO(unsigned long long)
CARB_TYPE_INFO(float)
CARB_TYPE_INFO(double)
CARB_TYPE_INFO(long double)
