# UnheardEngine
 Squall's custom Vulkan engine. <br>
 MIT licensed. <br>
 
# 2022-11-28: Minor optimizations. <br>
- Mesh Optimizations: Differentiate 16/32 bit index buffer. Use only Position attribute in IA stage, and access the other attributes from StructuredBuffer.
- Memory Optimizations: Imported textures and meshes will now try to share the vkMemory objects.
- FPS Limiter: Added simple FPS limit.
- Minor Editor Changes.
 
# 2022-11-25: The profile window. <br>
Before all kinds of optimization experiments, I added a profile window which shows CPU/GPU timing. <br>
Also added a new build configuration: Release_Editor, so I can have the performance of release build but it can still keep the editor. <br>
[![IMAGE ALT TEXT HERE](https://i.imgur.com/wpoHGKk.jpg)]

 
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
[![IMAGE ALT TEXT HERE](https://thegraphicguysquall.files.wordpress.com/2022/11/unheardengine-1.jpg)](https://www.youtube.com/watch?v=EtZUbPk3ZYA) 
