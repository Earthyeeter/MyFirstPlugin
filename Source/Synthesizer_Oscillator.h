//
// Created by trist on 3/23/2026.
//
#pragma once


#ifndef MYFIRSTPLUGIN_SYNTHESIZER_OSCILLATOR_H
#define MYFIRSTPLUGIN_SYNTHESIZER_OSCILLATOR_H

#include <ranges>
#include <juce_audio_devices/audio_io/juce_AudioIODevice.h>
#include <juce_audio_devices/midi_io/juce_MidiMessageCollector.h>
#include <juce_audio_devices/sources/juce_AudioSourcePlayer.h>



namespace synthesizer_oscillator
{
  using namespace juce;


  // global function (in this namespace) to add a sample by channel to the output buffer
  static void g_addSampleByChannel(AudioBuffer<float>& outputBuffer, const auto renderedSample, const int sampleIndex)
  {
    using namespace std::views;

    // addSample() is used instead of setSample() because we want polyphony!!
      // setSample() would always cause your synthesizer to be monophonic no matter how many voices you add
    for (const auto channelIndex : iota(0 ,outputBuffer.getNumChannels()))
      outputBuffer.addSample(channelIndex, sampleIndex, static_cast<float>(renderedSample));
  }


  // Describe the sounds being played
  struct sine_wave_sound : SynthesiserSound // sine wave sound DERIVES from synthesizer sound
  {
    bool appliesToNote(int /*noteNumber*/) override {return true;}
    bool appliesToChannel(int /*channelNumber*/) override {return true;}
  };
  struct saw_wave_sound : sine_wave_sound {};

  // Define how the sounds are actually played
  class sine_wave_voice final : public SynthesiserVoice
  {
    double current_phase = 0.0;
    double phase_change = 0.0;
    double level = 0.0;
    double tail_off = 0.0;
    double current_frequency = 0.0;
  public:

    bool canPlaySound(SynthesiserSound* sound) override
    {
      return dynamic_cast<sine_wave_sound*>(sound) != nullptr;
    }

    void startNote(const int midiNoteNumber, const float velocity, SynthesiserSound* /*sound*/, int /*currentPitchWheelPosition*/) override
    {
      level = static_cast<double>(velocity) * 0.15;
      tail_off = 0.0;

      current_frequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
      const auto cyclesPerSample = current_frequency / getSampleRate();
      phase_change = cyclesPerSample * MathConstants<double>::twoPi;
    }

    void stopNote(const float /*velocity*/, const bool allowTailOff) override
    {
      if (allowTailOff)
      {
        if (approximatelyEqual(tail_off, 0.0))
          tail_off = 1.0;
      }
      else
      {
        clearCurrentNote();
        phase_change = 0.0;
      }
    }

    static auto renderSineSample(const double current_phase, const double level, const double tail_off)
      {return std::sin(current_phase) * level * tail_off;}

    static auto renderSineSample(const double current_phase, const double level)
      {return std::sin(current_phase) * level;}

    void renderNextBlock (AudioBuffer<float>& outputBuffer, int sampleIndex, int numSamples) override
    {
      if (!approximatelyEqual(phase_change,0.0)) // good: do allat
      {
        if (!approximatelyEqual(tail_off, 0.0)) // Calculate the tail of audio
        {
          while (--numSamples >= 0) // go down for every sample
          {
            g_addSampleByChannel(outputBuffer, renderSineSample(current_phase, level, tail_off), sampleIndex);

            current_phase += phase_change; // change angle duh
            ++sampleIndex;

            tail_off *= 0.99; // just a lil off it

            if (tail_off <= 0.005) // no more sound!!!
            {
              clearCurrentNote();
              phase_change = 0.0;
              break;
            }
          }
        }
        else
        {
          while (--numSamples >= 0)
          {
            g_addSampleByChannel(outputBuffer, renderSineSample(current_phase, level), sampleIndex);

            current_phase += phase_change;
            ++sampleIndex;
          }
        }
      }
    }

    void pitchWheelMoved(int /*newPitchWheelValue*/) override {}

    void controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) override {}

  };

  class saw_wave_voice final : public SynthesiserVoice
  {
    double level = 0.0;
    double tail_off = 0.0;
    double current_frequency = 0.0;
  public:

    bool canPlaySound(SynthesiserSound* sound) override
    {
      return dynamic_cast<sine_wave_sound*>(sound) != nullptr;
    }

    void startNote(const int midiNoteNumber, const float velocity, SynthesiserSound* /*sound*/, int /*currentPitchWheelPosition*/) override
    {
      level = static_cast<double>(velocity) * 0.15;
      tail_off = 0.0;

      current_frequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    }

    void stopNote(const float /*velocity*/, const bool allowTailOff) override
    {
      if (allowTailOff)
      {
        if (approximatelyEqual(tail_off, 0.0))
          tail_off = 1.0;
      }
      else
        clearCurrentNote();
    }

    static double moduloOfOne(const double dividend)
      {return dividend - static_cast<int>(dividend);}


    // Renders the saw sample WITH a tail
    static auto renderSawSample(const int sampleIndex, const double current_frequency, const double level, const double tail_off)
      {return (2 * moduloOfOne(current_frequency*sampleIndex) * level - level) * tail_off;}


    // render the saw sample WITHOUT a tail
    static auto renderSawSample(const int sampleIndex, const double current_frequency, const double level)
      {return (2 * moduloOfOne(current_frequency*sampleIndex) * level - level);}

    void renderNextBlock(AudioBuffer<float>& outputBuffer, int sampleIndex, int numSamples) override
    {
      using namespace std::views;

      while (--numSamples >= 0) // go down for every sample until the next block
      {
        // just to be safe. I could use tail off > 0, but I don't see a reason why either method is better
        // SO I'm just using the safe way out!!!
        if (!approximatelyEqual(tail_off, 0.0))
        {
          g_addSampleByChannel(outputBuffer,
            renderSawSample(sampleIndex, current_frequency, level, tail_off), sampleIndex);
          ++sampleIndex;

          // Lower the tail_off
          tail_off *=0.99;

          // clear the note if tail off is at an insignificant value
          if (tail_off <= 0.005)
          {
            clearCurrentNote();
            break;
          }
        }
        else
        {
          g_addSampleByChannel(outputBuffer,
            renderSawSample(sampleIndex, current_frequency, level), sampleIndex);
          ++sampleIndex;
        }
      }
    }

    void pitchWheelMoved(int /*newPitchWheelValue*/) override {}

    void controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) override {}
  };

  struct SynthesizerAudioSource final : AudioSource
  {

    // Create our Synthesizer!!!
    Synthesiser synthesizer;
    // Create the Midi Keyboard that'll appear on screen
    MidiKeyboardState& midiKeyboardState;
    // Create the collector for all of our Midi messages
    MidiMessageCollector midiCollector;
    enum oscillatorType{sine, saw};

    explicit SynthesizerAudioSource(MidiKeyboardState& keyboard_state, const oscillatorType oscType) : midiKeyboardState(keyboard_state)
    {
      switch (oscType)
      {
        case sine:
          for (int i = 0; i < 8; i++)
            {synthesizer.addVoice(new sine_wave_voice());}
          useSineWaveSound();
          break;
        case saw:
          for (int i = 0; i < 8; i++)
            {synthesizer.addVoice(new saw_wave_voice());}
          useSawWaveSound();
          break;
        default:
          for (int i = 0; i < 8; i++)
            synthesizer.addVoice(new sine_wave_voice());

          useSineWaveSound();
          break;
      }
    }

    void useSineWaveSound()
    {
      synthesizer.clearSounds();
      synthesizer.addSound(new sine_wave_sound());
    }

    void useSawWaveSound()
    {
      synthesizer.clearSounds();
      synthesizer.addSound(new saw_wave_sound());
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, const double sampleRate) override
    {
      midiCollector.reset(sampleRate);
      synthesizer.setCurrentPlaybackSampleRate(sampleRate);
    }

    void releaseResources() override{}

    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override
    {
      //clear the audio source buffer
      bufferToFill.clearActiveBufferRegion();


      // clear the midi Buffer
      MidiBuffer incomingMidi;
      midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples);

      // change the keyboard state depending on incoming midi starting on sample 0
      midiKeyboardState.processNextMidiBuffer(incomingMidi, 0, bufferToFill.numSamples, true);

      //process the midi events and output
      synthesizer.renderNextBlock(*bufferToFill.buffer, incomingMidi, 0, bufferToFill.numSamples);
    }
  };

  class SynthesizerAudioDeviceCallback final : public AudioIODeviceCallback
  {
  AudioSourcePlayer audioSourcePlayer;

  public:
    SynthesizerAudioDeviceCallback( const float *const * inputChannelData,
    int numInputChannels,
    float *const *  outputChannelData,
    int numOutputChannels,
    int numSamples,
    const AudioIODeviceCallbackContext & 	context);

    void audioDeviceAboutToStart(AudioIODevice* device) override
    {
      audioSourcePlayer.audioDeviceAboutToStart(device);
    }
    void audioDeviceStopped() override
    {
      audioSourcePlayer.audioDeviceStopped();
    }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
      const int numInputChannels,
      float* const* outputChannelData,
      const int numOutputChannels,
      const int numSamples,
      const AudioIODeviceCallbackContext& context) override
    {
      audioSourcePlayer.audioDeviceIOCallbackWithContext(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, context);
    }
  };

}




#endif //MYFIRSTPLUGIN_SYNTHESIZER_OSCILLATOR_H