#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

int main (int /*argc*/, char** /*argv*/)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("my-tunes");

    int totalPasses   = 0;
    int totalFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* r = runner.getResult (i))
        {
            totalPasses   += r->passes;
            totalFailures += r->failures;
        }
    }

    std::cout << "\n=== MyTunesTests summary: "
              << totalPasses   << " passes, "
              << totalFailures << " failures ===\n";

    return totalFailures == 0 ? 0 : 1;
}
