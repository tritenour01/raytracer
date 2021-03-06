#include "raytracer.h"
#include "parser.h"

Raytracer::Raytracer(void)
{
    //set the default
    config.mode = Config::STANDARD;
    config.width = 100;
    config.height = 100;
    config.camera = NULL;
    config.ambient = 0.0f;
    config.backColor = Vector3(0.0f, 0.0f, 0.0f);
    config.reflectionDepth = 1;
    config.glossyReflectSampling = 1;
    config.glossyRefractSampling = 1;
    config.recursionThreshold = 1.0f / 256.0f;
    config.photonCount = 0;

    config.gamma = 1.0f;

    config.sampler = NULL;

    parser = new Parser(this);

    photonMap = NULL;

    //srand(time(NULL));
    srand(0);
}

Raytracer::~Raytracer(void)
{
    for(int i = 0; i < objects.size(); i++)
        delete objects.at(i);

    for(int i = 0; i < lights.size(); i++)
        delete lights.at(i);

    if(config.camera)
       delete config.camera;
    delete config.sampler;
    delete parser;
}

bool Raytracer::loadScene(string fileName)
{
    //load the scene into the tracer
    bool result = parser->loadScene(fileName, config);

    if(config.sampler == NULL)
        config.sampler = new simpleSampler(this, config);
    if(config.camera == NULL)
        config.camera = new Camera(Vector3(0, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0), config.width, config.height);

    if(config.mode == Config::PHOTON)
        setupPhotonMap();

    return result;
}

void Raytracer::setupPhotonMap(void)
{
    photonMap = new PhotonMap();

    float totalPower = 0.0f;
    for(int i = 0; i < lights.size(); i++)
        totalPower += lights[i]->getIntensity();

    for(int i = 0; i < lights.size(); i++){
        int numPhotons = (lights[i]->getIntensity() / totalPower) * config.photonCount;
        lights[i]->emitPhotons(*photonMap, numPhotons, config.photonBounces);
    }

    photonMap->setup();
}

//return the image width
int Raytracer::getWidth(void)
{
    return config.width;
}

//return the image height
int Raytracer::getHeight(void)
{
    return config.height;
}

void Raytracer::addObject(Shape* newObject)
{
    objects.push_back(newObject);
}

void Raytracer::addLight(Light* newLight)
{
    lights.push_back(newLight);
}

Vector3 Raytracer::tracePixel(int x, int y)
{
    Vector3 result = config.sampler->samplePixel(x, y);
    gammaCorrection(result);
    return result;
}

Vector3 Raytracer::traceRay(Ray& ray)
{
    intersectRay(ray);

    //if(ray.s){
    //    float ao = calculateAO(ray, 1);
    //    return 0.1f * ray.s->getMaterial().getDiffuse(ray) * ao;
    //}
    //return Vector3(1, 1, 1);

    if(ray.s)
        return computeColor(ray, 0, 1.0f);
    return config.backColor;
}

void Raytracer::gammaCorrection(Vector3& color)
{
    for(int i = 0; i < 3; i++)
        color.elements[i] = pow(color.elements[i], 1.0f / config.gamma);
}

//test the ray against all objects
bool Raytracer::intersectRay(Ray& ray)
{
    float minT = DBL_MAX;
    Shape* minS = NULL;
    Vector3 point;
    Hitpoint hit;
    float f1;
    float f2;
    Shape* s;
    for(int k = 0; k < objects.size(); k++){
        if(objects.at(k)->intersectRay(ray, hit)){
            if(hit.t > Ray::SMALL && hit.t < minT){
                minT = hit.t;
                minS = objects.at(k);
                point = hit.point;
                f1 = hit.f1;
                f2 = hit.f2;
                s = hit.s;
            }
        }
    }

    ray.t = minT;
    ray.s = minS;
    ray.point = point;
    ray.cacheFloat1 = f1;
    ray.cacheFloat2 = f2;
    ray.cacheShape = s;

    if(!minS)
        return false;

    return true;
}

float Raytracer::computeShadowFactor(Ray& ray, float range)
{
    float minT = DBL_MAX;
    Hitpoint hit;
    float factor = 1.0f;
    for(int k = 0; k < objects.size(); k++){
        if(objects.at(k)->getMaterial().isEmissive())
            continue;
        if(objects.at(k)->intersectRay(ray, hit)){
            if(hit.t > Ray::SMALL && hit.t <= range && hit.t < minT){
                minT = hit.t;
                factor = 0.0f;
                break;
            }
        }
    }

    ray.t = minT;
    return factor;
}

//determine the color based on the intersection point
Vector3 Raytracer::computeColor(Ray& ray, int depth, float factor)
{
    //recursive base case
    if(depth > config.reflectionDepth)
        return Vector3(0, 0, 0);

    //contrabution to recursion is too small
    if(factor < config.recursionThreshold)
        return Vector3(0, 0, 0);

    if(ray.s->getMaterial().isEmissive())
        return ray.s->getMaterial().getEmissiveColor();

    //normal at point
    Vector3 n = ray.s->computeNormal(ray);

    //reflection amount
    Vector3 reflection(0, 0, 0);
    if(ray.s->getMaterial().getReflective() > 0.0f){
        if(ray.s->getMaterial().getGlossiness() > 0.0f && depth == 0)
            reflection = calculateGlossyReflection(ray, n, depth, factor);
        else
            reflection = calculateReflection(ray, n, depth, factor);
    }

    //refraction amount
    Vector3 refraction(0, 0, 0);
    if(ray.s->getMaterial().getRefraction() > 0.0f){
        if(ray.s->getMaterial().getGlossiness() > 0.0f && depth == 0)
            refraction = calculateGlossyRefraction(ray, n, depth, factor);
        else
            refraction = calculateRefraction(ray, n, depth, factor);
    }

    //final color
    Vector3 color = calculateLightStandard(ray, n) + reflection + refraction;
    if(config.mode == Config::PHOTON)
        color += calculateLightPhoton(ray, n);
    return color;
}

//computes the light calculations
Vector3 Raytracer::calculateLightStandard(Ray& ray, Vector3& n)
{
    Vector3 diffuse = ray.s->getMaterial().getDiffuse(ray);

    Light* current;
    Vector3 totalColor = Vector3(0, 0, 0);

    //compute the effect of every light in the scene
    for(int i = 0; i < lights.size(); i++){
        current = lights[i];

        totalColor += current->illuminate(ray, n, diffuse);
    }

    Vector3 amb = /*calculateAO(ray, 100) * */config.ambient * diffuse;

    return amb + totalColor;
}

Vector3 Raytracer::calculateLightPhoton(Ray& ray, Vector3& n)
{
    Vector3 diffuse = ray.s->getMaterial().getDiffuse(ray);
    Vector3 amb = /*calculateAO(ray, 100) * */config.ambient * diffuse;

    Vector3 color(0, 0, 0);

    std::vector<Photon*> photons;
    photonMap->nearestN(ray.point, n, photons, config.maxPhotonSamples, config.photonSearchRadius);

    if(photons.size() == 0)
        return Vector3(0, 0, 0);

    float r = 0.0f;
    for(int i = 0; i < photons.size(); i++){
        Vector3 dir = ray.point - photons[i]->pos;
        float len = dir.getSqrLength();
        if(len > r)
            r = len;
    }

    float alpha = 0.918f;
    float beta = 1.953f;

    for(int i = 0; i < photons.size(); i++){
        Vector3 l = -photons[i]->direction;
        l.normalize();
        Vector3 factor = (1.0f / 3.141592) * diffuse * ray.s->getMaterial().getDiffuseFactor() * max(Vector3::DotProduct(l, n), 0.0f);
        Vector3 power = photons[i]->power;
        factor.x *= power.x;
        factor.y *= power.y;
        factor.z *= power.z;

        Vector3 dir = ray.point - photons[i]->pos;
        float len = dir.getSqrLength();

        //float gauss = alpha * (1.0f - ((1.0f - std::exp(-beta * len * len / (2.0f * r * r))) / (1.0f - std::exp(-beta))));

        color += factor;// * gauss;
    }

    float density = (3.141592653f * r * r);
    return color * (1.0f / density);
}

Vector3 Raytracer::calculateShading(Ray& ray, Vector3& n, Vector3& l, Vector3& diffuse)
{
    Vector3 color = Vector3(0, 0, 0);
    if(ray.s->getMaterial().getDiffuseFactor() > 0.0){
        //diffuse term
        //color += diffuse * ray.s->getMaterial().getDiffuseFactor() * max(Vector3::DotProduct(l, n), 0.0f);
        float roughness = 0.0f;
        float rSqrd = roughness * roughness;
        float A = 1.0f - 0.5f * (rSqrd / (rSqrd + 0.33f));
        float B = 0.45f * (rSqrd / (rSqrd + 0.09f));

        //view direction
        Vector3 v = ray.origin - ray.point;
        v.normalize();

        float cos1 = Vector3::DotProduct(v, n);
        float cos2 = Vector3::DotProduct(l, n);

        float sin1 = sqrtf(1.0f - cos1 * cos1);
        float sin2 = sqrtf(1.0f - cos2 * cos2);

        float s;
        float t;
        if(cos1 <= cos2){
            s = sin1;
            t = sin2 / cos2;
        }
        else{
            s = sin2;
            t = sin1 / cos1;
        }

        Vector3 VonXY = v - n * cos1;
        Vector3 LonXY = l - n * cos2;
        float C = max(0.0f, Vector3::DotProduct(VonXY, LonXY));
        //float C = max(0.0f, cos1 * cos2 + sin1 * sin2);

        Vector3 temp = (1.0f / 3.141592) * diffuse * ray.s->getMaterial().getDiffuseFactor() * max(0.0f, cos2) * (A + (B * C * s * t));
        color += temp;
    }
    if(ray.s->getMaterial().getSpecularFactor() > 0.0){

        //light reflection vector
        Vector3 r = (2.0f * Vector3::DotProduct(n, l) * n) - l;
        r.normalize();

        //view direction
        Vector3 v = ray.origin - ray.point;
        v.normalize();

        //specular term
        color += ray.s->getMaterial().getSpecularFactor() * ray.s->getMaterial().getSpecular() * pow(max(0.0f, Vector3::DotProduct(v, r)), ray.s->getMaterial().getShineness());
    }

    return color;
}

//computes the reflection color
Vector3 Raytracer::calculateReflection(Ray& ray, Vector3& n, int depth, float factor)
{
    //view direction
    Vector3 view = -ray.dir;

    //reflection vector
    Vector3 R = (2.0f * Vector3::DotProduct(n, view) * n) - view;

    //compute the reflection color
    Ray reflect = Ray(ray.point, R);
    Vector3 c;
    if(intersectRay(reflect)){
        c = ray.s->getMaterial().getReflective() * computeColor(reflect, depth + 1, factor * ray.s->getMaterial().getReflective());
    }
    else
        c = ray.s->getMaterial().getReflective() * config.backColor;

    Vector3 filter = ray.s->getMaterial().getReflectColor();
    c.x *= filter.x;
    c.y *= filter.y;
    c.z *= filter.z;

    return c;
}

bool Raytracer::refractVector(Vector3& normal, Vector3& view, Vector3& result, float IOR)
{
    bool TIR = false;
    float n = 1.0f;
    float nt = IOR;

    //entering the object
    if(Vector3::DotProduct(view, normal) < 0){
        //refraction index
        float compN = n / nt;

        //compute the refracted direction
        double c1, cs2;
        c1 = -Vector3::DotProduct(view, normal);
        cs2 = 1.0f - compN * compN * (1.0f - c1 * c1);
        result = compN * view + (compN * c1 - sqrt(cs2)) *  normal;
        result.normalize();
    }
    //exiting the glass
    else{
        //refraction index
        float compN = nt / n;

        double c1, cs2;
        c1 = Vector3::DotProduct(view, normal);
        cs2 = 1.0f - compN * compN * (1.0f - c1 * c1);

        if(cs2 < 0.0f)
            TIR = true;

        if(!TIR){
            //compute the refracted direction
            result = compN * view - (compN * c1 - sqrt(cs2)) *  normal;
            result.normalize();
        }
    }
    return TIR;

}

//compute the refraction color
Vector3 Raytracer::calculateRefraction(Ray& ray, Vector3& normal, int depth, float factor)
{
    //view direction
    Vector3 view = ray.dir;
    view.normalize();

    float cos1;
    float cos2;
    float n = 1.0f;
    float nt = ray.s->getMaterial().getIOR();

    Vector3 result;
    //bool TIR = refractVector(normal, view, result, nt);
    bool TIR = refractVector(normal, view, result, nt);

    if(Vector3::DotProduct(view, normal) < 0){
        cos1 = -Vector3::DotProduct(view, normal);
        cos2 = -Vector3::DotProduct(normal, result);
    }
    else{
        cos1 = Vector3::DotProduct(view, normal);
        cos2 = Vector3::DotProduct(normal, result);
    }

    //reflection vector
    Vector3 R = (2.0f * Vector3::DotProduct(normal, -view) * normal) + view;

    float parallel = (nt * cos1 - n * cos2) / (nt * cos1 + n * cos2);
    float perp = (n * cos1 - nt * cos2) / (n * cos1 + nt * cos2);

    float reflectComp = 0.5f * (parallel * parallel + perp * perp);

    //compute the reflection color
    Ray reflect = Ray(ray.point, R);
    Vector3 c;
    if(intersectRay(reflect)){
        if(TIR)
            c = computeColor(reflect, depth + 1, factor);
        else
            c = computeColor(reflect, depth + 1, factor * reflectComp);
    }
    else
        c = config.backColor;

    if(TIR)
        return c;

    //compute the refraction color
    Ray refract(ray.point, result);
    if(!intersectRay(refract))
        return config.backColor;
    Vector3 color = (1.0f - reflectComp) * ray.s->getMaterial().getRefraction() * computeColor(refract, depth + 1, factor  * (1.0f - reflectComp));
    return color + reflectComp * c;
}

float Raytracer::calculateAO(Ray& ray, int samples)
{
    int samplesX = 10;
    int samplesY = 10;
    Vector3 n = ray.s->computeNormal(ray);
    int hits = samplesX * samplesY;

    Vector3 tangent;
    if(fabs(n.x) >= Ray::SMALL || fabs(n.z) >= Ray::SMALL)
        tangent = Vector3::CrossProduct(n, Vector3(0, 1, 0));
    else
        tangent = Vector3(1, 0, 0);

    Vector3 bitangent = Vector3::CrossProduct(tangent, n);
    tangent.normalize();
    bitangent.normalize();

    Matrix4x4 tangentSpace;
    tangentSpace[0] = tangent.x;
    tangentSpace[1] = tangent.y;
    tangentSpace[2] = tangent.z;
    tangentSpace[4] = bitangent.x;
    tangentSpace[5] = bitangent.y;
    tangentSpace[6] = bitangent.z;
    tangentSpace[8] = n.x;
    tangentSpace[9] = n.y;
    tangentSpace[10] = n.z;

    float currentX = 0.0f;
    float currentY = 0.0f;

    float Xoffset = 2.0f * 3.1415938f / (float)samplesX;
    float Yoffset = 1.0f / (float)samplesY;

    float maxAngle = 75.0f;
    float m = cosf(maxAngle * 3.1415938f / 180.0f);

    for(int i = 0; i < samplesY; i++){
        for(int j = 0; j < samplesY; j++){
            float u = (float)(rand() % 1025) / 1024.0f * Yoffset + currentY;
            float v = (float)(rand() % 1025) / 1024.0f * Xoffset;
            float theta = 2.0f * 3.1415938f * v;
            float factor = sqrtf(1.0f - powf(1.0f - u * (1.0f - m), 2.0f));
            Vector3 d(factor * cosf(theta + currentX), factor * sinf(theta + currentX), 1 - u * (1.0f - m));

            Vector3 dir;
            Matrix4x4::transformDirection(tangentSpace, dir, d);

            Ray test(ray.point, dir);
            if(intersectRay(test))
                hits--;

            currentX += Xoffset;
        }
        currentX = 0.0f;
        currentY += Yoffset;
    }

    return (float)hits / ((float)samplesX * (float)samplesY);
}

Vector3 Raytracer::calculateGlossyReflection(Ray& ray, Vector3& n, int depth, float factor)
{
    Vector3 view = -ray.dir;
    Vector3 R = (2.0f * Vector3::DotProduct(n, view) * n) - view;
    R.normalize();

    Vector3 tangent;
    if(fabs(R.x) >= Ray::SMALL || fabs(R.z) >= Ray::SMALL)
        tangent = Vector3::CrossProduct(R, Vector3(0, 1, 0));
    else
        tangent = Vector3(1, 0, 0);

    Vector3 bitangent = Vector3::CrossProduct(tangent, R);
    tangent.normalize();
    bitangent.normalize();

    float currentX = 0.0f;
    float currentY = 0.0f;

    float Xoffset = 2.0f * 3.1415938f / (float)config.glossyReflectSampling;
    float Yoffset = 1.0f / (float)config.glossyReflectSampling;

    float angle = 80.0f * ray.s->getMaterial().getGlossiness();
    float diskSize = tanf(angle * 3.1415938f / 360.0f);

    Vector3 color(0, 0, 0);

    for(int i = 0; i < config.glossyReflectSampling; i++){
        for(int j = 0; j < config.glossyReflectSampling; j++){
            float u = (float)rand() / RAND_MAX * Yoffset + currentY;
            float v = (float)rand() / RAND_MAX * Xoffset;
            float theta = 2.0f * 3.1415938f * v;

            Vector3 d = R + (u * diskSize * cosf(theta + currentX) * tangent) + (u * diskSize * sinf(theta + currentX) * bitangent);

            if(Vector3::DotProduct(d, n) > 0){
                Ray test(ray.point, d);
                if(intersectRay(test)){
                    color += computeColor(test, depth + 1, factor * ray.s->getMaterial().getReflective() / (float)config.glossyReflectSampling);
                }
                else
                    color += config.backColor;
            }
            else{
                d = R - (u * diskSize * cosf(theta + currentX) * tangent) - (u * diskSize * sinf(theta + currentX) * bitangent);
                Ray test(ray.point, d);
                if(intersectRay(test)){
                    color += computeColor(test, depth + 1, factor * ray.s->getMaterial().getReflective() / (float)config.glossyReflectSampling);
                }
                else
                    color += config.backColor;
            }

            currentX += Xoffset;
        }
        currentX = 0.0f;
        currentY += Yoffset;
    }

    Vector3 filter = ray.s->getMaterial().getReflectColor();
    color.x *= filter.x;
    color.y *= filter.y;
    color.z *= filter.z;

    return color * ray.s->getMaterial().getReflective() * (1.0f / ((float)config.glossyReflectSampling * (float)config.glossyReflectSampling));
}

Vector3 Raytracer::calculateGlossyRefraction(Ray& ray, Vector3& normal, int depth, float factor)
{
    //view direction
    Vector3 view = ray.dir;
    view.normalize();

    float cos1;
    float cos2;
    float n = 1.0f;
    float nt = 1.5f;

    Vector3 result;
    bool TIR = refractVector(normal, view, result, nt);

    if(Vector3::DotProduct(view, normal) < 0){
        cos1 = -Vector3::DotProduct(view, normal);
        cos2 = -Vector3::DotProduct(normal, result);
    }
    else{
        cos1 = Vector3::DotProduct(view, normal);
        cos2 = Vector3::DotProduct(normal, result);
    }

    //reflection vector
    Vector3 refl = (2.0f * Vector3::DotProduct(normal, -view) * normal) + view;

    float parallel = (nt * cos1 - n * cos2) / (nt * cos1 + n * cos2);
    float perp = (n * cos1 - nt * cos2) / (n * cos1 + nt * cos2);

    float reflectComp = 0.5f * (parallel * parallel + perp * perp);

    //compute the reflection color
    Ray reflect = Ray(ray.point, refl);
    Vector3 c;
    if(intersectRay(reflect)){
        if(TIR)
            c = computeColor(reflect, depth + 1, factor);
        else
            c = computeColor(reflect, depth + 1, factor * reflectComp);
    }
    else
        c = config.backColor;

    if(TIR)
        return c;

    Vector3 tangent;
    if(fabs(result.x) >= Ray::SMALL || fabs(result.z) >= Ray::SMALL)
        tangent = Vector3::CrossProduct(result, Vector3(0, 1, 0));
    else
        tangent = Vector3(1, 0, 0);

    Vector3 bitangent = Vector3::CrossProduct(tangent, result);
    tangent.normalize();
    bitangent.normalize();

    float currentX = 0.0f;
    float currentY = 0.0f;

    float Xoffset = 2.0f * 3.1415938f / (float)config.glossyReflectSampling;
    float Yoffset = 1.0f / (float)config.glossyReflectSampling;

    float angle = 80.0f * ray.s->getMaterial().getGlossiness();
    float diskSize = tanf(angle * 3.1415938f / 360.0f);

    Vector3 color(0, 0, 0);

    for(int i = 0; i < config.glossyRefractSampling; i++){
        for(int j = 0; j < config.glossyRefractSampling; j++){
            float u = (float)rand() / RAND_MAX * Yoffset + currentY;
            float v = (float)rand() / RAND_MAX * Xoffset;
            float theta = 2.0f * 3.1415938f * v;

            Vector3 d = result + (u * diskSize * cosf(theta + currentX) * tangent) + (u * diskSize * sinf(theta + currentX) * bitangent);

            if(Vector3::DotProduct(d, normal) > 0){
                Ray test(ray.point, d);
                if(intersectRay(test)){
                    color += computeColor(test, depth + 1, factor * ray.s->getMaterial().getRefraction() / (float)config.glossyRefractSampling);
                }
                else
                    color += config.backColor;
            }
            else{
                d = result - (u * diskSize * cosf(theta + currentX) * tangent) - (u * diskSize * sinf(theta + currentX) * bitangent);
                Ray test(ray.point, d);
                if(intersectRay(test)){
                    color += computeColor(test, depth + 1, factor * ray.s->getMaterial().getRefraction() / (float)config.glossyRefractSampling);
                }
                else
                    color += config.backColor;
            }

            currentX += Xoffset;
        }
        currentX = 0.0f;
        currentY += Yoffset;
    }

    return reflectComp * c + color * (1.0f - reflectComp) * ray.s->getMaterial().getRefraction() * (1.0f / ((float)config.glossyRefractSampling * (float)config.glossyRefractSampling));
}
