# UnheardEngine
 Squall's custom Vulkan engine. <br>
 MIT licensed. <br>
 Full project download link: https://mega.nz/file/NgZRDIwb#rO3m74IuX-5Yv64ArJXuNcWFrH8npB15k01RAIGGjO0 <br>
 
# 2022-12-09: Preliminary Optimization. <br>
Brutal test with 13435 draw calls, cost only 2.6 - 3.6 ms with parallel submission! <br>
- Added profile window, release-editor build mode.
- Frustum culling.
- Ray traced occlusion culling. (Experimental)
- Mesh Optimization.
- PrePass Z rendering.
- FPS limiter.
- Memory optimization.
- Parallel command buffer recording.

![IMAGE ALT TEXT HERE](https://i.imgur.com/DzTlln4.jpg)
<br><br>
 
# 2022-11-20: The first version. <br>
Not many features in the first version, but covers all basic applications. <br>
- Deferred Rendering Pass (PBR)
- Ray Tracing Shadow
- Lighting Pass
- Skybox Pass
- Motion Vector Pass
- Tone mapping
- Temporal AA

<br> Hopefully this can help all Vulkan learners :) <br>
<br> Implementation Details: <br>
https://gamedev.net/blogs/entry/2274455-unheard-engine-2-months-journey-of-vulkan-learning/
<br><br> Demo Link: <br>
https://www.youtube.com/watch?v=EtZUbPk3ZYA
