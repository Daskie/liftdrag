#include "glad/glad.h"
#include "Common/GLSL.h"
#include "stb_image.h"
#include "Common/Shader.hpp"
#include "ProgTerrain.hpp"
#include "Common/Model.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace ProgTerrain {

    static constexpr bool k_doSky(false);

    // Our shader program
    unq<Shader> heightshader, progSky, progWater;

    unq<Model> skySphere;
    static const bool k_drawLines(false);
    static const bool k_drawGrey(false);

    // Contains vertex information for OpenGL
    GLuint TerrainVertexArrayID;
    GLuint WaterVertexArrayID;

    // Data necessary to give our box to OpenGL
    GLuint TerrainPosID, TerrainTexID, IndexBufferIDBox;
    GLuint WaterPosID, WaterTexID, WaterIndexBufferIDBox;

    //texture data
    GLuint GrassTexture, SnowTexture, SandTexture, CliffTexture;
    GLuint SkyTexture, NightTexture;
    GLuint GrassNormal, SnowNormal, SandNormal, CliffNormal;
    float time = 1.0;
    namespace {

        static const int k_meshSize(100);
        static const float k_meshRes(50.f); // Higher value = less verticies per unit of measurement
        void init_mesh() {
            //generate the VAO
            glGenVertexArrays(1, &TerrainVertexArrayID);
            glBindVertexArray(TerrainVertexArrayID);

            //generate vertex buffer to hand off to OGL
            glGenBuffers(1, &TerrainPosID);
            glBindBuffer(GL_ARRAY_BUFFER, TerrainPosID);

            // Size of the net mesh squared (grid) times 4 (verticies per rectangle)
            vec3 *vertices = new vec3[k_meshSize * k_meshSize * 4];

            for (int x = 0; x < k_meshSize; x++)
                for (int z = 0; z < k_meshSize; z++)
                {
                    vertices[x * 4 + z * k_meshSize * 4 + 0] = vec3(0.0, 0.0, 0.0) * k_meshRes + vec3(x, 0, z) * k_meshRes;
                    vertices[x * 4 + z * k_meshSize * 4 + 1] = vec3(1.0, 0.0, 0.0) * k_meshRes + vec3(x, 0, z) * k_meshRes;
                    vertices[x * 4 + z * k_meshSize * 4 + 2] = vec3(1.0, 0.0, 1.0) * k_meshRes + vec3(x, 0, z) * k_meshRes;
                    vertices[x * 4 + z * k_meshSize * 4 + 3] = vec3(0.0, 0.0, 1.0) * k_meshRes + vec3(x, 0, z) * k_meshRes;
                }
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * k_meshSize * k_meshSize * 4, vertices, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            //tex coords
            float t = k_meshRes / 100;
            //float t = k_meshRes / 100;

            vec2 *tex = new vec2[k_meshSize * k_meshSize * 4];
            for (int x = 0; x < k_meshSize; x++)
                for (int y = 0; y < k_meshSize; y++)
                {
                    tex[x * 4 + y * k_meshSize * 4 + 0] = vec2(0.0, 0.0) + vec2(x, y) * t;
                    tex[x * 4 + y * k_meshSize * 4 + 1] = vec2(t, 0.0) + vec2(x, y) * t;
                    tex[x * 4 + y * k_meshSize * 4 + 2] = vec2(t, t) + vec2(x, y) * t;
                    tex[x * 4 + y * k_meshSize * 4 + 3] = vec2(0.0, t) + vec2(x, y) * t;
                }
            glGenBuffers(1, &TerrainTexID);
            //set the current state to focus on our vertex buffer
            glBindBuffer(GL_ARRAY_BUFFER, TerrainTexID);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * k_meshSize * k_meshSize * 4, tex, GL_STATIC_DRAW);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            //free(tex);

            glGenBuffers(1, &IndexBufferIDBox);
            //set the current state to focus on our vertex buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);

            GLuint *elements = new GLuint[k_meshSize * k_meshSize * 6];
            int ind = 0;
            for (int i = 0; i < k_meshSize * k_meshSize * 6; i += 6, ind += 4)
            {
                elements[i + 0] = ind + 0;
                elements[i + 1] = ind + 1;
                elements[i + 2] = ind + 2;
                elements[i + 3] = ind + 0;
                elements[i + 4] = ind + 2;
                elements[i + 5] = ind + 3;
            }
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * k_meshSize * k_meshSize * 6, elements, GL_STATIC_DRAW);
            glBindVertexArray(0);
        }

        static const int k_waterSize(1);
        static const float k_waterRes(600.f);
        void init_water()
        {
            //generate the VAO
            glGenVertexArrays(1, &WaterVertexArrayID);
            glBindVertexArray(WaterVertexArrayID);

            //generate vertex buffer to hand off to OGL
            glGenBuffers(1, &WaterPosID);
            glBindBuffer(GL_ARRAY_BUFFER, WaterPosID);

            // Size of the net mesh squared (grid) times 4 (verticies per rectangle)
            vec3 *vertices = new vec3[k_waterSize * k_waterSize * 4];

            for (int x = 0; x < k_waterSize; x++)
                for (int z = 0; z < k_waterSize; z++)
                {
                    vertices[x * 4 + z * k_waterSize * 4 + 0] = vec3(0.0, 0.0, 0.0) * k_waterRes + vec3(x, 0, z) * k_waterRes;
                    vertices[x * 4 + z * k_waterSize * 4 + 1] = vec3(1.0, 0.0, 0.0) * k_waterRes + vec3(x, 0, z) * k_waterRes;
                    vertices[x * 4 + z * k_waterSize * 4 + 2] = vec3(1.0, 0.0, 1.0) * k_waterRes + vec3(x, 0, z) * k_waterRes;
                    vertices[x * 4 + z * k_waterSize * 4 + 3] = vec3(0.0, 0.0, 1.0) * k_waterRes + vec3(x, 0, z) * k_waterRes;
                }
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * k_waterSize * k_waterSize * 4, vertices, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

            //tex coords
            float t = k_waterRes / 100;
            //float t = k_waterRes / 100;

            vec2 *tex = new vec2[k_waterSize * k_waterSize * 4];
            for (int x = 0; x < k_waterSize; x++)
                for (int y = 0; y < k_waterSize; y++)
                {
                    tex[x * 4 + y * k_waterSize * 4 + 0] = vec2(0.0, 0.0) + vec2(x, y) * t;
                    tex[x * 4 + y * k_waterSize * 4 + 1] = vec2(t, 0.0) + vec2(x, y) * t;
                    tex[x * 4 + y * k_waterSize * 4 + 2] = vec2(t, t) + vec2(x, y) * t;
                    tex[x * 4 + y * k_waterSize * 4 + 3] = vec2(0.0, t) + vec2(x, y) * t;
                }
            glGenBuffers(1, &WaterTexID);
            //set the current state to focus on our vertex buffer
            glBindBuffer(GL_ARRAY_BUFFER, WaterTexID);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * k_waterSize * k_waterSize * 4, tex, GL_STATIC_DRAW);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            //free(tex);

            glGenBuffers(1, &WaterIndexBufferIDBox);
            //set the current state to focus on our vertex buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, WaterIndexBufferIDBox);

            GLuint *elements = new GLuint[k_waterSize * k_waterSize * 6];
            int ind = 0;
            for (int i = 0; i < k_waterSize * k_waterSize * 6; i += 6, ind += 4)
            {
                elements[i + 0] = ind + 0;
                elements[i + 1] = ind + 1;
                elements[i + 2] = ind + 2;
                elements[i + 3] = ind + 0;
                elements[i + 4] = ind + 2;
                elements[i + 5] = ind + 3;
            }
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * k_waterSize * k_waterSize * 6, elements, GL_STATIC_DRAW);
            glBindVertexArray(0);
        }

        void init_textures() {
            std::string texturesPath(g_resourcesDir + "/FlightSim/textures/");

            int width, height, channels;
            char filepath[1000];
            std::string str;
            unsigned char* data;

            // Grass texture
            str = texturesPath + "grass.jpg";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &GrassTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, GrassTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Grass normal map
            str = texturesPath + "grass_normal.png";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &GrassNormal);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, GrassNormal);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Snow texture
            str = texturesPath + "snow.jpg";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &SnowTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, SnowTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Snow normal map
            str = texturesPath + "snow_normal.png";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &SnowNormal);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, SnowNormal);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Sand texture
            str = texturesPath + "sand.jpg";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &SandTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, SandTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Sand normal map
            str = texturesPath + "sand_normal.png";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &SandNormal);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, SandNormal);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Cliff texture
            str = texturesPath + "cliff.jpg";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &CliffTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, CliffTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Cliff normal map
            str = texturesPath + "cliff_normal.png";
            strcpy_s(filepath, str.c_str());
            data = stbi_load(filepath, &width, &height, &channels, 4);
            glGenTextures(1, &CliffNormal);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, CliffNormal);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5);
            glGenerateMipmap(GL_TEXTURE_2D);

            if (k_doSky) {
                // Skybox Texture
                str = texturesPath + "sky.jpg";
                strcpy_s(filepath, str.c_str());
                data = stbi_load(filepath, &width, &height, &channels, 4);
                glGenTextures(1, &SkyTexture);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, SkyTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                // Skybox Texture
                str = texturesPath + "sky2.jpg";
                strcpy_s(filepath, str.c_str());
                data = stbi_load(filepath, &width, &height, &channels, 4);
                glGenTextures(1, &NightTexture);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, NightTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
        }

        void assign_textures() {
            //[TWOTEXTURES]
            //set the 2 textures to the correct samplers in the fragment shader:
            GLuint GrassTextureLocation, SnowTextureLocation, SandTextureLocation, CliffTextureLocation, SkyTextureLocation, NightTextureLocation;
            GLuint GrassNormalLocation, SnowNormalLocation, SandNormalLocation, CliffNormalLocation;

            GrassTextureLocation = glGetUniformLocation(heightshader->glId(), "grassSampler");
            GrassNormalLocation = glGetUniformLocation(heightshader->glId(), "grassNormal");
            SnowTextureLocation = glGetUniformLocation(heightshader->glId(), "snowSampler");
            SnowNormalLocation = glGetUniformLocation(heightshader->glId(), "snowNormal");
            SandTextureLocation = glGetUniformLocation(heightshader->glId(), "sandSampler");
            SandNormalLocation = glGetUniformLocation(heightshader->glId(), "sandNormal");
            CliffTextureLocation = glGetUniformLocation(heightshader->glId(), "cliffSampler");
            CliffNormalLocation = glGetUniformLocation(heightshader->glId(), "cliffNormal");
            // Then bind the uniform samplers to texture units:
            glUseProgram(heightshader->glId());
            glUniform1i(GrassTextureLocation, 0);
            glUniform1i(SnowTextureLocation, 1);
            glUniform1i(SandTextureLocation, 2);
            glUniform1i(CliffTextureLocation, 3);
            glUniform1i(CliffNormalLocation, 4);
            glUniform1i(SnowNormalLocation, 5);
            glUniform1i(GrassNormalLocation, 6);
            glUniform1i(SandNormalLocation, 7);

            if (k_doSky) {
                SkyTextureLocation = glGetUniformLocation(progSky->glId(), "dayTexSampler");
                NightTextureLocation = glGetUniformLocation(progSky->glId(), "nightTexSampler");
                glUseProgram(progSky->glId());
                glUniform1i(SkyTextureLocation, 0);
                glUniform1i(NightTextureLocation, 1);
            }
        }
    }
    void init_shaders() {
        GLSL::checkVersion();

        const std::string & shadersPath(g_resourcesDir + "/FlightSim/shaders/");


        // Initialize the GLSL program.
        if (!(heightshader = Shader::load(shadersPath + "height_vertex.glsl", shadersPath + "tesscontrol.glsl", shadersPath + "tesseval.glsl", shadersPath + "height_frag.glsl"))) {
            std::cerr << "Heightmap shaders failed to compile... exiting!" << std::endl;
            int hold;
            std::cin >> hold;
            exit(1);
        }

        if (k_doSky) {
            // Initialize the GLSL progSkyram.
            if (!(progSky = Shader::load(shadersPath + "skyvertex.glsl", shadersPath + "skyfrag.glsl"))) {
                std::cerr << "Skybox shaders failed to compile... exiting!" << std::endl;
                int hold;
                std::cin >> hold;
                exit(1);
            }
        }

        // Initialize the GLSL program.
        if (!(progWater = Shader::load(shadersPath + "water_vertex.glsl", shadersPath + "water_fragment.glsl"))) {
            std::cerr << "Water shaders failed to compile... exiting!" << std::endl;
            int hold;
            std::cin >> hold;
            exit(1);
        }
    }



    void init_geom()
    {
        init_mesh();
        init_water();

        if (k_doSky) {
            skySphere = Model::load(g_resourcesDir + "/models/sphere.obj");
        }

        init_textures();
        assign_textures();

    }

    void drawSkyBox(const mat4 &V, const mat4 &P, const vec3 &camPos) {
        vec3 camp = -camPos;
        mat4 TransXYZ = glm::translate(glm::mat4(1.0f), camp);
        TransXYZ = glm::translate(TransXYZ, vec3(0, -0.2, 0));
        mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(3.0f, 3.0f, 3.0f));

        mat4 M = TransXYZ * S;

        progSky->bind();
        progSky->uniform("P", P);
        progSky->uniform("V", V);
        progSky->uniform("M", M);
        progSky->uniform("campos", camPos);
        progSky->uniform("time", time);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, SkyTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, NightTexture);
        skySphere->draw();

        Shader::unbind();
    }

    void drawWater(const mat4 &V, const mat4 &P, const vec3 &camPos, const float &centerOffset, const vec3 &offset) {
        // Draw the Water -----------------------------------------------------------------

        progWater->bind();
        //glPolygonMode(GL_FRONT_AND_BACK, GL_LINES);
        mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(centerOffset, 2.0f, centerOffset));

        progWater->uniform("M", M);
        progWater->uniform("P", P);
        progWater->uniform("V", V);

        progWater->uniform("camoff", offset);
        progWater->uniform("campos", camPos);
        progWater->uniform("time", time);
        glBindVertexArray(WaterVertexArrayID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, WaterIndexBufferIDBox);
        //glDisable(GL_DEPTH_TEST);
        // Must use gl_patches w/ tessalation
        //glPatchParameteri(GL_PATCH_VERTICES, 3.0f);
        glDrawElements(GL_TRIANGLES, k_meshSize*k_meshSize * 6, GL_UNSIGNED_INT, (void*)0);
        //glEnable(GL_DEPTH_TEST);
        Shader::unbind();
    }

    void drawTerrain(const mat4 &V, const mat4 &P, const vec3 &camPos, const float &centerOffset, const vec3 &offset) {
        // Draw the terrain -----------------------------------------------------------------
        heightshader->bind();
        mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(centerOffset, 0.0f, centerOffset));
        heightshader->uniform("M", M);
        heightshader->uniform("P", P);
        heightshader->uniform("V", V);

        heightshader->uniform("camoff", offset);
        heightshader->uniform("campos", camPos);
        heightshader->uniform("time", time);
        heightshader->uniform("meshsize", k_meshSize);
        heightshader->uniform("resolution", k_meshRes);
        heightshader->uniform("drawGrey", k_drawGrey);
        glBindVertexArray(TerrainVertexArrayID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GrassTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, SnowTexture);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, SandTexture);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, CliffTexture);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, CliffNormal);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, SnowNormal);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, GrassNormal);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, SandNormal);

        glPatchParameteri(GL_PATCH_VERTICES, 3);
        glDrawElements(GL_PATCHES, k_meshSize*k_meshSize * 6, GL_UNSIGNED_INT, (void*)0);

        Shader::unbind();
    }

    void render(const mat4 &V, const mat4 &P, const vec3 &camPos) {
        if (k_doSky) {
            glDisable(GL_DEPTH_TEST);
            drawSkyBox(V, P, camPos);
            glEnable(GL_DEPTH_TEST);
        }
        float centerOffset = -k_meshSize * k_meshRes / 2.0f;
        vec3 offset = camPos;
        offset.y = 0;
        offset.x = ((int)(offset.x / k_meshRes)) * k_meshRes;
        offset.z = ((int)(offset.z / k_meshRes)) * k_meshRes;

        if (!k_drawGrey || !k_drawLines) {
            drawWater(V, P, camPos, centerOffset, offset);
        }
        drawTerrain(V, P, camPos, centerOffset, offset);
        glBindVertexArray(0);
    }
}
