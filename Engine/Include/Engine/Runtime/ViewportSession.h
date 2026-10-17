#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <functional>
#include <memory>
#include <stdexcept>

// Temporary bridge for code-built previews. Scene snapshots replace the factory later.
// All methods run on the engine thread. Only Active() may render/present.
class ViewportSession
{
public:
    enum class State { Edit, Playing, Paused };
    using Factory = std::function<std::unique_ptr<Game>()>;

    explicit ViewportSession(Factory factory) : factory_(std::move(factory)) {}
    bool Attach(HWND window, int width, int height)
    {
        play_.reset();
        edit_.reset();
        state_ = State::Edit;
        auto scene = factory_();
        if (!scene) return false;
        scene->SetSimulationEnabled(false);
        if (!scene->InitializeViewport(window, width, height)) return false;
        scene->SetVSync(false);
        edit_ = std::move(scene);
        width_ = width; height_ = height;
        return true;
    }
    Game* Active() const { return play_ ? play_.get() : edit_.get(); }
    State GetState() const { return state_; }
    void Resize(int width, int height)
    {
        width_ = width; height_ = height;
        if (Active()) Active()->OnWindowResized(width, height, !width || !height);
    }
    bool TogglePlay()
    {
        if (!edit_) return false;
        if (play_)
        {
            play_.reset();
            state_ = State::Edit;
            RestoreEditor();
            return true;
        }
        if (!width_ || !height_) return false;
        auto scene = factory_();
        if (!scene) return false;
        edit_->SuspendViewportTargets();
        try
        {
            if (!scene->InitializePlayViewport(*edit_))
                throw std::runtime_error("Play viewport initialization failed");
            scene->SetVSync(false);
            *scene->GetPlayer() = *edit_->GetPlayer();
            scene->GetPlayer()->SetGamePointer(scene.get());
            play_ = std::move(scene);
            state_ = State::Playing;
        }
        catch (...)
        {
            scene.reset(); // Drop all back-buffer references before restoring Edit.
            RestoreEditor();
            throw;
        }
        return true;
    }
    void TogglePause()
    {
        if (!play_) return;
        state_ = state_ == State::Playing ? State::Paused : State::Playing;
        play_->SetSimulationEnabled(state_ == State::Playing);
    }
private:
    void RestoreEditor()
    {
        if (!edit_->RestoreViewportTargets())
            throw std::runtime_error("Cannot restore editor viewport targets");
        Resize(width_, height_);
    }
    Factory factory_;
    std::unique_ptr<Game> edit_;
    std::unique_ptr<Game> play_; // Destroy before edit_: they share the viewport resources.
    State state_ = State::Edit;
    int width_ = 0, height_ = 0;
};
