/* =STS=> DataVector.h[5034].aa00   submit   SMID:1 */
/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2003-2008 Cyberkinetics, Inc
// (c) Copyright 2008-2012 Blackrock Microsystems
//
// $Workfile: DataVector.h $
// $Archive:  $
// $Revision: 1 $
// $Date: 11/6/08 12:30p $
// $Author: jscott $
//
// $NoKeywords: $
//
// This is a highly templated Vector class. It can make Vectors of
// arbitrary size and arbitrary type. However, some of the functions
// only have implementations up to a small number, such as
// constructors which initialize the Vector.
// This Vector class is intended for use with numeric types, as most
// of the operations are mathematical.
//////////////////////////////////////////////////////////////////////

#ifndef DATA_VECTOR_H_INCLUDED
#define DATA_VECTOR_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <algorithm>
#include <math.h>

///////////////////////////////////////////////////////////
// This is the main structure. Gives all operator overloads
template<class T, int N>
struct Vector
{
    T data[N];

    Vector();
    Vector(const T vec[N]);
    Vector(T t);
    Vector(T t0, T t1);
    Vector(T t0, T t1, T t2);
    Vector(T t0, T t1, T t2, T t3);

    void Set(const T vec[N]);
    void Set(T t);
    void Set(T t0, T t1);
    void Set(T t0, T t1, T t2);
    void Set(T t0, T t1, T t2, T t3);

    T& operator [] (int i);
    T operator [] (int i)const;

    template<class U>
    operator Vector<U,N>()const;

    Vector<T,N>& operator = (const T vec[N]);

    Vector<T,N> operator - ()const;

    Vector<T,N> operator + (Vector<T,N> vec)const;
    Vector<T,N> operator - (Vector<T,N> vec)const;
    Vector<T,N> operator * (Vector<T,N> vec)const;
    Vector<T,N> operator / (Vector<T,N> vec)const;

    Vector<T,N> operator += (Vector<T,N> vec);
    Vector<T,N> operator -= (Vector<T,N> vec);
    Vector<T,N> operator *= (Vector<T,N> vec);
    Vector<T,N> operator /= (Vector<T,N> vec);

    template<class U>
    Vector<T,N> operator + (U u)const;
    template<class U>
    Vector<T,N> operator - (U u)const;
    template<class U>
    Vector<T,N> operator * (U u)const;
    template<class U>
    Vector<T,N> operator / (U u)const;

    template<class U>
    Vector<T,N> operator += (U u);
    template<class U>
    Vector<T,N> operator -= (U u);
    template<class U>
    Vector<T,N> operator *= (U u);
    template<class U>
    Vector<T,N> operator /= (U u);

    T LengthSqrd()const;
    T Length()const;
    void SetLength(T len);
    void Normalize();
    Vector<T,N> Norm()const;

    bool IsSimilar(const Vector<T,N> &v, T maxDifference)const;

};

///////////////////////////////////////////////////////////
// Implementation of constructors

template<class T, int N>
Vector<T,N>::Vector(){}

template<class T, int N>
Vector<T,N>::Vector(const T vec[N]){Set(vec);}

template<class T, int N>
Vector<T,N>::Vector(T t){Set(t);}

template<class T, int N>
Vector<T,N>::Vector(T t0, T t1){Set(t0,t1);}

template<class T, int N>
Vector<T,N>::Vector(T t0, T t1, T t2){Set(t0,t1,t2);}

template<class T, int N>
Vector<T,N>::Vector(T t0, T t1, T t2, T t3){Set(t0,t1,t2,t3);}

///////////////////////////////////////////////////////////
// Implementation of setters

template<class T, int N>
void Vector<T,N>::Set(T t)
{
    for(int i = 0; i < N; i++)
        data[i] = t;
}

template<class T, int N>
void Vector<T,N>::Set(const T vec[N])
{
    for(int i = 0; i < N; i++)
        data[i] = vec[i];
}

template<class T, int N>
void Vector<T,N>::Set(T t0, T t1)
{
    data[0] = t0;
    data[1] = t1;
}

template<class T, int N>
void Vector<T,N>::Set(T t0, T t1, T t2)
{
    data[0] = t0;
    data[1] = t1;
    data[2] = t2;
}

template<class T, int N>
void Vector<T,N>::Set(T t0, T t1, T t2, T t3)
{
    data[0] = t0;
    data[1] = t1;
    data[2] = t2;
    data[3] = t3;
}

///////////////////////////////////////////////////////////
// Implementation of non-arithmetic member operator overloads

template<class T, int N>
inline T& Vector<T,N>::operator [] (int i)
{
    return data[i];
}

template<class T, int N>
inline T Vector<T,N>::operator [] (int i)const
{
    return data[i];
}

template<class T, int N>template<class U>
inline Vector<T,N>::operator Vector<U,N>()const
{
    Vector<U,N> v;
    for(int i = 0; i < N; ++i)
    {
        v[i] = static_cast<U>(data[i]);
    }

    return v;
}

template<class T, int N>
inline Vector<T,N>& Vector<T,N>::operator = (const T vec[N])
{
    for(int i = 0; i < N; i++)
        data[i] = vec[i];
    return *this;
}

#include <iostream>
template<class T, int N>
std::ostream& operator<<(std::ostream &s, const Vector<T,N> &v)
{
    s << "[";
    for(int i = 0; i < N; i++)
    {
        s << " " << v.data[i] << " ";
    }
    s << "]" << std::endl;
    return s;
}

///////////////////////////////////////////////////////////
// Implementation of arithmetic operator overloads which
// take a Vector as a parameter

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator - ()const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = -1*data[i];
    return retVal;

}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator + (Vector<T,N> vec)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] + vec[i];
    return retVal;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator - (Vector<T,N> vec)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] - vec[i];
    return retVal;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator * (Vector<T,N> vec)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] * vec[i];
    return retVal;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator / (Vector<T,N> vec)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] / vec[i];
    return retVal;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator += (Vector<T,N> vec)
{
    for(int i = 0; i < N; i++)
        data[i] += vec[i];
    return *this;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator -= (Vector<T,N> vec)
{
    for(int i = 0; i < N; i++)
        data[i] -= vec[i];
    return *this;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator *= (Vector<T,N> vec)
{
    for(int i = 0; i < N; i++)
        data[i] *= vec[i];
    return *this;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::operator /= (Vector<T,N> vec)
{
    for(int i = 0; i < N; i++)
        data[i] /= vec[i];
    return *this;
}

///////////////////////////////////////////////////////////
// Implementation of arithmetic operator overloads which
// take a scalar as a parameter

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator + (U u)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] + u;
    return retVal;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator - (U u)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] - u;
    return retVal;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator * (U u)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = static_cast<T>(data[i] * u);
    return retVal;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator / (U u)const
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = data[i] / u;
    return retVal;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator += (U u)
{
    for(int i = 0; i < N; i++)
        data[i] += u;
    return *this;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator -= (U u)
{
    for(int i = 0; i < N; i++)
        data[i] -= u;
    return *this;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator *= (U u)
{
    for(int i = 0; i < N; i++)
        data[i] = static_cast<T>(data[i]*u);
    return *this;
}

template<class T, int N>
template<class U>
Vector<T,N> Vector<T,N>::operator /= (U u)
{
    for(int i = 0; i < N; i++)
        data[i] /= u;
    return *this;
}

///////////////////////////////////////////////////////////
// Implementation of global arithmetic operator overloads
// (for when the scalar precedes the Vector)

template<class T, int N>
inline Vector<T,N> operator+(T t,Vector<T,N> v)
{
    return v+t;
}

template<class T, int N>
inline Vector<T,N> operator-(T t,Vector<T,N> v)
{
    return v-t;
}

template<class T, int N>
inline Vector<T,N> operator*(T t,Vector<T,N> v)
{
    return v*t;
}

template<class T, int N>
inline Vector<T,N> operator/(T t,Vector<T,N> v)
{
    return v/t;
}

///////////////////////////////////////////////////////////
// Member Vector function implementations

template<class T, int N>
inline T Vector<T,N>::LengthSqrd()const
{
    T lengthSqrd = 0;
    for(int i = 0; i < N; i++)
        lengthSqrd += data[i]*data[i];
    return lengthSqrd;
}

template<class T, int N>
inline T Vector<T,N>::Length()const
{
    return sqrt(LengthSqrd());
}

template<class T, int N>
inline void Vector<T,N>::SetLength(T len)
{
    len /= Length();
    for(int i = 0; i < N; i++)
        data[i] *= len;
}

template<class T, int N>
inline void Vector<T,N>::Normalize()
{
    T len = Length();
    for(int i = 0; i < N; i++)
        data[i] /= len;
}

template<class T, int N>
inline Vector<T,N> Vector<T,N>::Norm()const
{
    Vector<T,N> v = *this;
    v.Normalize();
    return v;
}


template<class T, int N>
inline bool Vector<T,N>::IsSimilar(const Vector<T,N> &v, T maxDifference)const
{
    for(int i = 0; i < N; i++)
        if(abs(data[i] - v[i]) > maxDifference)
            return false;
    return true;
}
///////////////////////////////////////////////////////////
// Global Vector function implementations

template<class T, int N>
inline T DotProduct(Vector<T,N> v1, Vector<T,N> v2)
{
    T DotProduct = 0;
    for(int i = 0; i < N; i++)
        DotProduct += v1[i]*v2[i];
    return DotProduct;
}

template<class T>
inline Vector<T,3> CrossProduct(const Vector<T,3> &v1, const Vector<T,3> &v2)
{
    Vector<T,3> res;
    res[0] = (v1[1] * v2[2] - v1[2] * v2[1]);
    res[1] = (v1[2] * v2[0] - v1[0] * v2[2]);
    res[2] = (v1[0] * v2[1] - v1[1] * v2[0]);
    return res;
}

template<class T, int N>
inline Vector<T,N> MaxVector(const Vector<T,N> &v1, const Vector<T,N> &v2)
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = max(v1[i],v2[i]);
    return retVal;
}

template<class T, int N>
inline Vector<T,N> MinVector(const Vector<T,N> &v1, const Vector<T,N> &v2)
{
    Vector<T,N> retVal;
    for(int i = 0; i < N; i++)
        retVal[i] = min(v1[i],v2[i]);
    return retVal;
}

// sets the Vector to a Vector which is orthogonal to the original
template<class T>
inline void Orthogonal(Vector<T,3> &v)
{
    if(v[1] == 0 && v[2] == 0)// we're dealing with the x axis
    {// just set it to the y-axis
        v[0] = 0;
        v[1] = 1;
    }
    else
    {
        v[0] = 0;
        T temp = v[1];
        v[1] = v[2];
        v[2] = -temp;
    }
}

///////////////////////////////////////////////////////////
// Rotation fuctions for Vectors in 3-space
// These functions rotate a Vector around an axis by the
// given angle in the clockwise direction (when looking
// along the positive axis). All angles are in radians
template<class T>
inline void RotateXAxis(double cosTheta, double sinTheta, Vector<T,3> &v)
{
    T temp = static_cast<T>(cosTheta*v[1]-sinTheta*v[2]);
    v[2] = static_cast<T>(sinTheta*v[1]+cosTheta*v[2]);
    v[1] = temp;
}

template<class T>
inline void RotateXAxis(double theta, Vector<T,3> &v)
{
    RotateXAxis(cos(theta),sin(theta),v);
}

template<class T>
inline void RotateYAxis(double cosTheta, double sinTheta, Vector<T,3> &v)
{
    T temp = static_cast<T>(cosTheta*v[0]+sinTheta*v[2]);
    v[2] = static_cast<T>(-sinTheta*v[0]+cosTheta*v[2]);
    v[0] = temp;
}

template<class T>
inline void RotateYAxis(double theta, Vector<T,3> &v)
{
    RotateYAxis(cos(theta),sin(theta),v);
}

template<class T>
inline void RotateZAxis(double cosTheta, double sinTheta, Vector<T,3> &v)
{
    T temp = static_cast<T>(cosTheta*v[0]-sinTheta*v[1]);
    v[1] = static_cast<T>(sinTheta*v[0]+cosTheta*v[1]);
    v[0] = temp;
}

template<class T>
inline void RotateZAxis(double theta, Vector<T,3> &v)
{
    RotateZAxis(cos(theta),sin(theta),v);
}

template<class T>
inline void RotateAroundAxis(double cosTheta, double sinTheta, Vector<T,3> &v, const Vector<T,3> &axis)
{
    Vector<T,3> temp;
    temp[0] = static_cast<T>(
        (axis[0]*axis[0]*(1 - cosTheta) + cosTheta)*v[0] +
        (axis[0]*axis[1]*(1 - cosTheta) - axis[2]*sinTheta)*v[1] +
        (axis[0]*axis[2]*(1 - cosTheta) + axis[1]*sinTheta)*v[2]);

    temp[1] = static_cast<T>(
        (axis[0]*axis[1]*(1 - cosTheta) + axis[2]*sinTheta)*v[0] +
        (axis[1]*axis[1]*(1 - cosTheta) + cosTheta)*v[1] +
        (axis[1]*axis[2]*(1 - cosTheta) - axis[0]*sinTheta)*v[2]);

    temp[2] = static_cast<T>(
        (axis[0]*axis[2]*(1 - cosTheta) - axis[1]*sinTheta)*v[0] +
        (axis[1]*axis[2]*(1 - cosTheta) + axis[0]*sinTheta)*v[1] +
        (axis[2]*axis[2]*(1 - cosTheta) + cosTheta)*v[2]);

    v = temp;
}

template<class T>
inline void RotateAroundAxis(double theta, Vector<T,3> &v, const Vector<T,3> &axis)
{
    RotateAroundAxis(cos(theta),sin(theta),v,axis);
}


// Author & Date: Jason Scott 2008
// Purpose: find the angle of rotation from one Vector to another, given the axis of rotation
// Input: v1 - original Vector
//        v2 - Vector after rotation
//        axis - the axis of rotation
// Returns: the angle of rotation, in radians, from v1 to v2,
//          in a clockwise direction (when looking along the axis)
//          This value will always be in the range [-PI, PI]
// Notes: The angle of rotation from one Vector to another is NOT the same as the angle between
//        them. The angle between two Vectors is necessarily positive (in range [0,PI)), while
//        the angle of rotation can account for negative rotations.
//        This function finds the angle between v1 and v2, theta, using the dot product. Then
//        the axis is used to perform test rotations using +theta and -theta.
//        The closest rotation indicates the proper angle of rotation.
template<class T>
inline double AngleOfRotation(const Vector<T,3> &v1, const Vector<T,3> &v2, const Vector<T,3> &axis)
{
    Vector<T,3> v1Norm = v1.Norm();
    Vector<T,3> v2Norm = v2.Norm();
    Vector<T,3> rotTest = v1Norm;
    // calculate cos(theta)
    double theta = DotProduct(v1Norm,v2Norm);
    // because of numerical error, ensure cos(theta) is in [-1,1]
    theta = std::min(1.0, theta);
    theta = std::max(theta, -1.0);
    theta = acos(theta);

    // perform test rotation using +theta
    RotateAroundAxis(theta,rotTest,axis);
    double error = (rotTest - v2Norm).LengthSqrd();
    rotTest = v1Norm;
    // perform test rotation using -theta
    RotateAroundAxis(-theta,rotTest,axis);

    // use whichever rotation comes closer to v2 (has smallest error)
    if(error < (rotTest-v2Norm).LengthSqrd())
        return theta;
    else return -theta;
}

// Author & Date: Jason Scott August 7 2009
// Purpose: find the angles of rotation to move from the standard coordinate frame to
//          the input coordinate frame.
//          In other words, applying the resulting rotations
//          (RotateXAxis(thetaX); RotateYAxis(thetaY); RotateZAxis(thetaZ); (IN THAT ORDER!))
//          to vectors along the coordinate axes will yield vectors along the new coordinate
//          frame.

// Input:
//  x - x-axis of the input coordinate frame
//  y - y-axis of the input coordinate frame
//  z - z-axis of the input coordinate frame
//  (x,y,z) should form a right-hand coordinate frame (ie. CrossProduct(|x|,|y|) = |z|)
// Output:
//  thetaX - x rotation angle. Must be applied first
//  thetaY - y rotation angle. Must be applied second
//  thetaZ - z rotation angle. Must be applied third
template<class T>
void GetRotationAngles(Vector<T,3> x, Vector<T,3> y, Vector<T,3> z, float &thetaX, float &thetaY, float &thetaZ)
{
    Vector<T,3> xAxis(1,0,0);
    Vector<T,3> yAxis(0,1,0);
    Vector<T,3> zAxis(0,0,1);

    // find thetaX
    // Project the z vector onto the yz plane and find the rotation needed to align it with the z-axis
    Vector<T,3> yzProj = z - xAxis*DotProduct(z, xAxis);
    thetaX = static_cast<float>(AngleOfRotation(yzProj, zAxis, xAxis));

    // Rotate the remaining vectors accordingly
    RotateXAxis(thetaX, x);
    RotateXAxis(thetaX, z);

    // find thetaY
    // Project the z vector onto the xz plane and find the rotation needed to align it with the z-axis
    Vector<T,3> xzProj = z - yAxis*DotProduct(z, yAxis);
    thetaY = static_cast<float>(AngleOfRotation(xzProj, zAxis, yAxis));

    // Rotate the remaining vectors accordingly
    RotateYAxis(thetaY, x);

    // find thetaZ
    // Project the x vector onto the xy plane and find the rotation needed to align it with the x-axis
    Vector<T,3> xyProj = x - zAxis*DotProduct(x, zAxis);
    thetaZ = static_cast<float>(AngleOfRotation(xyProj, xAxis, zAxis));

    thetaX = -thetaX;
    thetaY = -thetaY;
    thetaZ = -thetaZ;
}

template<class T>
inline void GetRotationAngles(const Vector<T,3> &x, const Vector<T,3> &y, const Vector<T,3> &z, float afTheta[3])
{
    GetRotationAngles(x, y, z, afTheta[0], afTheta[1], afTheta[2]);
}

// Author & Date: Jason Scott August 7 2009
// Purpose: Apply three angles of rotation to a vector.
//          Works in conjunction with GetRotationAngles
// Input:
//  v      - the vector to be rotated
//  thetaX - x rotation angle. Will be applied first
//  thetaY - y rotation angle. Will be applied second
//  thetaZ - z rotation angle. Will be applied third
template<class T>
inline void ApplyRotationAngles(Vector<T,3> &v, float thetaX, float thetaY, float thetaZ)
{
    RotateXAxis(thetaX, v);
    RotateYAxis(thetaY, v);
    RotateZAxis(thetaZ, v);
}

template<class T>
inline void ApplyRotationAngles(Vector<T,3> &v, const float afTheta[3])
{
    ApplyRotationAngles(v, afTheta[0], afTheta[1], afTheta[2]);
}


///////////////////////////////////////////////////////////
// Some typedefs for common type low degree Vectors
typedef Vector<double,4> Vector4d;
typedef Vector<double,3> Vector3d;
typedef Vector<double,2> Vector2d;
typedef Vector<float,4> Vector4f;
typedef Vector<float,3> Vector3f;
typedef Vector<float,2> Vector2f;
typedef Vector<int,4> Vector4i;
typedef Vector<int,3> Vector3i;
typedef Vector<int,2> Vector2i;

// an NxN matrix
template<class T, int N>
struct matrix
{
    T data[N*N];
};
// Data point from the NSP. Combination of unit and position
struct data_point
{
    unsigned int unit;
    Vector3f point;
};

#endif // include guard
