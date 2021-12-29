//
// Created by ezra on 12/6/17.
//

#ifndef _SOFTCUT_READWRITEHEAD_H_
#define _SOFTCUT_READWRITEHEAD_H_

#include <cstdint>

#include "FadeCurves.h"
#include "SubHead.h"
#include "TestBuffers.h"
#include "Types.h"

namespace softcut {

class ReadWriteHead {

    struct LoopState {
        bool recOnceFlag; // set to record one full loop
        int recOnceHead;  // keeps track of which subhead is recording once

        bool pingPongFlag;   // set to enable pingpong looping
        int pingPongHead;    // keeps track of which subhead is reversed in pingpong mode

        bool subWriteEnable[2];

        //-------------------------
    
        LoopState();

        // call before a position change to update record-once status
        void checkRecOnce(int activeHead);

        // check the state of ping-pong logic.
        // return true if the next loop positions should be reversed.
        bool checkPingPong(int head);
    };

  public:
    void init(FadeCurves *fc);

    // per-sample update functions
    void processSample(sample_t in, sample_t *out);
    void processSampleNoRead(sample_t in, sample_t *out);
    void processSampleNoWrite(sample_t in, sample_t *out);

    void setSampleRate(float sr);
    void setBuffer(sample_t *buf, uint32_t size);
    void setRate(rate_t x);

    // set loop (region) start point in seconds
    void setLoopStartSeconds(float x);
    // set loop (region) end point in seconds
    void setLoopEndSeconds(float x);
    // set crossfade time in seconds
    void setFadeTime(float secs);
    // enable or disable looping between region endpoints
    void setLoopFlag(bool val);

    void setRecOnceFlag(bool val);
    bool getRecOnceDone();
    bool getRecOnceActive();

    // set amplitudes
    void setRec(float x);
    void setPre(float x);

    // enqueue a position change with crossfade
    void cutToPos(float seconds);

    // immediately put both subheads in stopped state
    void stop();
    // immediately start playing  from current position (no fadein)
    void run();
    // (re-)enable both subheads for writing
    void enableWrite();

    // set delay between read and write heads, in samples
    // (negative offset means write head is placed before read head; this is the default)
    void setRecOffsetSamples(int d);

    phase_t getActivePhase();
    rate_t getRate();


  protected:
    friend class SubHead;

  private:
    // fade in to new position (given in samples)
    // assumption: phase is in range!
    void cutToPhase(phase_t newPhase);
    void enqueueCrossfade(phase_t newPhase);
    void dequeueCrossfade();
    void takeAction(Action act, int head);

    sample_t mixFade(sample_t x, sample_t y, float a, float b); // mix two inputs with phases
    void calcFadeInc();

  private:
    SubHead head[2];

    sample_t *buf; // audio buffer (allocated elsewhere)
    float sr;      // sample rate
    phase_t start; // start/end points
    phase_t end;
    phase_t queuedCrossfade;
    bool queuedCrossfadeFlag;

    float fadeTime; // fade time in seconds
    float fadeInc;  // linear fade increment per sample

    int active;    // current active play head index (0 or 1)
    bool loopFlag; // set to loop, unset for 1-shot
    float pre;     // pre-record level
    float rec;     // record level

    LoopState loopState;

    rate_t rate; // current rate
    TestBuffers testBuf;
};
} // namespace softcut
#endif // _SOFTCUT_READWRITEHEAD_H_
