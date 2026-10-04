#include "../../../../Public/MainGame/SpecialGameClasses/Katamari/KatamariGame.h"
#include "../../../../Public/Components/GameComponents.h"
#include "../../../../Public/MainGame/SpecialGameClasses/Katamari/KatamatiSphereComponent.h"


void KatamariGame::AfterInitialize()
{
    //FirstPlayer->SetCameraMode(CameraMode::ThirdPerson);
    //FirstPlayer->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
}

void KatamariGame::PreUpdate(float deltaTime)
{
    KatamatiSphereComponent* Sphere = dynamic_cast<KatamatiSphereComponent*>(FindComponent("Ball"));
    if (Sphere == nullptr)
    {
        return;
    }

    FirstPlayer->SetOrbitTarget(Sphere->GetCenter());

    HasCollision.clear();
    for (auto& pair : Components)
    {
        GameComponent* Component = pair.second.get();
        if (!Component->IsSkybox() && Component->HasCollison())
        {
            HasCollision.push_back(Component);
        }
    }
    Sphere->CheckCollisions(HasCollision);
}
