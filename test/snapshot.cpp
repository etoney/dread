// Headless checks run in CI:
//  1) renders the editor to gui.png
//  2) user-preset round trip: save -> list -> change -> reload -> delete
//  3) clicks SAVE and renders the name dialog to gui_save.png
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"

static bool writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    if (auto os = f.createOutputStream()) return juce::PNGImageFormat().writeImageToStream (img, *os);
    return false;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::File (juce::String (argv[1])).getParentDirectory() : juce::File::getCurrentWorkingDirectory());
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++failures; };
    {
        DreadProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("gui.png")), "main snapshot");

        // ---- opens on Init (Clean) with every effect off
        check (proc.getProgramName (proc.getCurrentProgram()) == "Init (Clean)", "opens on Init (Clean)");
        {
            bool off = true;
            for (auto id : { "dread", "torment", "ringMix", "wobble", "gateDepth", "distDrive", "delayMix", "reverbMix" })
                off = off && proc.apvts.getRawParameterValue (id)->load() < 0.0001f;
            check (off, "Init (Clean): all effects off");
        }
        // ---- RAISE HELL: changes effects, never touches sources / envelope / output
        {
            const float droneBefore = proc.apvts.getRawParameterValue ("droneLevel")->load();
            const float outBefore   = proc.apvts.getRawParameterValue ("outputDb")->load();
            const float attackBefore = proc.apvts.getRawParameterValue ("attack")->load();
            const auto character = proc.raiseHell();
            std::printf ("raise hell -> %s\n", character.toRawUTF8());
            check (character.isNotEmpty(), "raise hell returns a character");
            check (proc.apvts.getRawParameterValue ("reverbMix")->load() > 0.05f, "raise hell set the reverb");
            check (proc.apvts.getRawParameterValue ("droneLevel")->load() == droneBefore
                   && proc.apvts.getRawParameterValue ("outputDb")->load() == outBefore
                   && proc.apvts.getRawParameterValue ("attack")->load() == attackBefore, "raise hell leaves sources/envelope/output alone");
            juce::TextButton* hb = nullptr;
            for (auto* c : ed->getChildren()) if (auto* tb = dynamic_cast<juce::TextButton*> (c)) if (tb->getButtonText() == "RAISE HELL") hb = tb;
            check (hb != nullptr, "RAISE HELL button exists");
            if (hb != nullptr && hb->onClick) hb->onClick();
            check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("gui_hell.png")), "raise hell snapshot");
            proc.setCurrentProgram (0);
        }

        // ---- preset folder location (matches Possessed)
        const auto folder = DreadProcessor::getUserPresetFolder().getFullPathName();
        std::printf ("preset folder: %s\n", folder.toRawUTF8());
       #if JUCE_MAC
        check (folder.contains ("Library/Application Support/Castle Danger/DREAD/Presets"), "preset folder is in Application Support");
       #endif
        // ---- preset round trip
        const juce::String name = "CI Test Preset";
        const int before = proc.getNumPrograms();
        auto* drive = proc.apvts.getParameter ("distDrive");
        drive->setValueNotifyingHost (drive->convertTo0to1 (0.73f));
        check (proc.saveUserPreset (name), "save user preset");
        check (proc.getNumPrograms() == before + 1, "preset count +1 after save");
        check (proc.getProgramName (proc.getCurrentProgram()) == name, "saved preset becomes current");
        const int idx = proc.getCurrentProgram();
        proc.setCurrentProgram (0);
        check (std::abs (proc.apvts.getRawParameterValue ("distDrive")->load()) < 0.001f, "factory preset resets drive");
        proc.setCurrentProgram (idx);
        check (std::abs (proc.apvts.getRawParameterValue ("distDrive")->load() - 0.73f) < 0.01f, "user preset restores drive");
        check (proc.deleteUserPreset (idx), "delete user preset");
        check (proc.getNumPrograms() == before && ! proc.userPresetExists (name), "preset gone after delete");
        check (! proc.deleteUserPreset (0), "factory preset cannot be deleted");

        // ---- click SAVE and capture the dialog
        juce::TextButton* save = nullptr;
        for (auto* c : ed->getChildren())
            if (auto* tb = dynamic_cast<juce::TextButton*> (c)) if (tb->getButtonText() == "SAVE") save = tb;
        check (save != nullptr, "SAVE button exists");
        if (save != nullptr && save->onClick)
        {
            save->onClick();
            bool found = false;
            for (auto* c : ed->getChildren())
                if (auto* aw = dynamic_cast<juce::AlertWindow*> (c))
                {
                    found = true;
                    if (auto* te = aw->getTextEditor ("name")) te->setText ("Chained In The Basement");
                }
            check (found, "save dialog opens");
            check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("gui_save.png")), "dialog snapshot");
        }
        ed.reset();
    }
    std::printf ("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
