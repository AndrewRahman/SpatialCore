# OSD bus negotiation: can OSD see <49 outputs on a 50/60-ch REAPER track? (read-only, 2026-10-24)

Paths: V1 = `git show e3eb766:Source/PluginProcessor.cpp` (line numbers below); DEV = openspatialdelay-v1/ljubljana/Source/PluginProcessor.cpp.
JUCE: same submodule commit 5179f4e720 (Oct 2024) at v1.0.0 and dev. Formats: VST3 + AU on macOS (CMakeLists.txt:83-85 v1, :96-98 dev). No CLAP.

## 1. BusesProperties
Both versions identical: V1 PluginProcessor.cpp:1788-1797 (+ test ctor 1889-1895), DEV :872-882 (+ :972-978).
- Input "Input": stereo, enabled by default. 1 input bus.
- Output "Output": discreteChannels(50), enabled; stereo only if PluginHostType().isAbletonLive(). 1 output bus. No aux buses.

## 2. isBusesLayoutSupported (V1 :2447-2457 ... ; DEV :1569-1609, same logic)
- wrapperType_VST3: `return true` for ANY layout (:2453 area / DEV :1573-1578).
- AU / other: rejects any discrete main output (DEV :1588-1589). Then input must be mono/stereo or equal output set; then accepts if numCh >= smallest requiredChannels of any format (DEV :1600-1607, i.e. any named layout >=2 ch).
- Comment says "Ambisonics (discrete 4/9/..49) unaffected, use VST3". That is wrong for AU, see 4.

## 3. maxBusChannels
- `maxBusChannels = getTotalNumOutputChannels();` in prepareToPlay only: V1 :2943, DEV :1820. Member default 2 (PluginProcessor.h:818).
- grep: no numChannelsChanged / processorLayoutsChanged / getMainBus* / getChannelCountOfBus override anywhere (V1 or dev). Only other reader: editor (below).
- resolveEffectiveFormat (DEV :1659-1695; V1 equivalent :~2760-2800): Ambi requested but busChannels < required (49 for 6OA) -> falls to the largest NON-ambi format that fits (`isAmbisonicsOutput` skipped, :1688). With 16-35 ch that is 9.1.6 (16 ch); <16 smaller surround; 2 -> Binaural.
- requestOutputFormatChange (DEV :1702-1708) also uses the stale maxBusChannels.

## 4. JUCE 8 (5179f4e) VST3 / AU behaviour
VST3 (juce_audio_plugin_client_VST3.cpp, JUCE/modules/juce_audio_plugin_client/):
- getBusInfo :3080-3095 reports channelCount = bus->getLastEnabledLayout().size() = 50 for the default.
- getBusArrangement :3415-3427 needs getVst3SpeakerArrangement(layout) (juce_VST3Common.h:523-552). For discreteChannels(50) only discreteChannel0 maps (kSpeakerM, :275), so 1 bit != 50 -> returns nullopt -> getBusArrangement returns kResultFalse (jassertfalse in Debug). Also :2552 `jassert(checkBusFormatsAreNotDiscrete())` fails in Debug ("default layout must be non-discrete"). In Release these are silent.
  => the host is told "50 channels" by getBusInfo but cannot read an arrangement for it. What REAPER does then is NOT verifiable from source.
- setBusArrangements :3324-3413: converts host arrangements via getChannelSetForSpeakerArrangement (:556-571). Ambisonic mappings exist only for orders 5, 6, 7 (VST3Common.h:513-517): kAmbi6thOrderACN = 0x1FFFFFFFFFFFF (49 bits) -> ambisonic(6) = 49 ch (VST3_SDK vstspeaker.h:211); kAmbi7th = all 64 bits. Any other bitmask goes through getSpeakerOrder (:474-501) and may fail -> returns kResultFalse, layout unchanged. A 50-bit or 60-bit mask is not a defined ambisonic arrangement.
  If the layout converts, OSD's isBusesLayoutSupported says true, setBusesLayoutWithoutEnabling applies it, so a 49- or 64-ch request WOULD give >=49. If REAPER's request fails conversion, JUCE keeps/uses its "next best" (which is the current 50 default) and returns kResultFalse.
- JucePlugin_PreferredChannelConfigurations is not defined in OSD (grep'd CMake/juce_add_plugin: not set), so no channel whitelist.
AU (juce_audio_plugin_client_AU_1.mm):
- busIgnoresLayout :887-909: tests isLayoutSupported(discrete(50)); OSD's AU path returns false (discrete rejected) so layouts are NOT ignored; JUCE advertises tags from getSupportedLayoutTagsForBus :2600-2630.
- Tags = known CoreAudio tags that OSD accepts: HOA_ACN_SN3D only for orders 0..5 (juce_CoreAudioLayouts_mac.h:307-310, `order <= 5`) = max 36 channels, NOT 49; plus surround tables up to 9.1.6 (16 ch). The discrete-tag loop (:2610-2616) adds nothing because every discrete(N) is rejected.
- So via AU the largest negotiable output is 36 ch (5OA) or 16 surround; 49 (6OA) can never be advertised. SetAudioChannelLayout/Initialize (:2515-2530, setBusesLayout) re-validate against the same rejection.
  => On AU, OSD CANNOT reach 6th order in REAPER regardless of track width; maxBusChannels will be <=36 and 6OA falls back (to 9.1.6 if channels 16-35, else smaller). Code comment at DEV :1588 claiming Ambisonics is unaffected is wrong for 49 (and for 4/9/16/25/36 only works if the host picks the HOA tag).

## 5. Re-resolve on layout change?
- Processor: none (see 3). Format is re-resolved only in prepareToPlay (:1820-1823) and when the user touches the Format box (PluginEditor.cpp:927 -> requestOutputFormatChange). A host that changes layout then calls prepareToPlay again (JUCE re-activates) so maxBusChannels does refresh at that point, but no other hook.
- Editor timer (DEV PluginEditor.cpp:2287-2306): lines 2288-2293 read maxCh and set `lastMaxBusChannels = maxCh` inside the `if (maxCh != lastMaxBusChannels)`; line 2305 then tests `lastMaxBusChannels != getMaxBusChannels()`, which is always false after 2291 ran (and false if unchanged). CONFIRMED: dead code, never fires. Net effect: the timer only greys format items; it never re-resolves.
- Also the effective format is NOT persisted/refreshed from the grey state: if prepare saw 16 ch and later 49, nothing re-resolves until prepare runs again, and the user's saved index (6OA) is resolved afresh then, so it self-heals on the next prepare.

## 6. What the user sees in REAPER
- Plugin I/O (pin) button shows the output pin count REAPER has assigned (N out). If it says 16 (or 36, or 2) rather than 49+, OSD fell back.
- Editor: the Output Format combo greys every format with requiredChannels > maxBusChannels (PluginEditor.cpp:2288-2299), so "6th Order Ambisonics" shows greyed while the item text can still read as selected; the Algorithm box greys/relabels per active format (Ambisonics Encode only when the ACTIVE (effective) format is ambi, PluginEditor.cpp ~2310+). If active is 9.1.6, the Algorithm box shows the speaker algorithm list (VBAP etc.), not "Ambisonics Encode". That is the discriminator: Algorithm box listing speaker algorithms == fell back; greyed single "Ambisonics Encode" == real ambi.

## Verdict
- Code cannot prove REAPER gives >=49. Mechanically possible to see <49 even on a 50/60-ch track:
  * VST3: OSD default is discrete(50), which JUCE cannot express as a VST3 arrangement (getBusArrangement fails), and 50/60-bit masks are not named ambisonic arrangements. Only if REAPER sends kAmbi6thOrderACN (49) / kAmbi7thOrderACN (64) or a convertible bitmask does setBusArrangements succeed with >=49; otherwise it returns kResultFalse and JUCE keeps its default (50 in getTotalNumOutputChannels, which would still be >=49). So for VST3, the failure mode is "host negotiates a SMALLER named layout", not "stays at default"; the default itself (50) is >=49. If REAPER got 50 pins, maxBusChannels = 50 and 6OA is honoured. Unverified in REAPER; the I/O pin count and greyed combo are the cheap check.
  * AU: 49 can never be negotiated (HOA tags stop at order 5 = 36; discrete rejected). If the user ran the AU in REAPER, OSD would be capped at <=36 ch and 6OA falls back to 9.1.6 even on a 60-ch track. This is the strongest concrete way to get "fewer than 49".
- Both V1 and dev are identical on all of this (same JUCE, same bus code, same isBusesLayoutSupported, same prepare-time maxBusChannels).
- Fallback consequence (explains asymmetric back L/R): in 9.1.6 fallback, taps are routed to speaker feeds on channels 1..16 and fed to IEM AllRADecoder as if they were ACN channels. ACN 1..15 are not rear L/R, and the 16 channels are LFE-containing, so positions would look unrelated to azimuth and left/right would not mirror. This fits "back-left and back-right differ" better than any encoder bug. Needs the user's Algorithm-box/pin check to confirm.
- Other paths for back-L/back-R difference (no bus-related one found): buffer.clear() zeroes all channels before render (V1 :4645, dev :3041), so channel 50 (index 49) and beyond stay silent (engine writes only (order+1)^2 = 49 ch; post stage multiplies zeros). Dry goes to W only (V1 :4794-4801, dev :3343-3349), symmetric. Output limiter is per-channel tanh, symmetric. No aux buses, only bus 0. The only left/right-differing mechanisms remain per-tap delay multiples and tap azimuth/label (see osd-ambi-path.md section 4) and the fallback above.
