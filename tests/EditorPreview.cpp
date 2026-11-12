#include "Source/PluginProcessor.h"
#include "Source/PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseGUI;
    TdpAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);
    TdpAudioProcessorEditor editor(processor);
    editor.setVisible(true);
    editor.resized();

    const auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds(), false, 1.0f);
    if (! snapshot.isValid() || snapshot.getWidth() != 1620 || snapshot.getHeight() != 930)
    {
        std::cerr << "JUCE snapshot dimensions invalid\n";
        return 1;
    }

    const auto output = juce::File::getCurrentWorkingDirectory()
        .getChildFile("Transient_Designer_Plus_Actual_JUCE_GUI.png");
    output.deleteFile();
    auto stream = output.createOutputStream();
    if (stream == nullptr)
    {
        std::cerr << "Cannot create GUI screenshot file\n";
        return 2;
    }

    juce::PNGImageFormat png;
    if (! png.writeImageToStream(snapshot, *stream))
    {
        std::cerr << "Cannot encode JUCE PNG screenshot\n";
        return 3;
    }
    stream->flush();
    std::cout << "JUCE editor screenshot: " << output.getFullPathName()
              << " (" << snapshot.getWidth() << " x " << snapshot.getHeight() << ")\n";
    return 0;
}
