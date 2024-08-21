#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(glm::vec3 position, glm::quat rotation, float fov, float aspectRatio, float nearPlane, float farPlane)
    : Position(position), Rotation(rotation), Fov(fov), AspectRatio(aspectRatio), NearPlane(nearPlane),
      FarPlane(farPlane)
{
}

void Camera::Rotate(float pitch, float yaw, float roll)
{

    glm::quat pitchQuat = glm::angleAxis(glm::radians(pitch * Sensitivity), glm::vec3(1, 0, 0));
    glm::quat yawQuat = glm::angleAxis(glm::radians(yaw * Sensitivity), glm::vec3(0, 1, 0));
    glm::quat rollQuat = glm::angleAxis(glm::radians(roll * Sensitivity), glm::vec3(0, 0, -1));

    Rotation = pitchQuat * Rotation * yawQuat * rollQuat;

    UpdateDirections();
}

void Camera::SetRotation(float pitch, float yaw, float roll)
{
    glm::quat pitchQuat = glm::angleAxis(glm::radians(pitch), glm::vec3(1, 0, 0));
    glm::quat yawQuat = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
    glm::quat rollQuat = glm::angleAxis(glm::radians(roll), glm::vec3(0, 0, -1));

    Rotation = pitchQuat * yawQuat * rollQuat;

    UpdateDirections();
}

void Camera::Rotate(const glm::quat& rotation)
{
    Rotation = rotation * Rotation;

    UpdateDirections();
}

glm::mat4 Camera::GetViewMatrix()
{
    glm::mat4 viewMatrix = glm::mat4_cast(Rotation);
    viewMatrix = glm::translate(viewMatrix, -Position);
    return viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix()
{
    auto projectionMatrix = glm::perspective(glm::radians(Fov), AspectRatio, NearPlane, FarPlane);
    projectionMatrix[1][1] *= -1; // Flip Y axis, now it's y-up
    return projectionMatrix;
}

void Camera::MoveRight(float distance)
{
    Position += Right * distance * Speed;
}

void Camera::MoveForward(float distance)
{
    Position += Front * distance * Speed;
}

void Camera::UpdateDirections()
{
    Front = glm::normalize(glm::inverse(Rotation) * glm::vec3(0, 0, -1));
    Right = glm::normalize(glm::inverse(Rotation) * glm::vec3(1, 0, 0));
    Up = glm::cross(Right, Front);
}
