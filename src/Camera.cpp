#include "Camera.h"

static const vec3 gCameraUp(0.0f, 1.0f, 0.0f);

Camera::Camera(Camera::Type type)
    : mType(type)
    , mFovY(65.0f)
    , mNearZ(1.0f)
    , mFarZ(10000.0f)
    , mPosition(0.0f, 0.0f, 0.0f)
    , mDirection(0.0f, 0.0f, 1.0f)
{
}

Camera::~Camera() {
}

void Camera::SetViewport(const Recti& viewport) {
    mViewport = viewport;
    this->MakeProjection();
}

void Camera::SetFovY(const float fovy) {
    mFovY = fovy;
    this->MakeProjection();
}

void Camera::SetViewPlanes(const float nearZ, const float farZ) {
    mNearZ = nearZ;
    mFarZ = farZ;
    this->MakeProjection();
}

void Camera::SetPosition(const vec3& pos) {
    mPosition = pos;
    this->MakeTransform();
}

void Camera::LookAt(const vec3& pos, const vec3& target) {
    mPosition = pos;
    mDirection = normalize(target - pos);

    this->MakeTransform();
}

void Camera::Move(const float side, const float direction) {
    vec3 cameraSide = normalize(cross(mDirection, gCameraUp));

    mPosition += cameraSide * side;
    mPosition += mDirection * direction;

    this->MakeTransform();
}

void Camera::Rotate(const float angleX, const float angleY) {
    vec3 side = cross(mDirection, gCameraUp);
    quat pitchQ = QAngleAxis(Deg2Rad(angleY), side);
    quat headingQ = QAngleAxis(Deg2Rad(angleX), gCameraUp);
    //add the two quaternions
    quat temp = normalize(pitchQ * headingQ);
    // finally rotate our direction
    mDirection = normalize(QRotate(temp, mDirection));

    this->MakeTransform();
}

const mat4& Camera::GetProjection() const {
    return mProjection;
}

const mat4& Camera::GetTransform() const {
    return mTransform;
}

const vec3& Camera::GetPosition() const {
    return mPosition;
}

const vec3& Camera::GetDirection() const {
    return mDirection;
}

const vec3 Camera::GetUp() const {
    return vec3(mTransform[0][1], mTransform[1][1], mTransform[2][1]);
}

const vec3 Camera::GetSide() const {
    return vec3(mTransform[0][0], mTransform[1][0], mTransform[2][0]);
}

void Camera::MakeProjection() {
    if (Camera::Type::Perspective == mType) {
        const float aspect = static_cast<float>(mViewport.right - mViewport.left) / static_cast<float>(mViewport.bottom - mViewport.top);
        // right-handed and depth range [0, 1]
        mProjection = MatProjection(Deg2Rad(mFovY), aspect, mNearZ, mFarZ);
    } else {
        mProjection = MatOrtho(static_cast<float>(mViewport.left),
                               static_cast<float>(mViewport.right),
                               static_cast<float>(mViewport.bottom),
                               static_cast<float>(mViewport.top),
                               mNearZ, mFarZ);
    }
}

void Camera::MakeTransform() {
    mTransform = MatLookAt(mPosition, mPosition + mDirection, vec3(0.0f, 1.0f, 0.0f));
}
