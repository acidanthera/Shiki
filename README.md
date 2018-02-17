Shiki
=====

Protected video playback support and hardware video acceleration on various officially unsupported hardware.  

#### Documentation
Read FAQs before asking any questions:  
- [English FAQ](https://github.com/vit9696/Shiki/blob/master/Manual/FAQ.en.md)
- [Русский FAQ](https://github.com/vit9696/Shiki/blob/master/Manual/FAQ.ru.md)
- [简体中文 FAQ](https://github.com/vit9696/Shiki/blob/master/Manual/FAQ.zh_CN.md)

#### Configuration
Add `-shikidbg` to enable debug printing (available in DEBUG binaries).  
Add `-shikioff` to disable Shiki.  
Add `-shikibeta` to enable Shiki on unsupported os versions (10.13 and below are enabled by default).  
Add `shikigva=X` (where X is a bitmask) to enable additional GVA patches.

An up-to-date extensive explanation of every bit `shikigva` takes could be found [here](https://github.com/vit9696/Shiki/blob/master/Shiki/kern_start.cpp#L18).

#### Support and discussion
- [InsanelyMac topic](http://www.insanelymac.com/forum/topic/312278-shiki-—-userspace-patcher/) in English
- [AppleLife topic](https://applelife.ru/threads/shiki-patcher-polzovatelskogo-urovnja.1349123/) in Russian
- [An easy to read manual](https://applelife.ru/threads/zavod-intel-quick-sync-video.817923/) about Intel Quick Sync configuration is also available in Russian.

#### Downloads
Available on the [releases](https://github.com/vit9696/Shiki/releases) page.
