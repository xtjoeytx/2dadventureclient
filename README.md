# 2dadventureclient

Client modification by: xx-shitai-xx
Created by: Joey, Nalin, dufresnep, Codr, Marlon.
Based off the original work by 39ster.
For their additional work on the old gserver, special thanks go to:
	Agret, Beholder, Joey, Marlon, Nalin, and Pac.
	
## Building

```
fetch v8
cd v8
git checkout refs/tags/7.4.288.26 -b sample -t
gclient sync -D
gclient sync
tools/dev/v8gen.py x64.release.sample
ninja -j 8 -C out.gn/x64.release.sample v8_monolith
```
