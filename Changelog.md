Shiki Changelog
==================

#### v2.1.0
- Requires Lilu 1.2.0 or higher
- Added hardware video decoding fix for SKL/KBL & NVIDIA (`shikigva=4`)
- Added Lilu 1.2.0 safe mode loading compatibility

#### v2.0.5
- Added High Sierra installer detection

#### v2.0.4
- Added High Sierra to the list of compatible OS
- Changed `-shikigva` to `shikigva=1`
- Added bit 2 to (e.g. `shikigva=2`) to disable hweBGRA (useful for some AMD)
- Added FAQ in simplified Chinese (thx PMHeart)

#### v2.0.3
- Incorporate safer patches accordingly to calling conventions

#### v2.0.2
- Added OSBundleCompatibleVersion

#### v2.0.1
- Requires Lilu 1.1.0 or newer
- Added more key streaming patches for testing purposes
- Added `-shikigva` boot argument to allow online hardware video decoder

#### v2.0.0
- Rewrote as a [Lilu.kext](https://github.com/vit9696/Lilu) plugin
- Added FAQ entries describing how to workaround Intel Azul freezes
- Opened the source code

#### v1.9.0
- Fixed error logging on 10.12
- Added 10.12.1 beta compatibility

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
