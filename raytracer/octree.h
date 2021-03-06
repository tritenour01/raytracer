#ifndef OCTREE_H_INCLUDED
#define OCTREE_H_INCLUDED

#include <vector>
#include "vector.h"
#include "ray.h"
#include "box.h"
#include "debug.h"
#include "log.h"

class Triangle;

struct Node
{
    int num;
    Node* parent;
    Node* children;
    vector<Triangle*> data;
};

struct BoundInfo
{
    float Min[3];
    float Max[3];
    float lmax;
    float umin;
};

class Octree
{
    public:

        Octree(int, int);
        ~Octree(void);

        void createTree(vector<Triangle*>*);
        bool intersectRay(Ray&, Hitpoint&);
        Vector3 getNormal(Ray&);
        void getUV(Vector3&, Ray&, float&, float&);

    private:

        bool triangleAABBIntersect(Triangle*, Vector3*);
        void projectTriangle(Triangle*, Vector3&, float&, float&);
        void projectAABB(int, Vector3*, Vector3&, float&, float&);
        bool intervalsOverlap(float, float, float, float);
        bool pointInAABB(Vector3&, Vector3&, Vector3&);

        void insertTriangles(vector<Triangle*>&, Vector3, Vector3, Node*, int);
        void createNodes(Node*);

        void computeBounds(vector<Triangle*>, Vector3&, Vector3&);
        void computePoints(Vector3*, float*, float*);
        void computeNum(Node*);
        void computeIntersectRange(char, BoundInfo&, float*, float*, float*);

        bool intersectSubTrees(Ray&, Hitpoint&, Node*, BoundInfo&, char);

        void deleteTree(Node*);

        Node* root;
        int maxData;
        int maxDepth;

        Vector3 minBound;
        Vector3 maxBound;

        const int d = 10;
};

#endif // OCTREE_H_INCLUDED
