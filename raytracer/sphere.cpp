#include "sphere.h"
#include "raytracer.h"

Sphere::Sphere(Vector3 c, float r)
{
    //set the sphere attributes
    center = c;
    radius = r;
}

bool Sphere::Intersection(Ray& r, Hitpoint& h)
{
    //compute the A, B, and C values of the polynomial
    float B = Vector3::DotProduct(2.0 * r.dir, r.origin - center);
    float A = Vector3::DotProduct(r.dir, r.dir);
    float C = Vector3::DotProduct(r.origin - center, r.origin - center) - pow(radius, 2.0);

    //compute the discriminite
    float discriminent = pow(B, 2.0) - (4.0 * A * C);

    //no intersection (imaginary root)
    if(discriminent < 0.0f)
        return false;

    //compute closest t
    float sqrtDisc = sqrt(discriminent);
    float t1 = (-B + sqrtDisc) / (2.0f * A);
    float t2 = (-B - sqrtDisc) / (2.0f * A);

    if(t1 > t2){
        float temp = t1;
        t1 = t2;
        t2 = temp;
    }

    if(t1 < Ray::SMALL)
        h.t = t2;
    else
        h.t = t1;

    return true;
}

//return the normal based on the intersection point
Vector3 Sphere::getNormal(Ray& ray)
{
    if(isTransformed){
        Vector3 newPoint;
        Matrix4x4::transformPoint(invTrans, newPoint, ray.point);
        return newPoint - center;
    }
    return ray.point - center;
}

void Sphere::getUV(Vector3& point, Ray& ray, float& u, float& v)
{
    Vector3 n = point - center;
    n.normalize();
    u = 0.5f + (atan2(n.z, n.x) / (2.0f * 3.141592653f));
    v = 0.5f + (asin(n.y) / 3.141592653f);
}
