//
//  equation.hpp
//  SampleCommon
//
//  Created by Samir Sinha on 2/27/16.
//  Copyright © 2016 Cinekine. All rights reserved.
//

#ifndef CINEK_MATH_EQUATION_HPP
#define CINEK_MATH_EQUATION_HPP

#include "keyframe.hpp"

namespace cinek {

template <typename _property> struct equation {
    typedef keyframe<_property> keyframe_type;
    typedef typename keyframe_type::property_type property_type;
    typedef transition transition_type;
    transition_type type;
    equation(transition_type t = transition_type::kDefault) : type(t) {}
    void calc(property_type &out, const keyframe_type &left, const keyframe_type &right,
              double time) const;
};

template <typename _property>
void tweenProperty(_property &out, const keyframe<_property> &left,
                   const keyframe<_property> &right, double scalar);

} // namespace cinek

#endif
