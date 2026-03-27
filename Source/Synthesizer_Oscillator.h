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

#include "../cmake-build-debug/_deps/juce-src/modules/juce_graphics/fonts/harfbuzz/hb-ot-math-table.hh"


namespace synthesizer_oscillator
{
  using namespace juce;

  // Describe the sounds being played
  struct sine_wave_sound : SynthesiserSound // sine wave sound DERIVES from synthesizer sound
  {
    bool appliesToNote(int noteNumber) override {return true;}
    bool appliesToChannel(int channelNumber) override {return true;}
  };
  struct saw_wave_sound : sine_wave_sound {};

  // Define how the sounds are actually played
  class sine_wave_voice : public SynthesiserVoice
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

    void startNote(int midiNoteNumber, const float velocity, SynthesiserSound* sound, int currentPitchWheelPosition) override
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

    void renderNextBlock (AudioBuffer<float>& outputBuffer, int sampleIndex, int numSamples) override
    {
      if (phase_change != 0.0) // good: do allat
      {
        if (tail_off > 0.0) // Calculate the tail of audio
        {
          while (--numSamples >= 0) // go down for every sample
          {
            const auto renderedSample = std::sin (current_phase) * level * tail_off; // current sample = sine(angle) at volume * tail off AKA decrement volume

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;) // for every channel.... add the sample to the starting sample
              outputBuffer.addSample (i, sampleIndex, static_cast<float>(renderedSample));

            current_phase += phase_change; // change angle duh
            ++sampleIndex; // go to next sample

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
            const auto renderedSample = std::sin(current_phase) * level;

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
              outputBuffer.addSample (i, sampleIndex, static_cast<float>(renderedSample));

            current_phase += phase_change;
            ++sampleIndex;
          }
        }
      }
    }

    void pitchWheelMoved(int newPitchWheelValue) override {}

    void controllerMoved(int controllerNumber, int newControllerValue) override {}

  };

  class saw_wave_voice : public SynthesiserVoice
  {
    double level = 0.0;
    double tail_off = 0.0;
    double current_frequency = 0.0;
  public:

    bool canPlaySound(SynthesiserSound* sound) override
    {
      return dynamic_cast<sine_wave_sound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, const float velocity, SynthesiserSound* sound, int currentPitchWheelPosition) override
    {
      level = static_cast<double>(velocity) * 0.15;
      tail_off = 0.0;
      midiNoteNumber = getCurrentlyPlayingNote();

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

    static double moduloOfOne(const double dividend){return dividend - static_cast<int>(dividend);}

    static void renderSawSample(AudioBuffer<float>& outputBuffer, const int sampleIndex, const double current_frequency, const double level, const double tail_off)
    {
      using namespace std::views;

        const auto renderedSample = (2 * moduloOfOne(current_frequency*sampleIndex) * level - level) * tail_off;

        for (const auto channelIndex : iota(0 ,outputBuffer.getNumChannels()))
          outputBuffer.setSample(channelIndex, sampleIndex, static_cast<float>(renderedSample));
    }
    static void renderSawSample(AudioBuffer<float>& outputBuffer, const int sampleIndex, const double current_frequency, const double level)
    {
      using namespace std::views;

      const auto renderedSample = (2 * moduloOfOne(current_frequency*sampleIndex) * level - level);

      for (const auto channelIndex : iota(0 ,outputBuffer.getNumChannels()))
        outputBuffer.setSample(channelIndex, sampleIndex, static_cast<float>(renderedSample));
    }

    void renderNextBlock(AudioBuffer<float>& outputBuffer, int sampleIndex, int numSamples) override
    {
      using namespace std::views;

      while (--numSamples >= 0) // go down for every sample until the next block
      {
        if (tail_off > 0.0)
        {
          renderSawSample(outputBuffer, sampleIndex, current_frequency, level, tail_off);
          ++sampleIndex;

          tail_off *=0.99;

          if (tail_off <= 0.005) // no more sound!!!
          {
            clearCurrentNote();
            break;
          }
        }
        else
        {
          renderSawSample(outputBuffer, sampleIndex, current_frequency, level);
          ++sampleIndex;
        }
      }
    }

    void pitchWheelMoved(int newPitchWheelValue) override {}

    void controllerMoved(int controllerNumber, int newControllerValue) override {}
  };

  struct SynthesizerAudioSource : AudioSource
  {
    Synthesiser synthesizer; // actual synth duh
    MidiKeyboardState& midiKeyboardState; // midi keyboard on screen
    MidiMessageCollector midiCollector; // where all them messages are

    SynthesizerAudioSource(MidiKeyboardState& keyboard_state) : midiKeyboardState(keyboard_state)
    {
      for (int i = 0; i < 8; i++)
        synthesizer.addVoice(new sine_wave_voice());

      useSineWaveSound();
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

  class SynthesizerAudioDeviceCallback : public AudioIODeviceCallback
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