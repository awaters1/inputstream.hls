# inputstream.mpd

This is a dash mpd file addon for kodi's new InputStream Interface.

- get the latest kodi development sources
- build the addon regarding the guidelines building kodi binary addons
- configure the addon by adding URL prefixes wich are allowed to be played by this addon
- create a .strm file / or addon with passes a .mpd url extension
- open the strm file in kodi

Example:
- configuration: http://download.tsi.telecom-paristech.fr
- .strm file: http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-BS.mpd

methods implemented:
- Open() -> Plays the .mpd URL
- Close() -> Stops playing session
- GetCapabilities()
- GetStreamIds() -> returns all available Adaptationsets
- GetStreamInfo() -> returns major stream information for both streams (a/v)
- EnableStream() -> Enables / Disables a given stream
- DemuxRead() -> Returns DTS priorized packets from all enabled streams
- DemuxSeekTime(secs) -> Seeks if possible to the requested (PTS) position inside all selected streams