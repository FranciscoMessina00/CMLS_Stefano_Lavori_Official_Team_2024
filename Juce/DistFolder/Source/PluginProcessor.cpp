/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DistFolderAudioProcessor::DistFolderAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    waveshaper.functionToUse = [](float x) //init waveshaper function
        {
            return std::tanh(x);
        };

    params.resize(3);

    //OSC functions
    connect(9000);
    juce::OSCReceiver::addListener(this, "/distFolderParams");
}

DistFolderAudioProcessor::~DistFolderAudioProcessor()
{
}

//==============================================================================
const juce::String DistFolderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DistFolderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DistFolderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DistFolderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DistFolderAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DistFolderAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DistFolderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DistFolderAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DistFolderAudioProcessor::getProgramName (int index)
{
    return {};
}

void DistFolderAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DistFolderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    
}

void DistFolderAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DistFolderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DistFolderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state

    settings = getSettings(apvts);

    
    for (float sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float dry_sample = *buffer.getWritePointer(channel, sample);


            *buffer.getWritePointer(channel, sample) *= settings.fold_amount;

            while (abs(*buffer.getWritePointer(channel, sample)) > 1)
            {
                if (*buffer.getWritePointer(channel, sample) > 1.0f)
                {
                    // The reflection is with respect to the 1 level, not 0
                    *buffer.getWritePointer(channel, sample) = 2.0f - *buffer.getWritePointer(channel, sample);
                }

                if (*buffer.getWritePointer(channel, sample) < -1.0f)
                {
                    // The reflection is with respect to the -1 level, not 0
                    *buffer.getWritePointer(channel, sample) = -2.0f - *buffer.getWritePointer(channel, sample);
                }
            }

            *buffer.getWritePointer(channel, sample) = waveshaper.processSample(*buffer.getWritePointer(channel, sample) * settings.dist_amount); //distortion implementation
                     
            *buffer.getWritePointer(channel, sample) = std::tanh(*buffer.getWritePointer(channel, sample) * settings.dry_wet + dry_sample * (1.0f - settings.dry_wet));
        }
    }
}

//==============================================================================
bool DistFolderAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DistFolderAudioProcessor::createEditor()
{
    //return new DistFolderAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this); //testing editor
}

//==============================================================================
void DistFolderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void DistFolderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
	if (tree.isValid())
	{
		apvts.replaceState(tree);
        settings = getSettings(apvts);
	}
    
}

void DistFolderAudioProcessor::oscMessageReceived(const juce::OSCMessage& message)
{
    if (message.size() == 3)
    {
 /*       float fold_amt = message[0].getFloat32();
      std::cout << fold_amt <<std::endl;*/
        settings.fold_amount = message[0].getFloat32();
        settings.dist_amount = message[1].getFloat32();
        settings.dry_wet = message[2].getFloat32();

    }
}


PluginSettings getSettings(juce::AudioProcessorValueTreeState& apvts) {
    PluginSettings settings;

    settings.fold_amount = apvts.getRawParameterValue("Folder Amount")->load();
    settings.dist_amount = apvts.getRawParameterValue("Distortion Amount")->load();
    settings.dry_wet = apvts.getRawParameterValue("Dry/Wet")->load();

    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout
DistFolderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Folder Amount", //ID
        "Folder Amount", //Name
        juce::NormalisableRange<float>(1.f, 60.f, 0.1f, 1.f), //min, max, increment, skew factor
        1.f)); //default value

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Distortion Amount", //ID
        "Distortion Amount", //Name
        juce::NormalisableRange<float>(1.f, 200.f, 1.f, 1.0f), //min, max, increment, skew factor
        1.f)); //default value

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Dry/Wet", //ID
        "Dry/Wet", //Name
        juce::NormalisableRange<float>(0.f, 1.f, 0.1f, 1.f), //min, max, increment, skew factor
        0.f)); //default value

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistFolderAudioProcessor();
}
