Shiki Changelog
==================
#### v1.8.1
- Added fps.1_0 livestream playback to Safari via `-shikifps` boot argument
- Added version print to the kernel log
- Improved performance on 10.12, should be equal to 10.11 now
- Enabled SSSE3 intrinsics to slightly improve the performance
- Fixed a rare kernel panic on initialisation failure
- Fixed a rare page fault during initialisation
- Fixed page patcher failing to apply some modifications

#### v1.7.0
- Fixed a rare kernel panic on 10.10 and 10.11
- Fixed `-shikifast` mode for 10.12
- Enabled `-shikifast` mode on 10.12 by default

#### v1.5.2
- Disabled Shiki when loading in installer

#### v1.5.1
- Disabled Shiki when loading in recovery

#### v1.5.0
- Added 10.12 Beta support
- Added VMware Fusion support (10.12 only)

#### v1.3.0
- Fixed 10.9.x incompatibilities
- Fixed rare kernel panics and hibernation issues
- Added a possible workaround for 10.10 issues (`-shikislow` boot argument)
- Improved overall stability and performance

#### v1.0.0
- Initial release
