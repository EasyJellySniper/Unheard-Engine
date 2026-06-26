# UnheardEngine
 Cross-platform (Linux/Windows) Vulkan-based rendering engine focused on system-level architecture and GPU pipeline design.<br>

• Built a rendering engine as a platform for exploring architectural decisions in modern graphics APIs.<br>
• Designed a cross-platform architecture, decoupling platform systems (windowing, input, lifecycle) from rendering logic (Linux/Windows, GLFW, CMake).<br>
• Implemented modern rendering techniques, including mesh shaders and real-time ray tracing (shadows, reflections, global illumination).<br>
• Developed a GPU memory management system using heap-based allocation and resource pooling to reduce fragmentation and improve allocation efficiency.<br>
• Designed a frame pipeline with explicit pass scheduling and async compute integration, managing dependencies across graphics and compute workloads.<br>
• Evaluated performance and architectural trade-offs (e.g., async compute, mesh shader pipelines), balancing complexity, scalability, and runtime cost.<br>
• Implemented a multi-threaded runtime architecture (main/render/worker threads) to improve CPU-side scalability and task distribution.<br>
• Data-driven management for certain scenarios.

 Full project download link: https://mega.nz/folder/stgwzQib#ct2EQZm5_w_M53RizRlGHw <br>
 (Only UnheardEngine.zip is needed) <br>
 If you have hard time running this project, check the SDK Dependency.txt and install necessary SDKs. <br>
 Recommended to use the full project link, as there are a few test assets. <br>

<br> [RT shadow test](https://www.youtube.com/watch?v=GW9OTBdLGaA) <br>
<br> [RT reflection test](https://www.youtube.com/watch?v=YAWn0Yahfwc) <br>
<br> [RT indirect test](https://www.youtube.com/watch?v=xKl12bAfDKQ) <br>
<br> [Ubuntu-24.04 test](https://i.ibb.co/wrBk9XQy/1776268311027.jpg)<br>
<br> UHE Documentation - https://drive.google.com/file/d/1252nT6MCYtAvNGXpDP39aBeUCfDs7TeN/view?usp=sharing <br>
