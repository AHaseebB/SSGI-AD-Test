#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>

class Camera
{
public:
    Camera();

    void Set(const glm::vec3& position, const glm::vec3& viewpoint, const glm::vec3& upVector);
    void ResetMouseTracking();
    void RespondToMouseMovement();
    void Strafe(float amount);
    void MoveVertical(float amount);
    void Advance(float amount);
    void ChangeSpeed(float speed);

    const glm::vec3& GetUpVector() const;
    glm::vec3 GetStrafeVector() const;
	glm::vec3 GetForward() const;
    const glm::vec3& GetView() const;
    const glm::vec3& GetPosition() const;
    float GetSpeed() const;

    void SetPerspectiveMatrix(const glm::mat4& projection);
    void SetOrthographicMatrix(const glm::mat4& projection);
    const glm::mat4& GetPerspectiveMatrix() const;
    const glm::mat4& GetOrthographicMatrix() const;

    glm::mat3 ComputeNormalMatrix(const glm::mat4& model) const;
    glm::mat4 GetViewMatrix() const;

private:
    void UpdateViewFromAngles();

private:
    glm::vec3 _position;
    glm::vec3 _view;
    glm::vec3 _upVector;

    float _movementSpeed;
    float _mouseSensitivity;
    float _yaw;
    float _pitch;

    bool _firstMouse;
    double _lastMouseX;
    double _lastMouseY;

    glm::mat4 _perspectiveProjection;
    glm::mat4 _orthographicProjection;
};