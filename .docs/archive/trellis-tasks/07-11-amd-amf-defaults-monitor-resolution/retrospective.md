# Bug Analysis: Hardware encoder defaults passed UI checks but failed vendor initialization

## 1. Root Cause Category

- **Primary: D - Test Coverage Gap.** Validation stopped at configuration,
  Settings UI, compilation, staging, and installer integrity. None of those
  exercises the target vendor's encoder initialization call.
- **Secondary: B - Cross-Layer Contract.** The complete boundary from Profile
  JSON through plugin normalization to the vendor API was not treated as one
  executable contract.
- **Contributing: E - Implicit Assumption.** The implementation assumed that a
  registered encoder and a valid-looking automatic/default value implied the
  driver would accept the resolved runtime parameter.

The AMD-specific defect was an unsigned-enum sentinel: `H264ProfileLevel` uses
`uint8_t`, so `(H264ProfileLevel)-1` became 255 and could never equal integer
`-1`. At 2560x1440 high FPS, no Level 5.2 table row matched; the loop read past
the array and passed an arbitrary value such as 249 to AMF.

## 2. Why Earlier Fixes Failed

1. **NVIDIA compatibility fix:** The shipped legacy `preset:"hp"` looked valid
   but driver 610.47 rejected the old `nvEncGetEncodePresetConfig` path with
   `NV_ENC_ERR_UNSUPPORTED_PARAM`. Changing JSON alone could not modernize the
   SDK/plugin runtime path; the durable fix later ported the complete P1-P7,
   tuning, multipass, compatibility-version, and `ConfigEx` path.
2. **AMD defaults fix:** Exact `AMF.H264.*` keys, encoder selection, UI preset,
   QP, and monitor resolution were corrected and visually accepted. Recording
   still failed because the automatic Profile Level helper lived deeper in the
   plugin and was only exercised by real encoder creation.
3. **Process failure:** Both incidents initially treated schema/UI acceptance
   as runtime acceptance. Representative vendor hardware testing happened only
   after packaging instead of being an explicit completion gate.

## 3. Prevention Mechanisms

| Priority | Mechanism | Specific Action | Status |
|---|---|---|---|
| P0 | Architecture | Bounded capability tables, checked 64-bit arithmetic, explicit above-range behavior | DONE |
| P0 | Test coverage | AMF regression test covers normal/max/above-max/zero/overflow inputs | DONE |
| P0 | Runtime gate | NVIDIA/AMD defaults require representative start/record/stop and log inspection | DONE |
| P0 | Product guardrail | Vendor fixes may not lower the global `480/1` Profile contract | DONE |
| P1 | Documentation | Added cross-vendor initialization boundary contract to Bootstrap spec | DONE |
| P1 | Code review | Added hardware encoder/driver checklist to cross-layer guide | DONE |

## 4. Systematic Expansion

- **Similar issues:** QSV capability levels, codec/profile tables, FFmpeg option
  forwarding, SDK structure versions, rate denominators, queue sizing, and any
  automatic value resolved only during vendor initialization.
- **Design improvement:** Own validation at the plugin/vendor boundary, where
  the effective value is known. Configuration and UI layers must not duplicate
  or guess hardware capability logic.
- **Process improvement:** A hardware encoder default is incomplete until its
  full config-to-driver path records and stops on representative hardware. A
  local machine without that vendor keeps the acceptance gate pending.
- **Knowledge gap closed:** "Encoder registered" means discoverable, not
  usable under OBS-PT's high-FPS workload.

## 5. Knowledge Capture

- [x] Added `.trellis/spec/obspt-bootstrap.md` hardware initialization contract.
- [x] Added `.trellis/spec/guides/cross-layer-thinking-guide.md` review checklist.
- [x] Retained the AMF-specific unsigned-sentinel mistake and regression test.
- [x] Recorded AMD tester acceptance in the active task.
- [x] Checked for `src/templates/markdown/spec/`; this project has no template
      copy, so `.trellis/spec/` is the only spec source to update.
