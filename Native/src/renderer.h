#pragma once
#include "math_types.h"
#include "native_assets.h"
#include "original_showroom_lighting.h"
#include "original_rear_view.h"
#include "original_course_fog.h"
#include "original_course_lighting.h"
#include "unity_scene_capture.h"
#include "host_platform.h"
#if !defined(IDAS3_PORTABLE_SCENE)
#include <d3d11.h>
#include <wrl/client.h>
#endif
#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace idas3 {
struct Color { float r=1,g=1,b=1,a=1; };
struct Vertex { Vec3 position,normal; Color color; float u=0,v=0; Color offsetColor{0,0,0,0}; };
struct MeshRange {
    std::uint32_t first=0,count=0,texture=0xffffffff,tsp=0,pcw=0,isp=0,gmp=0;
    bool original=false,emissive=false;
    std::uint32_t gloss=0;
    bool billboard=false;
    // Source screen owners can change the parallel light between submissions.
    std::optional<std::array<float,3>> originalLightDirection;
    // The course's ARRAY lightset is distinct from each car's lightset.
    bool courseLighting=false;
    // Bit0: primary view; bit1: rear view. Course/body ranges use both.
    std::uint32_t viewMask=3;
    // Independent source ARRAY scopes:0 none,1 player,2 rival.
    std::uint32_t carLighting=0;
    // Geometry ownership is independent of ARRAY/fallback light selection.
    // Intro scenery uses the existing fallback lights but is still a course.
    bool courseGeometry=false;
    // Explicit car-owner source culling, independent of lighting. Reflections
    // require a separate owner opt-in; singular/billboard instances are excluded.
    bool sourceFaceCulling=false;
    // Nonzero only for a complete, immutable cached vertex range. Material
    // ownership may change independently. Appending vertices invalidates it.
    std::uint64_t geometryId=0;
};
class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<MeshRange> ranges;
    void append(const Mesh& mesh);
    void beginRange(std::uint32_t texture=0xffffffff,std::uint32_t tsp=0,
        std::uint32_t pcw=0,std::uint32_t isp=0,std::uint32_t gmp=0,bool original=false,bool emissive=false,
        std::uint32_t gloss=0,bool billboard=false,
        std::optional<std::array<float,3>> originalLightDirection=std::nullopt,bool courseLighting=false,std::uint32_t viewMask=3,std::uint32_t carLighting=0,bool courseGeometry=false,bool sourceFaceCulling=false);
    void triangle(Vec3 a,Vec3 b,Vec3 c,Color color);
    void quad(Vec3 a,Vec3 b,Vec3 c,Vec3 d,Color color);
    void box(Vec3 center,Vec3 size,float yaw,Color color);
    void tree(Vec3 base,float height);
    void car(Vec3 pos,float yaw,Color color,bool braking,float steer=0);
    void originalCar(const NativeModel& model,const NativeAssembly& assembly,Vec3 pos,float yaw,std::uint32_t textureBase=0);
    // Presentation pose in host model coordinates, matrix order Y-X-Z.
    // Original actor convention: yaw=d10+pi, pitch=-d0C, roll=-d14.
    // RY(y)RX(p)RZ(r)RY(pi) = RY(y+pi)RX(-p)RZ(-r): the model
    // faces+Z while original travel faces-Z. Uses host trig, not FSCA parity.
    void originalCar(const NativeModel& model,const NativeAssembly& assembly,Vec3 pos,
        float yaw,float pitch,float roll,std::uint32_t textureBase=0,
        std::span<const std::uint32_t> illuminatedChunks={},bool sourceFaceCulling=false,bool reflectedFaceCulling=false);
    void originalWorldChunk(const NativeModel& model,std::uint32_t chunk,std::uint32_t textureBase=0);
};
// Source course assemblies are immutable until the scene is reloaded. Keep
// only the current assembly, so memory does not grow with distance driven.
class CourseMeshCache {
public:
    void invalidate(){assembly_=nullptr;model_=nullptr;}
    void appendTo(Mesh& destination,const NativeModel& model,const NativeAssembly& assembly,bool courseGeometry=false);
    // Splice separately selected scenery into the cached static course. Only
    // new/changed instances are transformed again, never the whole course.
    void appendWithInsertions(Mesh&,const NativeModel&,const NativeAssembly&,
        std::span<const NativeAssemblyInsertion>,bool courseGeometry=false);
private:
    void prepare(const NativeModel&,const NativeAssembly&,bool courseGeometry);
    const NativeAssembly* assembly_=nullptr;
    const NativeModel* model_=nullptr;
    bool courseGeometry_=false;
    Mesh mesh_;
    std::vector<std::size_t> instanceVertices_;
    struct ObjectMesh {NativeAssembly assembly;Mesh mesh,scratch;std::vector<std::size_t> instanceVertices;};
    std::vector<ObjectMesh> objectMeshes_;
};
struct OverlayPass {
    const std::uint32_t* pixels=nullptr;
    // RGB has already been weighted by source alpha. Add it to destination
    // RGB while preserving destination alpha, including over a 3D aperture.
    bool additive=false;
    // A640x480 source canvas is uploaded at its native size and fitted to the
    // display. This avoids a full-resolution CPU image for every menu layer.
    bool originalCanvas=false;
};
class Renderer {
public:
    bool initialize(HWND window,int width,int height,bool softwareDiagnostic=false);
    // Pure CPU scene publication. Creates no graphics device/context/target.
    bool initializeSceneCapture(int width,int height);
    const UnitySceneCapture* sceneCapture()const{return capturedScene?&*capturedScene:nullptr;}
    // Unity D3D11 integration. All methods and destruction run on the host's
    // render thread. A private deferred context preserves the host's immediate
    // context state; draw() submits before returning and never presents.
#if !defined(IDAS3_PORTABLE_SCENE)
    bool initializeSharedDevice(ID3D11Device* hostDevice,int width,int height);
    // Borrowed BGRA8_UNORM texture, valid until resize/destruction. Unity uses
    // TextureFormat.BGRA32 and must refresh its external texture after resize.
    ID3D11Texture2D* sharedTexture()const{return sharedDevice?target.Get():nullptr;}
    std::uint64_t sharedTextureGeneration()const{return textureGeneration;}
#endif
    bool resize(int width,int height);
    bool loadTextures(const NativeTextureBank& bank,bool append=false);
    bool draw(const Mesh& mesh,Vec3 eye,Vec3 target,bool night,bool wet,const std::uint32_t* overlay=nullptr,bool overlayBehindScene=false,
        const OriginalShowroomLighting* showroomLighting=nullptr,const OriginalRearViewFrame* rearView=nullptr,
        std::span<const OverlayPass> foreground={});
    bool saveBitmap(const std::wstring& path);
    // Opt-in offscreen profiling only. Ordinary play never waits on queries.
    bool measureGpuFrame=false;
    bool readGpuMilliseconds(double& milliseconds);
    std::string error;
    int width=0,height=0;
    float verticalFieldOfView=.95f;
    float nearClip=1.f,farClip=5000.f;
    bool overrideClearColor=false;
    Color clearColor{0,0,0,1};
    bool fitOriginalViewport=false;
    // A positive value preserves an authored projection on a resized display.
    float projectionAspect=0;
    // A rectangle of the 640x480 source canvas the scene draws into, for a
    // screen that frames a car inside one of its panels. A zero width draws
    // the scene across the whole display, as every other screen does.
    struct { float x=0,y=0,width=0,height=0; } sceneViewport;
    Vec3 cameraUp{0,1,0};
    // Source screen-owner fades apply after both the scene and its 2D banks.
    std::uint32_t screenFadeArgb=0;
    Vec3 vehiclePosition{},vehicleForward{0,0,1};
    bool vehicleLights=true;
    Vec3 opponentPosition{},opponentForward{0,0,1};
    bool opponentLights=false;
    // Native point-light fallback for geometry without a source ARRAY scope.
    std::vector<Vec3> courseLampPositions;
    bool courseLampLighting=true;
    // Imported course lamps supplement source car lighting in the Unity shader.
    bool supplementalCourseLampLighting=false;
    // Borrowed source scene state; the caller keeps it alive through draw().
    // Showrooms always bypass course fog. Main and mirror use their own depth.
    const original::OriginalCourseFog* courseFog=nullptr;
    const original::OriginalCourseLighting* courseLighting=nullptr;
    // Each car submits its own ARRAY before its body transform. All source
    // lights are transformed by the current view, independently in the mirror.
    const original::OriginalCourseLighting* playerLighting=nullptr;
    const original::OriginalCourseLighting* rivalLighting=nullptr;
    Color originalVertexFogColor{0,0,0,1};
    // PVR reset value, and the value the game runs with: its PT_ALPHA_REF
    // setter at 0C1F91E0 has no caller anywhere in the image, so the register
    // is never written and punch-through keeps alpha==255.
    std::uint8_t originalAlphaReference=255;
private:
#if !defined(IDAS3_PORTABLE_SCENE)
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Query> gpuDisjoint,gpuStart,gpuEnd;
    Ptr<ID3D11Device> device;
    Ptr<ID3D11DeviceContext> context;
    Ptr<ID3D11DeviceContext> hostImmediate;
    Ptr<IDXGISwapChain> swapchain;
    Ptr<ID3D11Texture2D> target,depth,hudTexture,originalHudTexture;
    Ptr<ID3D11RenderTargetView> rtv;
    Ptr<ID3D11DepthStencilView> dsv;
    Ptr<ID3D11VertexShader> vs,hudVS;
    Ptr<ID3D11GeometryShader> showroomGS;
    Ptr<ID3D11PixelShader> ps,hudPS,fadePS;
    Ptr<ID3D11InputLayout> layout;
    Ptr<ID3D11Buffer> vertexBuffer,constants,materialConstants,fadeConstants,fogConstants,courseLightConstants;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3D11DepthStencilState> noDepth;
    Ptr<ID3D11BlendState> blend,additiveOverlayBlend;
    Ptr<ID3D11ShaderResourceView> hudView,originalHudView;
    Ptr<ID3D11SamplerState> sampler;
    std::vector<Ptr<ID3D11ShaderResourceView>> modelTextures;
    Ptr<ID3D11ShaderResourceView> whiteTexture;
    Ptr<ID3D11SamplerState> modelSampler;
    std::array<Ptr<ID3D11SamplerState>,32> originalSamplers;
    std::array<Ptr<ID3D11BlendState>,64> originalBlends;
    std::array<Ptr<ID3D11DepthStencilState>,16> originalDepth;
    std::size_t capacity=0;
    bool diagnostic=false;
    bool sharedDevice=false;
#endif
    std::optional<UnitySceneCapture> capturedScene;
#if !defined(IDAS3_PORTABLE_SCENE)
    std::uint64_t textureGeneration=0;
    bool initializeResources(int width,int height);
    bool submitCommands();
    void discardCommands();
    bool drawOverlay(const std::uint32_t* overlay,bool additive=false,bool originalCanvas=false);
    void drawScreenFade();
    bool fail(HRESULT hr,const char* where);
#endif
};
}
