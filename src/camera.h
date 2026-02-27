#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct CameraSettings
{
    float aspect_ratio = 1.0f;
    int image_width = 100;
    int samples_per_pixel = 1;
    int max_bounces = 8;
    float vfov = 90;
    float focus_dist = 10.0;
    float defocus_angle = 0.0;
    glm::vec3 lookfrom = glm::vec3(0, 0, 0);
    glm::vec3 lookat = glm::vec3(0, 0, -1);
    glm::vec3 vup = glm::vec3(0, 1, 0);
};

// Struct used for sending to gpu
struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 inv_view;
    glm::mat4 inv_projection;
    glm::vec3 lookfrom; // camera position, std140 aligns vec3 to vec4
    float focus_distance;
    float defocus_angle;
};

class Camera {
    public:
        CameraSettings settings;
        CameraData data;

        int image_width;
        int image_height;

        glm::vec3 forward, right, up;
        float yaw; 
        float pitch;

        bool moving = false;
        struct KeyMappings
        {
            int moveLeft = GLFW_KEY_A;
            int moveRight = GLFW_KEY_D;
            int moveForward = GLFW_KEY_W;
            int moveBackward = GLFW_KEY_S;
            int moveUp = GLFW_KEY_SPACE;
            int moveDown = GLFW_KEY_LEFT_CONTROL;
            int lookLeft = GLFW_KEY_LEFT;
            int lookRight = GLFW_KEY_RIGHT;
            int lookUp  = GLFW_KEY_UP;
            int lookDown = GLFW_KEY_DOWN;
        };
        
        KeyMappings keys{};
        float moveSpeed{3.f};
        float lookSpeed{1.1f};

        Camera(CameraSettings &settings) {
            
            this->settings = settings;
            data.lookfrom = glm::vec4(settings.lookfrom, 1.0f);
            image_width = settings.image_width;
            image_height = (int)(settings.image_width / settings.aspect_ratio);

            
            forward = glm::normalize(settings.lookat - settings.lookfrom);
            right = glm::normalize(glm::cross(forward, settings.vup));
            up = glm::cross(right, forward); // ensures orthonormal
            
            yaw = -165.0f; // initialized looking toward -Z
            pitch = -5.0f;

            data.view = glm::lookAt(settings.lookfrom, settings.lookat, settings.vup);
            data.projection = glm::perspective(glm::radians(settings.vfov), settings.aspect_ratio, 0.1f, 1000.0f);
    
            data.inv_view = glm::inverse(data.view);
            data.inv_projection = glm::inverse(data.projection);
            
            data.focus_distance = settings.focus_dist;
            data.defocus_angle = settings.defocus_angle;
        }

        void updateViewMatrix() {
            glm::vec3 center = data.lookfrom + forward;
            data.view = glm::lookAt(data.lookfrom, center, up);
            updateInvMatrices();
        }

        void translate(glm::vec3 delta) {
            data.lookfrom += delta.x * right + delta.y * up + delta.z * forward;
            updateViewMatrix();
        }

        void updateDirectionVectors() {
            float radYaw = glm::radians(yaw);
            float radPitch = glm::radians(pitch);

            forward = glm::normalize(glm::vec3(
                cos(radYaw) * cos(radPitch),
                sin(radPitch),
                sin(radYaw) * cos(radPitch)
            ));
            right = glm::normalize(glm::cross(forward, settings.vup));
            up = glm::normalize(glm::cross(right, forward));
            updateViewMatrix();
        }

        void updateInvMatrices(){
            data.inv_view = glm::inverse(data.view);
            data.inv_projection = glm::inverse(data.projection);
        }


        
    void update(GLFWwindow* window, float dt, Camera& camera) {
        moving = false;
        float speed = moveSpeed * dt;
        float nspeed = -speed;

        if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) {
            moving = true;
            camera.translate(glm::vec3(nspeed, 0, 0));
        }
        if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) {
            moving = true;
            camera.translate(glm::vec3(speed, 0, 0));
        }
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) {
            moving = true;
            camera.translate(glm::vec3(0, 0, speed));
        }
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) {
            moving = true;
            camera.translate(glm::vec3(0, 0, nspeed));
        }
        if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) {
            moving = true;
            camera.translate(glm::vec3(0, speed, 0));
        }
        if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) {
            moving = true;
            camera.translate(glm::vec3(0, nspeed, 0));
        }

        if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) {
            moving = true;
            yaw -= lookSpeed * dt * 60.0f;
        }
        if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) {
            moving = true;
            yaw += lookSpeed * dt * 60.0f;
        }
        if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) {
            moving = true;
            pitch += lookSpeed * dt * 60.0f;
        }
        if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) {
            moving = true;
            pitch -= lookSpeed * dt * 60.0f;
        }

        pitch = glm::clamp(pitch, -89.0f, 89.0f);
        updateDirectionVectors();

    }

    };
    
