#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include "../Core/FloatSemanticsGuard.h"   // WR-04/IN-16/D-21: the NaN sentinel and the isfinite checks need IEEE semantics
#include <cmath>

namespace spatialcore
{

//==============================================================================
// Cartesian->Polar conversion per ITU-R BS.2127-0, moved verbatim from
// OpenSpatialDelayProcessor.cpp's file-local cartesianToPolar().
//==============================================================================
static inline void cartesianToPolar (float x, float y, float z,
                                     float& azDeg, float& elDeg, float& dist)
{
    static constexpr float kPi = 3.14159265358979323846f;
    azDeg = std::atan2 (-x, y) * (180.0f / kPi);
    float r = std::sqrt (x * x + y * y);
    elDeg = std::atan2 (z, r) * (180.0f / kPi);
    dist = juce::jlimit (0.0f, 1.0f, std::sqrt (x * x + y * y + z * z));
}

//==============================================================================
// Argument readers and range limits for untrusted input (Phase 4, D-21, T-04-05,
// T-04-06). The datagrams are unauthenticated, so an argument is read only after
// its OSC type is known: juce::OSCArgument::getFloat32()/getInt32() on the wrong
// type asserts in Debug and returns garbage in Release. In-range float32 values
// from a compliant ADM-OSC sender pass through unchanged, bit for bit.
//==============================================================================

// float32 with a finite value; false for any other type, NaN or infinity.
static inline bool readFloat (const juce::OSCArgument& arg, float& out)
{
    if (! arg.isFloat32())
        return false;
    const float v = arg.getFloat32();
    if (! std::isfinite (v))
        return false;
    out = v;
    return true;
}

// float32 (finite) or int32, for the on/off and index parameters that senders
// have always been allowed to write as either; false for any other type.
static inline bool readNumeric (const juce::OSCArgument& arg, float& out)
{
    if (arg.isInt32())
    {
        out = static_cast<float> (arg.getInt32());
        return true;
    }
    return readFloat (arg, out);
}

// Leaves [-180, 180] untouched; anything else is mapped into range with ONE
// remainder operation, so 1e10 and 1e30 cost the same as 181 (a consumer's
// subtract-360 loop on the raw value would not return).
static inline float wrapAzimuthBounded (float azDeg)
{
    if (azDeg >= -180.0f && azDeg <= 180.0f)
        return azDeg;
    return std::remainder (azDeg, 360.0f);
}

static inline float clampElevation (float elDeg)   { return juce::jlimit (-90.0f, 90.0f, elDeg); }
static inline float clampDistance (float dist)     { return juce::jlimit (0.0f, 1.0f, dist); }
static inline float clampCartesian (float v)       { return juce::jlimit (-1.0f, 1.0f, v); }

//==============================================================================
// OSC Receive -- message thread callback (MessageLoopCallback). Accepts
// ADM-OSC standard (/adm/obj/N/) and OSD custom (/osd/obj/N/, /osd/global/)
// address grammar, moved verbatim from
// OpenSpatialDelayProcessor::oscMessageReceived (Source/PluginProcessor.cpp) --
// message-parsing logic unchanged (Pitfall 4); only the dispatch target
// changes from direct member calls (handleOSCPosition/handleOSCParam) to the
// Listener callback interface, since this class no longer has access to
// APVTS/processor state directly. Phase 4 adds the no-argument query branch
// (D-08a) and the type, finiteness and range checks above (D-21).
//==============================================================================
void ADMOSCReceiver::oscMessageReceived (const juce::OSCMessage& message)
{
    const auto address = message.getAddressPattern().toString();

    // --- /adm/obj/N/... or /osd/obj/N/... — per-object messages ---
    if (address.startsWith ("/adm/obj/") || address.startsWith ("/osd/obj/"))
    {
        auto afterObj = address.substring (9);  // both prefixes are 9 chars
        auto slashIdx = afterObj.indexOf ("/");
        if (slashIdx < 0) return;

        int objNum = afterObj.substring (0, slashIdx).getIntValue();
        if (objNum < 1 || objNum > MAX_SOURCES) return;
        int objIdx = objNum - 1;

        auto property = afterObj.substring (slashIdx);

        // --- Queries: a position property with no arguments asks for the current
        // value (ADM-OSC, D-08a). Any other property with no arguments is ignored.
        // Both prefixes share this branch, so /osd/obj/N/ aliases are queries too.
        if (message.size() == 0)
        {
            ADMPositionQuery kind;
            if      (property == "/azim") kind = ADMPositionQuery::azim;
            else if (property == "/elev") kind = ADMPositionQuery::elev;
            else if (property == "/dist") kind = ADMPositionQuery::dist;
            else if (property == "/aed")  kind = ADMPositionQuery::aed;
            else if (property == "/xyz")  kind = ADMPositionQuery::xyz;
            else return;

            listeners.call ([objIdx, kind] (Listener& l) { l.admPositionQueried (objIdx, kind); });
            return;
        }

        // --- Position messages (accepted on both /adm/ and /osd/) ---
        // NOTE: the pre-move OSD handler read the OTHER two axes from cached
        // APVTS state so a single-axis message (e.g. /azim) could report a
        // full (az, el, dist) triple to handleOSCPosition. This class has no
        // APVTS access, so single-axis messages are forwarded with the other
        // two axes as NaN; the Listener sentinel-checks (std::isnan) and only
        // updates the axis that was actually received. Cartesian /x /y /z
        // partial-update state (oscCartesianX/Y/Z) also stays owned by the
        // consumer for the same reason -- forwarded as raw single-axis params.
        // Because NaN is that sentinel, a NaN (or infinity) in the INPUT drops the
        // whole message instead of being forwarded as "axis not sent" (D-21).
        float a = 0.0f, b = 0.0f, c = 0.0f;

        if (property == "/azim")
        {
            if (readFloat (message[0], a))
                listeners.call ([objIdx, v = wrapAzimuthBounded (a)] (Listener& l)
                { l.admPositionReceived (objIdx, v, NAN, NAN); });
        }
        else if (property == "/elev")
        {
            if (readFloat (message[0], a))
                listeners.call ([objIdx, v = clampElevation (a)] (Listener& l)
                { l.admPositionReceived (objIdx, NAN, v, NAN); });
        }
        else if (property == "/dist")
        {
            if (readFloat (message[0], a))
                listeners.call ([objIdx, v = clampDistance (a)] (Listener& l)
                { l.admPositionReceived (objIdx, NAN, NAN, v); });
        }
        else if (property == "/aed")
        {
            if (message.size() >= 3
                && readFloat (message[0], a) && readFloat (message[1], b) && readFloat (message[2], c))
            {
                const float az = wrapAzimuthBounded (a), el = clampElevation (b), d = clampDistance (c);
                listeners.call ([objIdx, az, el, d] (Listener& l)
                { l.admPositionReceived (objIdx, az, el, d); });
            }
        }
        else if (property == "/xyz")
        {
            if (message.size() >= 3
                && readFloat (message[0], a) && readFloat (message[1], b) && readFloat (message[2], c))
            {
                float azDeg, elDeg, dist;
                cartesianToPolar (a, b, c, azDeg, elDeg, dist);
                listeners.call ([objIdx, azDeg, elDeg, dist] (Listener& l)
                { l.admPositionReceived (objIdx, azDeg, elDeg, dist); });
            }
        }
        else if (property == "/x" || property == "/y" || property == "/z")
        {
            // Partial Cartesian axis update -- consumer owns the running
            // (x, y, z) state and recomputes polar position on receipt
            // (mirrors the pre-move oscCartesianX/Y/Z member fields).
            if (readFloat (message[0], a))
                listeners.call ([objIdx, property, v = clampCartesian (a)] (Listener& l)
                { l.admObjectParamReceived (objIdx, property.substring (1), v); });
        }
        // --- Per-object non-position params (/osd/obj/N/ only) ---
        // float32 only: the continuous controls.
        else if (property == "/doppler" || property == "/pitch" || property == "/speed")
        {
            if (readFloat (message[0], a))
                listeners.call ([objIdx, name = property.substring (1), a] (Listener& l)
                { l.admObjectParamReceived (objIdx, name, a); });
        }
        // float32 or int32: the switches and indices senders may write as either.
        else if (property == "/enabled" || property == "/trajectory"
                 || property == "/direction" || property == "/input")
        {
            if (readNumeric (message[0], a))
                listeners.call ([objIdx, name = property.substring (1), a] (Listener& l)
                { l.admObjectParamReceived (objIdx, name, a); });
        }
    }
    // --- /osd/global/... — global parameter messages ---
    else if (address.startsWith ("/osd/global/"))
    {
        auto property = address.substring (12);  // skip "/osd/global/" → "delaytime" etc.
        if (message.size() < 1) return;

        float val = 0.0f;
        if (! readNumeric (message[0], val)) return;

        listeners.call ([property, val] (Listener& l) { l.admGlobalParamReceived (property, val); });
    }
}

} // namespace spatialcore
