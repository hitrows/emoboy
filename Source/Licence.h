#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

// GOG-style, ported from the "Not Sure" project (2026-08-21, see its
// LICENCE-SPEC.md for the full reasoning): a purchase gets a small signed
// text file, not copy protection. The plugin is fully functional with or
// without one - the only difference is what the licence status reads.
//
// This is deliberately NOT enforcement. Because the plugin works fully
// without a file, anyone sharing a copy simply omits the file. What it
// actually is: a small thing a buyer gets that a non-buyer does not.
namespace emoboy
{

// Public half of the signing key pair, embedded in every build. The private
// half never ships - generated once via tools/make-licence --genkey and
// kept outside this repo (see LICENCE-SPEC.md's equivalent for "Not Sure";
// EmoBoy's own private key lives at ~/Private/emoboy-signing.key on the
// signing machine). A separate key pair from "Not Sure"'s own - licences
// are scoped per product, never shared across two plugins. If this were
// ever a default-constructed/malformed key instead, verifyLicence() below
// fails safe to "unregistered" rather than accepting anything.
inline constexpr const char* kPublicKey =
    "5,d2e8af434d35eb6e7e0b29567ed00b752a13465393a6a51b224a4a452dee3eabcf01addcbc71b0cb3427ef8eabdb3dcccb88d52d41914f816ec575073f7d87d7";

// Signs the concatenation of name and email with privateKey and returns the
// exact three-line text a licence.txt should contain. Pure - touches no
// disk, no global state.
juce::String signLicence (const juce::String& name, const juce::String& email,
                          const juce::RSAKey& privateKey);

// Verifies fileText against publicKey. Never throws and never partially
// trusts a bad file: anything short of a fully valid, matching signature
// yields "unregistered" - empty input, wrong line count, unparseable hex, a
// signature that decodes to something other than name+email (wrong key,
// corruption, or a hand-edited file). A valid file yields
// "licensed to <name>". Pure, like signLicence.
juce::String verifyLicence (const juce::String& fileText, const juce::RSAKey& publicKey);

// App-lifetime singleton. Reads the fixed licence file once - on first
// access, from the processor constructor - and caches the result; never
// re-reads, never touches the audio thread. Wraps verifyLicence() with the
// real path and the embedded kPublicKey.
class LicenceChecker
{
public:
    static LicenceChecker& getInstance();

    const juce::String& getStatusText() const noexcept { return statusText; }

private:
    LicenceChecker();

    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenceChecker)
};

} // namespace emoboy
