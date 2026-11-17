#include <Engine/Resources/Paths.h>
#include <Engine/Scene/Trace.h>
#include <iostream>

int main()
{
    const auto executable = EnginePaths::ExecutableDirectory();
    const auto hit = Trace::IntersectTriangle({{0, 0, 1}, {0, 0, -1}, 10},
        {-1, -1, 0}, {1, -1, 0}, {0, 1, 0});
    if (!executable.is_absolute() || !hit) return 1;
    std::cout << "Installed SDK: paths, trace and public headers work\n";
}
