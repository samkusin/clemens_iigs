//
//  sequence.inl
//  SampleCommon
//
//  Created by Samir Sinha on 3/1/16.
//  Copyright © 2016 Cinekine. All rights reserved.
//

namespace cinek {

template <typename _PropertyId, typename _Keyframe>
sequence<_PropertyId, _Keyframe>::sequence()
    : _propertyId(), _frames(), _transitions(), _startTime(0) {}

template <typename _PropertyId, typename _Keyframe>
sequence<_PropertyId, _Keyframe>::sequence(const property_key_type &propId, int kfcount)
    : _propertyId(propId), _frames(), _transitions(), _startTime(0) {
    _frames.reserve(kfcount);
    _transitions.reserve(_frames.capacity() * 2 - 1);
}

template <typename _PropertyId, typename _Keyframe>
sequence<_PropertyId, _Keyframe>::sequence(const property_key_type &propId, double startTime,
                                           const keyframe_type &kfA, const keyframe_type &kfB,
                                           transition_type trans, int kfcount)
    : _propertyId(propId), _frames(), _transitions(), _startTime(startTime) {
    _frames.reserve(kfcount);
    _transitions.reserve(_frames.capacity() * 2 - 1);

    _frames.emplace_back(kfA);

    insertKeyframe(kfB, trans);
}

template <typename _PropertyId, typename _Keyframe>
sequence<_PropertyId, _Keyframe>::sequence(sequence<_PropertyId, _Keyframe> &&other)
    : _propertyId(other._propertyId), _frames(std::move(other._frames)),
      _transitions(std::move(other._transitions)), _startTime(other._startTime) {
    other._propertyId = property_key_type();
    other._startTime = double(0);
}

template <typename _PropertyId, typename _Keyframe>
auto sequence<_PropertyId, _Keyframe>::sequence::operator=(sequence<_PropertyId, _Keyframe> &&other)
    -> sequence<_PropertyId, _Keyframe> & {
    _propertyId = other._propertyId;
    _frames = std::move(other._frames);
    _transitions = std::move(other._transitions);
    _startTime = other._startTime;
    other._startTime = double(0);
    other._propertyId = property_key_type();
    return *this;
}

template <typename _PropertyId, typename _Keyframe>
void sequence<_PropertyId, _Keyframe>::insertKeyframe(const keyframe_type &kfToAdd,
                                                      transition_type transIn) {
    equation_type eq;
    eq.type = transIn != transition_type::kDefault ? transIn : transition_type::kLinear;

    insertKeyframeEquation(kfToAdd, eq);
}

template <typename _PropertyId, typename _Keyframe>
void sequence<_PropertyId, _Keyframe>::insertKeyframeEquation(const keyframe_type &kfToAdd,
                                                              equation_type &eq) {
    auto kfIt = _frames.begin();

    if (!_frames.empty()) {
        auto kfTransIt = _transitions.begin();

        for (; kfIt != _frames.end(); ++kfIt) {
            keyframe_type &kf = *kfIt;
            if (kfToAdd == kf) {
                if (kfTransIt != _transitions.end())
                    *kfTransIt = eq;
                kf = kfToAdd;
                return;
            } else if (kfToAdd < kf) {
                break;
            }
            //  only advance transition iterator if we've cleared the first
            //  keyframe
            if (kfIt != _frames.begin() && kfTransIt != _transitions.end())
                ++kfTransIt;
        }

        _transitions.insert(kfTransIt, eq);
    }
    _frames.emplace(kfIt, kfToAdd);
}

template <typename _PropertyId, typename _Keyframe>
void sequence<_PropertyId, _Keyframe>::setEndTransition(transition_type transIn) {
    if (_transitions.empty())
        return;
    equation_type eq;
    eq.type = transIn;
    _transitions.back() = eq;
}

template <typename _PropertyId, typename _Keyframe>
bool sequence<_PropertyId, _Keyframe>::calcPropertyAtTime(property_type &prop, double time) const {
    prop = property_type();

    float timeInSeq = time - _startTime;
    bool end = false;
    for (uint32_t i = 0; i < _frames.size() - 1; ++i) {
        const keyframe_type &kfA = _frames[i];
        const keyframe_type &kfB = _frames[i + 1];
        if ((i + 1) >= (_frames.size() - 1)) {
            //  last keyframe, time overflow
            if (kfB.time < timeInSeq)
                timeInSeq = kfB.time;
            if (timeInSeq >= kfB.time)
                end = true;
        }

        _transitions[i].calc(prop, kfA, kfB, timeInSeq);
        //  overlapping keyframes are not allow.  so this check
        //  ensures that later keyframes do not overwrite the currently
        //  active keyframe
        if (timeInSeq < kfB.time)
            break;
    }

    return !end;
}

} // namespace cinek
