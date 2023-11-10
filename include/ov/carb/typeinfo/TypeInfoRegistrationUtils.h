// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include "ITypeInfo.h"

#include <vector>

namespace carb
{
namespace typeinfo
{

template <class T>
class RecordRegistrator
{
public:
    RecordRegistrator() : m_name(getType<T>().name), m_size(sizeof(T))
    {
    }

    template <typename R>
    RecordRegistrator<T>& addField(const char* name, R T::*mem)
    {
        TypeHash type = getType<R>().hash;
        uint32_t offset = carb::offsetOf(mem);
        m_fields.push_back({ type, offset, name, {}, {} });
        return *this;
    }

    void commit(ITypeInfo* info)
    {
        info->registerRecordTypeEx(m_name, m_size, { m_fields.data(), m_fields.size() }, {});
    }

private:
    const char* m_name;
    uint64_t m_size;
    std::vector<FieldDesc> m_fields;
};

} // namespace typeinfo
} // namespace carb
