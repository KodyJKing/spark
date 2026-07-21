#include <random>
#include "Math.hpp"

namespace Math {
    float lerp( float start, float end, float t ) {
        return start + (end - start) * t;
    }

    float unlerp( float start, float end, float x ) {
        return (x - start) / (end - start);
    }

    float clamp( float x, float min, float max ) {
        return x < min ? min : x > max ? max : x;
    }

    float smoothstep( float edge0, float edge1, float x ) {
        x = clamp( (x - edge0) / (edge1 - edge0), 0.0f, 1.0f );
        return x * x * (3 - 2 * x);
    }

    float randomGaussian( float mean, float stddev ) {
        static std::mt19937 rng( std::random_device{}() );
        std::normal_distribution<float> dist( mean, stddev );
        return dist( rng );
    }

    float convertFov(float fov, float currentDimension, float targetDimension) {
        // Convert FOV from one dimension to another (e.g. vertical to horizontal).
        // Uses the formula: tan(fov_target / 2) = (targetDim / currentDim) * tan(fov_current / 2)
        return 2.0f * atanf((targetDimension / currentDimension) * tanf(fov / 2.0f));
    }
}