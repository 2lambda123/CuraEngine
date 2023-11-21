// Copyright (c) 2020 Ultimaker B.V.
// CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef POINT3F_H
#define POINT3F_H

#include <math.h>
#include <stdint.h>

#include "IntPoint.h"
#include "Point3d.h"


namespace cura
{

/*
Floating point 3D points are used during model loading as 3D vectors.
They represent millimeters in 3D space.
This class should not be used for geometric computation. Use Point3d for this purpose.
*/
class Point3f
{
public:
    float x, y, z;

    Point3f()
    {
    }

    Point3f(double _x, double _y, double _z)
        : x(_x)
        , y(_y)
        , z(_z)
    {
    }

    Point3d toPoint3d() const
    {
        return Point3d(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z));
    }
};

} // namespace cura
#endif // POINT3F_H
