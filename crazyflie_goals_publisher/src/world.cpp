#include <cmath>
#include "world.h"


Occupator::Occupator(const std::string &name, double x0, double y0, double z0)
    : name		(name)
    , x			(x0)
    , y			(y0)
    , z			(z0)
    , region 	(nullptr)
{}


void Occupator::setXYZ(double x, double y, double z) {
    this->x = x;
    this->y = y;
    this->z = z;
}


Region::Region()
    : m_occupationMutex	()
    , m_owner		(nullptr)
{}


inline bool Region::isFree() const {
    return (m_owner == nullptr);
}


// Amount of blocks on each of axis
#define dimOZ m_regions.size()
#define dimOY m_regions[0].size()
#define dimOX m_regions[0][0].size()


World::World(
    double worldWidth, double worldLength, double worldHeight,
    double regWidth,   double regLength,   double regHeight,
    double offsetOX,   double offsetOY,    double offsetOZ)
    : m_regWidth            (regWidth)
    , m_regLength           (regLength)
    , m_regHeight           (regHeight)
    , m_offsetOX            (offsetOX)
    , m_offsetOY            (offsetOY)
    , m_offsetOZ            (offsetOZ)
    , m_registerMutex		 ()
{
    double _dimOZ = std::ceil(worldHeight / regHeight);
    double _dimOY = std::ceil(worldLength / regLength);
    double _dimOX = std::ceil(worldWidth  / regWidth);

    // Fill the world
    for (size_t i = 0; i < _dimOZ; ++i) {
        std::vector<std::vector<Region *>> vecOY;

        for (size_t j = 0; j < _dimOY; ++j) {
            std::vector<Region *> vecOX;

            for (size_t k = 0; k < _dimOX; ++k)
                vecOX.push_back(new Region);
            vecOY.push_back(vecOX);
        }

        m_regions.push_back(vecOY);
    }
}


World::~World() {
    for (size_t i = 0; i < dimOZ; ++i)
        for (size_t j = 0; j < dimOY; ++j)
            for (size_t k = 0; k < dimOX; ++k)
                delete m_regions[i][j][k];
}


inline double World::moveX(double x) const {
    return x + m_offsetOX;
}

inline double World::moveY(double y) const {
    return y + m_offsetOY;
}

inline double World::moveZ(double z) const {
    return z + m_offsetOZ;
}


bool World::addOccupator(Occupator &occupator) {
    std::lock_guard<std::mutex> locker(m_registerMutex);

    double movedX = moveX(occupator.x);
    double movedY = moveX(occupator.y);
    double movedZ = moveX(occupator.z);

    // Get region that containes occupator position
    size_t xNum = movedX / m_regWidth;
    size_t yNum = movedY / m_regLength;
    size_t zNum = movedZ / m_regHeight;

    // Checking if occupator crosses border of the world
    if (xNum >= dimOX || movedX < 0 ||
        yNum >= dimOY || movedY < 0 ||
        zNum >= dimOZ || movedZ < 0)
    {
        ROS_ERROR("%s%s", occupator.name.c_str(), " is outside of the world!");
        return false;
    }

    Region *currReg = m_regions[zNum][yNum][xNum];

    // Cheking if distances between each of occupators are ok
    bool distancesAreOk = true;
    double eps = 0.4;

    for (tf::Vector3 point: m_registrationPoints)
        if (std::sqrt(std::pow(movedX - point.x(), 2) + std::pow(movedY - point.y(), 2)) < eps) {
            distancesAreOk = false;
            break;
        }

    if (!currReg->isFree() || !distancesAreOk) {
        ROS_ERROR("Could not register %s%s", occupator.name.c_str(),
                  ": is too close to other occupator!");
        return false;
    }

    // Occupy starting region
    currReg->m_owner = &occupator;
    occupator.region = currReg;

    m_registrationPoints.push_back(tf::Vector3(movedX, movedY, movedZ));

    return true;
}


bool World::occupyRegion(Occupator &occupator, double x, double y, double z) {
    // Get region that containes occupator position
    size_t xNum = moveX(occupator.x) / m_regWidth;
    size_t yNum = moveY(occupator.y) / m_regLength;
    size_t zNum = moveZ(occupator.z) / m_regHeight;
    Region *selectedReg = m_regions[zNum][yNum][xNum];

    std::lock_guard<std::mutex> locker(selectedReg->m_occupationMutex);

    if (selectedReg->isFree()) {
        // free old region of ocuppator
        occupator.region->m_owner = nullptr;

        // occupy new region
        occupator.region = selectedReg;
        selectedReg->m_owner = &occupator;

        return true;
    }
    // If occupator have been owning selected region
    else if (selectedReg->m_owner == &occupator)
        return true;

    return false;
}


bool World::isAtSafePosition(const Occupator &occupator, double eps) const {
    double x = moveX(occupator.x);
    double y = moveY(occupator.y);
    double z = moveZ(occupator.z);

    size_t oldX = x / m_regWidth;
    size_t oldY = y / m_regLength;
    size_t oldZ = z / m_regHeight;

    size_t regX = m_regWidth  / 3;
    size_t regY = m_regLength / 3;
    size_t regZ = m_regHeight / 3;

    size_t newX = oldX + ((oldX < regX && oldX > 0)? -1 : 0) + ((oldX > 2 * regX && oldX + 1 < dimOX)? 1 : 0);
    size_t newY = oldY + ((oldY < regY && oldY > 0)? -1 : 0) + ((oldY > 2 * regY && oldY + 1 < dimOY)? 1 : 0);
    size_t newZ = oldZ + ((oldZ < regZ && oldZ > 0)? -1 : 0) + ((oldZ > 2 * regZ && oldZ + 1 < dimOZ)? 1 : 0);

    // Calculate distance between point (x, y, z) and selected region
    auto dist = [this](double x, double y, double z, const Region *region) -> double {
        double occupator_x = moveX(region->m_owner->x);
        double occupator_y = moveY(region->m_owner->y);
        double occupator_z = moveZ(region->m_owner->z);

        if (fabs(z - occupator_z) > m_regHeight / 2) {
            return std::sqrt(std::pow(x - occupator_x, 2) +
                             std::pow(y - occupator_y, 2) +
                             std::pow(z - occupator_z, 2));
        }

        return std::sqrt(std::pow(x - occupator_x, 2) +
                         std::pow(y - occupator_y, 2));
    };

    Region *reg = nullptr;
    size_t isNewX = newX - oldX;
    size_t isNewY = newY - oldY;
    size_t isNewZ = newZ - oldZ;

    /*
     * Find nearest occupied regions and
     * calculate distances from their owners to point (x, y, z)
     */

    reg = m_regions[oldZ][oldY][newX];
    if (!isNewZ && !isNewY && isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    reg = m_regions[oldZ][newY][oldX];
    if (!isNewZ && isNewY && !isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    reg = m_regions[oldZ][newY][newX];
    if (!isNewZ && isNewY && isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    reg = m_regions[newZ][oldY][oldX];
    if (isNewZ && !isNewY && !isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    reg = m_regions[newZ][oldY][newX];
    if (isNewZ && !isNewY && isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    reg = m_regions[newZ][newY][oldX];
    if (isNewZ && isNewY && !isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    reg = m_regions[newZ][newY][newX];
    if (isNewZ && isNewY && isNewX && !reg->isFree())
        if (dist(x, y, z, reg) < eps)
            return false;

    return true;
}


tf::Vector3 World::getFreeCenter(const Occupator &occupator) const {
    /*
     * If there is deadlock then check ways to step back like in picture
     *    |----|----|----|
     *    |    |    |    |
     *    |    | 1  |    |
     *    |----|----|----|
     *    |    |    |    |
     *    |  2 | X  | 4  |
     *    |----|----|----|
     *    |    |    |    |
     *    |    | 3  |    |
     *    |----|----|----|
     * If there are no ways to step back then check upward way
     * If it is not free then return point (x, y, z) itself
     */

    struct Step {
        long long x, y, z;

        Step(long long x,
             long long y,
             long long z) :
            x(x), y(y), z(z) {}
    };

    std::vector<Step> steps = {Step(0, -1, 0), Step(-1, 0, 0), Step(0, 1, 0), Step(1, 0, 0), Step(0, 0, 1)};

    long long currX = moveX(occupator.x) / m_regWidth;
    long long currY = moveY(occupator.y) / m_regLength;
    long long currZ = moveZ(occupator.z) / m_regHeight;

    for (auto step: steps) {
        long long newX = currX + step.x;
        long long newY = currY + step.y;
        long long newZ = currZ + step.z;

        // Checking if robot will cross border of the "world"
        if (newX >= dimOX || newX < 0 || newY >= dimOY || newY < 0 || newZ >= dimOZ || newZ < 0)
            continue;

        // Returnes center of free region
        if (m_regions[newZ][newY][newX]->isFree())
            return tf::Vector3((newX + 0.5) * m_regWidth  - m_offsetOX,
                               (newY + 0.5) * m_regLength - m_offsetOY,
                               (newZ + 0.5) * m_regHeight - m_offsetOZ);
    }

    return tf::Vector3(occupator.x, occupator.y, occupator.z);
}


double World::getOXMin() const {
    return -m_offsetOX;
}

double World::getOYMin() const {
    return -m_offsetOY;
}

double World::getOZMin() const {
    return -m_offsetOZ;
}

double World::getOXMax() const {
    return dimOX * m_regWidth - m_offsetOX;
}

double World::getOYMax() const {
    return dimOY * m_regLength - m_offsetOY;
}

double World::getOZMax() const {
    return dimOZ * m_regHeight - m_offsetOZ;
}