/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_CAMERA_H
#define MANUEME_CAMERA_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <assimp/camera.h>

class Camera {
public:
    Camera();
    explicit Camera(const aiCamera& t_aiCamera);

    struct {
        glm::mat4 perspective;
        glm::quat orientation;
        glm::mat4 view;
    } matrices;

    float rotationSpeed = 1.0f;
    float movementSpeed = 1.0f;

    struct {
        bool left = false;
        bool right = false;
        bool up = false;
        bool down = false;
    } keys;

    bool moving();

    void setPerspective(float t_fov, float t_aspect, float t_znear, float t_zfar);

    void updateAspectRatio(float t_aspect);

    void setPosition(glm::vec3 t_position);

    void rotate(float t_yaw, float t_pitch);

    void translate(glm::vec3 t_delta);

    void setRotationSpeed(float t_rotationSpeed);

    void setMovementSpeed(float t_movementSpeed);

    void update(float t_deltaTime);

private:
    float fov;
    float znear, zfar;

    glm::vec3 position = glm::vec3();
};

#endif // MANUEME_CAMERA_H
