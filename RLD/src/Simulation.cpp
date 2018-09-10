#include "Simulation.hpp"

#include <memory>
#include <iostream>

#include "glad/glad.h"
#include "Common/Program.h"
#include "Common/GLSL.h"
#include "glm/gtc/matrix_transform.hpp"

#include "Common/Util.hpp"



namespace rld {

    static const int k_size(720); // Width and height of the textures, which are square
    static constexpr int k_sliceCount(100); // Should also change in `Results.cpp`

    static constexpr int k_maxPixelsDivisor(16); // max dense pixels is the total pixels divided by this
    static const int k_maxGeoPixels(k_size * k_size / k_maxPixelsDivisor);
    static const int k_maxAirPixels(k_maxGeoPixels);

    static constexpr int k_maxGeoPerAir(3); // Maximum number of different geo pixels that an air pixel can be associated with



    struct GeoPixel {
        vec2 windPos;
        ivec2 texCoord;
        vec4 normal;
    };

    struct AirPixel {
        vec2 windPos;
        vec2 backforce;
        vec4 velocity;
    };

    struct AirGeoMapElement {
        s32 geoCount;
        s32 geoIndices[k_maxGeoPerAir];
    };

    struct Constants {
        s32 swap;
        s32 maxGeoPixels;
        s32 maxAirPixels;
        s32 screenSize;
        float windframeSize;
        float sliceSize;
        float windSpeed;
        float dt;
        float momentOfInertia;
        s32 slice;
        float sliceZ;
        u32 debug;
    };

    struct Mutables {
        int padding0;
        int geoCount;
        int airCount[2];
        vec4 lift;
        vec4 drag;
        vec4 torque;
    };



    static const Model * s_model;
    static mat4 s_modelMat;
    static mat3 s_normalMat;
    static float s_windframeWidth; // width and height of the windframe
    static float s_windframeDepth; // depth of the windframe
    static float s_sliceSize; // Distance between slices in wind space
    static float s_windSpeed;
    static float s_dt; // The time it would take to travel `s_sliceSize` at `s_windSpeed`
    static float s_momentOfInertia;
    static bool s_debug; // Whether to enable non essentials like side view or active pixel highlighting

    static int s_currentSlice(0); // slice index [0, k_nSlices)
    static float s_angleOfAttack(0.0f); // IN DEGREES
    static float s_rudderAngle(0.0f); // IN DEGREES
    static float s_elevatorAngle(0.0f); // IN DEGREES
    static float s_aileronAngle(0.0f); // IN DEGREES
    static vec3 s_sweepLift; // accumulating lift force for entire sweep
    static vec3 s_sweepDrag; // accumulating drag force for entire sweep
    static vec3 s_sweepTorque; // accumulating torque for entire sweep
    static std::vector<vec3> s_sliceLifts; // Lift for each slice
    static std::vector<vec3> s_sliceDrags; // Drag for each slice
    static std::vector<vec3> s_sliceTorques; // Torque for each slice
    static int s_swap;

    static shr<Program> s_foilProg;

    static Constants s_constants;
    static Mutables s_mutables;
    static uint s_constantsUBO;
    static uint s_mutablesSSBO;
    static uint s_geoPixelsSSBO;
    static uint s_airPixelsSSBO;
    static uint s_airGeoMapSSBO;

    static uint s_fbo;
    static uint s_fboTex;
    static uint s_fboNormTex;
    static uint s_flagTex;
    static uint s_sideTex;

    static uint s_prospectProg;
    static uint s_outlineProg;
    static uint s_moveProg;
    static uint s_drawProg;



    static uint loadShader(const std::string & vertPath, const std::string & fragPath) {
        // TODO
    }

    static uint loadShader(const std::string & compPath) {
        auto [res, str](Util::readTextFile(compPath));
        if (!res) {
            std::cerr << "Failed to read file: " << compPath << std::endl;
            return 0;
        }

        const char * cStr(str.c_str());
        uint shaderId(glCreateShader(GL_COMPUTE_SHADER));
        glShaderSource(shaderId, 1, &cStr, nullptr);
        CHECKED_GL_CALL(glCompileShader(shaderId));
        int rc; 
        CHECKED_GL_CALL(glGetShaderiv(shaderId, GL_COMPILE_STATUS, &rc));
        if (!rc) {
            GLSL::printShaderInfoLog(shaderId);
            std::cerr << "Failed to compile" << std::endl;
            return 0;
        }

        uint progId(glCreateProgram());
        glAttachShader(progId, shaderId);
        glLinkProgram(progId);
        glGetProgramiv(progId, GL_LINK_STATUS, &rc);
        if (!rc) {
            GLSL::printProgramInfoLog(progId);
            std::cerr << "Failed to link" << std::endl;
            return 0;
        }

        if (glGetError()) {
            std::cerr << "OpenGL error" << std::endl;
            return 0;
        }

        return progId;
    }

    static bool setupShaders(const std::string & resourcesDir) {
        std::string shadersDir(resourcesDir + "/shaders");

        // Foil Shader
        s_foilProg = std::make_shared<Program>();
        s_foilProg->setVerbose(true);
        s_foilProg->setShaderNames(shadersDir + "/foil.vert", shadersDir + "/foil.frag");
        if (!s_foilProg->init()) {
            std::cerr << "Failed to initialize foil shader" << std::endl;
            return false;
        }
        s_foilProg->addUniform("u_projMat");
        s_foilProg->addUniform("u_viewMat");
        s_foilProg->addUniform("u_modelMat");
        s_foilProg->addUniform("u_normalMat");

        // Prospect Compute Shader
        if (!(s_prospectProg = loadShader(shadersDir + "/sim_prospect.comp"))) {
            std::cerr << "Failed to load prospect shader" << std::endl;
            return false;
        }

        // Outline Compute Shader
        if (!(s_outlineProg = loadShader(shadersDir + "/sim_outline.comp"))) {
            std::cerr << "Failed to load outline shader" << std::endl;
            return false;
        }
    
        // Move Compute Shader
        if (!(s_moveProg = loadShader(shadersDir + "/sim_move.comp"))) {
            std::cerr << "Failed to load move shader" << std::endl;
            return false;
        }
    
        // Draw Compute Shader
        if (!(s_drawProg = loadShader(shadersDir + "/sim_draw.comp"))) {
            std::cerr << "Failed to load draw shader" << std::endl;
            return false;
        }

        return true;
    }

    static bool setupFramebuffer() {
        float emptyColor[4]{};

        // Color texture
        glGenTextures(1, &s_fboTex);
        glBindTexture(GL_TEXTURE_2D, s_fboTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, emptyColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, k_size, k_size);
        glBindTexture(GL_TEXTURE_2D, 0);

        //sideview texture
        glGenTextures(1, &s_sideTex);
        glBindTexture(GL_TEXTURE_2D, s_sideTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, emptyColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, k_size, k_size);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Normal texture
        glGenTextures(1, &s_fboNormTex);
        glBindTexture(GL_TEXTURE_2D, s_fboNormTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, emptyColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16_SNORM, k_size, k_size);

        // Depth render buffer
        uint fboDepthRB(0);
        glGenRenderbuffers(1, &fboDepthRB);
        glBindRenderbuffer(GL_RENDERBUFFER, fboDepthRB);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, k_size, k_size);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Create FBO
        glGenFramebuffers(1, &s_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_fboTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, s_fboNormTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboDepthRB);
        uint drawBuffers[]{ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, drawBuffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer is incomplete" << std::endl;
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (glGetError() != GL_NO_ERROR) {
            std::cerr << "OpenGL error" << std::endl;
            return false;
        }

        return true;
    }

    static void computeProspect() {
        glUseProgram(s_prospectProg);

        glDispatchCompute((k_size + 7) / 8, (k_size + 7) / 8, 1); // Must also tweak in shader
        glMemoryBarrier(GL_ALL_BARRIER_BITS); // TODO: don't need all     
    }

    static void computeOutline() {
        glUseProgram(s_outlineProg);

        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS); // TODO: don't need all
    }

    static void computeMove() {
        glUseProgram(s_moveProg);

        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS); // TODO: don't need all  
    }

    static void computeDraw() {
        glUseProgram(s_drawProg);
    
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS); // TODO: don't need all
    }

    static void renderGeometry() {
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        glViewport(0, 0, k_size, k_size);

        // Clear framebuffer.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        s_foilProg->bind();

        float zNear(s_windframeDepth * -0.5f + s_currentSlice * s_sliceSize);
        float windframeRadius(s_windframeWidth * 0.5f);
        mat4 projMat(glm::ortho(
            -windframeRadius, // left
            windframeRadius,  // right
            -windframeRadius, // bottom
            windframeRadius,  // top
            zNear, // near
            zNear + s_sliceSize // far
        ));        
        glUniformMatrix4fv(s_foilProg->getUniform("u_projMat"), 1, GL_FALSE, reinterpret_cast<const float *>(&projMat));

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        s_model->draw(s_modelMat, s_normalMat, s_foilProg->getUniform("u_modelMat"), s_foilProg->getUniform("u_normalMat"));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        s_model->draw(s_modelMat, s_normalMat, s_foilProg->getUniform("u_modelMat"), s_foilProg->getUniform("u_normalMat"));

        glMemoryBarrier(GL_ALL_BARRIER_BITS); // TODO: don't need all

        s_foilProg->unbind();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    static void uploadConstants() {
        glBindBuffer(GL_UNIFORM_BUFFER, s_constantsUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Constants), &s_constants);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    static void uploadMutables() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_mutablesSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(Mutables), &s_mutables);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    static void downloadMutables() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_mutablesSSBO);
        void * p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        std::memcpy(&s_mutables, p, sizeof(Mutables));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    static void resetConstants() {
        s_constants.swap = 0;
        s_constants.maxGeoPixels = k_maxGeoPixels;
        s_constants.maxAirPixels = k_maxAirPixels;
        s_constants.screenSize = k_size;
        s_constants.windframeSize = s_windframeWidth;
        s_constants.sliceSize = s_sliceSize;
        s_constants.windSpeed = s_windSpeed;
        s_constants.dt = s_dt;
        s_constants.momentOfInertia = s_momentOfInertia;
        s_constants.slice = 0;
        s_constants.sliceZ = s_windframeDepth * -0.5f;
        s_constants.debug = s_debug;
    }

    static void resetMutables() {
        s_mutables.geoCount = 0;
        s_mutables.airCount[0] = 0;
        s_mutables.airCount[1] = 0;
        s_mutables.lift = vec4();
        s_mutables.drag = vec4();
        s_mutables.torque = vec4();
    }

    static void clearFlagTex() {
        int clearVal(0);
        glClearTexImage(s_flagTex, 0, GL_RED_INTEGER, GL_INT, &clearVal);
    }

    static void clearSideTex() {
        u08 clearVal[4]{};
        glClearTexImage(s_sideTex, 0, GL_RGBA, GL_UNSIGNED_BYTE, &clearVal);
    }

    static void setBindings() {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, s_constantsUBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_mutablesSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, s_geoPixelsSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, s_airPixelsSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, s_airGeoMapSSBO);

        glBindImageTexture(0,     s_fboTex, 0, GL_FALSE, 0, GL_READ_WRITE,       GL_RGBA8);
        glBindImageTexture(2, s_fboNormTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16_SNORM);
        glBindImageTexture(3,    s_flagTex, 0, GL_FALSE, 0, GL_READ_WRITE,        GL_R32I);
        glBindImageTexture(4,    s_sideTex, 0, GL_FALSE, 0, GL_READ_WRITE,       GL_RGBA8);    
    }



    bool setup(const std::string & resourceDir) {
        // Setup shaders
        if (!setupShaders(resourceDir)) {
            std::cerr << "Failed to setup shaders" << std::endl;
            return false;
        }

        // Setup constants UBO
        glGenBuffers(1, &s_constantsUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, s_constantsUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Constants), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Setup mutables SSBO
        glGenBuffers(1, &s_mutablesSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_mutablesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Mutables), nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Setup geometry pixels SSBO
        glGenBuffers(1, &s_geoPixelsSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_geoPixelsSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, k_maxGeoPixels * sizeof(GeoPixel), nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Setup air pixels SSBO
        glGenBuffers(1, &s_airPixelsSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_airPixelsSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, k_maxAirPixels * 2 * sizeof(AirPixel), nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Setup air geo map SSBO
        glGenBuffers(1, &s_airGeoMapSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_airGeoMapSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, k_maxAirPixels * sizeof(AirGeoMapElement), nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Setup flag texture
        glGenTextures(1, &s_flagTex);
        glBindTexture(GL_TEXTURE_2D, s_flagTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, k_size, k_size);
        uint clearcolor = 0;
        glClearTexImage(s_flagTex, 0, GL_RED_INTEGER, GL_INT, &clearcolor);

        if (glGetError() != GL_NO_ERROR) {
            std::cerr << "OpenGL error" << std::endl;
            return false;
        }

        // Setup framebuffer
        if (!setupFramebuffer()) {
            std::cerr << "Failed to setup framebuffer" << std::endl;
            return false;
        }
    
        return true;
    }

    void cleanup() {
        // TODO
    }

    void set(
        const Model & model,
        const mat4 & modelMat,
        const mat3 & normalMat,
        float momentOfInertia,
        float windframeWidth,
        float windframeDepth,
        float windSpeed,
        bool debug
    ) {
        s_model = &model;
        s_modelMat = modelMat;
        s_normalMat = normalMat;
        s_windframeWidth = windframeWidth;
        s_windframeDepth = windframeDepth;
        s_sliceSize = s_windframeDepth / k_sliceCount;
        s_windSpeed = windSpeed;
        s_dt = s_sliceSize / s_windSpeed;
        s_momentOfInertia = momentOfInertia;
        s_debug = debug;
    }

    bool step(bool isExternalCall) {
        if (isExternalCall) {
            setBindings();
        }

        // Reset for new sweep
        if (s_currentSlice == 0) {
            resetConstants();
            resetMutables();
            if (s_debug) clearSideTex();
            s_sweepLift = vec3();
            s_sweepDrag = vec3();
            s_sweepTorque = vec3();
            s_sliceLifts.clear();
            s_sliceDrags.clear();
            s_sliceTorques.clear();
            s_swap = 1;
        }
        
        s_swap = 1 - s_swap;

        s_constants.swap = s_swap;
        s_constants.slice = s_currentSlice;
        s_constants.sliceZ = s_windframeDepth * -0.5f + s_currentSlice * s_sliceSize;
        s_mutables.geoCount = 0;
        s_mutables.airCount[s_swap] = 0;
        uploadConstants();
        uploadMutables();

        renderGeometry(); // Render geometry to fbo
        computeProspect(); // Scan fbo and generate geo pixels
        clearFlagTex();
        computeDraw(); // Draw any existing air pixels to the fbo and save their indices in the flag texture
        computeOutline(); // Map air pixels to geometry, and generate new air pixels and draw them to the fbo
        computeMove(); // Calculate lift/drag and move any existing air pixels in relation to the geometry

        downloadMutables();
        vec3 lift(s_mutables.lift);
        vec3 drag(s_mutables.drag);
        vec3 torque(s_mutables.torque);
        s_sweepLift += lift;
        s_sweepDrag += drag;
        s_sweepTorque += torque;
        s_sliceLifts.push_back(lift);
        s_sliceDrags.push_back(drag);
        s_sliceTorques.push_back(torque);

        if (++s_currentSlice >= k_sliceCount) {
            s_currentSlice = 0;
            return true;
        }

        return false;
    }

    void sweep() {
        s_currentSlice = 0;
        setBindings();
        while (!step(false));
    }

    int slice() {
        return s_currentSlice;
    }

    int sliceCount() {
        return k_sliceCount;
    }

    const vec3 & lift() {
        return s_sweepLift;
    }

    const vec3 & lift(int slice) {
        return s_sliceLifts[slice];
    }

    const vec3 & drag() {
        return s_sweepDrag;
    }

    const vec3 & drag(int slice) {
        return s_sliceDrags[slice];
    }

    const vec3 & torque() {
        return s_sweepTorque;
    }

    const vec3 & torque(int slice) {
        return s_sliceTorques[slice];
    }

    uint frontTex() {
        return s_fboTex;
    }

    uint sideTex() {
        return s_sideTex;
    }

    int size() {
        return k_size;
    }


}