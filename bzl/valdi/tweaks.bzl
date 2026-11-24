"""Turns `valdi_application(tweaks = {...})` into the platform config the runtime reads at startup."""

def _validate(tweaks):
    for key, value in tweaks.items():
        if not key.startswith("VALDI_"):
            fail("tweak '{}' must start with VALDI_".format(key))
        if type(value) not in ["bool", "int", "string"]:
            fail("tweak '{}' must be a bool, int or string, got {}".format(key, type(value)))

def _escape_xml(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")

def _bool_text(value):
    return "1" if value else "0"

def valdi_tweaks_plist_entry(tweaks):
    """The `ValdiTweaks` Info.plist dictionary for a template substitution; empty when there are no tweaks."""
    _validate(tweaks)
    if not tweaks:
        return ""
    lines = ["\t<key>ValdiTweaks</key>", "\t<dict>"]
    for key in sorted(tweaks.keys()):
        value = tweaks[key]
        lines.append("\t\t<key>{}</key>".format(_escape_xml(key)))
        if type(value) == "bool":
            lines.append("\t\t<{}/>".format("true" if value else "false"))
        elif type(value) == "int":
            lines.append("\t\t<integer>{}</integer>".format(value))
        else:
            lines.append("\t\t<string>{}</string>".format(_escape_xml(value)))
    lines.append("\t</dict>")
    return "\n".join(lines)

def valdi_tweaks_manifest_entry(tweaks):
    """The manifest `<meta-data>` element the Android bootstrap activity reads; empty when there are no tweaks."""
    _validate(tweaks)
    if not tweaks:
        return ""
    entries = []
    for key in sorted(tweaks.keys()):
        value = tweaks[key]
        text = _bool_text(value) if type(value) == "bool" else str(value)
        if "," in text or "=" in text:
            fail("tweak '{}' value can't contain ',' or '=' on Android: {}".format(key, text))
        entries.append("{}={}".format(key, text))
    return '<meta-data android:name="com.snap.valdi.tweaks" android:value="{}" />'.format(
        _escape_xml(",".join(entries)),
    )
