# UnheardEngine
 Squall's custom Vulkan engine. Which is a RENDERING ONLY engine at the moment. <br>
 For those who are interested in graphics programming and learning Vulkan :) MIT licensed. <br>
 Full project download link: https://mega.nz/folder/stgwzQib#ct2EQZm5_w_M53RizRlGHw <br>
 If you have hard time running this project, check the SDK Dependency.txt and install necessary SDKs. <br>
 Recommended to use the full project link, as there are a few test assets. <br>

 <br> Implementation details are wrote in my GameDev blog https://www.gamedev.net/blogs/blog/5158-the-graphic-guy-squall/ <br>
 <br> Current rendering pipeline:
 - Depth pre pass (optional)
 - Deferred based pass
 - Motion vector pass
 - Light culling for point/spot lights
 - Ray tracing shadows, soft RT shadows
 - Deferred lighting
 - Skybox pass
 - Translucent pass
 - Tone mapping, TAA
 - HDR support (if enabled and supported)

<br> GUIs are simple at the moment and will be replaced by ImGui in the future.


<br> Day test scene: https://www.youtube.com/watch?v=bmW_U1yBwxw <br>
<br> Night test scene: https://www.youtube.com/watch?v=ewRdUFsdBGg <br>
<br> HDR test: https://www.youtube.com/watch?v=9PLZA3NtMnA <br>
