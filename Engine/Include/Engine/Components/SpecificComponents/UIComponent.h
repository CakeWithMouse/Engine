#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <Engine/Components/GameComponents.h>
#include <Engine/UI/UIElement.h>

/**
 * Draws a screen-space interface: panels, bars, text and buttons, in one pass after everything
 * else, and routes the mouse to it.
 *
 * Everything it owns ends up in a single vertex buffer and a single draw call, because the font
 * atlas carries a solid white block that the flat-coloured elements sample.
 *
 * The tree can be built from code or read from a markup file (see Engine/UI/UIMarkup.h). With a
 * markup file the component watches it and rebuilds the tree when it changes, so the interface is
 * edited in a text editor while the game keeps running. A rebuild replaces every element, so
 * pointers into the tree do not survive it: game code re-binds when GetMarkupRevision() changes.
 */
class UIComponent : public GameComponent
{
public:
    UIComponent();
    ~UIComponent() override;

    /** The root fills the window; everything else is positioned inside it. */
    UIElement& GetRoot() { return *Root; }
    const UIElement& GetRoot() const { return *Root; }

    /** The component takes the element; the returned pointer stays valid until the tree changes. */
    template <typename T>
    T* Add(std::unique_ptr<T> element)
    {
        return Root->Add(std::move(element));
    }

    UIPanel* AddPanel(const UIRect& rect, const glm::vec4& color);
    UILabel* AddLabel(const UIRect& rect, const std::string& utf8, const glm::vec4& color);
    UIBar* AddBar(const UIRect& rect);
    UIButton* AddButton(const UIRect& rect, const std::string& utf8, std::function<void()> onClick);
    void Clear();

    /** Depth-first search by name and type; nullptr when the markup has no such element. */
    template <typename T>
    T* Find(const std::string& name)
    {
        return Root->FindAs<T>(name);
    }

    /**
     * Replaces the tree with the one in the file and watches it from then on. A file that cannot
     * be read or parsed is reported and changes nothing: the interface already on screen stays.
     */
    bool LoadMarkup(const std::string& path);
    /** Stops watching; the tree that was loaded stays as it is. */
    void ClearMarkup();
    /** Goes up by one every time the tree is replaced, so game code knows to look its elements up again. */
    uint32_t GetMarkupRevision() const { return MarkupRevision; }
    // These bindings belong to the component, not to a particular markup tree.
    void BindClick(const std::string& name, std::function<void()> handler);
    void UnbindClick(const std::string& name);
    bool SetLabelText(const std::string& name, const std::string& text);
    bool SetBarFill(const std::string& name, float fill);
    const std::string& GetMarkupError() const { return MarkupError; }
    /** Off disables only the watching, not the initial load. */
    void SetMarkupHotReload(bool enabled) { bMarkupHotReload = enabled; }

    /** Size of the rasterised font. Call before the first frame; rebuilding is not free. */
    void SetFont(const FontAtlasSettings& settings);
    const FontAtlas& GetFont() const { return Font; }

    /** The element under the cursor, or nullptr. Useful to stop the camera stealing a drag. */
    UIElement* GetHovered() const { return Router.GetHovered(); }

    void Tick(float deltaTime) override;
    void PrepareInput(); // Called by Game before gameplay input and callbacks.
    bool WantsPointer() const { return bVisible && (Router.GetHovered() || Router.HasCapture()); }
    void ResetInput() { Router.Reset(); }
    void CreateBuffers(ID3D11Device* Device) override;
    void Render(ID3D11DeviceContext* context) override;

    /** Hides the whole interface without throwing its elements away. */
    void SetVisible(bool visible);
    bool IsVisible() const { return bVisible; }

    /** The UI is not part of the world: no shadows, no culling, no depth. */
    bool IsRenderable() const override { return bVisible; }
    bool IsScreenSpaceUI() const override { return true; }
    bool CastsShadow() const override { return false; }
    bool SupportsFrustumCulling() const override { return false; }

private:
    /** Layout of cbuffer UIBuffer (b0) in Shaders/UI.hlsl. */
    struct UIConstants
    {
        DirectX::XMFLOAT4 ScreenSize{1.0f, 1.0f, 0.0f, 0.0f};
    };

    bool EnsureFont(ID3D11Device* device);
    void LayoutTree(float screenWidth, float screenHeight);
    void BuildVertices(float screenWidth, float screenHeight);
    bool UploadVertices(ID3D11DeviceContext* context, ID3D11Device* device);
    /** Reloads when the file on disk is newer than the tree in memory. */
    void PollMarkup();
    bool ReadMarkup(const std::string& path);
    void ApplyBindings();
    std::map<std::string, std::function<void()>> ClickBindings;
    std::string MarkupError;

    std::unique_ptr<UIElement> Root = std::make_unique<UIElement>();
    UIInputRouter Router;
    bool bVisible = true;
    FontAtlas Font;
    FontAtlasSettings FontSettings;
    bool bFontBuildFailed = false;

    std::string MarkupPath;
    std::filesystem::file_time_type MarkupWriteTime{};
    uint32_t MarkupRevision = 0;
    bool bMarkupHotReload = true;
    /** A failed reload is reported once, not every frame the broken file is still on disk. */
    bool bMarkupErrorReported = false;

    UIDrawList DrawList;
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterState;
    size_t VertexCapacity = 0;
};
