//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
////////////////////////////////////////////////////////////////////////
// This file is generated by a script.  Do not edit directly.  Edit the
// range.template.h file to make changes.

#ifndef PXR_BASE_GF_RANGE1F_H
#define PXR_BASE_GF_RANGE1F_H

/// \file gf/range1f.h
/// \ingroup group_gf_BasicGeometry

#include "pxr/pxr.h"

#include "pxr/base/gf/api.h"
#include "pxr/base/gf/traits.h"

#include <boost/functional/hash.hpp>

#include <cfloat>
#include <cstddef>
#include <iosfwd>

PXR_NAMESPACE_OPEN_SCOPE

class GfRange1d;
class GfRange1f;

template <>
struct GfIsGfRange<class GfRange1f> { static const bool value = true; };

/// \class GfRange1f
/// \ingroup group_gf_BasicGeometry
///
/// Basic type: 1-dimensional floating point range.
///
/// This class represents a 1-dimensional range (or interval) All
/// operations are component-wise and conform to interval mathematics. An
/// empty range is one where max < min.
/// The default empty is [FLT_MAX,-FLT_MAX]
class GfRange1f
{
public:

    /// Helper typedef.
    typedef float MinMaxType;

    static const size_t dimension = 1;
    typedef MinMaxType ScalarType;

    /// Sets the range to an empty interval
    // TODO check whether this can be deprecated.
    void inline SetEmpty() {
	_min =  FLT_MAX;
	_max = -FLT_MAX;
    }

    /// The default constructor creates an empty range.
    GfRange1f() {
        SetEmpty();
    }

    /// This constructor initializes the minimum and maximum points.
    GfRange1f(float min, float max)
        : _min(min), _max(max)
    {
    }

    /// Returns the minimum value of the range.
    float GetMin() const { return _min; }

    /// Returns the maximum value of the range.
    float GetMax() const { return _max; }

    /// Returns the size of the range.
    float GetSize() const { return _max - _min; }

    /// Returns the midpoint of the range, that is, 0.5*(min+max).
    /// Note: this returns zero in the case of default-constructed ranges,
    /// or ranges set via SetEmpty().
    float GetMidpoint() const {
        return static_cast<ScalarType>(0.5) * _min
               + static_cast<ScalarType>(0.5) * _max;
    }

    /// Sets the minimum value of the range.
    void SetMin(float min) { _min = min; }

    /// Sets the maximum value of the range.
    void SetMax(float max) { _max = max; }

    /// Returns whether the range is empty (max < min).
    bool IsEmpty() const {
        return _min > _max;
    }

    /// Modifies the range if necessary to surround the given value.
    /// \deprecated Use UnionWith() instead.
    void ExtendBy(float point) { UnionWith(point); }

    /// Modifies the range if necessary to surround the given range.
    /// \deprecated Use UnionWith() instead.
    void ExtendBy(const GfRange1f &range) { UnionWith(range); }

    /// Returns true if the \p point is located inside the range. As with all
    /// operations of this type, the range is assumed to include its extrema.
    bool Contains(float point) const {
        return (point >= _min && point <= _max);
    }

    /// Returns true if the \p range is located entirely inside the range. As
    /// with all operations of this type, the ranges are assumed to include
    /// their extrema.
    bool Contains(const GfRange1f &range) const {
        return Contains(range._min) && Contains(range._max);
    }

    /// Returns true if the \p point is located inside the range. As with all
    /// operations of this type, the range is assumed to include its extrema.
    /// \deprecated Use Contains() instead.
    bool IsInside(float point) const {
        return Contains(point);
    }

    /// Returns true if the \p range is located entirely inside the range. As
    /// with all operations of this type, the ranges are assumed to include
    /// their extrema.
    /// \deprecated Use Contains() instead.
    bool IsInside(const GfRange1f &range) const {
        return Contains(range);
    }

    /// Returns true if the \p range is located entirely outside the range. As
    /// with all operations of this type, the ranges are assumed to include
    /// their extrema.
    bool IsOutside(const GfRange1f &range) const {
        return (range._max < _min || range._min > _max);
    }

    /// Returns the smallest \c GfRange1f which contains both \p a and \p b.
    static GfRange1f GetUnion(const GfRange1f &a, const GfRange1f &b) {
        GfRange1f res = a;
        _FindMin(res._min,b._min);
        _FindMax(res._max,b._max);
        return res;
    }

    /// Extend \p this to include \p b.
    const GfRange1f &UnionWith(const GfRange1f &b) {
        _FindMin(_min,b._min);
        _FindMax(_max,b._max);
        return *this;
    }

    /// Extend \p this to include \p b.
    const GfRange1f &UnionWith(float b) {
        _FindMin(_min,b);
        _FindMax(_max,b);
        return *this;
    }

    /// Returns the smallest \c GfRange1f which contains both \p a and \p b
    /// \deprecated Use GetUnion() instead.
    static GfRange1f Union(const GfRange1f &a, const GfRange1f &b) {
        return GetUnion(a, b);
    }

    /// Extend \p this to include \p b.
    /// \deprecated Use UnionWith() instead.
    const GfRange1f &Union(const GfRange1f &b) {
        return UnionWith(b);
    }

    /// Extend \p this to include \p b.
    /// \deprecated Use UnionWith() instead.
    const GfRange1f &Union(float b) {
        return UnionWith(b);
    }

    /// Returns a \c GfRange1f that describes the intersection of \p a and \p b.
    static GfRange1f GetIntersection(const GfRange1f &a, const GfRange1f &b) {
        GfRange1f res = a;
        _FindMax(res._min,b._min);
        _FindMin(res._max,b._max);
        return res;
    }

    /// Returns a \c GfRange1f that describes the intersection of \p a and \p b.
    /// \deprecated Use GetIntersection() instead.
    static GfRange1f Intersection(const GfRange1f &a, const GfRange1f &b) {
        return GetIntersection(a, b);
    }

    /// Modifies this range to hold its intersection with \p b and returns the
    /// result
    const GfRange1f &IntersectWith(const GfRange1f &b) {
        _FindMax(_min,b._min);
        _FindMin(_max,b._max);
        return *this;
    }

    /// Modifies this range to hold its intersection with \p b and returns the
    /// result.
    /// \deprecated Use IntersectWith() instead.
    const GfRange1f &Intersection(const GfRange1f &b) {
        return IntersectWith(b);
    }

    /// unary sum.
    GfRange1f &operator +=(const GfRange1f &b) {
        _min += b._min;
        _max += b._max;
        return *this;
    }

    /// unary difference.
    GfRange1f &operator -=(const GfRange1f &b) {
        _min -= b._max;
        _max -= b._min;
        return *this;
    }

    /// unary multiply.
    GfRange1f &operator *=(double m) {
        if (m > 0) {
            _min *= m;
            _max *= m;
        } else {
            float tmp = _min;
            _min = _max * m;
            _max = tmp * m;
        }
        return *this;
    }

    /// unary division.
    GfRange1f &operator /=(double m) {
        return *this *= (1.0 / m);
    }

    /// binary sum.
    GfRange1f operator +(const GfRange1f &b) const {
        return GfRange1f(_min + b._min, _max + b._max);
    }


    /// binary difference.
    GfRange1f operator -(const GfRange1f &b) const {
        return GfRange1f(_min - b._max, _max - b._min);
    }

    /// scalar multiply.
    friend GfRange1f operator *(double m, const GfRange1f &r) {
        return (m > 0 ? 
            GfRange1f(r._min*m, r._max*m) : 
            GfRange1f(r._max*m, r._min*m));
    }

    /// scalar multiply.
    friend GfRange1f operator *(const GfRange1f &r, double m) {
        return (m > 0 ? 
            GfRange1f(r._min*m, r._max*m) : 
            GfRange1f(r._max*m, r._min*m));
    }

    /// scalar divide.
    friend GfRange1f operator /(const GfRange1f &r, double m) {
        return r * (1.0 / m);
    }

    /// hash.
    friend inline size_t hash_value(const GfRange1f &r) {
        size_t h = 0;
        boost::hash_combine(h, r._min);
        boost::hash_combine(h, r._max);
        return h;
    }

    /// The min and max points must match exactly for equality.
    bool operator ==(const GfRange1f &b) const {
        return (_min == b._min && _max == b._max);
    }

    bool operator !=(const GfRange1f &b) const {
        return !(*this == b);
    }

    /// Compare this range to a GfRange1d.
    ///
    /// The values must match exactly and it does exactly what you might
    /// expect when comparing float and double values.
    GF_API inline bool operator ==(const GfRange1d& other) const;
    GF_API inline bool operator !=(const GfRange1d& other) const;

    /// Compute the squared distance from a point to the range.
    GF_API
    double GetDistanceSquared(float p) const;


  private:
    /// Minimum and maximum points.
    float _min, _max;

    /// Extends minimum point if necessary to contain given point.
    static void _FindMin(float &dest, float point) {
        if (point < dest) dest = point;
    }

    /// Extends maximum point if necessary to contain given point.
    static void _FindMax(float &dest, float point) {
        if (point > dest) dest = point;
    }
};

/// Output a GfRange1f.
/// \ingroup group_gf_DebuggingOutput
GF_API std::ostream& operator<<(std::ostream &, GfRange1f const &);

PXR_NAMESPACE_CLOSE_SCOPE
#include "pxr/base/gf/range1d.h"
PXR_NAMESPACE_OPEN_SCOPE

inline bool
GfRange1f::operator ==(const GfRange1d& other) const {
    return _min == float(other.GetMin()) &&
           _max == float(other.GetMax());
}

inline bool
GfRange1f::operator !=(const GfRange1d& other) const {
    return !(*this == other);
}


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_BASE_GF_RANGE1F_H
