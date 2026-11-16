#include "render/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;

glm::vec3 Camera::getPos() const {
    float cosP = std::cos(pitch);
    float sinP = std::sin(pitch);
    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);
    return target + glm::vec3(
        distance * cosP * sinY,
        distance * sinP,
        distance * cosP * cosY
    );
}

glm::mat4 Camera::getView() const {
    return glm::lookAt(getPos(), target, glm::vec3(0.f, 1.f, 0.f));
}

glm::mat4 Camera::getProjection(float aspect) const {
    return glm::perspective(glm::radians(45.f), aspect, 0.01f, 1000.f);
}

void Camera::onMouseDrag(float dx, float dy) {
    float sensitivity = 0.005f;
    yaw   += dx * sensitivity;
    pitch += dy * sensitivity;
    pitch = std::clamp(pitch, -PI/2.f + 0.01f, PI/2.f - 0.01f);
}

void Camera::onScroll(float dy) {
    distance -= dy * 0.5f;
    distance = std::clamp(distance, 0.5f, 500.f);
}
