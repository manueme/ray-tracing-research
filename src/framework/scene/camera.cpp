/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "camera.h"

Camera::Camera()
{
    // some default values
    setPosition(glm::vec3(0.0f, -1.5f, 0.0f)); // Start camera higher so it's not on the floor
    setMovementSpeed(500.0f);
    setRotationSpeed(0.5f);
    setPerspective(60.0f, 1.0f, 0.1f, 5000.0f);
    const auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto front = glm::vec3(1.0f, 0.0f, 0.0f);
    matrices.orientation = glm::quat_cast(glm::lookAtRH(glm::vec3(0.0), front, up));
    matrices.view = glm::translate(glm::mat4_cast(matrices.orientation), -position);
};

Camera::Camera(const aiCamera& t_aiCamera)
{
    const auto aiCameraPos = t_aiCamera.mPosition;
    const auto cameraPos = glm::vec3(aiCameraPos.x, -aiCameraPos.y, aiCameraPos.z);
    const auto aiCameraFront = t_aiCamera.mLookAt;
    const auto aiCameraUp = t_aiCamera.mUp;
    setPosition(cameraPos);
    const auto front
        = glm::normalize(glm::vec3(aiCameraFront.x, -aiCameraFront.y, aiCameraFront.z));
    const auto up = glm::normalize(glm::vec3(aiCameraUp.x, aiCameraUp.y, aiCameraUp.z));
    matrices.orientation = glm::quat_cast(glm::lookAtRH(glm::vec3(0.0), front, up));
    matrices.view = glm::translate(glm::mat4_cast(matrices.orientation), -position);
}

bool Camera::moving() { return keys.left || keys.right || keys.up || keys.down; }

void Camera::setPerspective(float t_fov, float t_aspect, float t_znear, float t_zfar)
{
    this->fov = t_fov;
    this->znear = t_znear;
    this->zfar = t_zfar;
    matrices.perspective = glm::perspective(glm::radians(t_fov), t_aspect, t_znear, t_zfar);
}

void Camera::updateAspectRatio(float aspect)
{
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
}

void Camera::setPosition(glm::vec3 t_position) { this->position = t_position; }

void Camera::rotate(float t_yaw, float t_pitch)
{
    glm::quat pitch = glm::quat(glm::vec3(glm::radians(-t_pitch), 0.0f, 0.0f));
    glm::quat yaw = glm::quat(glm::vec3(0.f, glm::radians(-t_yaw), 0.f));
    matrices.orientation = pitch * matrices.orientation
        * yaw; // glm::yawPitchRoll(glm::radians(t_yaw), glm::radians(t_pitch), 0.0f);
    matrices.view = glm::translate(glm::mat4_cast(matrices.orientation), -position);
}

void Camera::translate(glm::vec3 t_delta)
{
    this->position += t_delta;
    matrices.view = glm::translate(glm::mat4_cast(matrices.orientation), -position);
}

void Camera::setRotationSpeed(float t_rotationSpeed) { this->rotationSpeed = t_rotationSpeed; }

void Camera::setMovementSpeed(float t_movementSpeed) { this->movementSpeed = t_movementSpeed; }

void Camera::update(float t_deltaTime)
{
    if (moving()) {
        float moveSpeed = t_deltaTime * movementSpeed;
        const glm::vec3 front
            = -glm::vec3(matrices.view[0][2], matrices.view[1][2], matrices.view[2][2]);

        if (keys.up)
            position += front * moveSpeed;
        if (keys.down)
            position -= front * moveSpeed;
        if (keys.left)
            position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
        if (keys.right)
            position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

        matrices.view = glm::translate(glm::mat4_cast(matrices.orientation), -position);
    }
}
