# Validates the shipped services.json codec declarations without loading the
# plugin: same parse rules as rtmp_common_get_supported_video_codecs.

import io
import json
import sys

PATH = "plugins/rtmp-services/data/services.json"


def load():
    with io.open(PATH, encoding="utf-8") as f:
        return json.load(f)


def find_service(services, aliases):
    for s in services:
        if s.get("name") in aliases:
            return s
    return None


services = load()["services"]
failures = 0


def check(cond, msg):
    global failures
    if cond:
        print(f"  PASS: {msg}")
    else:
        failures += 1
        print(f"  FAIL: {msg}")


bilibili = find_service(services, ["Bilibili Live"])
check(bilibili is not None, "Bilibili Live service found")
vc = (bilibili or {}).get("supported video codecs")
check(vc == ["h264"], "Bilibili video codecs == [h264] (got %r)" % (vc,))

twitch = find_service(services, ["Twitch"])
check(twitch is not None, "Twitch service found")
check(twitch.get("supported video codecs") is None,
      "Twitch has no declared restriction (legacy OBS-PT default)")

custom = find_service(services, ["custom"])
print("services.json path:", PATH)

print("\n%d failure(s)" % failures)
sys.exit(1 if failures else 0)
