#include "spark/SparkAPI.h"

namespace Math {
    SPARK_API float lerp( float start, float end, float t );
    SPARK_API float unlerp( float start, float end, float x );
    SPARK_API float clamp( float x, float min, float max );
    SPARK_API float smoothstep( float edge0, float edge1, float x );
    SPARK_API float randomGaussian(float mean, float stddev);
    SPARK_API float convertFov(float fov, float currentDimension, float targetDimension);
}