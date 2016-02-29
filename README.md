# inputstream.mpd

This is a dash mpd file addon for kodi's new InputStream Interface.

- get the latest kodi development sources
- build the addon regarding the guidelines building kodi binary addons
- configure the addon by adding URL prefixes wich are allowed to be played by this addon
- create a .strm file / or addon with passes a .mpd url extension
- open the strm file in kodi

##### Example:
- configuration: [http://download.tsi.telecom-paristech.fr]
- .strm file: [http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-BS.mpd]

##### Notes:
- the current kodi master tree does not include the necessary inputstream interface.    
There is an PR for this: [https://github.com/xbmc/xbmc/pull/9173]  
You can use theh inputstream branch from @fernetmenta: [https://github.com/fernetmenta/xbmc/tree/inputstream]
- There is still some work to be done on measuring the download speed for adaptive bitrate switching, currently 4MBit/s is the default value which can be overriden in the settings dialog of the addon
- The URL entries in the settings dialog support regular expressions, please note that at least 6 chars must match the regexp to be valid. Example: http://.*.videodownload.xy supports all subdomains of videodownload.xy

##### Credits:
[@fernetmenta](github.com/fernetmenta) Best support I ever got regarding streams / codecs and kodi internals.  
[@notpiff](https://github.com/notspiff) Thanks for your ideas / tipps regarding kodi file system  
[bento4 library](https://www.bento4.com/) For me the best library choice for mp4 streams. Well written and extensible!
