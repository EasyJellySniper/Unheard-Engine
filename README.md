# UnheardEngine
 Squall's custom Vulkan engine. Which is a RENDERING ONLY engine at the moment. <br>
 For those who are interested in graphics programming :) MIT licensed. <br>
 Full project download link: https://mega.nz/folder/stgwzQib#ct2EQZm5_w_M53RizRlGHw <br>
 If you have hard time running this project, check the SDK Dependency.txt and install necessary SDKs. <br>
 Recommended to use the full project link, as there are a few test assets. <br>

# 2023-06-26: Refactoring material data
Besides refactoring, I also figure out a way to pass material data to the hit group shader. <br>
Now the ray tracing shadow reacts to my material graph system too. <br>
<br> Implementation Details: <br>
https://www.gamedev.net/blogs/entry/2276272-vulkan-bringing-material-data-to-ray-tracing-shader-and-differentiate-them/
<br><br>

# 2023-04-22: Adding material graph system. <br>
Added simple graph system for materials. Feature preview: <br>
https://www.youtube.com/watch?v=0Qi0WXeB2yw
<br><br> Implementation Details: <br>
https://www.gamedev.net/blogs/entry/2275925-unheard-engine-build-a-material-graph-system-from-scratch/
<br><br>
 
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

<br> Implementation Details: <br>
https://www.gamedev.net/blogs/entry/2274699-unheard-engine-preliminary-optimizations/
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
