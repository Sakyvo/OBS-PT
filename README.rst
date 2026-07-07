OBS-PT <https://github.com/Sakyvo/OBS-PT>
=========================================

OBS-PT is an independent OBS Studio fork positioned as **OBS For PotPvP**.
It is built for Minecraft Java 1.7.10 / 1.8.9 PotPvP players who want a
recording setup that is ready for high-FPS capture and frame-blending
workflows without tuning upstream OBS from scratch.

What is OBS-PT?
---------------

OBS-PT keeps the capture, compositing, encoding, recording, and streaming
foundation of OBS Studio, then ships a PotPvP-focused default experience:

- PotPvP recording presets for Minecraft Java 1.7.10 / 1.8.9 and common
  clients such as Lunar Client, Badlion Client, and PvPLounge.
- High-FPS recording defaults, including a 480 FPS fractional-FPS profile for
  slow motion and frame blending in editing software.
- Pre-optimized recording settings for common hardware encoder paths, so new
  installs start from practical PotPvP defaults instead of generic OBS
  defaults.
- Hybrid MP4 as the default normal-recording format. It writes ``.mp4`` files
  while reducing the chance that an interrupted recording leaves an unusable
  file. Ordinary MP4 remains available as a manual fallback.
- A portable install model. User data lives under the install root in
  ``obs-studio/`` instead of the system ``%APPDATA%`` folder.

OBS-PT is distributed under the GNU General Public License v2 (or any later
version). See the accompanying ``COPYING`` file for details.

Quick Links
-----------

- Releases: https://github.com/Sakyvo/OBS-PT/releases

- Bug Tracker: https://github.com/Sakyvo/OBS-PT/issues

- Upstream OBS Studio: https://github.com/obsproject/obs-studio

- OBS Studio Website: https://obsproject.com

Upstream Attribution
--------------------

OBS-PT is based on OBS Studio. OBS Studio is maintained by the OBS Project and
its contributors. OBS-PT is a separate fork with PotPvP-specific defaults,
branding, packaging, and recording behavior.

Build Notes
-----------

OBS-PT follows the OBS Studio source layout and build system. Windows release
packages are produced from the repository source and packaged as a portable
OBS-PT installer.
