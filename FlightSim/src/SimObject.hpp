#pragma once

#include "Common/Global.hpp"

class SimObject {
    vec3 acc;
    vec3 a_vel;
    vec3 a_acc;
    float mass;
    bool gravityOn;
    float timeScale;
    float thrust;
public:
    vec3 pos;
    vec3 vel;
    vec3 a_pos;
    vec3 thrust_vel;

    SimObject();
    mat4 getTransform();
    void addTranslationalForce(vec3 force);
    void addAngularForce(vec3 force);
    void update();
    void setGravityOn(bool _gravityOn);
    void setMass(float _mass);
    void setTimeScale(float _timeScale);
    void setThrust(float _thrust);
    mat4 getTranslate();
    mat4 getRotate();
    mat4 getTransformRelativeTo(vec3 dir);
};