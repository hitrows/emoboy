// ---------------------------------------------------------------------------
// Offscreen editor snapshot tool - renders EmoBoyEditor to PNG files without
// opening a window. Built because screen-recording permission wasn't
// available in this session to just screenshot the real app, and "does the
// pink line actually land on the fader" is not something worth guessing at
// from pixel arithmetic alone. Not shipped, not part of the plugin.
// ---------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

namespace
{
    void setParam (EmoBoyProcessor& proc, const juce::String& id, float actualValue)
    {
        if (auto* param = proc.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (actualValue));
    }

    void snapshot (EmoBoyProcessor& proc, const char* filename)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setVisible (true);

        // Let SliderAttachment's async parameter->UI updates flush before
        // we grab pixels - they're dispatched via the message loop, not
        // applied synchronously inside setValueNotifyingHost().
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        auto image = editor->createComponentSnapshot (editor->getLocalBounds());
        juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (filename));
        juce::PNGImageFormat png;
        juce::FileOutputStream stream (outFile);
        png.writeImageToStream (image, stream);
        printf ("wrote %s (%dx%d)\n", filename, image.getWidth(), image.getHeight());
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        // Defaults: pitch=0, formant=0, drive=0, mix=100 - sanity check the
        // resting position first.
        snapshot (proc, "preview_default.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::pitch, 12.0f);
        setParam (proc, Param::formant, -12.0f);
        setParam (proc, Param::drive, 100.0f);
        setParam (proc, Param::mix, 0.0f);
        snapshot (proc, "preview_extremes.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::pitch, 6.0f);
        setParam (proc, Param::formant, -6.0f);
        setParam (proc, Param::drive, 50.0f);
        setParam (proc, Param::mix, 50.0f);
        snapshot (proc, "preview_mid.png");
    }

    return 0;
}
