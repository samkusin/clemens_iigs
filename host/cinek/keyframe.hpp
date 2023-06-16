//
//  keyframe.hpp
//  SampleCommon
//
//  Created by Samir Sinha on 2/25/16.
//  Copyright © 2016 Cinekine. All rights reserved.
//

#ifndef CINEK_MATH_KF_HPP
#define CINEK_MATH_KF_HPP

namespace cinek {

enum class transition {
    kDefault,
    kLinear,
    kEase,
    kEaseIn,
    kEaseOut,
    kEaseCubic,
    kEaseInCubic,
    kEaseOutCubic,
    kSine
};

/// The keyframe class defines a data point within a sequence
///
template <typename _property> struct keyframe {
    typedef _property property_type;

    property_type prop;

    double time;

    keyframe(const property_type &property, float t) : prop(property), time(t) {}
    keyframe() : prop(), time(0.f) {}
};

template <typename _property>
bool operator<(const keyframe<_property> &a, const keyframe<_property> &b) {
    return a.time < b.time;
}
template <typename _property>
bool operator==(const keyframe<_property> &a, const keyframe<_property> &b) {
    return a.time == b.time;
}

} // namespace cinek

#endif /* keyframe_h */
