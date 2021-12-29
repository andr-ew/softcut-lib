//
// Created by ezra on 12/6/17.
//
#include <cmath>
#include <limits>


#include "softcut/Interpolate.h"
#include "softcut/Resampler.h"

#include "softcut/ReadWriteHead.h"

using namespace softcut;
using namespace std;

void ReadWriteHead::init(FadeCurves *fc) {
    start = 0.f;
    end = 0.f;
    active = 0;
    rate = 1.f;
    setFadeTime(0.1f);
    //testBuf.init();
    queuedCrossfade = 0;
    queuedCrossfadeFlag = false;
    head[0].init(fc);
    head[1].init(fc);
    // subWriteEnable[0]=true;
    // subWriteEnable[1]=true;
}

ReadWriteHead::LoopState::LoopState() { 
    subWriteEnable[0] = true;
    subWriteEnable[1] = true;
    pingPongFlag = false;
    pingPongHead = -1;
    recOnceFlag = false;
    recOnceHead = -1;
}

void ReadWriteHead::processSample(sample_t in, sample_t *out) {
    *out = mixFade(head[0].peek(), head[1].peek(), head[0].fade(), head[1].fade());

    BOOST_ASSERT_MSG(!(head[0].state_ == Playing && head[1].state_ == Playing), "multiple active heads");

    // if (recOnceFlag || recOnceDone || (recOnceHead > -1)) {
    //     if (recOnceHead > -1) {
    //         head[recOnceHead].poke(in, pre, rec);
    //     }
    // } else {
    //     head[0].poke(in, pre, rec);
    //     head[1].poke(in, pre, rec);
    // }

    for (int i=0; i<2; ++i) { 
        if (loopState.subWriteEnable[i]) { head[i].poke(in, pre,  rec); }
    }
    
    takeAction(head[0].updatePhase(start, end, loopFlag), 0);
    takeAction(head[1].updatePhase(start, end, loopFlag), 1);

    head[0].updateFade(fadeInc);
    head[1].updateFade(fadeInc);

    dequeueCrossfade();
}


void ReadWriteHead::processSampleNoRead(sample_t in, sample_t *out) {
    (void)out;

    BOOST_ASSERT_MSG(!(head[0].state_ == Playing && head[1].state_ == Playing), "multiple active heads");

    // if (recOnceFlag || recOnceDone || (recOnceHead > -1)) {
    //     if (recOnceHead > -1) {
    //         head[recOnceHead].poke(in, pre, rec);
    //     }
    // } else {
    //     head[0].poke(in, pre, rec);
    //     head[1].poke(in, pre, rec);
    // }

    for (int i=0; i<2; ++i) { 
        if (loopState.subWriteEnable[i]) { head[i].poke(in, pre,  rec); }
    }

    takeAction(head[0].updatePhase(start, end, loopFlag), 0);
    takeAction(head[1].updatePhase(start, end, loopFlag), 1);

    head[0].updateFade(fadeInc);
    head[1].updateFade(fadeInc);

    dequeueCrossfade();
}

void ReadWriteHead::processSampleNoWrite(sample_t in, sample_t *out) {
    (void)in;
    *out = mixFade(head[0].peek(), head[1].peek(), head[0].fade(), head[1].fade());

    BOOST_ASSERT_MSG(!(head[0].state_ == Playing && head[1].state_ == Playing), "multiple active heads");

    takeAction(head[0].updatePhase(start, end, loopFlag), 0);
    takeAction(head[1].updatePhase(start, end, loopFlag), 1);

    head[0].updateFade(fadeInc);
    head[1].updateFade(fadeInc);
    
    dequeueCrossfade();
}

void ReadWriteHead::setRate(rate_t x)
{
    rate = x;
    calcFadeInc();
    head[0].setRate(x);
    head[1].setRate(x);
}

void ReadWriteHead::setLoopStartSeconds(float x)
{
    start = x * sr;
    queuedCrossfadeFlag = false;
}

void ReadWriteHead::setLoopEndSeconds(float x)
{
    end = x * sr;
    queuedCrossfadeFlag = false;
}

void ReadWriteHead::takeAction(Action act, int head)
{
    switch (act) {
        case Action::LoopPos:
            if (loopState.checkPingPong(head)) {
                enqueueCrossfade(start);
            } else {  
                enqueueCrossfade(end);
            }

            break;
        case Action::LoopNeg:
            if (loopState.checkPingPong(head)) {
                enqueueCrossfade(start);
            } else {
                enqueueCrossfade(end);
            }
            break;
        case Action::Stop:
            break;
        case Action::None:
        default: ;;
    }
}

void ReadWriteHead::LoopState::checkRecOnce(int activeHead) { 
    if (recOnceFlag) { 
        // record-once was requested;
        // time to activate the other head for writing
        recOnceFlag = false;
        recOnceHead = activeHead ^ 1;
        subWriteEnable[recOnceHead] = true;
    }

    else if (recOnceHead == activeHead) {
        // the active head is currently recording once.
        // we want to ensure the other head is disabled for write,
        // and keep this one active as it fades out
        subWriteEnable[activeHead^1] = false;
    } 
    
    else if (recOnceHead != -1) { 
        // the other head is recording once;
        // since it's not active, it must be done fading out,
        // so it's time to disable it
        subWriteEnable[recOnceHead] = false;
        recOnceHead = -1;
    }       
}


bool ReadWriteHead::LoopState::checkPingPong(int activeHead) { 
    if (pingPongFlag) { 
        if (pingPongHead == -1) {
            // we are in pingpong mode, last loop not reversed
            // so next loop should be reversed
            pingPongHead = activeHead ^ 1;
            return true;
        } else {
            // in pingpong mode, with some subhead currently reversed (presumably the active one.)
            // next loop should be normal playback
            pingPongHead = -1;
            return false;
        }
    }
    return false;
}

// int ReadWriteHead::LoopState::handleAction(Action act, int head) { 
//     int res = 0;
//     switch (act) {
//         case Action::LoopPos:
//             checkRecOnce(head);
//             res = 1;
//             break;
//         case Action::LoopNeg:
//             checkRecOnce(head);
//             res = -1;
//             break;
//         case Action::Stop:
//             break;
//         case Action::None:
//         default: ;;
//     }
//     return res;
// }


void ReadWriteHead::enqueueCrossfade(phase_t pos) {
    queuedCrossfade = pos;
    queuedCrossfadeFlag = true;
}

void ReadWriteHead::dequeueCrossfade() {
    State s = head[active].state();
    if(! (s == State::FadeIn || s == State::FadeOut)) {
	if(queuedCrossfadeFlag ) {
	    cutToPhase(queuedCrossfade);
	}
	queuedCrossfadeFlag = false;
    }
}

void ReadWriteHead::cutToPhase(phase_t pos) {
    State s = head[active].state();

    if(s == State::FadeIn || s == State::FadeOut) {
	// should never enter this condition
	// std::cerr << "badness! we performed a cut while still fading" << std::endl;
	return;
    }

    loopState.checkRecOnce(active);

    // activate the inactive head
    int newActive = active ^ 1;
    if(s != State::Stopped) {
        head[active].setState(State::FadeOut);
    }

    // if (recOnceHead==newActive) {
    //     recOnceHead = -1;
    //     recOnceDone = true;
    // }
    // if (recOnceFlag) {
    //     recOnceFlag = false;
    //     recOnceHead = newActive;
    // }

    head[newActive].setState(State::FadeIn);
    head[newActive].setPhase(pos);

    if (loopState.pingPongHead == newActive) {
        head[newActive].setRateSign(-1);
    } else {
        head[newActive].setRateSign(1);
    }

    head[active].active_ = false;
    head[newActive].active_ = true;
    active = newActive;
}

void ReadWriteHead::setFadeTime(float secs) {
    fadeTime = secs;
    calcFadeInc();
}

void ReadWriteHead::calcFadeInc() {
    fadeInc = (float) fabs(rate) / std::max(1.f, (fadeTime * sr));
    fadeInc = std::max(0.f, std::min(fadeInc, 1.f));
}

void ReadWriteHead::setBuffer(float *b, uint32_t bf) {
    buf = b;
    head[0].setBuffer(b, bf);
    head[1].setBuffer(b, bf);
}

void ReadWriteHead::setLoopFlag(bool val) {
    loopFlag = val;
}

void ReadWriteHead::setRecOnceFlag(bool val) {
    // recOnceFlag = val;
    // recOnceDone = false;
    // recOnceHead = -1;
    loopState.recOnceFlag = val;
}

// bool ReadWriteHead::getRecOnceDone() {
//   return recOnceDone;
// }

// bool ReadWriteHead::getRecOnceActive() {
//   return (recOnceDone || recOnceFlag || recOnceHead>-1);
// }

void ReadWriteHead::setSampleRate(float sr_) {
    sr = sr_;
    head[0].setSampleRate(sr);
    head[1].setSampleRate(sr);
}

sample_t ReadWriteHead::mixFade(sample_t x, sample_t y, float a, float b) {
        return x * sinf(a * (float)M_PI_2) + y * sinf(b * (float) M_PI_2);
}

void ReadWriteHead::setRec(float x) {
    rec = x;
}

void ReadWriteHead::setPre(float x) {
    pre = x;
}

phase_t ReadWriteHead::getActivePhase() {
  return head[active].phase();
}

void ReadWriteHead::cutToPos(float seconds) {
    auto s = head[active].state();
    if (s == State::FadeIn || s == State::FadeOut) {	
	    enqueueCrossfade(seconds * sr);
    } else {
	    cutToPhase(seconds * sr);
    }
}

rate_t ReadWriteHead::getRate() {
    return rate;
}

void ReadWriteHead::setRecOffsetSamples(int d) {
    head[0].setRecOffsetSamples(d);
    head[1].setRecOffsetSamples(d);
}

void ReadWriteHead::stop() {
    head[0].setState(State::Stopped);
    head[1].setState(State::Stopped);
}

void ReadWriteHead::run() {
    head[active].setState(State::Playing);
}

void ReadWriteHead::enableWrite() {
    loopState.subWriteEnable[0] = true;
    loopState.subWriteEnable[1] = true;
}