//------------------------------------------------------------------------------
//  VoxelTest.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/App.h"
#include "Gfx/Gfx.h"
#include "glm/vec4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "shaders.h"

#define STBVOX_CONFIG_MODE (30)
#define STB_VOXEL_RENDER_IMPLEMENTATION
#include "stb_voxel_render.h"

using namespace Oryol;

class VoxelTest : public App {
public:
    AppState::Code OnInit();
    AppState::Code OnRunning();
    AppState::Code OnCleanup();

    void init_stbvox();
    void init_volume();
    void init_mesh_data();
    void init_resources(const GfxSetup& gfxSetup);
    void update_camera();
    void update_shader_params();
    void upload_mesh_data();

    int32 frameCount = 0;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;

    stbvox_mesh_maker meshMaker;
    bool meshDataDirty = false;

    ClearState clearState;
    Id mesh;
    Id drawState;
    Shaders::Voxel::VSParams vsParams;
    int32 numQuads = 0;

    static const int SizeX = 8;
    static const int SizeY = 8;
    static const int SizeZ = 8;
    uchar volume[SizeX][SizeY][SizeZ];

    static const int MaxNumVertices = (1<<16);
    struct Vertex {
        uint32 attr_vertex = 0;
        uint32 attr_face = 0;
    } vertices[MaxNumVertices];
};
OryolMain(VoxelTest);

//------------------------------------------------------------------------------
AppState::Code
VoxelTest::OnInit() {
    auto gfxSetup = GfxSetup::WindowMSAA4(800, 600, "Oryol Voxel Test");
    Gfx::Setup(gfxSetup);
    this->clearState = ClearState::ClearAll(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), 1.0f, 0);

    this->init_stbvox();
    this->init_volume();
    this->init_mesh_data();
    this->init_resources(gfxSetup);

    // setup projection and view matrices
    const float32 fbWidth = (const float32) Gfx::DisplayAttrs().FramebufferWidth;
    const float32 fbHeight = (const float32) Gfx::DisplayAttrs().FramebufferHeight;
    this->proj = glm::perspectiveFov(glm::radians(45.0f), fbWidth, fbHeight, 0.01f, 100.0f);
    this->view = glm::lookAt(glm::vec3(0.0f, 2.5f, 0.0f), glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    this->model = glm::mat4();

    return App::OnInit();
}

//------------------------------------------------------------------------------
AppState::Code
VoxelTest::OnRunning() {
    this->frameCount++;
    this->update_camera();
    this->update_shader_params();
    Gfx::ApplyDefaultRenderTarget(this->clearState);
    if (this->meshDataDirty) {
        if (this->numQuads > 0) {
            Gfx::UpdateVertices(this->mesh, this->vertices, this->numQuads * 6 * sizeof(Vertex));
        }
        this->meshDataDirty = false;
    }
    Gfx::ApplyDrawState(this->drawState);
    Gfx::ApplyUniformBlock(this->vsParams);
    Gfx::Draw(PrimitiveGroup(0, this->numQuads*6));
    Gfx::CommitFrame();
    
    return Gfx::QuitRequested() ? AppState::Cleanup : AppState::Running;
}

//------------------------------------------------------------------------------
AppState::Code
VoxelTest::OnCleanup() {
    Gfx::Discard();
    return App::OnCleanup();
}

//------------------------------------------------------------------------------
void
VoxelTest::update_camera() {
    float32 angle = this->frameCount * 0.01f;
    glm::vec3 pos(glm::sin(angle) * 20.0f, 10.0f, glm::cos(angle) * 20.0f);
    this->view = glm::lookAt(pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

//------------------------------------------------------------------------------
void
VoxelTest::update_shader_params() {
    this->vsParams.ModelViewProjection = this->proj * this->view * this->model;
    float transform[3][3];
    stbvox_get_transform(&this->meshMaker, transform);
    this->vsParams.Scale.x = transform[0][0];
    this->vsParams.Scale.y = transform[0][1];
    this->vsParams.Scale.z = transform[0][2];
    this->vsParams.Translate.x = transform[1][0];
    this->vsParams.Translate.y = transform[1][1];
    this->vsParams.Translate.z = transform[1][2];
    this->vsParams.TexTranslate.x = transform[2][0];
    this->vsParams.TexTranslate.x = transform[2][1];
    this->vsParams.TexTranslate.x = transform[2][2];
}

//------------------------------------------------------------------------------
void
VoxelTest::init_stbvox() {
    stbvox_init_mesh_maker(&this->meshMaker);
    stbvox_set_buffer(&this->meshMaker, 0, 0, this->vertices, sizeof(this->vertices));
    stbvox_set_default_mesh(&this->meshMaker, 0);
    stbvox_set_input_stride(&this->meshMaker, SizeY*SizeZ, SizeZ);
    stbvox_set_input_range(&this->meshMaker, 1, 1, 1, SizeX-1, SizeY-1, SizeZ-1);

    stbvox_input_description* desc = stbvox_get_input_description(&this->meshMaker);
    desc->blocktype = &(this->volume[0][0][0]);
}

//------------------------------------------------------------------------------
void
VoxelTest::init_volume() {
    Memory::Clear(this->volume, sizeof(this->volume));
    this->volume[1][1][1] = 2;
    this->volume[3][3][3] = 1;
    this->volume[4][4][4] = 1;
    this->volume[5][5][5] = 1;
    this->volume[SizeX-2][SizeY-2][SizeZ-2] = 2;
}

//------------------------------------------------------------------------------
void
VoxelTest::init_resources(const GfxSetup& gfxSetup) {

    // dynamic vertex buffer and static index buffer
    static const int numQuads = (1<<16) / 4;
    static const int numVertices = numQuads * 4;
    static const int numIndices = numQuads * 6;
    auto meshSetup = MeshSetup::FromData(Usage::Dynamic, Usage::Immutable);
    meshSetup.Layout
        .Add(VertexAttr::Position, VertexFormat::UByte4)
        .Add(VertexAttr::Normal, VertexFormat::UByte4);
    meshSetup.NumVertices = numVertices;
    meshSetup.NumIndices  = numIndices;
    meshSetup.IndicesType = IndexType::Index16;
    meshSetup.PrimType = PrimitiveType::Triangles;
    meshSetup.DataVertexOffset = InvalidIndex;
    meshSetup.DataIndexOffset = 0;

    uint16 indices[numIndices];
    for (int quadIndex = 0; quadIndex < numQuads; quadIndex++) {
        int baseVertexIndex = quadIndex * 4;
        int ii = quadIndex * 6;
        indices[ii]   = baseVertexIndex + 0;
        indices[ii+1] = baseVertexIndex + 1;
        indices[ii+2] = baseVertexIndex + 2;
        indices[ii+3] = baseVertexIndex + 0;
        indices[ii+4] = baseVertexIndex + 2;
        indices[ii+5] = baseVertexIndex + 3;
    }
    this->mesh = Gfx::CreateResource(meshSetup, indices, sizeof(indices));

    // shader and drawstate
    Id shd = Gfx::CreateResource(Shaders::Voxel::Setup());
    auto dss = DrawStateSetup::FromMeshAndShader(this->mesh, shd);
    dss.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
    dss.DepthStencilState.DepthWriteEnabled = true;
    dss.RasterizerState.CullFaceEnabled = true;
    dss.RasterizerState.SampleCount = gfxSetup.SampleCount;
    this->drawState = Gfx::CreateResource(dss);
}

//------------------------------------------------------------------------------
void
VoxelTest::init_mesh_data() {
    stbvox_make_mesh(&this->meshMaker);
    this->numQuads = stbvox_get_quad_count(&this->meshMaker, 0);
    this->meshDataDirty = true;

    // dump generated data
    for (int quad = 0; quad < numQuads; quad++) {
        Log::Info("quad %d\n", quad);
        for (int corner = 0; corner < 4; corner++) {
            Log::Info("  corner %d\n", corner);
            int vtx = quad * 4 + corner;
            const uint32 attr_vertex = this->vertices[vtx].attr_vertex;
            const uint32 attr_face = this->vertices[vtx].attr_face;
            float offset_x = float(attr_vertex & 255);
            float offset_y = float((attr_vertex >> 8) & 255);
            float offset_z = float((attr_vertex >> 16) & 255);
            float amb_occ  = float((attr_vertex >> 24) & 63) / 63.0f;
            Log::Info("    offset  = %.3f,%.3f,%.3f\n"
                      "    amb_occ = %f\n"
                      "    facedata = 0x%08x\n",
                      offset_x, offset_y, offset_z,
                      amb_occ, attr_face);
        }
    }
}
