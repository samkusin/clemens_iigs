//
//  sequence.hpp
//  SampleCommon
//
//  Created by Samir Sinha on 3/1/16.
//  Copyright © 2016 Cinekine. All rights reserved.
//

#ifndef CINEK_MATH_SEQUENCE_HPP
#define CINEK_MATH_SEQUENCE_HPP

#include "equation.hpp"
#include "keyframe.hpp"
#include <vector>

namespace cinek {

template <typename _PropertyId, typename _Keyframe> class sequence {
  public:
    typedef _Keyframe keyframe_type;
    typedef transition transition_type;
    typedef typename keyframe_type::property_type property_type;
    typedef _PropertyId property_key_type;
    typedef equation<property_type> equation_type;

    sequence();

    sequence(const property_key_type &propId, int kfcount);

    sequence(const property_key_type &propId, double startTime, const keyframe_type &kfA,
             const keyframe_type &kfB, transition_type trans, int kfcount);

    sequence(const sequence &other) = delete;
    sequence &operator=(const sequence &seq) = delete;

    sequence(sequence &&other);
    sequence &operator=(sequence &&other);

    void insertKeyframe(const keyframe_type &kf, transition_type transIn);
    void insertKeyframeEquation(const keyframe_type &kf, equation_type &eq);

    void setEndTransition(transition_type transIn);

    /// Calculates the sequence's animated property based on the specified
    /// time.
    /// @param  prop  The output property name and value
    /// @param  time  The time value following the sequence's start time -
    ///               that is, 'time' and AnimationSequence::startTime()
    ///               should come from the same source.  It is not time
    ///               relative to startTime but an absolute value.
    /// @return If True, indicates sequence has completed
    bool calcPropertyAtTime(property_type &prop, double time) const;

    const property_key_type &propertyId() const { return _propertyId; }

    double startTime() const { return _startTime; }

  private:
    property_key_type _propertyId;

    std::vector<keyframe_type> _frames;
    std::vector<equation_type> _transitions;

    double _startTime;
};

} // namespace cinek

#endif
