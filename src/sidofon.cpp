#include <iomanip>
#include "dsp/digital.hpp"

#include "plugin.hpp"
#include "sid.h"
#include "voice_regs.h"
#include "filter_regs.h"

struct Sidofon : Module {
    enum ParamIds {
        // Voice 1
        PITCH_PARAM,
        PULSE_WIDTH_PARAM,
        WAVE_TRI_PARAM,
        WAVE_SAW_PARAM,
        WAVE_PULSE_PARAM,
        WAVE_NOISE_PARAM,
        GATE_PARAM,
        SYNC_PARAM,
        RING_MOD_PARAM,
        TEST_PARAM,
        ATTACK_PARAM,
        DECAY_PARAM,
        SUSTAIN_PARAM,
        RELEASE_PARAM,
        // Filter
        FILTER_VOICE1_PARAM,
        FILTER_VOICE2_PARAM,
        FILTER_VOICE3_PARAM,
        FILTER_LO_PASS_PARAM,
        FILTER_BAND_PASS_PARAM,
        FILTER_HI_PASS_PARAM,
        FILTER_RESONANCE_PARAM,
        VOICE3OFF_PARAM,
        VOLUME_PARAM,
        // Clock
        PAL_NTSC_PARAM,
        OVERCLOCK_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        // Voice 1
        PITCH_INPUT,
        PULSE_WIDTH_INPUT,
        WAVE_TRI_INPUT,
        WAVE_SAW_INPUT,
        WAVE_PULSE_INPUT,
        WAVE_NOISE_INPUT,
        GATE_INPUT,
        SYNC_INPUT,
        RING_MOD_INPUT,
        TEST_INPUT,
        ATTACK_INPUT,
        DECAY_INPUT,
        SUSTAIN_INPUT,
        RELEASE_INPUT,
        // Filter
        FILTER_VOICE1_INPUT,
        FILTER_VOICE2_INPUT,
        FILTER_VOICE3_INPUT,
        FILTER_LO_PASS_INPUT,
        FILTER_BAND_PASS_INPUT,
        FILTER_HI_PASS_INPUT,
        FILTER_RESONANCE_INPUT,
        VOICE3OFF_INPUT,
        VOLUME_INPUT,
        // Main
        CLOCK_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_OUTPUT,
        CLOCK_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        // Voice 1
        WAVE_TRI_LIGHT,
        WAVE_SAW_LIGHT,
        WAVE_PULSE_LIGHT,
        WAVE_NOISE_LIGHT,
        GATE_LIGHT,
        SYNC_LIGHT,
        RING_MOD_LIGHT,
        TEST_LIGHT,
        ATTACK_LIGHT,
        DECAY_LIGHT,
        SUSTAIN_LIGHT,
        RELEASE_LIGHT,
        // Filter
        FILTER_VOICE1_LIGHT,
        FILTER_VOICE2_LIGHT,
        FILTER_VOICE3_LIGHT,
        FILTER_LO_PASS_LIGHT,
        FILTER_BAND_PASS_LIGHT,
        FILTER_HI_PASS_LIGHT,
        FILTER_RESONANCE_LIGHT,
        VOICE3OFF_LIGHT,
        VOLUME_LIGHT,
        NUM_LIGHTS
    };

    dsp::PulseGenerator pulseGen;
    float counter;
    float period;

    reSID::SID sid;
    float cfgSampleRate;
    reSID::cycle_count clockSteps;
    static constexpr float clockHzPAL  =  985248.0f;
    static constexpr float clockHzNTSC = 1022727.0f;
    static constexpr float clockHz = clockHzNTSC;
    static constexpr float playHz = 50.0f;
    static constexpr float triggerTime = 1e-4f;
    uint16_t oldSidReg;

    VoiceRegs voiceRegs[VoiceRegs::NUM_VOICES];
    FilterRegs filterRegs;

    Sidofon() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        // voice 1
        configParam(PITCH_PARAM, -54.0, 54.0, 0.0, "Pitch", " Hz", std::pow(2.f, 1.f/12.f), dsp::FREQ_C4, 0.f);
        configParam(PULSE_WIDTH_PARAM, -1.0, 1.0, 0.0, "Pulse Width", " %", 0.f, 100.f);
        configParam(WAVE_TRI_PARAM, 0.0f, 1.0f, 0.0f, "Triangle", "");
        configParam(WAVE_SAW_PARAM, 0.0f, 1.0f, 0.0f, "Sawtooth", "");
        configParam(WAVE_PULSE_PARAM, 0.0f, 1.0f, 0.0f, "Pulse", "");
        configParam(WAVE_NOISE_PARAM, 0.0f, 1.0f, 0.0f, "Noise", "");
        configParam(GATE_PARAM, 0.0f, 1.0f, 0.0f, "Gate", "");
        configParam(SYNC_PARAM, 0.0f, 1.0f, 0.0f, "Sync", "");
        configParam(RING_MOD_PARAM, 0.0f, 1.0f, 0.0f, "Ring Mod", "");
        configParam(TEST_PARAM, 0.0f, 1.0f, 0.0f, "Test", "");
        configParam(ATTACK_PARAM, 0.0f, 1.0f, 0.0f, "Attack", " %", 0.f, 100.f);
        configParam(DECAY_PARAM, 0.0f, 1.0f, 0.0f, "Decay", " %", 0.f, 100.f);
        configParam(SUSTAIN_PARAM, 0.0f, 1.0f, 1.0f, "Sustain", " %", 0.f, 100.f);
        configParam(RELEASE_PARAM, 0.0f, 1.0f, 0.0f, "Release", " %", 0.f, 100.f);

        // filter
        configParam(FILTER_VOICE1_PARAM, 0.0f, 1.0f, 0.0f, "Filter Voice 1");
        configParam(FILTER_VOICE2_PARAM, 0.0f, 1.0f, 0.0f, "Filter Voice 2");
        configParam(FILTER_VOICE3_PARAM, 0.0f, 1.0f, 0.0f, "Filter Voice 3");
        configParam(FILTER_LO_PASS_PARAM, 0.0f, 1.0f, 0.0f, "Low Pass Filter");
        configParam(FILTER_BAND_PASS_PARAM, 0.0f, 1.0f, 0.0f, "Band Pass Filter");
        configParam(FILTER_HI_PASS_PARAM, 0.0f, 1.0f, 0.0f, "High Pass Filter");
        configParam(FILTER_RESONANCE_PARAM, 0.0f, 15.0f, 0.0f, "Filter Resonance");
        configParam(VOICE3OFF_PARAM, 0.0f, 1.0f, 0.0f, "Voice 3 Off");
        configParam(VOLUME_PARAM, 0.0f, 15.0f, 15.0f, "Master Volume");

        // clock
        configParam(PAL_NTSC_PARAM, 0.0f, 1.0f, 0.0f, "PAL/NTSC", "");
        configParam(OVERCLOCK_PARAM, 1.f, 8.f, 1.f, "Overclock", "x");
      }

    inline uint16_t freq2sidreg(float freq)
    {
        return (uint16_t)freq * 16777216.0f / clockHz;
    }

    void reconfigureSampleRate(float sampleRate)
    {
        sid.reset();
        sid.set_sampling_parameters(clockHz, reSID::SAMPLE_FAST, sampleRate);
        
        // CPU clock steps between audio samples
        clockSteps = (reSID::cycle_count)roundf(clockHz / sampleRate);

        for(int i=0;i<VoiceRegs::NUM_VOICES;i++) {
            voiceRegs[i].reset();
        }
        filterRegs.reset();
    }

    bool getSwitchValue(int inputId, int paramId) 
    {
        float val = params[paramId].getValue();
        if(inputs[inputId].isConnected()) {
            val += inputs[inputId].getVoltage();
        }
        return val >= 1.0f;
    }

    uint8_t getADSRValue(int inputId, int paramId)
    {
        float val = params[paramId].getValue() * 15.0f;
        if(inputs[inputId].isConnected()) {
            val += inputs[inputId].getVoltage() / 10.0f;
        }
        val = clamp(val * 15.f, 0.f, 15.f);
        return (uint8_t)val;
    }

    void updateVoice(int voiceNo, int inputOffset, int paramOffset)
    {
        // update pitch
        float pitchKnob = params[PITCH_PARAM + paramOffset].getValue();
        float pitchCV = 12.f * inputs[PITCH_INPUT + inputOffset].getVoltage();
        float pitch = dsp::FREQ_C4 * std::pow(2.f, (pitchKnob + pitchCV) / 12.f);
        uint16_t pitchReg = freq2sidreg(pitch);
        voiceRegs[voiceNo].setFreq(pitchReg);

        // update gate
        bool gate = getSwitchValue(GATE_INPUT + inputOffset, GATE_PARAM + paramOffset);
        voiceRegs[voiceNo].setGate(gate);

        // update sync
        bool sync = getSwitchValue(SYNC_INPUT + inputOffset, SYNC_PARAM + paramOffset);
        voiceRegs[voiceNo].setSync(sync);

        // update ringmod
        bool ringMod = getSwitchValue(RING_MOD_INPUT + inputOffset, RING_MOD_PARAM + paramOffset);
        voiceRegs[voiceNo].setRingMod(ringMod);

        // update test
        bool test = getSwitchValue(TEST_INPUT + inputOffset, TEST_PARAM + paramOffset);
        voiceRegs[voiceNo].setTest(test);

        // update ADSR
        voiceRegs[voiceNo].setAttack(getADSRValue(ATTACK_INPUT + inputOffset, ATTACK_PARAM + paramOffset));
        voiceRegs[voiceNo].setDecay(getADSRValue(DECAY_INPUT + inputOffset, DECAY_PARAM + paramOffset));
        voiceRegs[voiceNo].setSustain(getADSRValue(SUSTAIN_INPUT + inputOffset, SUSTAIN_PARAM + paramOffset));
        voiceRegs[voiceNo].setRelease(getADSRValue(RELEASE_INPUT + inputOffset, RELEASE_PARAM + paramOffset));
    }

    void updateLights(int voiceNo, int lightOffset, uint32_t sampleTime)
    {
        VoiceRegs &regs = voiceRegs[voiceNo];

        lights[GATE_LIGHT].setSmoothBrightness(regs.getGate(), sampleTime);
        lights[SYNC_LIGHT].setSmoothBrightness(regs.getSync(), sampleTime);
        lights[RING_MOD_LIGHT].setSmoothBrightness(regs.getRingMod(), sampleTime);
        lights[TEST_LIGHT].setSmoothBrightness(regs.getTest(), sampleTime);
    }

    void process(const ProcessArgs& args) override {
        // reconfigure sid engine?
        if(cfgSampleRate != args.sampleRate) {
            cfgSampleRate = args.sampleRate;
            reconfigureSampleRate(args.sampleRate);

            // hack some voice0
            filterRegs.setVolume(FilterRegs::VOLUME_MAX);
            voiceRegs[0].setWaveform((uint8_t)VoiceRegs::WAVE_TRIANGLE);
            voiceRegs[0].setFreq(0x1cd6);
            voiceRegs[0].setSustain(VoiceRegs::SUSTAIN_MAX);
        }
    
        updateVoice(0, 0, 0);

        updateLights(0,0,args.sampleTime);

        // realize changed SID regs
        for(int i=0;i<VoiceRegs::NUM_VOICES;i++) {
            voiceRegs[i].realize(sid, i);
        }
        filterRegs.realize(sid);

        // emulate SID for some CPU clocks
        sid.clock(clockSteps);

        // retrieve SID audio sample and convert to voltage
        int16_t sample = sid.output();
        float audio = sample * 10.0f / 32768.0f;
        outputs[AUDIO_OUTPUT].setVoltage(audio);

        // set trigger for vsync Hz
        period = args.sampleRate / playHz;
        if(counter > period) {
            pulseGen.trigger(triggerTime);
            counter -= period;
        }
        counter++;
        float triggerValue = pulseGen.process(args.sampleTime);
        outputs[CLOCK_OUTPUT].setVoltage(triggerValue * 10.f);
    }
};


struct SidofonWidget : ModuleWidget {
    SidofonWidget(Sidofon* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/sidofon.svg")));

        // ----- Voice 1 -----
        // params
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(12, 26)), module, Sidofon::PITCH_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(48, 26)), module, Sidofon::PULSE_WIDTH_PARAM));

        addParam(createParamCentered<CKSS>(mm2px(Vec(24, 24)), module, Sidofon::WAVE_TRI_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(28, 24)), module, Sidofon::WAVE_SAW_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(32, 24)), module, Sidofon::WAVE_PULSE_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(36, 24)), module, Sidofon::WAVE_NOISE_PARAM));

        addParam(createParamCentered<CKSS>(mm2px(Vec(24, 38)), module, Sidofon::GATE_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(28, 38)), module, Sidofon::SYNC_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(32, 38)), module, Sidofon::RING_MOD_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(36, 38)), module, Sidofon::TEST_PARAM));

        addParam(createParamCentered<Trimpot>(mm2px(Vec(12, 48)), module, Sidofon::ATTACK_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(24, 48)), module, Sidofon::DECAY_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(36, 48)), module, Sidofon::SUSTAIN_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(48, 48)), module, Sidofon::RELEASE_PARAM));

        addParam(createParamCentered<CKSS>(mm2px(Vec(106, 262)), module, Sidofon::PAL_NTSC_PARAM));

        // light
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(17, 85)), module, Sidofon::WAVE_TRI_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(29, 85)), module, Sidofon::WAVE_SAW_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(41, 85)), module, Sidofon::WAVE_PULSE_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(53, 85)), module, Sidofon::WAVE_NOISE_LIGHT));

        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(17, 101)), module, Sidofon::GATE_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(29, 101)), module, Sidofon::SYNC_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(41, 101)), module, Sidofon::RING_MOD_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(53, 101)), module, Sidofon::TEST_LIGHT));

        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(17, 117)), module, Sidofon::ATTACK_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(29, 117)), module, Sidofon::DECAY_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(41, 117)), module, Sidofon::SUSTAIN_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(53, 117)), module, Sidofon::RELEASE_LIGHT));

        // inputs
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12, 64)), module, Sidofon::PITCH_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 64)), module, Sidofon::PULSE_WIDTH_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12, 80)), module, Sidofon::WAVE_TRI_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24, 80)), module, Sidofon::WAVE_SAW_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 80)), module, Sidofon::WAVE_PULSE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(48, 80)), module, Sidofon::WAVE_NOISE_INPUT));
        
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12, 96)), module, Sidofon::GATE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24, 96)), module, Sidofon::SYNC_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 96)), module, Sidofon::RING_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(48, 96)), module, Sidofon::TEST_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12, 112)), module, Sidofon::ATTACK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24, 112)), module, Sidofon::DECAY_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 112)), module, Sidofon::SUSTAIN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(48, 112)), module, Sidofon::RELEASE_INPUT));

        // ----- Filter -----
        // params
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(180, 26)), module, Sidofon::FILTER_RESONANCE_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(204, 26)), module, Sidofon::VOLUME_PARAM));

        addParam(createParamCentered<CKSS>(mm2px(Vec(192, 24)), module, Sidofon::VOICE3OFF_PARAM));

        addParam(createParamCentered<CKSS>(mm2px(Vec(178, 46)), module, Sidofon::FILTER_VOICE1_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(182, 46)), module, Sidofon::FILTER_VOICE2_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(186, 46)), module, Sidofon::FILTER_VOICE3_PARAM));

        addParam(createParamCentered<CKSS>(mm2px(Vec(198, 46)), module, Sidofon::FILTER_LO_PASS_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(202, 46)), module, Sidofon::FILTER_BAND_PASS_PARAM));
        addParam(createParamCentered<CKSS>(mm2px(Vec(206, 46)), module, Sidofon::FILTER_HI_PASS_PARAM));

        // light
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(185, 69)), module, Sidofon::FILTER_VOICE1_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(197, 69)), module, Sidofon::FILTER_VOICE2_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(209, 69)), module, Sidofon::FILTER_VOICE3_LIGHT));

        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(185, 85)), module, Sidofon::FILTER_LO_PASS_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(197, 85)), module, Sidofon::FILTER_BAND_PASS_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(209, 85)), module, Sidofon::FILTER_HI_PASS_LIGHT));

        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(185, 101)), module, Sidofon::FILTER_RESONANCE_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(197, 101)), module, Sidofon::VOICE3OFF_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(209, 101)), module, Sidofon::VOLUME_LIGHT));

        // inputs
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(180, 64)), module, Sidofon::FILTER_VOICE1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(192, 64)), module, Sidofon::FILTER_VOICE2_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(204, 64)), module, Sidofon::FILTER_VOICE3_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(180, 80)), module, Sidofon::FILTER_LO_PASS_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(192, 80)), module, Sidofon::FILTER_BAND_PASS_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(204, 80)), module, Sidofon::FILTER_HI_PASS_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(180, 96)), module, Sidofon::FILTER_RESONANCE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(192, 96)), module, Sidofon::VOICE3OFF_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(204, 96)), module, Sidofon::VOLUME_INPUT));

        // ----- Main -----
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(180, 112)), module, Sidofon::CLOCK_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(192, 112)), module, Sidofon::CLOCK_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(204, 112)), module, Sidofon::AUDIO_OUTPUT));
    }
};


Model* modelSidofon = createModel<Sidofon, SidofonWidget>("sidofon");