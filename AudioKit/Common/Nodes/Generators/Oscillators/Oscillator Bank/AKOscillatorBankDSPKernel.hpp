//
//  AKOscillatorBankDSPKernel.hpp
//  AudioKit
//
//  Created by Aurelius Prochazka, revision history on Github.
//  Copyright © 2018 AudioKit. All rights reserved.
//

#pragma once

#import "AKBankDSPKernel.hpp"

enum {
    standardBankEnumElements()
};

class AKOscillatorBankDSPKernel : public AKBankDSPKernel, public AKOutputBuffered {
public:
    float cutoffFrequency = 22050.0f;
    float cutoffIntensity = 1.0f;

    // MARK: Types
    struct NoteState {
        NoteState* next;
        NoteState* prev;
        AKOscillatorBankDSPKernel* kernel;

        enum { stageOff, stageOn, stageRelease };
        int stage = stageOff;

        float internalGate = 0;
        float amp = 0;
        float filter = 0;

        sp_adsr *adsr;
        sp_adsr *fadsr;

        sp_osc *osc;

        sp_crossfade *filterCrossFade;
        sp_moogladder *lowPass;

        void init() {
            sp_adsr_create(&adsr);
            sp_adsr_create(&fadsr);
            sp_osc_create(&osc);
            sp_crossfade_create(&filterCrossFade);
            sp_moogladder_create(&lowPass);
            sp_adsr_init(kernel->sp, adsr);
            sp_adsr_init(kernel->sp, fadsr);
            sp_osc_init(kernel->sp, osc, kernel->ftbl, 0);
            sp_crossfade_init(kernel->sp, filterCrossFade);
            sp_moogladder_init(kernel->sp, lowPass);
            osc->freq = 0;
            osc->amp = 0;
        }


        void clear() {
            stage = stageOff;
            amp = 0;
        }

        // linked list management
        void remove() {
            if (prev) prev->next = next;
            else kernel->playingNotes = next;

            if (next) next->prev = prev;

            //prev = next = nullptr; Had to remove due to a click, potentially bad

            --kernel->playingNotesCount;

            sp_osc_destroy(&osc);
            sp_adsr_destroy(&fadsr);
            sp_adsr_destroy(&adsr);
            sp_crossfade_destroy(&filterCrossFade);
            sp_moogladder_destroy(&lowPass);
        }

        void add() {
            init();
            prev = nullptr;
            next = kernel->playingNotes;
            if (next) next->prev = this;
            kernel->playingNotes = this;
            ++kernel->playingNotesCount;
        }

        void noteOn(int noteNumber, int velocity)
        {
            noteOn(noteNumber, velocity, (float)noteToHz(noteNumber));
        }

        void noteOn(int noteNumber, int velocity, float frequency)
        {
            if (velocity == 0) {
                if (stage == stageOn) {
                    stage = stageRelease;
                    internalGate = 0;
                }
            } else {
                if (stage == stageOff) { add(); }
                osc->freq = (float)frequency;
                osc->amp = (float)pow2(velocity / 127.);
                stage = stageOn;
                internalGate = 1;
            }
        }


        void run(int frameCount, float* outL, float* outR)
        {
            float originalFrequency = osc->freq;

          const float filterCutoffFreq = kernel->cutoffFrequency;
          const float filterCutoffIntensity = kernel->cutoffIntensity;
//          const float filterCutoffIntensity = std::min(kernel->cutoffIntensity, filterCutoffFreq);
//          const float range = filterCutoffFreq - filterCutoffIntensity;

            filterCrossFade->pos = 1.0;
          lowPass->res = 0.1;

            osc->freq *= powf(2, kernel->pitchBend / 12.0);
            osc->freq = clamp(osc->freq, 2.0f, 22050.0f);
            float bentFrequency = osc->freq;

            adsr->atk = (float)kernel->attackDuration;
            adsr->dec = (float)kernel->decayDuration;
            adsr->sus = (float)kernel->sustainLevel;
            adsr->rel = (float)kernel->releaseDuration;

            fadsr->atk = (float)kernel->attackDuration / filterCutoffIntensity;
            fadsr->dec = (float)kernel->decayDuration / filterCutoffIntensity;
            fadsr->sus = (float)kernel->sustainLevel / filterCutoffIntensity;
            fadsr->rel = (float)kernel->releaseDuration / filterCutoffIntensity;

//            fadsr->atk = std::min(std::max((float)kernel->attackDuration, 0.0005f), 2.0f);
//            fadsr->dec = std::min(std::max((float)kernel->decayDuration, 0.005f), 2.0f);
//            fadsr->sus = std::min(std::max((float)kernel->sustainLevel, 0.0f), 1.0f);
//            fadsr->rel = std::min(std::max((float)kernel->releaseDuration, 0.0f), 2.0f);

//          printf("%f\n", 1.0f - filter);

            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                float x = 0;
                float depth = kernel->vibratoDepth / 12.0;
                float variation = sinf((kernel->currentRunningIndex + frameIndex) * 2 * 2 * M_PI * kernel->vibratoRate / kernel->sampleRate);
                osc->freq = bentFrequency * powf(2, depth * variation);

                sp_adsr_compute(kernel->sp, adsr, &internalGate, &amp);
                sp_adsr_compute(kernel->sp, fadsr, &internalGate, &filter);

                float filterEnvLFOMix = 1.0f;

//                printf("%f\n", ampedFilterCutoffFreq);
                float ampedFilterCutoffFreq = filterCutoffFreq - (filterCutoffFreq * (1.0f - filter));
                ampedFilterCutoffFreq = std::min(std::max(ampedFilterCutoffFreq, 0.0f), 22050.0f);
                lowPass->freq = ampedFilterCutoffFreq;

                sp_osc_compute(kernel->sp, osc, nil, &x);

                float oscOut = amp * x;

                float filterOut = 0.0f;
                sp_moogladder_compute(kernel->sp, lowPass, &oscOut, &filterOut);

                float finalOut = 0.0f;
                sp_crossfade_compute(kernel->sp, filterCrossFade, &oscOut, &filterOut, &finalOut);

                *outL++ += finalOut;
                *outR++ += finalOut;
            }
            osc->freq = originalFrequency;
            if (stage == stageRelease && amp < 0.00001) {
                clear();
                remove();
            }
        }

    };

    // MARK: Member Functions

    AKOscillatorBankDSPKernel() {
        noteStates.resize(128);
        for (NoteState& state : noteStates) {
            state.kernel = this;
        }
    }

    void setupWaveform(uint32_t size) {
        ftbl_size = size;
        sp_ftbl_create(sp, &ftbl, ftbl_size);
    }

    void setWaveformValue(uint32_t index, float value) {
        ftbl->tbl[index] = value;
    }

    void reset() {
        for (NoteState& state : noteStates) {
            state.clear();
        }
        playingNotes = nullptr;
        AKBankDSPKernel::reset();
    }

    standardBankKernelFunctions()

    void setParameter(AUParameterAddress address, AUValue value) {
        switch (address) {
                standardBankSetParameters()
        }
    }
    AUValue getParameter(AUParameterAddress address) {
        switch (address) {
                standardBankGetParameters()
        }
    }

    void startRamp(AUParameterAddress address, AUValue value, AUAudioFrameCount duration) override {
        switch (address) {
                standardBankStartRamps()
        }
    }

    standardHandleMIDI()

    void process(AUAudioFrameCount frameCount, AUAudioFrameCount bufferOffset) override {

        float* outL = (float*)outBufferListPtr->mBuffers[0].mData + bufferOffset;
        float* outR = (float*)outBufferListPtr->mBuffers[1].mData + bufferOffset;

        standardBankGetAndSteps()

        NoteState* noteState = playingNotes;
        while (noteState) {
            noteState->run(frameCount, outL, outR);
            noteState = noteState->next;
        }
        currentRunningIndex += frameCount / 2;

        for (AUAudioFrameCount i = 0; i < frameCount; ++i) {
            outL[i] *= .5f;
            outR[i] *= .5f;
        }
    }

    // MARK: Member Variables

private:
    std::vector<NoteState> noteStates;

    sp_ftbl *ftbl;
    UInt32 ftbl_size = 4096;

public:
    NoteState* playingNotes = nullptr;
};
