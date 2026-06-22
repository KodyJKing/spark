namespace Math {
    float lerp( float start, float end, float t );
    float unlerp( float start, float end, float x );
    float clamp( float x, float min, float max );
    float smoothstep( float edge0, float edge1, float x );
    float randomGaussian(float mean, float stddev);
    float convertFov(float fov, float currentDimension, float targetDimension);
}