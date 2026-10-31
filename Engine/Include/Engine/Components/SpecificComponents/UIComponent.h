#pragma once

#include <memory>
#include <vector>
#include <Engine/Components/GameComponents.h>
#include <Engine/UI/UIElement.h>

/**
 * Draws a screen-space interface: panels, bars and text, in one pass after everything else.
 *
 * Everything it owns ends up in a single vertex buffer and a single draw call, because the font
 * atlas carries a solid white block that the flat-coloured elements sample. The component keeps
 * the elements, so a scene adds a label once and then only changes its text.
 */
class UIComponent : public GameComponent
{
public:
    UIComponent();
    ~UIComponent() override;

    /** The component takes the element; the returned pointer stays valid until it is removed. */
    template <typename T>
    T* Add(std::unique_ptr<T> element)
    {
        T* raw = element.get();
        Elements.push_back(std::move(element));
        return raw;
    }

    UIPanel* AddPanel(const UIRect& rect, const glm::vec4& color);
    UILabel* AddLabel(const UIRect& rect, const std::string& utf8, const glm::vec4& color);
    UIBar* AddBar(const UIRect& rect);
    void Clear() { Elements.clear(); }

    /** Size of the rasterised font. Call before the first frame; rebuilding is not free. */
    void SetFont(const FontAtlasSettings& settings);
    const FontAtlas& GetFont() const { return Font; }

    void CreateBuffers(ID3D11Device* Device) override;
    void Render(ID3D11DeviceContext* context) override;

    /** Hides the whole interface without throwing its elements away. */
    void SetVisible(bool visible) { bVisible = visible; }
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
    void BuildVertices(float screenWidth, float screenHeight);
    bool UploadVertices(ID3D11DeviceContext* context, ID3D11Device* device);

    std::vector<std::unique_ptr<UIElement>> Elements;
    bool bVisible = true;
    FontAtlas Font;
    FontAtlasSettings FontSettings;
    bool bFontBuildFailed = false;

    UIDrawList DrawList;
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterState;
    size_t VertexCapacity = 0;
};
