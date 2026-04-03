#include <Project/Camera.hpp>

#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera():
     _position(0.0f, 0.0f, 0.0f),
     _view(0.0f, 0.0f, 0.0f),
     _upVector(0.0f, 1.0f, 0.0f),
     _movementSpeed(5.0f),
     _mouseSensitivity(0.1f),
     _yaw(-90.0f),
     _pitch(0.0f),
     _firstMouse(true),
     _lastMouseX(0.0),
     _lastMouseY(0.0),
     _perspectiveProjection(1.0f),
     _orthographicProjection(1.0f)
{
    const glm::vec3 direction = glm::normalize(_view - _position);
    _pitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
    _yaw = glm::degrees(std::atan2(direction.z, direction.x));
}

void Camera::Set(const glm::vec3& position, const glm::vec3& viewpoint, const glm::vec3& upVector)
{
    _position = position;
    _view = viewpoint;
    _upVector = glm::normalize(upVector);

    const glm::vec3 direction = glm::normalize(_view - _position);
    _pitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
    _yaw = glm::degrees(std::atan2(direction.z, direction.x));
}

void Camera::ResetMouseTracking()
{
    _firstMouse = true;
}

void Camera::RespondToMouseMovement()
{
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
    {
        return;
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (_firstMouse)
    {
        _lastMouseX = mouseX;
        _lastMouseY = mouseY;
        _firstMouse = false;
        return;
    }

    const float deltaX = static_cast<float>(mouseX - _lastMouseX);
    const float deltaY = static_cast<float>(_lastMouseY - mouseY);

    _lastMouseX = mouseX;
    _lastMouseY = mouseY;

    _yaw += deltaX * _mouseSensitivity;
    _pitch += deltaY * _mouseSensitivity;
    _pitch = std::clamp(_pitch, -89.9f, 89.9f);

    UpdateViewFromAngles();
}

void Camera::Strafe(float amount)
{
    const glm::vec3 strafe = GetStrafeVector();
    _position += strafe * amount;
    _view += strafe * amount;
}

void Camera::MoveVertical(float amount)
{
    const glm::vec3 vertical = glm::normalize(_upVector);
    _position += vertical * amount;
    _view += vertical * amount;
}

void Camera::Advance(float amount)
{
    const glm::vec3 forward = glm::normalize(_view - _position);
    _position += forward * amount;
    _view += forward * amount;
}

void Camera::ChangeSpeed(float speed)
{
    _movementSpeed = speed;
}

const glm::vec3& Camera::GetUpVector() const
{
    return _upVector;
}

glm::vec3 Camera::GetStrafeVector() const
{
    return glm::normalize(glm::cross(GetForward(), _upVector));
}

glm::vec3 Camera::GetForward() const
{
    return glm::normalize(_view - _position);
}

const glm::vec3& Camera::GetView() const
{
    return _view;
}

const glm::vec3& Camera::GetPosition() const
{
    return _position;
}

float Camera::GetSpeed() const
{
    return _movementSpeed;
}

void Camera::SetPerspectiveMatrix(const glm::mat4& projection)
{
    _perspectiveProjection = projection;
}

void Camera::SetOrthographicMatrix(const glm::mat4& projection)
{
    _orthographicProjection = projection;
}

const glm::mat4& Camera::GetPerspectiveMatrix() const
{
    return _perspectiveProjection;
}

const glm::mat4& Camera::GetOrthographicMatrix() const
{
    return _orthographicProjection;
}

glm::mat3 Camera::ComputeNormalMatrix(const glm::mat4& model) const
{
    const glm::mat4 modelView = GetViewMatrix() * model;
    return glm::transpose(glm::inverse(glm::mat3(modelView)));
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(_position, _view, _upVector);
}

void Camera::UpdateViewFromAngles()
{
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(_yaw)) * std::cos(glm::radians(_pitch));
    forward.y = std::sin(glm::radians(_pitch));
    forward.z = std::sin(glm::radians(_yaw)) * std::cos(glm::radians(_pitch));
    forward = glm::normalize(forward);

    _view = _position + forward;
}