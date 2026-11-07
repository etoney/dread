// Headless GUI snapshot: renders the plugin editor to a PNG (used by CI so the UI can be reviewed).
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("gui.png").getFullPathName());
    int code = 1;
    {
        DreadProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        out.deleteFile();
        if (auto os = out.createOutputStream())
            code = juce::PNGImageFormat().writeImageToStream (img, *os) ? 0 : 1;
        ed.reset();
    }
    std::printf ("snapshot %s -> %s\n", code == 0 ? "ok" : "FAILED", out.getFullPathName().toRawUTF8());
    return code;
}
