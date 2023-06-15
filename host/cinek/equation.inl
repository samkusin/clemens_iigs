//
//  equation.inl
//  SampleCommon
//
//  Created by Samir Sinha on 2/28/16.
//  Copyright © 2016 Cinekine. All rights reserved.
//

namespace cinek {

template <typename _property>
void equation<_property>::calc(property_type &out, const keyframe_type &left,
                               const keyframe_type &right, double time) const {
    if (time < left.time) {
        out = left.prop;
        return;
    }
    if (time > right.time) {
        out = right.prop;
        return;
    }

    double timeRange = right.time - left.time;
    double timeIn = time - left.time;
    if (timeRange < timeIn)
        timeIn = timeRange;

    auto scalar = timeIn / timeRange;

    switch (type) {
    case transition_type::kLinear:
        break;
    case transition_type::kEaseIn: {
        scalar = -(scalar * (scalar - 2));
    } break;
    case transition_type::kEaseOut: {
        scalar = scalar * scalar;
    } break;
    case transition_type::kEase: {
        //  scalar = (2 * scalar) or (2*t/T)
        float scalarX2 = 2.f * scalar;
        if (scalarX2 < 1.f) {
            scalar = 0.5f * scalarX2 * scalarX2;
        } else {
            scalarX2 -= 1.f;
            scalar = 0.5f * (scalarX2 * (scalarX2 - 2) - 1);
        }
    } break;
    case transition_type::kEaseInCubic: {
        float scalarMin1 = scalar - 1;
        scalar = scalarMin1 * scalarMin1 * scalarMin1 + 1;
    } break;
    case transition_type::kEaseOutCubic: {
        scalar = scalar * scalar * scalar;
    } break;
    case transition_type::kEaseCubic: {
        //  scalar = (2 * scalar) or (2*t/T)
        float scalarX2 = 2.f * scalar;
        if (scalarX2 < 1.f) {
            scalar = 0.5f * scalarX2 * scalarX2 * scalarX2;
        } else {
            scalarX2 -= 2.f;
            scalar = 0.5f * (scalarX2 * scalarX2 * scalarX2 + 2);
        }
    } break;
    case transition_type::kSine: {
        scalar = -0.5f * (std::cosf(M_PI * scalar) - 1);
    } break;
    default:
        break;
    };

    tweenProperty<_property>(out, left, right, scalar);
}

template <typename _property>
void tweenProperty(_property &out, const keyframe<_property> &left,
                   const keyframe<_property> &right, double scalar) {
    out = left.prop + (right.prop - left.prop) * scalar;
}

} // namespace cinek
