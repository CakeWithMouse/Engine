#pragma once

namespace EditorIcons
{
    inline constexpr const char* SVG_BULB = R"SVG(<!--
category: Design
tags: [bulb, lightbulb, light, idea, lamp, tip, hint, creativity, innovation]
version: "1.0"
unicode: "ea51"
-->
<svg
  xmlns="http://www.w3.org/2000/svg"
  width="24"
  height="24"
  viewBox="0 0 24 24"
  fill="none"
  stroke="#FFFFFF"
  stroke-width="2"
  stroke-linecap="round"
  stroke-linejoin="round"
>
  <path d="M3 12h1m8 -9v1m8 8h1m-15.4 -6.4l.7 .7m12.1 -.7l-.7 .7" />
  <path d="M9 16a5 5 0 1 1 6 0a3.5 3.5 0 0 0 -1 3a2 2 0 0 1 -4 0a3.5 3.5 0 0 0 -1 -3" />
  <path d="M9.7 17l4.6 0" />
</svg>
)SVG";

    inline constexpr const char* SVG_SUN = R"SVG(<!--
tags: [light-mode, light, theme, day, brightness, weather, sunny, bright, sun]
category: Weather
version: "1.0"
unicode: "eb30"
-->
<svg
  xmlns="http://www.w3.org/2000/svg"
  width="24"
  height="24"
  viewBox="0 0 24 24"
  fill="none"
  stroke="#FFFFFF"
  stroke-width="2"
  stroke-linecap="round"
  stroke-linejoin="round"
>
  <path d="M8 12a4 4 0 1 0 8 0a4 4 0 1 0 -8 0" />
  <path d="M3 12h1m8 -9v1m8 8h1m-9 8v1m-6.4 -15.4l.7 .7m12.1 -.7l-.7 .7m0 11.4l.7 .7m-12.1 -.7l-.7 .7" />
</svg>
)SVG";

    inline constexpr const char* SVG_CAMERA = R"SVG(<!--
tags: [camera, photo, photography, picture, video, lens, image, snapshot]
category: Media
version: "1.0"
unicode: "ea54"
-->
<svg
  xmlns="http://www.w3.org/2000/svg"
  width="24"
  height="24"
  viewBox="0 0 24 24"
  fill="none"
  stroke="#FFFFFF"
  stroke-width="2"
  stroke-linecap="round"
  stroke-linejoin="round"
>
  <path d="M5 7h1a2 2 0 0 0 2 -2a1 1 0 0 1 1 -1h6a1 1 0 0 1 1 1a2 2 0 0 0 2 2h1a2 2 0 0 1 2 2v9a2 2 0 0 1 -2 2h-14a2 2 0 0 1 -2 -2v-9a2 2 0 0 1 2 -2" />
  <path d="M9 13a3 3 0 1 0 6 0a3 3 0 0 0 -6 0" />
</svg>
)SVG";

    inline constexpr const char* SVG_VIDEO = R"SVG(<!--
tags: [video, camera, camcorder, recording, movie, film, record, vlog]
category: Media
version: "1.25"
unicode: "ed22"
-->
<svg
  xmlns="http://www.w3.org/2000/svg"
  width="24"
  height="24"
  viewBox="0 0 24 24"
  fill="none"
  stroke="#FFFFFF"
  stroke-width="2"
  stroke-linecap="round"
  stroke-linejoin="round"
>
  <path d="M15 10l4.553 -2.276a1 1 0 0 1 1.447 .894v6.764a1 1 0 0 1 -1.447 .894l-4.553 -2.276v-4" />
  <path d="M3 8a2 2 0 0 1 2 -2h8a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-8a2 2 0 0 1 -2 -2l0 -8" />
</svg>
)SVG";

    inline constexpr const char* SVG_PARTICLES = R"SVG(<!--
category: System
tags: [3d, 3d-printing, sphere, cube, model, object, vector, rendering, ar]
version: "1.20"
unicode: "ecd7"
-->
<svg
  xmlns="http://www.w3.org/2000/svg"
  width="24"
  height="24"
  viewBox="0 0 24 24"
  fill="none"
  stroke="#FFFFFF"
  stroke-width="2"
  stroke-linecap="round"
  stroke-linejoin="round"
>
  <path d="M6 17.6l-2 -1.1v-2.5" />
  <path d="M4 10v-2.5l2 -1.1" />
  <path d="M10 4.1l2 -1.1l2 1.1" />
  <path d="M18 6.4l2 1.1v2.5" />
  <path d="M20 14v2.5l-2 1.12" />
  <path d="M14 19.9l-2 1.1l-2 -1.1" />
  <path d="M12 12l2 -1.1" />
  <path d="M18 8.6l2 -1.1" />
  <path d="M12 12l0 2.5" />
  <path d="M12 18.5l0 2.5" />
  <path d="M12 12l-2 -1.12" />
  <path d="M6 8.6l-2 -1.1" />
</svg>
)SVG";

    inline constexpr const char* SVG_USER = R"SVG(<!--
tags: [person, account, profile, avatar, member, people, human, login, identity, user]
category: System
version: "1.0"
unicode: "eb4d"
-->
<svg
  xmlns="http://www.w3.org/2000/svg"
  width="24"
  height="24"
  viewBox="0 0 24 24"
  fill="none"
  stroke="#FFFFFF"
  stroke-width="2"
  stroke-linecap="round"
  stroke-linejoin="round"
>
  <path d="M8 7a4 4 0 1 0 8 0a4 4 0 0 0 -8 0" />
  <path d="M6 21v-2a4 4 0 0 1 4 -4h4a4 4 0 0 1 4 4v2" />
</svg>
)SVG";

    enum class IconId : uint32_t
    {
        PointLight = 1,
        SunSky = 2,
        Camera = 3,
        GameCamera = 4,
        Particles = 5,
        Character = 6
    };
}
