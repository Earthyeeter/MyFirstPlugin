//
// Created by trist on 3/23/2026.
//
#pragma once


#ifndef MYFIRSTPLUGIN_SYNTHESIZER_OSCILLATOR_H
#define MYFIRSTPLUGIN_SYNTHESIZER_OSCILLATOR_H
#include <ranges>
#include <unordered_set>

#include "../cmake-build-release/_deps/juce-src/modules/juce_graphics/fonts/harfbuzz/hb-ot-math-table.hh"

namespace synthesizer_oscillator
{
  using namespace juce;

  struct sine_wave_sound : SynthesiserSound // sine wave sound DERIVES from synthesizer sound
  {
    bool appliesToNote(int noteNumber) override {return true;}
    bool appliesToChannel(int channelNumber) override {return true;}
  };
  struct saw_wave_sound : virtual sine_wave_sound {};

  class sine_wave_voice : virtual public SynthesiserVoice
  {
  protected:
    double current_phase = 0.0, phase_change = 0.0, level = 0.0, tail_off = 0.0;
    double current_frequency = 0.0;
  public:

    bool canPlaySound(SynthesiserSound* sound) override
    {
      return dynamic_cast<sine_wave_sound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, const float velocity, SynthesiserSound* sound, int currentPitchWheelPosition) override
    {
      level = static_cast<double>(velocity) * .15;
      tail_off = 0.0;
      midiNoteNumber = getCurrentlyPlayingNote();

      current_frequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
      const auto cyclesPerSample = current_frequency / getSampleRate();
      phase_change = cyclesPerSample * MathConstants<double>::twoPi;
    }

    void stopNote(float velocity, bool allowTailOff) override
    {
      if (allowTailOff)
      {
        if (approximatelyEqual(tail_off, 0.0))
          tail_off = static_cast<double>(velocity);
      }
      else
      {
        clearCurrentNote();
        phase_change = 0.0;
      }
    }

    void renderNextBlock (AudioBuffer<double>& outputBuffer, int startSample, int numSamples) override
    {
      if (phase_change != 0) // good: do allat
      {
        if (tail_off > 0.0) // Calculate the tail of audio
        {
          while (--numSamples >= 0) // go down for every sample
          {
            const auto currentSample = std::sin (current_phase) * level * tail_off; // current sample = sine(angle) at volume * tail off AKA decrement volume

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;) // for every channel.... add the sample to the starting sample
              outputBuffer.addSample (i, startSample, currentSample);

            current_phase += phase_change; // change angle duh
            ++startSample; // go to next sample

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
            const auto currentSample = std::sin(current_phase) * level;

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
              outputBuffer.addSample (i, startSample, currentSample);

            current_phase += phase_change;
            ++startSample;
          }
        }
      }
    }
  };

  class saw_wave_voice : public sine_wave_voice
  {
    std::unordered_set<double> overtone_set;
  public:

    static double moduloOfOne(const double &dividend){return dividend - static_cast<int>(dividend);}

    void renderNextBlock(AudioBuffer<double>& outputBuffer, int startSample, int numSamples) override
    {
      if (phase_change != 0)
      {
        if (tail_off > 0.0) // Calculate the tail of audio
        {
          while (--numSamples >= 0) // go down for every sample
          {
            const auto currentSample = (2 * moduloOfOne(current_frequency*startSample) * level - level) * tail_off;

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
              outputBuffer.addSample (i, startSample, currentSample);

            ++startSample;

            tail_off *= 0.99; // just a lil off it

            if (tail_off <= 0.005) // no more sound!!!
            {
              clearCurrentNote();
              phase_change = 0.0;
              break;
            }
          }
        }
      }
      else
      {
        while (--numSamples >= 0) // go down for every sample
        {
          const auto currentSample = 2 * (moduloOfOne(current_frequency)*startSample) * level - level;

          for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
            outputBuffer.addSample (i, startSample, currentSample);

          ++startSample;
        }
      }
    }


  };

};




#endif //MYFIRSTPLUGIN_SYNTHESIZER_OSCILLATOR_H